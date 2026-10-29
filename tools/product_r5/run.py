from pathlib import Path
import subprocess, json, os, sys, tarfile, argparse
r=Path(__file__).resolve().parents[2]; p=r/'integration/product'; t=r/'tools/product_r5'
ap=argparse.ArgumentParser();ap.add_argument('--resources',type=Path,required=True);ap.add_argument('--baseline',type=Path,required=True);a=ap.parse_args()
out=r/'TestBuilds/product-r5';ev=r/'evidence/product-r5';out.mkdir(parents=True,exist_ok=True);ev.mkdir(parents=True,exist_ok=True)
with tarfile.open(a.baseline/'Source.tar.gz') as ar:old={m.name.removeprefix('DraftorSource/'):ar.extractfile(m).read() for m in ar if m.isfile()}
changed=[n for n,b in old.items() if n.startswith('integration/product/') and (r/n).read_bytes()!=b]
assert set(changed)=={'integration/product/platform/device_main.cpp','integration/product/scripts/build_s3.py','integration/product/CMakeLists.txt'},changed
device=(p/'platform/device_main.cpp').read_text();before=old['integration/product/platform/device_main.cpp'].decode()
def span(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
for begin,end in [('bool usbKey(', '\n#else\nbool usbActive()'),('__attribute__((noinline)) void heapIntegrityStopTurn()', 's3hid::InputRoute inputRoute;'),(' // R1_MENU_FOOTER_BEGIN',' // R1_MENU_FOOTER_END')]:assert span(device,begin,end)==span(before,begin,end)
assert 'usbCausalKey' not in device and 'usb_causal_probe' not in device
assert 'if(heapIntegrityStopped){hidprobe::point(hidprobe::Stopped);heapIntegrityStopTurn();return;}' in device
dispatch=span(before,'#else\n    if(wifiKey(e))','\n#endif\n#endif\n    // Layout').removeprefix('#else\n')
assert dispatch in device
actual=span(device,'s3app::TransferInputState usbInput()','\n#else\nbool usbActive()').replace('s3app::stability::capture(','capture_test::capture(')
(t/'usb_key_actual.inc').write_text(actual+'\n')
s=(p/'platform/ble_keyboard_host.cpp').read_text();assert s==old['integration/product/platform/ble_keyboard_host.cpp'].decode()
for name,begin,end in [('notify_actual.inc','void notify(','\nclass ClientCallbacks'),('invalidate_actual.inc','void invalidate(','\nvoid notify('),('hogp_actual.inc','// DRAFTOR_HOGP_INPUTS_BEGIN','// DRAFTOR_HOGP_INPUTS_END'),('prime_actual.inc','     s3hid::State initial[8];','     ble_gap_conn_desc identity{};'),('clear_actual.inc','   portENTER_CRITICAL(&stateMux);\n   for(auto& sub:subscriptions)','   delete client;')]: (out/name).write_text(span(s,begin,end))
(ev/'SCOPE.json').write_text(json.dumps({'baseline':str(a.baseline),'modified_product_files':changed,'BLE_HOGP_and_lifecycle_byte_identical_R4':True,'usbKey_heap_stop_footer_byte_identical_R4':True,'original_dispatch_restored_exactly':True,'all_remaining_product_files_byte_identical_R4':True,'diagnostics_tested_off':True},indent=2)+'\n')
results=[]
def run(name,cmd,cwd=out):
 env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',TMPDIR=str(out))
 with (ev/(name+'.log')).open('w') as log:q=subprocess.run(list(map(str,cmd)),cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
 txt=(ev/(name+'.log')).read_text();ok=q.returncode==0 and not any(v in txt for v in ['ERROR: AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer'])
 results.append({'name':name,'pass':ok,'exit':q.returncode,'command':list(map(str,cmd))});(ev/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,ok,flush=True)
 if not ok:print(txt[-7000:]);raise SystemExit(1)
for mode in ['normal','sanitize']:
 flags=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DDRAFTOR_WS_HOGP_INPUTS=1','-DDRAFTOR_WS_HID_DIAG=0']
 if mode=='sanitize':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 includes=['-I'+str(x) for x in [p/'platform',p/'dependencies/common',p/'dependencies/s3/libraries/BLE/src',out]]
 exe=out/(mode+'-subscription-off');run(mode+'-subscription-build',flags+includes+[t/'subscription_off.cpp',p/'platform/hid_keyboard.cpp',p/'dependencies/common/body_key_repeat.cpp','-o',exe]);run(mode+'-subscription-off',[exe,r/'tools/hogp_r4/HID_REPORT_MAP_R3.bin'])
 b=out/mode;b.mkdir(exist_ok=True)
 config='-DDRAFTOR_BLOCK_BACKEND=1 -DHANIME_TEXT_CHAIN=1 -DDRAFTOR_SHARED_TEXT_VARIANT=3 -DDRAFTOR_PERFORMANCE_COMMON=1 -DDRAFTOR_TEXT_RESIDENT=0 -DDRAFTOR_INPUT_PRESENT_COORD=0 -DDRAFTOR_AUTH_HANDOFF=1 -DDRAFTOR_DISPLAY_TRANSACTION=1 -DDRAFTOR_WS_RUNTIME_DIAG=0 -DDRAFTOR_WS_HID_DIAG=0 -DDRAFTOR_WS_CAUSAL_DIAG=0'
 config+=' -I'+str(p/'dependencies/core/src')+' -I'+str(r/'TestBuilds/menu-font')
 run(mode+'-configure',['cmake','-S',p,'-B',b,'-DDRAFTOR_PUBLIC_VALIDATION=ON','-DDRAFTOR_RELEASE_BUILD=ON','-DDRAFTOR_USB_ENABLED=ON','-DSANITIZE='+('ON' if mode=='sanitize' else 'OFF'),'-DCMAKE_CXX_FLAGS='+config,'-DHANIME_RESOURCE_ROOT='+str(a.resources.resolve()),'-DHANIME_INDEX_ROOT='+str(a.resources.resolve()),'-DPython3_EXECUTABLE='+sys.executable])
 targets=['product_r5_usb_key','usb_product_flow','usb_document_transaction','usb_inbox_cleanup','usb_first_init','r2_usb_display','runtime_r1_menu']
 run(mode+'-build',['cmake','--build',b,'--parallel','2','--target',*targets])
 for name in targets:
  fixture=b/('fixture-'+name);fixture.mkdir(exist_ok=True);run(mode+'-'+name,[b/name],fixture)
 run(mode+'-usb-first-guard-failure',[b/'usb_first_init','guard-failure'])
print('PASS targeted normal-product tests, normal and ASan/UBSan; no hardware I/O')
