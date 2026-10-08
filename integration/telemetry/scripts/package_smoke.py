#!/usr/bin/env python3
"""Build and test a relocated review ZIP, never using the original build tree."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
ROOT=Path(__file__).resolve().parents[1]
ARCHIVE=ROOT/'dist/Draftor_Behavior_Telemetry_v0_1_REVIEW.zip'
UNPACK=ROOT/'_tmp/package_smoke'

def main():
    if UNPACK.exists(): shutil.rmtree(UNPACK)
    UNPACK.mkdir(parents=True)
    with zipfile.ZipFile(ARCHIVE) as bundle:
        for name in bundle.namelist():
            parts=Path(name).parts
            if Path(name).is_absolute() or '..' in parts or parts[0]!=ROOT.name:
                raise SystemExit('unexpected archive path')
            if any(part in {'_build','_tmp','__pycache__','dist'} for part in parts[1:]):
                raise SystemExit('archive contains regenerable output')
        bundle.extractall(UNPACK)
    project=UNPACK/ROOT.name
    commands=[['cmake','-S','.','-B','_build/smoke','-DCMAKE_BUILD_TYPE=Release'],
              ['cmake','--build','_build/smoke','-j','4'],
              ['ctest','--test-dir','_build/smoke','--output-on-failure'],
              ['_build/smoke/hardening_tests','evidence/hardening_stream.bin'],
              [sys.executable,'scripts/generate_schema.py','--check'],
              [sys.executable,'scripts/run_host_tests.py'],
              [sys.executable,'scripts/check_consumer.py','--sanitizer-options']]
    with (ROOT/'evidence/package_smoke.log').open('w') as log:
        log.write('HOST Phase 2 Step 0: isolated relocation build, no original cache dependency.\n')
        for command in commands:
            result=subprocess.run(command,cwd=project,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,
                                  env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
            log.write('$ '+' '.join(command)+'\n'+result.stdout+'Exit: '+str(result.returncode)+'\n\n'); log.flush()
            if result.returncode: raise SystemExit(result.stdout)
        log.write('PASS: relocated C++ build/CTest, schema, full Host tests and external consumer.\n')
    print('PASS isolated review package smoke')
if __name__=='__main__': main()
