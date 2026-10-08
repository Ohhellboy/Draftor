#!/usr/bin/env python3
"""Package explicit review inputs. Never includes build trees, dependencies or archives."""
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
ALLOWED_DIRS={'include','src','host','tests','examples','scripts','schema','evidence'}
EXCLUDED={'__pycache__','.pytest_cache','.DS_Store'}
archive=ROOT/'dist/Draftor_Behavior_Telemetry_v0_1_REVIEW.zip'
archive.parent.mkdir(exist_ok=True)
files=[]
for path in sorted(ROOT.rglob('*')):
    relative=path.relative_to(ROOT)
    if path.is_symlink(): raise SystemExit('Review tree must not contain symlinks: '+str(relative))
    if not path.is_file() or any(part in EXCLUDED for part in relative.parts): continue
    if len(relative.parts)==1 and (path.suffix=='.md' or path.name in ('CMakeLists.txt','.gitignore')):
        files.append(path)
    elif relative.parts[0] in ALLOWED_DIRS and path.suffix not in ('.pyc','.zip'):
        files.append(path)
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as output:
    for path in files: output.write(path,ROOT.name+'/'+str(path.relative_to(ROOT)))
with zipfile.ZipFile(archive) as output:
    if output.testzip() is not None: raise SystemExit('Archive verification failed')
print(f'{archive}\n{len(files)} files; {archive.stat().st_size} bytes; ZIP integrity PASS (no hash generation)')
