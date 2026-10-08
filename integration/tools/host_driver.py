#!/usr/bin/env python3
from pathlib import Path
import argparse,json,os,shlex,shutil,subprocess,time
p=argparse.ArgumentParser();p.add_argument('stage');p.add_argument('--profile',default='0');p.add_argument('--capture',default='0');p.add_argument('--source');p.add_argument('--san',action='store_true');p.add_argument('--release',action='store_true');p.add_argument('--public-validation',action='store_true');p.add_argument('--save-diagnostic',choices=['0','1'],default='0');p.add_argument('--configure',action='store_true');p.add_argument('--build',nargs='*');p.add_argument('--ctest');p.add_argument('--test-jobs',type=int,default=1);p.add_argument('--timeout',type=int,default=600);p.add_argument('--allow-fail',action='store_true');p.add_argument('--stop-on-failure',action='store_true');a=p.parse_args()
A=Path(__file__).resolve().parents[1];B=A.parent;S=Path(a.source).resolve() if a.source else A/'product';mode='sanitize' if a.san else 'normal';T=A/'TestBuilds/host'/a.stage/mode;E=A/'evidence/test_results'/a.stage/mode;tmp=T/'tmp'
for d in (T,E,tmp,A/'TestBuilds/.tmp'):d.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMPDIR=str(tmp),TMP=str(tmp),TEMP=str(tmp),PYTHONDONTWRITEBYTECODE='1',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
cmake=shutil.which('cmake');commands=[]
def call(label,argv,cwd=T,allow=False):
 start=time.monotonic();argv=list(map(str,argv));log=E/(label+'.log')
 with log.open('w') as f:r=subprocess.run(argv,cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT)
 history=E/'runs';history.mkdir(exist_ok=True);tag=str(time.time_ns());archived=history/(tag+'-'+label+'.log');shutil.copy2(log,archived)
 if label=='ctest' and (E/'results.xml').exists():shutil.copy2(E/'results.xml',history/(tag+'-results.xml'))
 commands.append(dict(label=label,argv=argv,cwd=str(cwd),exit=r.returncode,seconds=time.monotonic()-start,log=str(archived)))
 with (E/'commands.jsonl').open('a') as f:f.write(json.dumps(commands[-1])+'\n')
 print(a.stage,mode,label,r.returncode,flush=True)
 if r.returncode and not allow:print(log.read_text()[-12000:]);raise SystemExit(r.returncode)
if a.configure:call('configure',[cmake,'-S',S,'-B',T,'-DCMAKE_BUILD_TYPE=Debug','-DSANITIZE='+('ON' if a.san else 'OFF'),'-DHANIME_SAVE_SUBSTAGE_DIAGNOSTIC='+a.save_diagnostic,'-DDRAFTOR_RELEASE_BUILD='+('ON' if a.release else 'OFF'),'-DDRAFTOR_PUBLIC_VALIDATION='+('ON' if a.public_validation else 'OFF'),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DDRAFTOR_TELEMETRY_PROFILE='+a.profile,'-DDRAFTOR_TELEMETRY_CAPTURE='+a.capture,'-DHANIME_RESOURCE_ROOT='+str(A/'resources/legacy'),'-DHANIME_INDEX_ROOT='+str(A/'resources/staged')])
if a.build is not None:call('build',[cmake,'--build',T,'-j'+os.environ.get('DRAFTOR_BUILD_JOBS','1'),*(['--target',*a.build] if a.build else [])])
if a.ctest:
 # The USB reference tests intentionally retain stores. Every invocation starts
 # with a new store; keep all prior fixtures instead of deleting test evidence.
 generated=T/'usb_module/fixtures'
 if generated.exists():
  assert generated.is_dir() and not generated.is_symlink() and generated.resolve().is_relative_to(T.resolve())
  retained=T/'retained_usb_fixtures'/str(time.time_ns());retained.parent.mkdir(exist_ok=True)
  generated.rename(retained)
  with (E/'fixture_history.jsonl').open('a') as f:f.write(json.dumps(dict(source=str(generated),preserved_as=str(retained)))+'\n')
 call('ctest',[str(Path(cmake).with_name('ctest')),'-j'+str(a.test_jobs),*(['--stop-on-failure'] if a.stop_on_failure else []),'--output-on-failure','--timeout',str(a.timeout),'--test-output-size-passed','1048576','--test-output-size-failed','1048576','--output-junit',E/'results.xml','-R',a.ctest],allow=a.allow_fail)
