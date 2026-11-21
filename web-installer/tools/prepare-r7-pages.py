#!/usr/bin/env python3
"""Prepare an audited locked-only Pages commit. Never pushes or changes main."""
from pathlib import Path
import hashlib,json,os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]; out=(root/sys.argv[1]).resolve()
assert out.parent==root and out.name.startswith('dist-r7-locked'), 'Only a dedicated locked output is eligible'
build=json.loads((out/'BUILD.json').read_text())
assert build['hardwareUpdateWrites'] is False and build['hardwareFreshWrites'] is False and build['publicInstallation'] is False
assert not build.get('reviewOnly') and not (out/'DO_NOT_DEPLOY.txt').exists()
assert 'ReadOnlyDriver' in (out/'assets/app.js').read_text()
assert (out/'CNAME').read_text()=='draftor.tcssn.org\n'
rows={}
for line in (out/'SITE_SHA256SUMS').read_text().splitlines():
 h,n=line.split('  ',1);p=out/n;assert not p.is_symlink() and hashlib.sha256(p.read_bytes()).hexdigest()==h;rows[n]=h
files=sorted(p for p in out.rglob('*') if p.is_file())
assert {p.relative_to(out).as_posix() for p in files}==set(rows)|{'SITE_SHA256SUMS'}
parent=subprocess.check_output(['git','rev-parse','refs/heads/gh-pages'],cwd=str(root)).decode().strip()
index=str(Path(tempfile.mkdtemp(prefix='draftor-r7-pages-',dir='/private/tmp'))/'index')
env=dict(os.environ,GIT_INDEX_FILE=index)
subprocess.run(['git','read-tree','--empty'],cwd=str(root),env=env,check=True)
entries=[]
for p in files:
 blob=subprocess.check_output(['git','hash-object','-w','--stdin'],cwd=str(root),input=p.read_bytes()).decode().strip()
 entries.append('100644 '+blob+'\t'+p.relative_to(out).as_posix()+'\n')
subprocess.run(['git','update-index','--index-info'],cwd=str(root),env=env,input=''.join(entries).encode(),check=True)
tree=subprocess.check_output(['git','write-tree'],cwd=str(root),env=env).decode().strip()
message='Deploy audited R7 locked site\n\nSource '+build['sourceCommit']+'\nNo public hardware writing or erasure.\n'
commit=subprocess.check_output(['git','-c','user.name=Ohhellboy','-c','user.email=Ohhellboy@users.noreply.github.com','commit-tree',tree,'-p',parent],cwd=str(root),input=message.encode()).decode().strip()
subprocess.run(['git','update-ref','refs/heads/gh-pages',commit,parent],cwd=str(root),check=True)
print(json.dumps({'pagesCommit':commit,'parent':parent,'tree':tree,'sourceCommit':build['sourceCommit'],'files':len(files),'manifestSha256':hashlib.sha256((out/'SITE_SHA256SUMS').read_bytes()).hexdigest()}))
