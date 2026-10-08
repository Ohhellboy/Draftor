#!/usr/bin/env python3
"""Exact source diff and immutable-input audit against accepted API0.11 ZIP."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import zipfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args();root=a.root.resolve();ev=a.evidence.resolve();ev.mkdir(parents=True,exist_ok=True)
expected='7075dd47b44685f641a04aa3765801f609a3694c3445009433ae62e6f40d8968'
with a.baseline.open('rb') as stream: assert hashlib.file_digest(stream,'sha256').hexdigest()==expected
prefixes=('src/','include/','tests/','tools/','examples/','resources/','registry/','dependencies/','generated_fixture/')
docs={'README.md','REPORT.md','PUBLIC_API.md','IME_ARCHITECTURE.md','VISIBLE_TEXT_HANDOFF.md','OUTPUT_PERF_HANDOFF.md'}
allowed={'src/verified_resource.cpp','src/bounded_output.cpp','src/paged_shared_resource.cpp',
         'include/hanime/verified_resource.hpp','include/hanime/bounded_output.hpp','include/hanime/paged_shared_resource.hpp',
         'tests/test_packed_auth.cpp','examples/visible_text_consumer/main.cpp','examples/visible_text_consumer/README.md',
         'examples/public_consumer/CMakeLists.txt','examples/composition_consumer/CMakeLists.txt',
         'CMakeLists.txt','tools/package_startup.py','tools/run_visible_text_consumer.py','tools/PACKED_AUTH_FORMAT.md'}
rows={};sd=[];dd=[]
def record(name,before,after,added=False):
    changed=before!=after
    rows[name]=dict(added=added,changed=changed,bytes=len(after),before_sha256=None if added else hashlib.sha256(before).hexdigest(),after_sha256=hashlib.sha256(after).hexdigest())
    if changed:
        if not added: assert name in allowed|docs,'out-of-scope change: '+name
        assert not name.startswith(('resources/','registry/','dependencies/','generated_fixture/')),'protected input drift: '+name
        target=dd if name in docs or name.endswith('.md') else sd
        target.extend(difflib.unified_diff(before.decode().splitlines(True),after.decode().splitlines(True),
            fromfile='accepted_0110/'+name,tofile='candidate_0120/'+name))
with zipfile.ZipFile(a.baseline) as z:
    assert z.testzip() is None
    old={name.removeprefix('HANIME_Independent_Core/'):name for name in z.namelist()}
    for name,path in old.items():
        if name in docs or name=='CMakeLists.txt' or name.startswith(prefixes):
            assert (root/name).is_file(),'removed input: '+name
            record(name,z.read(path),(root/name).read_bytes())
    paths=[root/'OUTPUT_PERF_HANDOFF.md']
    for prefix in prefixes: paths+=list((root/prefix).rglob('*'))
    for path in sorted(paths):
        if not path.is_file() or '__pycache__' in path.parts or path.name=='.DS_Store': continue
        name=path.relative_to(root).as_posix()
        if name not in old: record(name,b'',path.read_bytes(),True)
(ev/'SOURCE_CHANGES.diff').write_text(''.join(sd))
(ev/'DOCUMENT_CHANGES.diff').write_text(''.join(dd))
result=dict(status='PASS',baseline_sha256=expected,version='0.12.0',
    unchanged=sum(not r['changed'] for r in rows.values()),
    changed=[n for n,r in rows.items() if r['changed'] and not r['added']],
    added=[n for n,r in rows.items() if r['added']],files=rows,
    publication={n:r for n,r in rows.items() if n.startswith('resources/packed_auth_v2/')},
    ownership='Core tree only; no extension/adapter implementation edits or agent delegation. Frozen BasicHost sources, R1, logical resources and pins unchanged.')
(ev/'CHANGE_AUDIT.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print('PASS unchanged:',result['unchanged'],'changed:',len(result['changed']),'added:',len(result['added']))
