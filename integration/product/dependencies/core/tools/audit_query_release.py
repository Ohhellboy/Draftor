#!/usr/bin/env python3
"""Record exact in-scope changes against the opening snapshot and frozen API090.

The small opening snapshot contains this core's code/docs, not a legacy IME.
Authoritative binary resource protection is checked against the accepted ZIP.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--root', type=Path, required=True)
p.add_argument('--before', type=Path, required=True)
p.add_argument('--baseline-zip', type=Path, required=True)
p.add_argument('--logs', type=Path, required=True)
a = p.parse_args()
root, before, logs = a.root.resolve(), a.before.resolve(), a.logs.resolve()
logs.mkdir(parents=True, exist_ok=True)
baseline_hash = hashlib.file_digest(a.baseline_zip.open('rb'), 'sha256').hexdigest()
assert baseline_hash == 'ae277c7f36ed6555dfb1ec5c406667d0347d458569fd763817b42f52cfc1248d'
protected = {}
with zipfile.ZipFile(a.baseline_zip) as z:
    for info in z.infolist():
        name = info.filename.removeprefix('HANIME_Independent_Core/')
        if name.startswith(('resources/production/', 'resources/verified_v1/', 'registry/', 'dependencies/')) and not info.is_dir():
            old = hashlib.sha256(z.read(info)).hexdigest()
            with (root / name).open('rb') as f:
                new = hashlib.file_digest(f, 'sha256').hexdigest()
            assert old == new, 'protected change: ' + name
            protected[name] = new
selected = set()
for directory in ('src', 'include', 'tests', 'tools', 'examples'):
    for base in (root, before):
        selected |= {f.relative_to(base).as_posix() for f in (base / directory).rglob('*')
                     if f.is_file() and '__pycache__' not in f.parts and f.name != '.DS_Store'}
selected |= {'CMakeLists.txt', 'IME_ARCHITECTURE.md', 'PUBLIC_API.md', 'REPORT.md',
             'README.md', 'STARTUP_HANDOFF.md', 'QUERY_HANDOFF.md'}
changes, diff = [], []
for name in sorted(selected):
    old = (before / name).read_bytes() if (before / name).exists() else b''
    new = (root / name).read_bytes() if (root / name).exists() else b''
    if old == new:
        continue
    changes.append(dict(file=name, before_sha=hashlib.sha256(old).hexdigest() if old else None,
                        after_sha=hashlib.sha256(new).hexdigest() if new else None,
                        added=not (before / name).exists(), deleted=not (root / name).exists()))
    try:
        diff.extend(difflib.unified_diff(old.decode().splitlines(True), new.decode().splitlines(True),
                                       fromfile='before/' + name, tofile='after/' + name))
    except UnicodeDecodeError:
        diff.append('Binary file: ' + name + '\n')
(logs / 'DIFF.patch').write_text(''.join(diff))
(logs / 'CHANGE_AUDIT.json').write_text(json.dumps(dict(baseline_sha=baseline_hash,
    before_snapshot=str(before), protected_unchanged=protected, changes=changes,
    review_status='LOCAL HOST VERIFIED / INDEPENDENT REVIEW PENDING'), indent=2) + '\n')
print('PASS protected files:', len(protected), 'changed/new text files:', len(changes))
