#!/usr/bin/env python3
"""Copy only seven SHA-pinned Basic Host consumer inputs, never its core dependency."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('basic_host_source', type=Path)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
pins = re.findall(r'require_basic_file\((\S+) ([0-9a-f]{64})\)',
                  (root / 'examples/startup_consumer/CMakeLists.txt').read_text())
assert len(pins) == 7
out = root / 'dependencies/basic_host_v0_5'
records = []
for name, expected in pins:
    source, target = args.basic_host_source.resolve() / name, out / name
    actual = hashlib.sha256(source.read_bytes()).hexdigest()
    assert actual == expected, str(source)
    if target.exists():
        assert hashlib.sha256(target.read_bytes()).hexdigest() == expected
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    records.append({'path': name, 'sha256': actual})
(out / 'FROZEN_FILES.json').write_text(json.dumps({
    'version': 'Basic Host v0.5, seven-input consumer-only snapshot',
    'source_package_sha256': '7ebeef6c4d1a10508cd5f0c4414193f820c6aecd14f9b43bd35bba6ddf71c36b',
    'files': records, 'core_dependency_copied': False,
    'policy': 'Immutable test dependency, not a maintenance fork. Source hashes checked by consumer CMake.'
}, indent=2) + '\n')
print('PASS seven frozen consumer inputs; maintenance source unchanged')
