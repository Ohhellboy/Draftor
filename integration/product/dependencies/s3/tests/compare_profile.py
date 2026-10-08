#!/usr/bin/env python3
"""Rebuild the instrumented, unchanged v0.2 behavior in a disposable directory.
Requires the preserved sibling v0.2 source (or HANIME_V02). Never modifies it.
Host timings compare host runs only; I/O and glyph counts also run on firmware.
"""
from pathlib import Path
import os, hashlib, json, shutil, subprocess, re
p=Path(__file__).resolve().parent.parent
old=Path(os.environ.get('HANIME_V02',p.with_name('HANIME_ESP32S3_EnglishWriting_v0_2')))
manifest=json.loads((p/'evidence/V02_SOURCE_SHA256.json').read_text())
for relative, expected in manifest.items():
 assert hashlib.sha256((old/relative).read_bytes()).hexdigest()==expected, relative
work=p/'tests/build-baseline'
source=work/'source'
source.mkdir(parents=True,exist_ok=True)
for generated in ('operation_profile.cpp','operation_profile.hpp'):
 (source/generated).unlink(missing_ok=True)
for f in old.rglob('*'):
 rel=f.relative_to(old)
 if any(x.startswith('build') or x in ('dist','evidence','__pycache__','.git','TF_CARD','tools','scripts') for x in rel.parts):continue
 if f.is_file() and (f.suffix in ('.cpp','.hpp','.h') or f.name=='CMakeLists.txt'):
  target=source/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target)
# Measurement only: same v0.2 behavior; instrument actual POSIX calls and add dirty scenario.
for name in ('operation_profile.cpp','operation_profile.hpp'):
 shutil.copy2(p/name,source/name)
shutil.copy2(p/'evidence/V02_PROFILE_VFS.cpp',source/'s3_vfs.cpp')
shutil.copy2(p/'evidence/V02_PROFILE_TESTS.cpp',source/'tests/host_tests.cpp')
subprocess.run(['cmake','-S',str(source),'-B',str(work/'build'),'-DCMAKE_BUILD_TYPE=Release'],check=True)
subprocess.run(['cmake','--build',str(work/'build'),'-j','4'],check=True)
with (p/'evidence/PROFILE_BEFORE.log').open('w') as log:
 subprocess.run([str(work/'build/host_tests'),'--profile'],stdout=log,check=True)
subprocess.run(['cmake','-S',str(p),'-B',str(p/'tests/build-perf'),'-DCMAKE_BUILD_TYPE=Release'],check=True)
subprocess.run(['cmake','--build',str(p/'tests/build-perf'),'-j','4'],check=True)
with (p/'evidence/PROFILE_AFTER.log').open('w') as log:
 subprocess.run([str(p/'tests/build-perf/host_tests'),'--profile'],stdout=log,check=True)
print('Compared instrumented v0.2 behavior and v0.3 with Release host build; see PROFILE_BEFORE/AFTER.log')

# Check that the reduction in filesystem calls did not reduce the saved/read/verified data.
profiles={}
for name in ('BEFORE','AFTER'):
 rows=[]
 for line in (p/('evidence/PROFILE_'+name+'.log')).read_text().splitlines():
  if line.startswith('[OPEN_PROFILE]'):
   rows.append(dict(re.findall(r'(\w+)=([^ ]+)',line)))
  elif line.startswith('[OPEN_IO]'):
   rows[-1].update(dict(re.findall(r'(\w+)=([^ ]+)',line)))
  elif line.startswith('[ACCESS_CALLS]'):
   for key,value in re.findall(r'(\w+)=(\d+/\d+/\d+)',line):
    count,total,maximum=map(int,value.split('/'))
    rows[-1].setdefault('syscalls',{})[key]={'count':count,'sum_us':total,'max_us':maximum}
 profiles[name]=rows
assert len(profiles['BEFORE'])==len(profiles['AFTER'])==20
for before,after in zip(profiles['BEFORE'],profiles['AFTER']):
 for field in ('op','bytes','cursor','read_bytes','crc_bytes','writes','write_bytes','glyph_calls'):
  assert before[field]==after[field],(field,before['op'],before[field],after[field])
 assert before['syscalls']['fsync']['count']==after['syscalls']['fsync']['count']
(p/'evidence/PROFILE_SUMMARY.json').write_text(json.dumps(profiles,indent=2)+'\n')
print('PASS 20 operation pairs: equal body size/cursor/read bytes/CRC bytes/write bytes/glyph calls/fsync count.')
