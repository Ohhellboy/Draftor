#!/usr/bin/env python3
"""Freeze a complete new release, CRC/hash-check it, and test the FINAL ZIP once.

Builds are explicitly outside the source tree. No downloads, no source rewrite
from an older ZIP, no automatic deletion. --work and --output must be new paths.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--query-repair', action='store_true', help='Package API 0.10.0 query increment and its public consumer')
    p.add_argument('--visible-text', action='store_true', help='Package API 0.10.1 visible-text increment and Basic Host 0.6 consumer')
    p.add_argument('--packed-auth', action='store_true', help='Package API 0.11.0 F2 PackedAuth increment and selected mixed release')
    p.add_argument('--output-perf', action='store_true', help='Package API 0.12.0 output/anchor reuse and unchanged mixed release')
    args = p.parse_args()
    root, output, work = args.root.resolve(), args.output.resolve(), args.work.resolve()
    assert not output.exists() and not work.exists(), 'new release and clean work paths required'
    assert root not in work.parents and root != work, 'builds outside source tree'
    assert sum((args.visible_text, args.query_repair, args.packed_auth, args.output_perf)) <= 1, 'select one release mode'
    # Output-perf keeps all prior format-1/2 and public compatibility gates.
    if args.output_perf: args.packed_auth = True
    version = '0.11.0' if args.packed_auth else ('0.10.1' if args.visible_text else ('0.10.0' if args.query_repair else '0.9.0'))
    if args.output_perf: version = '0.12.0'
    assert 'VERSION ' + version in (root / 'CMakeLists.txt').read_text(), 'release mode/core version mismatch'
    evidence_name = 'visible_text_io_v2' if args.packed_auth else ('visible_text_io_v1' if args.visible_text else ('query_repair_v1' if args.query_repair else 'startup_repair_v1'))
    if args.output_perf: evidence_name = 'output_perf_v1'
    selected = set(json.loads((root / 'evidence/COMPOSITION_PACKAGE_SELECTION.json').read_text())['files'])
    selected.discard('PACKAGE_CONTENTS.sha256')
    for prefix in ['src', 'include', 'tests', 'tools', 'examples', 'resources', 'registry', 'generated_fixture', 'dependencies']:
        for file in (root / prefix).rglob('*'):
            if file.is_file() and file.name != '.DS_Store' and '__pycache__' not in file.parts:
                selected.add(file.relative_to(root).as_posix())
    evidence_dirs = ['startup_repair_v1']
    if args.query_repair or args.visible_text or args.packed_auth:
        evidence_dirs += ['startup_review_received_v1', 'query_repair_v1']
    if args.visible_text or args.packed_auth:
        evidence_dirs += ['visible_text_io_v1']
    if args.packed_auth:
        evidence_dirs += ['visible_text_io_v2']
    if args.output_perf:
        evidence_dirs += ['output_perf_v1']
        selected.add('OUTPUT_PERF_HANDOFF.md')
    for directory in evidence_dirs:
        for file in (root / 'evidence' / directory).rglob('*'):
            rel = file.relative_to(root / 'evidence' / directory)
            if file.is_file() and not any(part in {'before', 'record_before', 'record_before_final', '__pycache__'} for part in rel.parts):
                selected.add(file.relative_to(root).as_posix())
    selected.update(['README.md', 'REPORT.md', 'PUBLIC_API.md', 'IME_ARCHITECTURE.md', 'CMakeLists.txt', 'STARTUP_HANDOFF.md',
                     'evidence/COMPOSITION_USER_ACCEPTANCE.md', 'evidence/COMPOSITION_ACCEPTANCE_VERIFY.json'])
    if args.query_repair or args.visible_text or args.packed_auth:
        selected.add('QUERY_HANDOFF.md')
    if args.visible_text or args.packed_auth:
        selected.add('VISIBLE_TEXT_HANDOFF.md')
    if args.packed_auth:
        # User corrected ownership: keep this workspace background, not core delivery.
        selected.difference_update({
            'evidence/visible_text_io_v2/COMBINED_DELIVERY_REQUIREMENTS.md',
            'evidence/visible_text_io_v2/BACKGROUND_SCOPE_CORRECTION.md'})
    selection = root / 'evidence' / evidence_name / 'PACKAGE_SELECTION.json'
    selected.add(selection.relative_to(root).as_posix())
    selection.write_text(json.dumps({'version': version, 'files': sorted(selected),
        'exclusions': 'Build/toolchain directories, temporary unpack trees, before snapshots, old engineering release ZIPs. Frozen resource sources and selected review evidence are retained.',
        'self_contained_basic_host': 'seven hash-pinned consumer inputs (3 cpp, 3 headers, syllable include), no second core'}, indent=2) + '\n')
    assert all((root / name).is_file() for name in selected)
    assert not any('..' in Path(name).parts or Path(name).is_absolute() for name in selected)
    hashes = {name: sha(root / name) for name in sorted(selected)}
    (root / 'PACKAGE_CONTENTS.sha256').write_text(''.join(h + '  ' + n + '\n' for n, h in hashes.items()))
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name in sorted(selected | {'PACKAGE_CONTENTS.sha256'}):
            z.write(root / name, 'HANIME_Independent_Core/' + name)
    package_sha = sha(output)
    work.mkdir(parents=True)
    with zipfile.ZipFile(output) as z:
        assert z.testzip() is None
        assert set(z.namelist()) == {'HANIME_Independent_Core/' + n for n in selected | {'PACKAGE_CONTENTS.sha256'}}
        for name, expected in hashes.items():
            assert hashlib.sha256(z.read('HANIME_Independent_Core/' + name)).hexdigest() == expected, name
        z.extractall(work / 'unpacked')
    clean = work / 'unpacked/HANIME_Independent_Core'
    logs = output.with_name(output.stem + '_CLEAN')
    logs.mkdir()
    commands = []

    def run(label, command):
        with (logs / (label + '.log')).open('w') as log:
            proc = subprocess.run(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT)
        commands.append({'label': label, 'argv': list(map(str, command)), 'exit': proc.returncode})
        (logs / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert proc.returncode == 0, label + ' failed; see ' + str(logs)

    run('CORE', [sys.executable, clean / 'tools/run_startup_validation.py', '--root', clean,
                 '--build', work / 'core_build', '--logs', logs / 'core'])
    cmake = shutil.which('cmake')
    assert cmake
    run('CONSUMER_CONFIGURE', [cmake, '-S', clean / 'examples/startup_consumer', '-B', work / 'consumer_build',
        '-DHANIME_CORE_PATH=' + str(clean), '-DBASIC_HOST_PATH=' + str(clean / 'dependencies/basic_host_v0_5'),
        '-DHANIME_RELEASE_PIN_DIR=' + str(clean / 'resources/verified_v1'), '-DCMAKE_BUILD_TYPE=Release'])
    run('CONSUMER_BUILD', [cmake, '--build', work / 'consumer_build', '--parallel', '4'])
    run('CONSUMER_RUN', [work / 'consumer_build/startup_consumer', clean / 'resources/production/order_a', clean / 'resources/verified_v1'])
    run('COST_GATE', [sys.executable, clean / 'tools/summarize_startup_costs.py', logs / 'CONSUMER_RUN.log'])
    run('INDEX_ORACLE', [sys.executable, clean / 'tools/verify_release_index.py', clean / 'resources/production/order_a', clean / 'resources/verified_v1'])
    if args.query_repair:
        run('QUERY_CONSUMER', [sys.executable, clean / 'tools/run_query_consumer.py', '--root', clean,
            '--build', work / 'query_consumer', '--logs', logs / 'query_consumer'])
        run('QUERY_INDEX_ORACLE', [sys.executable, clean / 'tools/verify_release_index.py',
            clean / 'resources/production/order_a', clean / 'resources/base_order_v1'])
    if args.visible_text or args.packed_auth:
        run('VISIBLE_CONSUMER', [sys.executable, clean / 'tools/run_visible_text_consumer.py', '--root', clean,
            '--build', work / 'visible_consumer', '--logs', logs / 'visible_consumer'])
        run('QUERY_INDEX_ORACLE', [sys.executable, clean / 'tools/verify_release_index.py',
            clean / 'resources/production/order_a', clean / 'resources/base_order_v1'])
    if args.packed_auth:
        run('PACKED_CONSUMER', [sys.executable, clean / 'tools/run_visible_text_consumer.py', '--root', clean,
            '--release', clean / 'resources/packed_auth_v2', '--build', work / 'visible_consumer',
            '--logs', logs / 'packed_consumer'])
        run('PACKED_INDEX_ORACLE', [sys.executable, clean / 'tools/verify_release_index.py',
            clean / 'resources/production/order_a', clean / 'resources/packed_auth_v2'])
    if args.output_perf:
        for case in ('all', 'reuse'):
            run('OUTPUT_PERF_' + case.upper(), [sys.executable, clean / 'tools/run_visible_text_consumer.py',
                '--root', clean, '--release', clean / 'resources/packed_auth_v2',
                '--build', work / 'visible_consumer', '--logs', logs / ('output_perf_' + case),
                '--profile', 'balanced', '--case', case])
            actual = json.loads((logs / ('output_perf_' + case) / 'METRICS.json').read_text())
            label = 'balanced' if case == 'all' else 'balanced_reuse'
            reference = json.loads((clean / 'evidence/output_perf_v1/final_matrix' / label / 'METRICS.json').read_text())
            for key in ('ITEM', 'MEMBER', 'DOCUMENT', 'PAGING', 'STATE'):
                assert actual.get(key, []) == reference.get(key, []), key + ' final package drift'
            for x, y in zip(actual['COST'], reference['COST'], strict=True):
                assert all(x[k] == y[k] for k in ('phase', 'reads', 'bytes', 'seeks', 'steps', 'work'))
    unchanged = all(sha(clean / name) == expected for name, expected in hashes.items())
    assert unchanged and sha(output) == package_sha
    result = {'package': str(output), 'bytes': output.stat().st_size, 'sha256': package_sha,
        'crc': 'PASS', 'crc_files': len(selected) + 1, 'content_sha256': 'PASS', 'content_files': len(selected),
        'exact_coverage_except_manifest_self': True, 'clean_directory': str(clean), 'clean_test': 'PASS',
        'clean_scope': ('Ordinary full CTest23; Basic Host0.6 legacy355, PackedAuth-off355, balanced355+reuse48; Basic Host0.5 and release oracles; final outputs and deterministic costs match packaged evidence' if args.output_perf else
                       'Ordinary full CTest22, frozen Basic Host 0.6 outside-core 355 phases with selected PackedAuth and legacy formats, Basic Host 0.5 compatibility and all release oracles' if args.packed_auth else
                       'Ordinary full CTest21, frozen Basic Host 0.6 outside-core full visible-text consumer, Basic Host 0.5 compatibility and five-index oracle' if args.visible_text else
                       'Ordinary full CTest20, outside-core differential/bounded query consumer, fixed Basic Host compatibility and five-index oracle' if args.query_repair else
                        'Ordinary full CTest19 plus fixed Basic Host public consumer and index hashlib oracle') + '; no clean sanitizer repeat',
        'extracted_files_unchanged': unchanged, 'commands': commands, 'device_30s': 'OPEN', 'logs': str(logs)}
    output.with_name(output.stem + '_VERIFY.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2), flush=True)


if __name__ == '__main__':
    main()
