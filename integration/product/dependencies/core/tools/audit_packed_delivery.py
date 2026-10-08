#!/usr/bin/env python3
"""Compare the F2 maintenance tree with its immutable accepted ZIP and publishes.

Read-only with respect to source/resources/baseline; writes only audit evidence.
The original engineering ZIP is an audit input, never nested in the new release.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--san-publish', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    root, baseline, sanitized, evidence = [p.resolve() for p in
        (args.root, args.baseline, args.san_publish, args.evidence)]
    expected = '93d90eb184a7c1fa2025aee93402e6ae69924681ce58958408f7cd71c020b4d6'
    assert file_sha(baseline) == expected, 'not the accepted 0.10.1 baseline'
    prefixes = ('src/', 'include/', 'tests/', 'tools/', 'examples/', 'resources/',
                'registry/', 'dependencies/', 'generated_fixture/')
    allowed_changed = {
        'CMakeLists.txt', 'examples/composition_consumer/CMakeLists.txt',
        'examples/public_consumer/CMakeLists.txt', 'examples/visible_text_consumer/main.cpp',
        'include/hanime/resource_publisher.hpp', 'include/hanime/verified_resource.hpp',
        'src/resource_publisher.cpp', 'src/verified_resource.cpp', 'tools/PUBLISH_VERIFIED.md',
        'tools/compare_visible_costs.py', 'tools/package_startup.py', 'tools/publish_verified.cpp',
        'tools/run_visible_text_consumer.py', 'tools/verify_release_index.py'}
    docs = {'README.md', 'PUBLIC_API.md', 'IME_ARCHITECTURE.md', 'REPORT.md', 'VISIBLE_TEXT_HANDOFF.md'}
    rows, source_diff, doc_diff = {}, [], []

    def append_diff(before, after, name, target):
        if Path(name).suffix not in {'.cpp', '.hpp', '.py', '.md', '.txt', '.json'}:
            return
        target.extend(difflib.unified_diff(before.decode().splitlines(True),
            after.decode().splitlines(True), fromfile='accepted_0101/' + name,
            tofile='candidate_0110/' + name))

    with zipfile.ZipFile(baseline) as archive:
        assert archive.testzip() is None
        old = {n.removeprefix('HANIME_Independent_Core/'): n for n in archive.namelist()
               if n.startswith('HANIME_Independent_Core/') and not n.endswith('/')}
        for name, member in sorted(old.items()):
            if name != 'CMakeLists.txt' and not name.startswith(prefixes) and name not in docs:
                continue
            path = root / name
            assert path.is_file(), 'missing accepted member: ' + name
            before, after = archive.read(member), path.read_bytes()
            changed = before != after
            rows[name] = dict(before_sha256=digest(before), after_sha256=digest(after),
                              changed=changed, bytes=len(after))
            if changed:
                assert name in allowed_changed or name in docs, 'unexpected change: ' + name
                append_diff(before, after, name, doc_diff if name in docs else source_diff)
        for prefix in prefixes:
            for path in sorted((root / prefix).rglob('*')):
                if not path.is_file() or path.name == '.DS_Store' or '__pycache__' in path.parts:
                    continue
                name = path.relative_to(root).as_posix()
                if name not in old:
                    after = path.read_bytes()
                    rows[name] = dict(added=True, after_sha256=digest(after), bytes=len(after))
                    append_diff(b'', after, name, source_diff)
    publication = {}
    normal = root / 'resources/packed_auth_v2'
    assert {p.name for p in normal.iterdir()} == {p.name for p in sanitized.iterdir()}
    for path in sorted(normal.iterdir()):
        sha = file_sha(path)
        assert sha == file_sha(sanitized / path.name), 'publication differs: ' + path.name
        legacy = root / 'resources/base_order_v1' / path.name
        unchanged = legacy.is_file() and file_sha(legacy) == sha
        publication[path.name] = dict(bytes=path.stat().st_size, sha256=sha,
            ordinary_sanitizer_equal=True, same_as_legacy=unchanged)
    assert len(publication) == 9 and sum(x['same_as_legacy'] for x in publication.values()) == 6
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / 'SOURCE_CHANGES.diff').write_text(''.join(source_diff))
    (evidence / 'DOCUMENT_CHANGES.diff').write_text(''.join(doc_diff))
    result = dict(status='PASS', baseline=dict(path=str(baseline), bytes=baseline.stat().st_size,
        sha256=expected, crc='PASS'), current_source_version='0.11.0',
        unchanged_existing=sum(not x.get('changed', False) and not x.get('added', False)
                               for x in rows.values()),
        changed_existing=[name for name, row in rows.items() if row.get('changed')],
        added=[name for name, row in rows.items() if row.get('added')], files=rows,
        selected_publication=publication,
        scope='Core maintenance tree only; original logical resources, R1, seven frozen Basic Host 0.6 inputs and old tests protected. No adapter/extension working tree writes.',
        scope_excluded='Device background notes retained separately in workspace, not part of core ZIP or implementation')
    (evidence / 'CHANGE_AUDIT.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS protected inputs; 9 ordinary/Sanitizer publication files equal, 6 legacy unchanged')


if __name__ == '__main__':
    main()
