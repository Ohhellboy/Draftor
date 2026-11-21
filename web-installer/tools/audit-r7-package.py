#!/usr/bin/env python3
import hashlib,json,sys,zipfile
from pathlib import Path
root=Path(sys.argv[1]);rows={}
for line in (root/'SITE_SHA256SUMS').read_text().splitlines():
 h,n=line.split('  ',1);p=root/n;assert p.is_file() and not p.is_symlink();assert hashlib.sha256(p.read_bytes()).hexdigest()==h;rows[n]=h
assert {str(p.relative_to(root)) for p in root.rglob('*') if p.is_file()}==set(rows)|{'SITE_SHA256SUMS'}
assert not any(any(x in n.lower() for x in ['simulation','device-test','first-install-test','full_flash','active_app_slot','private','factoryonly']) for n in rows)
patterns=[b'/Volumes/OnlyRos',b'/Users/wangruotao',b'.draftor_private',b'01a1f3e0-9da0',b'chrome-extension://']
text_ext={'.js','.mjs','.py','.md','.json','.html','.map','.txt','.css'}
checked=0
for p in root.rglob('*'):
 if p.is_file() and p.suffix in text_ext:
  assert not any(x in p.read_bytes() for x in patterns),str(p);checked+=1
with zipfile.ZipFile(root/'sources/Draftor_Web_R7_source.zip') as z:
 assert z.testzip() is None
 for n in z.namelist():
  assert not any(x in n for x in ['.private','evidence/','delivery/','tests/','full_flash','active_app_slot','factory/','dist-'])
  if Path(n).suffix in text_ext:assert not any(x in z.read(n) for x in patterns),n
 record=json.loads(z.read('SOURCE_IDENTITY.json'))
 for n,h in record['files'].items():assert hashlib.sha256(z.read(n)).hexdigest()==h
bundle=(root/'assets/app.js').read_text()
assert 'ReadOnlyDriver' in bundle and 'DEVICE_TEST_UPDATE' not in bundle and 'new S3Rom(transport)' in bundle
m=json.loads((root/'BUILD.json').read_text());assert not m['hardwareUpdateWrites'] and not m['hardwareFreshWrites'] and not m['publicInstallation']
result={'status':'PASS','manifestFiles':len(rows),'textFilesScanned':checked,'sourceFilesVerified':len(record['files']),'writesLocked':True,'noPrivatePayloads':True,'noExecutableSimulationEntry':True,'sourceCommit':m['sourceCommit'],'manifestSha256':hashlib.sha256((root/'SITE_SHA256SUMS').read_bytes()).hexdigest()}
Path('evidence/R7_20261121/locked-package-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
