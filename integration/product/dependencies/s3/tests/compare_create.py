#!/usr/bin/env python3
"""Use an unchanged, SHA-checked v0.5.2 archive plus the same measurement driver.
No baseline source is edited. Temporary build stays under tests/build-create-before.
"""
from pathlib import Path
import hashlib,zipfile,subprocess,os
p=Path(__file__).resolve().parents[1]
archive=Path(os.environ.get('HANIME_V052_ZIP',p.parent/'HANIME_ESP32S3_EnglishWriting_v0_5_2_DocManager_FULL_CANDIDATE.zip'))
assert hashlib.sha256(archive.read_bytes()).hexdigest()=='3bbf039b1884d53093806f72b05702c301fccb53972966dec3a73c7e2a6c0d45'
work=p/'tests/build-create-before';work.mkdir(exist_ok=True)
with zipfile.ZipFile(archive) as z:z.extractall(work)
b=work/'HANIME_ESP32S3_EnglishWriting_v0_5_2_DocManager'
(b/'tests/create_profile.cpp').write_bytes((p/'tests/create_profile.cpp').read_bytes())
f=b/'CMakeLists.txt';f.write_text(f.read_text()+'\nadd_executable(create_profile tests/create_profile.cpp)\ntarget_link_libraries(create_profile writing)\n')
subprocess.run(['cmake','-S',str(b),'-B',str(work/'build')],check=True)
subprocess.run(['cmake','--build',str(work/'build'),'--target','create_profile','-j','4'],check=True)
with (p/'evidence/B_CREATE_BEFORE.csv').open('w') as out:subprocess.run([str(work/'build/create_profile')],stdout=out,check=True)
with (p/'evidence/B_CREATE_FINAL.csv').open('w') as out:subprocess.run([str(p/'tests/build/create_profile')],stdout=out,check=True)
print('PASS baseline unchanged production files; only CMake measurement target and shared driver added in disposable extraction; host filesystem, not S3 timings')
