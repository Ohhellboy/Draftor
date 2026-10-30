from pathlib import Path
import os,sys,subprocess,json,argparse
r=Path(__file__).resolve().parents[2];p=r/'integration/product'
ap=argparse.ArgumentParser();ap.add_argument('--mode',choices=['normal','sanitize'],required=True);ap.add_argument('--resources',type=Path,required=True);a=ap.parse_args()
out=r/'TestBuilds/runtime-r1'/a.mode;ev=r/'evidence/runtime-r1'/a.mode
out.mkdir(parents=True,exist_ok=True);ev.mkdir(parents=True,exist_ok=True)
previews=ev/'previews';previews.mkdir(exist_ok=True)
env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',R2_UI_PREVIEW_DIR=str(previews))
results=[]
def run(name,args,cwd=out):
 with (ev/(name+'.log')).open('w') as log:
  ret=subprocess.run([str(x) for x in args],cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
 results.append(dict(name=name,exit=ret.returncode,command=[str(x) for x in args]));(ev/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n')
 print(name,ret.returncode,flush=True)
 if ret.returncode:raise SystemExit(ret.returncode)
device=(p/'platform/device_main.cpp').read_text()
start=device.index('s3app::TransferInputState usbInput()');end=device.index('\n#else\nbool usbActive()',start)
actual=device[start:end].replace('s3app::stability::capture(','capture_test::capture(')
(r/'tools/runtime_r1/usb_key_actual.inc').write_text(actual+'\n')
(r/'tools/runtime_r1/footer_actual.inc').write_text(device[device.index(' // R1_MENU_FOOTER_BEGIN'):device.index(' // R1_MENU_FOOTER_END')])
# Prove the tested menu predicate owns the whole existing footer chain and that
# the intentional stop still precedes keyboard, rendering and storage work.
footer=device[device.index('// R1_MENU_FOOTER_BEGIN'):device.index('// R1_MENU_FOOTER_END')]
assert footer.index('menuOwnsFooter')<footer.index('else if(app&&app->findOpen())')<footer.index('else if(editorPage)')<footer.index('Raw buffered')
assert device.index('if(heapIntegrityStopped){heapIntegrityStopTurn();return;}',device.index('void deviceLoop()'))<device.index('rd::heartbeat();',device.index('void deviceLoop()'))
stop=device[device.index('__attribute__((noinline)) void heapIntegrityStopTurn()'):device.index('s3hid::InputRoute inputRoute;')]
assert 'rd::service();' in stop and 'heapIntegrityStopped=false' not in stop
assert 'runtime_diag::endTransport();\n  if(!endpoint.start' in (p/'platform/usb_platform_s3.hpp').read_text()
flags='-DDRAFTOR_BLOCK_BACKEND=1 -DHANIME_TEXT_CHAIN=1 -DDRAFTOR_SHARED_TEXT_VARIANT=3 -DDRAFTOR_PERFORMANCE_COMMON=1 -DDRAFTOR_TEXT_RESIDENT=0 -DDRAFTOR_INPUT_PRESENT_COORD=0 -DDRAFTOR_AUTH_HANDOFF=1 -DDRAFTOR_DISPLAY_TRANSACTION=1'
flags+=' -I'+str(p/'dependencies/core/src')+' -I'+str(r/'TestBuilds/menu-font')
run('configure',['cmake','-S',p,'-B',out,'-DDRAFTOR_PUBLIC_VALIDATION=ON','-DDRAFTOR_RELEASE_BUILD=ON','-DDRAFTOR_USB_ENABLED=ON','-DSANITIZE='+('ON' if a.mode=='sanitize' else 'OFF'),'-DCMAKE_CXX_FLAGS='+flags,'-DHANIME_RESOURCE_ROOT='+str(a.resources.resolve()),'-DHANIME_INDEX_ROOT='+str(a.resources.resolve()),'-DPython3_EXECUTABLE='+sys.executable])
targets=['runtime_r1_usb_key','runtime_r1_menu','runtime_r1_footer','usb_first_init','usb_product_flow','usb_startup_feedback','r2_usb_display','r2_document_menu','file_feedback','ws_preview']
run('build',['cmake','--build',out,'--parallel','2','--target',*targets])
for name in targets:
 fixture=out/('fixture-'+name);fixture.mkdir(exist_ok=True)
 run(name,[out/name,*([previews] if name in ('ws_preview','runtime_r1_footer') else [])],fixture)
run('usb_first_init_guard_failure',[out/'usb_first_init','guard-failure'])
cmd=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-DDRAFTOR_BOARD=2','-DDRAFTOR_WS_RUNTIME_DIAG=1']
if a.mode=='sanitize':cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
cmd+=['-I'+str(r/'tools/runtime_r1/stubs'),'-I'+str(p/'platform'),'-I'+str(p/'src'),'-I'+str(p/'dependencies/common'),r/'tools/runtime_r1/diag_contract.cpp',p/'platform/runtime_findings_diag.cpp','-o',out/'diag_contract']
run('diag_contract_build',cmd);run('diag_contract',[out/'diag_contract'])
print('PASS R1 targeted checks only; no device/TF access, no resource hashing')
