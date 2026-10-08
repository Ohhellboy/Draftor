#!/usr/bin/env python3
"""Compare the visible-text increment to the accepted, frozen API 0.10.0 ZIP.

Only evidence is written. Never restore old source/documents or mutate the ZIP.
Use Python 3.11+; normal ZIP CRC and content hashes are both checked.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, output = args.root.resolve(), args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    expected_zip = '83e45d08d343740341a1c8cbb3a87f0bd2b65434849f25758ce2c5cd39e021c1'
    with args.baseline.open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == expected_zip
    allowed = {'src/paged_shared_resource.cpp',
               'include/hanime/paged_shared_resource.hpp'}
    protected, changed, patches = [], [], []
    with zipfile.ZipFile(args.baseline) as archive:
        assert archive.testzip() is None
        prefix = 'HANIME_Independent_Core/'
        manifest = archive.read(prefix + 'PACKAGE_CONTENTS.sha256').decode()
        hashes = dict((line.split('  ', 1)[1], line.split('  ', 1)[0])
                      for line in manifest.splitlines())
        assert set(archive.namelist()) == {prefix + n for n in hashes} | {prefix + 'PACKAGE_CONTENTS.sha256'}
        for name, old_hash in hashes.items():
            before = archive.read(prefix + name)
            assert digest(before) == old_hash, name
            current = root / name
            if name.startswith(('src/', 'include/', 'resources/', 'registry/', 'generated_fixture/')):
                assert current.is_file(), 'protected file missing: ' + name
                after_hash = digest(current.read_bytes())
                if name not in allowed:
                    assert old_hash == after_hash, 'protected content changed: ' + name
                protected.append(dict(file=name, before=old_hash, after=after_hash))
            if name.startswith(('src/', 'include/', 'tests/', 'tools/', 'examples/')) or name in {
                    'CMakeLists.txt', 'README.md', 'PUBLIC_API.md', 'IME_ARCHITECTURE.md', 'REPORT.md'}:
                assert current.is_file(), 'baseline file missing: ' + name
                after = current.read_bytes()
                if before != after:
                    changed.append(dict(file=name, before=old_hash, after=digest(after)))
                    patches.extend(difflib.unified_diff(before.decode().splitlines(True),
                        after.decode().splitlines(True), 'a/' + name, 'b/' + name))
        additions = []
        for folder in ('tests', 'tools', 'examples'):
            for file in sorted((root / folder).rglob('*')):
                name = file.relative_to(root).as_posix()
                if file.is_file() and name not in hashes and '__pycache__' not in file.parts:
                    additions.append(dict(file=name, sha256=digest(file.read_bytes())))
                    patches.extend(difflib.unified_diff([], file.read_text().splitlines(True), '/dev/null', 'b/' + name))
    production_changes = [r['file'] for r in changed if r['file'].startswith(('src/', 'include/'))]
    assert set(production_changes) == allowed, production_changes
    (output / 'DIFF.patch').write_text(''.join(patches))
    (output / 'CHANGE_AUDIT.json').write_text(json.dumps(dict(
        baseline_sha256=expected_zip, baseline_crc='PASS', baseline_manifest='PASS',
        protected=protected, changed=changed, additions=additions,
        resource_registry_format_and_pin_bytes='UNCHANGED',
        note='Header change is config documentation only; public declarations unchanged. No claim about other-window mutable work.'), indent=2) + '\n')
    print('PASS protected resource/registry/core comparison:', len(protected), 'files')


if __name__ == '__main__':
    main()
