#!/usr/bin/env python3
"""Audit this task's saved baseline without reading old IME implementations."""
import difflib
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
evidence = root / 'evidence/startup_repair_v1'
baseline = json.loads((evidence / 'BASELINE.json').read_text())['core_working_hashes']

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

checks = {name: {'before': expected, 'after': sha(root / name),
                'same': sha(root / name) == expected} for name, expected in baseline.items()}
protected = {name: value for name, value in checks.items()
             if name.startswith(('resources/', 'registry/', 'generated_fixture/'))}
assert all(value['same'] for value in protected.values()), 'frozen resource/registry changed'
before = evidence / 'before/project'
prefixes = ['src', 'include', 'tests', 'tools', 'examples', 'dependencies']
names = {'CMakeLists.txt', 'README.md', 'REPORT.md', 'PUBLIC_API.md', 'IME_ARCHITECTURE.md', 'STARTUP_HANDOFF.md'}
for prefix in prefixes:
    for base in [root / prefix, before / prefix]:
        if base.exists():
            for file in base.rglob('*'):
                if file.is_file() and file.name != '.DS_Store' and '__pycache__' not in file.parts:
                    names.add(prefix + '/' + file.relative_to(base).as_posix())
patch = []
changed, added = [], []
for name in sorted(names):
    old, new = before / name, root / name
    if '/' not in name and not old.exists():
        old = evidence / 'before' / name
    if name == 'tools/audit_startup_changes.py':
        pass  # The tool is intentionally part of its own source audit.
    a = old.read_text() if old.exists() else ''
    b = new.read_text() if new.exists() else ''
    if a != b:
        (changed if old.exists() else added).append(name)
        patch.extend(difflib.unified_diff(a.splitlines(True), b.splitlines(True),
                     fromfile='before/' + name if old.exists() else '/dev/null', tofile='after/' + name))
(evidence / 'CHANGES.diff').write_text(''.join(patch))
(evidence / 'CHANGE_AUDIT.json').write_text(json.dumps({
    'task': 'STARTUP_REPAIR_V1_0', 'baseline_check_count': len(checks), 'baseline': checks,
    'protected_resource_registry_count': len(protected), 'protected_unchanged': True,
    'changed_existing_source_docs': changed, 'added_source_docs_dependencies': added,
    'scope': 'Saved independent-core baseline only; no old IME implementation read. New binary sidecars separately hashed by manifest.'
}, indent=2) + '\n')
records = []
record_audit = {}
for name in ['00-当前状态.md', '01-工程日志.md', '02-设计决策.md', '03-待验证事项.md']:
    old, current = evidence / 'record_before_final' / name, root.parent / name
    if old.exists():
        a, b = old.read_text(), current.read_text()
        records.extend(difflib.unified_diff(a.splitlines(True), b.splitlines(True),
                       fromfile='before/' + name, tofile='after/' + name))
        record_audit[name] = {'before': sha(old), 'after': sha(current), 'changed': a != b}
if record_audit:
    (evidence / 'WORKSPACE_RECORDS.diff').write_text(''.join(records))
    (evidence / 'WORKSPACE_RECORDS.json').write_text(json.dumps(record_audit, indent=2) + '\n')
print('PASS baseline protected data:', len(protected), 'changed:', len(changed), 'added:', len(added))
