#!/usr/bin/env python3
"""Publish this fixed candidate from validated HNS/HNM, without experimental paths.

The shipped input lock is a Host release-preparation dependency. The device must
use the compiled publisher-generated pin, never import trust from TF JSON.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--publisher', required=True, type=Path)
    parser.add_argument('--logical', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent.parent
    lock = json.loads((source / 'release_inputs.json').read_text())
    logical = args.logical.resolve(strict=True)
    output = args.out.resolve()
    require(not output.exists(), 'candidate output must be new; accepted publications are immutable')
    for name, expected in lock['inputs'].items():
        require(digest(logical / name) == expected, 'fixed input identity mismatch: ' + name)
    command = [str(args.publisher.resolve(strict=True)), '--shared', str(logical / 'shared.hns'),
               '--namespace', str(lock['namespace']), '--bundle', str(lock['shared_bundle_id']),
               '--scheme', 'xiaohe', str(logical / 'xiaohe.hnm'),
               '--scheme', 'full_pinyin', str(logical / 'full_pinyin.hnm'),
               '--base-order', '--base-order-format', '2', '--packed-auth', '--out', str(output)]
    subprocess.run(command, check=True)
    manifest = json.loads((output / 'release_manifest.json').read_text())
    require(manifest['status'] == 'HOST_FINAL_BYTES_VALIDATED', 'publisher did not qualify final bytes')
    require(len(manifest['files']) == 5, 'unexpected release role set')
    for item in manifest['files']:
        require(item['namespace'] == lock['namespace'] and item['shared_bundle_id'] == lock['shared_bundle_id']
                and item['runtime_ref_count'] == lock['runtime_ref_count'], 'release identity changed')
        if item['role'] == 'base_order':
            require(item['resource_format'] == 2 and item['verification_format'] == 1
                    and item['directory_and_tail_equivalent'], 'missing HBO2 qualification')
            require(item['derived_source_sha256'] == lock['validated_hbo1'][item['logical_name']],
                    'direct formal facts did not reconstruct frozen HBO1')
        else:
            require(item['file_sha256'] == lock['inputs'][item['source_file']], 'primary resource changed')
    for group in ('unchanged_proofs', 'accepted_hbo2'):
        for name, expected in lock[group].items():
            require(digest(output / name) == expected, 'accepted derivation differs: ' + name)
    # Metadata records the issuer's result, not a source from which to trust a root.
    binding = {'release_id': lock['release_id'], 'runtime_public_api': lock['public_api'],
               'state': 'RELEASE_CANDIDATE_ONLY', 'production_migration': 'REQUIRES_WORK_ACCEPTANCE',
               'input_lock_sha256': digest(source / 'release_inputs.json'),
               'pin_sha256': digest(output / 'release_pin.hpp'),
               'manifest_sha256': digest(output / 'release_manifest.json'),
               'input_files': lock['inputs'],
               'transport_files': {name: digest(output / name)
                                   for name in sorted({**lock['unchanged_proofs'], **lock['accepted_hbo2']})}}
    (output / 'RELEASE_BINDING.json').write_text(json.dumps(binding, indent=2) + '\n')
    print('CANDIDATE GENERATED', lock['release_id'], 'HBO1 reconstruction/HBO2 bytes/proofs PASS')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit('CANDIDATE REJECT: ' + str(error)) from error
