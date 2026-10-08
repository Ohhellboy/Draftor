"""Execute exact S3 admission body and PsramDisk methods with a Host heap seam.
No device. Same cases run against preserved parent and candidate source bytes.
"""
from pathlib import Path
import argparse, json, os, subprocess, sys
p=argparse.ArgumentParser();p.add_argument('--source',type=Path);p.add_argument('--mode',choices=['normal','sanitize'],default='normal');p.add_argument('--label',default='candidate');p.add_argument('--expect-failures',action='store_true');a=p.parse_args()
i=Path(__file__).resolve().parents[2];r=i.parent;source=(a.source or i).resolve()
out=r/'evidence/inbox_admission'/a.label/a.mode;out.mkdir(parents=True,exist_ok=True)
build=r/'TestBuilds/inbox_admission'/a.label/a.mode;build.mkdir(parents=True,exist_ok=True);tmp=r/'TestBuilds/.tmp';tmp.mkdir(parents=True,exist_ok=True)
header=(source/'module/platform/esp32s3/fatfs_inbox.hpp').read_text();impl=(source/'module/platform/esp32s3/fatfs_inbox.cpp').read_text();platform=(source/'product/platform/usb_platform_s3.hpp').read_text()
start=platform.index(' usb::Status allocateInbox()noexcept override{');end=platform.index(' void freeInbox()',start)
body=platform[start:end].replace(' override','')
cls=header[header.index('class PsramDisk final'):header.index('class FatFsInbox final')]
methods=impl[impl.index('PsramDisk::~PsramDisk()'):impl.index('FatFsInbox* FatFsInbox::drives_')]
generated='#include "draftor_usb_transfer/ports.hpp"\n#include <cstdint>\n#include <cstring>\n#include <limits>\n#include "heap_seam.hpp"\nnamespace draftor::usb::s3 {\n'+cls+methods+'}\nnamespace usb=draftor::usb;\nstruct Admission { usb::s3::PsramDisk disk_; explicit Admission(std::size_t n):disk_(n){}\n'+body+'};\n'
(build/'actual_admission.hpp').write_text(generated);(out/'actual_admission.hpp').write_text(generated)
flags=['-std=c++17','-Wall','-Wextra','-Werror','-g']
if a.mode=='sanitize':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
cmd=['clang++',*flags,'-I'+str(build),'-I'+str(Path(__file__).parent),'-I'+str(i/'module/include'),'-I'+str(i/'shared/include'),str(Path(__file__).with_name('cases.cpp')),'-o',str(build/'cases')]
env=dict(os.environ,TMPDIR=str(tmp),TMP=str(tmp),TEMP=str(tmp),PYTHONDONTWRITEBYTECODE='1',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
compiled=subprocess.run(cmd,env=env,text=True,capture_output=True);(out/'compile.log').write_text(compiled.stdout+compiled.stderr)
assert compiled.returncode==0,compiled.stderr
run=subprocess.run([str(build/'cases')],env=env,text=True,capture_output=True);(out/'run.log').write_text(run.stdout+run.stderr)
failures=[s for s in run.stdout.splitlines() if s.startswith('FAIL ')];passed=[s for s in run.stdout.splitlines() if s.startswith('PASS ')]
expected=86 if a.expect_failures else 0
ok=run.returncode==expected and len(failures)+len(passed)==18 and (bool(failures) if a.expect_failures else not failures)
result=dict(status='EXPECTED_COUNTEREXAMPLE' if a.expect_failures and ok else 'PASS' if ok else 'FAIL',source=str(source),mode=a.mode,command=cmd,exit=run.returncode,expected_exit=expected,passed=passed,failures=failures,coverage='Exact extracted S3UsbPlatform::allocateInbox and entire PsramDisk class/methods; controlled heap only; not target/device')
(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(run.stdout,end='');print(result['status'],flush=True)
sys.exit(0 if ok else 1)
