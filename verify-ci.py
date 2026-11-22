import hashlib,json,zipfile
from pathlib import Path
root=Path(__file__).resolve().parent
if (root/'PAYLOAD_SHA256.json').exists():
 for n,h in json.loads((root/'PAYLOAD_SHA256.json').read_text()).items():
  assert hashlib.sha256((root/n).read_bytes()).hexdigest()==h,n
out=root/'work'/'test-results';out.mkdir(parents=True,exist_ok=True)
site=root/'work'/'dist-r11-public'; frozen=root/'frozen'
diff=[];same=[]
for f in frozen.rglob('*'):
 if not f.is_file():continue
 n=f.relative_to(frozen).as_posix()
 if n in ['SITE_SHA256SUMS','sources/Draftor_Web_R11_source.zip']:continue
 if f.read_bytes()!=(site/n).read_bytes():diff.append(n)
 else:same.append(n)
with zipfile.ZipFile(root/'source.zip') as a,zipfile.ZipFile(site/'sources/Draftor_Web_R11_source.zip') as b:
 assert set(a.namelist())==set(b.namelist())
 assert all(a.read(n)==b.read(n) for n in a.namelist())
result={'runtimeAssetsMatch':not diff,'filesCompared':len(same)+len(diff),'mismatches':diff,'sourceArchiveAllEntriesEqual':True,'sourceArchiveContainerMayDifferByHostZipMetadata':True,'sourceCommit':json.loads((site/'BUILD.json').read_text())['sourceCommit']}
(out/'windows-source-rebuild.json').write_text(json.dumps(result,indent=2))
assert not diff,diff
print(json.dumps(result))
