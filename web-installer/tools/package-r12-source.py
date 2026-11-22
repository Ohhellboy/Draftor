#!/usr/bin/env python3
"""Deterministic allowlisted corresponding source; never includes delivery/private evidence."""
import hashlib,json,subprocess,sys,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);commit=sys.argv[2]
identity=root/'SOURCE_IDENTITY.json'
if identity.exists():
    record=json.loads(identity.read_text());assert record['sourceCommit']==commit;names=sorted(record['files'])
else:
    tracked=subprocess.check_output(['git','ls-files','-z'],cwd=str(root)).decode().split('\0')
    exact={'index.html','interrupted.html','recovery.html','package.json','package-lock.json','release-lock.json','factory-lock.json','tools/verify-release.mjs','tools/build-r9.mjs','tools/package-r9-source.py','tools/validate-r9-pages.mjs','tools/serve-r9.mjs','tools/r9-software-check.mjs','tools/build-r9-ui-test.mjs','tools/r9-file-probe.html','docs/R9_BUILD.md','docs/RECOVERY_GUIDE.zh-CN.md','docs/RECOVERY_GUIDE.en.md','docs/WEB_LICENSE.md','docs/R10_BUILD.md','docs/R11_BUILD.md'}
    names=sorted(n for n in tracked if n and (n in exact or (n.startswith('tools/') and ('r10' in n or 'r11' in n or 'r12' in n or n.endswith('inherited-core.json'))) or n.startswith(('docs/r10/','docs/r11/','docs/r12/')) or n.startswith(('src/','public/','tests/','reference/','release-inputs/')))
        and n!='release-inputs/BRIEF.md' and not(n.startswith('public/sources/') and n!='public/sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip')
        and not(n.startswith('release-inputs/') and n.endswith('_Resources.zip')))
    factory='release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin'
    assert hashlib.sha256((root/factory).read_bytes()).hexdigest()=='d0591315a722d33f4a08931a0341ab840a6c15c56b289d621e8fc18bec8d55a8'
    names=sorted(set(names+[factory]));record={'sourceCommit':commit,'files':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in names}}
assert names
for n in names:
    p=root/n;assert not n.startswith('/') and '..' not in Path(n).parts and p.is_file() and not p.is_symlink()
    assert hashlib.sha256(p.read_bytes()).hexdigest()==record['files'][n]
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'x',compression=zipfile.ZIP_DEFLATED) as z:
    for n in names+['SOURCE_IDENTITY.json']:
        b=(root/n).read_bytes() if n!='SOURCE_IDENTITY.json' else (json.dumps(record,indent=2,sort_keys=True)+'\n').encode()
        info=zipfile.ZipInfo(n,(2026,11,22,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16;z.writestr(info,b)
print(json.dumps({'sourceCommit':commit,'files':len(names),'sha256':hashlib.sha256(out.read_bytes()).hexdigest()}))
