#!/usr/bin/env python3
"""Deterministic corresponding-source ZIP: tracked, allowlisted inputs only."""
import hashlib,json,subprocess,sys,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]); commit=sys.argv[2]
identity=root/'SOURCE_IDENTITY.json'
if identity.exists():
    record=json.loads(identity.read_text()); assert record['sourceCommit']==commit
    names=sorted(record['files'])
else:
    names=subprocess.check_output(['git','ls-files','-z'],cwd=str(root)).decode().split('\0')
    exact={'index.html','package.json','package-lock.json','release-lock.json','factory-lock.json',
           'tools/build-r7.mjs','tools/build-r7-candidate.mjs','docs/RECOVERY_R7.html','tools/package-r7-source.py','tools/verify-release.mjs','docs/WEB_LICENSE.md','docs/R7_BUILD.md'}
    names=sorted(n for n in names if n and (n in exact or n.startswith(('src/','public/','release-inputs/','reference/')))
                 and n not in {'src/demo.js','src/simulator.js','src/main.js','release-inputs/BRIEF.md','public/sources/Draftor_Web_Installer_R1_source.zip'}
                 and not n.startswith('release-inputs/factory/') and not (n.startswith('release-inputs/') and n.endswith('_Resources.zip')))
    record={'sourceCommit':commit,'files':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in names}}
assert names and all((root/n).is_file() and not (root/n).is_symlink() for n in names)
for n in names:
    assert hashlib.sha256((root/n).read_bytes()).hexdigest()==record['files'][n]
    assert not n.startswith('/') and '..' not in Path(n).parts
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'x',compression=zipfile.ZIP_DEFLATED) as z:
    for n in names+['SOURCE_IDENTITY.json']:
        b=(root/n).read_bytes() if n!='SOURCE_IDENTITY.json' else (json.dumps(record,indent=2,sort_keys=True)+'\n').encode()
        info=zipfile.ZipInfo(n,date_time=(2026,11,21,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
        z.writestr(info,b)
print(json.dumps({'sourceCommit':commit,'files':len(names),'sha256':hashlib.sha256(out.read_bytes()).hexdigest()}))
