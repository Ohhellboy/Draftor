#!/usr/bin/env python3
"""Create a fresh complete candidate consumption directory; never edit a pin pool.

This Host operation does not activate a device release. Work must accept the
candidate before ADAPTER changes a target combination. No JSON is trusted by
the runtime: the matching release_pin.hpp must be compiled into the consumer.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def stage(candidate, logical, legacy_transport, destination, lock):
    if destination.exists():
        raise RuntimeError('destination already exists; never modify an active or frozen dependency pool')
    binding = json.loads((candidate / 'RELEASE_BINDING.json').read_text())
    if binding['release_id'] != lock['release_id']:
        raise RuntimeError('candidate release identity mismatch')
    files = {}
    for name, expected in lock['accepted_hbo2'].items():
        files[name] = (candidate / name, expected)
    for name, expected in lock['unchanged_proofs'].items():
        files[name] = (legacy_transport / name, expected)
    for name, expected in lock['inputs'].items():
        if name.endswith('.hnm'):
            files[name] = (logical / name, expected)
    files['release_pin.hpp'] = (candidate / 'release_pin.hpp', binding['pin_sha256'])
    files['release_manifest.json'] = (candidate / 'release_manifest.json', binding['manifest_sha256'])
    for name, (source, expected) in files.items():
        if not source.is_file() or digest(source) != expected:
            raise RuntimeError('missing/mismatched candidate dependency: ' + name)
    destination.mkdir()
    pending = destination / 'STAGING_INCOMPLETE'
    pending.write_text('Do not mount: staging or verification has not completed.\n')
    for name, (source, expected) in files.items():
        target = destination / name
        shutil.copyfile(source, target)
        if digest(target) != expected:
            raise RuntimeError('copy verification failed: ' + name)
    shutil.copyfile(candidate / 'RELEASE_BINDING.json', destination / 'RELEASE_BINDING.json')
    (destination / 'CANDIDATE_READY.json').write_text(json.dumps({
        'release_id': lock['release_id'], 'state': 'STAGED_FOR_REVIEW_NOT_ACTIVATED',
        'runtime_public_api': lock['public_api'], 'resource_files': len(files) - 2,
        'compiled_pin_sha256': binding['pin_sha256'],
        'rollback': 'Deactivate all readers/cursors; mount the complete prior release with its prior compiled pin.'
    }, indent=2) + '\n')
    pending.unlink()  # Only our marker, after every required file is verified.


def main():
    parser = argparse.ArgumentParser()
    for name in ('candidate', 'logical', 'legacy-transport', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    lock = json.loads((Path(__file__).resolve().parent.parent / 'release_inputs.json').read_text())
    stage(args.candidate.resolve(strict=True), args.logical.resolve(strict=True),
          args.legacy_transport.resolve(strict=True), args.out.resolve(), lock)
    print('STAGED', lock['release_id'], 'complete dependency set; activation NOT authorized')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        raise SystemExit('STAGE REJECT: ' + str(error)) from error
