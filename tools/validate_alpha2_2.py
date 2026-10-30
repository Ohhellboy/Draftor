#!/usr/bin/env python3
"""Focused public checks for Alpha 2.2; no device or network access."""
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Ruotao Wang
from pathlib import Path
import argparse,json,os,subprocess,sys

r=Path(__file__).resolve().parents[1];p=r/'integration/product'
a=argparse.ArgumentParser();a.add_argument('--resources',type=Path,required=True);a=a.parse_args()
out=r/'TestBuilds/alpha2-2';ev=r/'evidence/alpha2-2';out.mkdir(parents=True,exist_ok=True);ev.mkdir(parents=True,exist_ok=True)
results=[]
def run(name,cmd,cwd=r):
    env=dict(os.environ,TMPDIR=str(out),PYTHONDONTWRITEBYTECODE='1')
    with (ev/(name+'.log')).open('w') as log:process=subprocess.run([str(x) for x in cmd],cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
    text=(ev/(name+'.log')).read_text();ok=process.returncode==0 and not any(x in text for x in ('ERROR: AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer'))
    results.append({'name':name,'pass':ok,'exit':process.returncode,'command':[str(x) for x in cmd]});(ev/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n')
    print(name,ok,flush=True)
    if not ok:print(text[-6000:]);raise SystemExit(1)
def span(s,start,end):return s[s.index(start):s.index(end,s.index(start))]
source=(p/'platform/ble_keyboard_host.cpp').read_text()
for name,start,end in [('notify_actual.inc','void notify(','\nclass ClientCallbacks'),('invalidate_actual.inc','void invalidate(','\nvoid notify('),('hogp_actual.inc','// DRAFTOR_HOGP_INPUTS_BEGIN','// DRAFTOR_HOGP_INPUTS_END'),('prime_actual.inc','     s3hid::State initial[8];','     ble_gap_conn_desc identity{};'),('clear_actual.inc','   portENTER_CRITICAL(&stateMux);\n   for(auto& sub:subscriptions)','   delete client;')]:
    (out/name).write_text(span(source,start,end))
run('installer',[sys.executable,'-B',r/'tools/test_install_firmware.py','-v'])
for mode in ('normal','sanitize'):
    flags=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsized-deallocation','-DDRAFTOR_WS_HOGP_INPUTS=1','-DDRAFTOR_WS_HID_DIAG=0']
    if mode=='sanitize':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    includes=['-I'+str(x) for x in (p/'platform',p/'dependencies/common',p/'dependencies/s3/libraries/BLE/src',out)]
    exe=out/(mode+'-hogp-off')
    run(mode+'-hogp-build',flags+includes+[r/'tools/product_r5/subscription_off.cpp',p/'platform/hid_keyboard.cpp',p/'dependencies/common/body_key_repeat.cpp','-o',exe])
    run(mode+'-hogp-off',[exe,r/'tools/hogp_r4/HID_REPORT_MAP_R3.bin'])
    b=out/mode
    cfg='-DDRAFTOR_BLOCK_BACKEND=1 -DHANIME_TEXT_CHAIN=1 -DDRAFTOR_SHARED_TEXT_VARIANT=3 -DDRAFTOR_PERFORMANCE_COMMON=1 -DDRAFTOR_TEXT_RESIDENT=0 -DDRAFTOR_INPUT_PRESENT_COORD=0 -DDRAFTOR_AUTH_HANDOFF=1 -DDRAFTOR_DISPLAY_TRANSACTION=1 -DDRAFTOR_WS_RUNTIME_DIAG=0 -DDRAFTOR_WS_HID_DIAG=0 -DDRAFTOR_WS_CAUSAL_DIAG=0 -I'+str(p/'dependencies/core/src')
    run(mode+'-configure',['cmake','-S',p,'-B',b,'-DDRAFTOR_PUBLIC_VALIDATION=ON','-DDRAFTOR_RELEASE_BUILD=ON','-DDRAFTOR_USB_ENABLED=ON','-DSANITIZE='+('ON' if mode=='sanitize' else 'OFF'),'-DCMAKE_CXX_FLAGS='+cfg,'-DHANIME_RESOURCE_ROOT='+str(a.resources.resolve()),'-DHANIME_INDEX_ROOT='+str(a.resources.resolve()),'-DPython3_EXECUTABLE='+sys.executable])
    targets=['storage_startup_restore','usb_document_transaction','usb_product_flow','usb_inbox_cleanup','usb_first_init','product_r5_usb_key','runtime_r1_menu','r2_usb_display']
    run(mode+'-build',['cmake','--build',b,'--parallel','2','--target',*targets])
    for name in targets:
        fixture=b/('fixture-'+name);fixture.mkdir(exist_ok=True)
        run(mode+'-'+name,[b/name],fixture)
    run(mode+'-usb-guard-failure',[b/'usb_first_init','guard-failure'])
print('PASS selected behavior checks, normal and ASan/UBSan; configuration/build steps are not additional behavior tests.')
