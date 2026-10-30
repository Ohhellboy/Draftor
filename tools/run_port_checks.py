#!/usr/bin/env python3
"""Offline finite port checks. Every file fixture stays inside this source tree.
Requires same-release BuildInputs and an explicit extracted Resources directory.
Never accesses a device, serial port, removable mount or network.
"""
from pathlib import Path
import argparse, subprocess, os, sys, json, hashlib
p=argparse.ArgumentParser()
p.add_argument('--resources', required=True, type=Path)
p.add_argument('--mode',choices=['normal','sanitize'],required=True)
p.add_argument('--previews',type=Path)
a=p.parse_args();r=Path(__file__).resolve().parents[1];product=r/'integration/product'
resources=a.resources.resolve()
assert resources.is_dir() and not resources.is_symlink()
out=r/'TestBuilds/ws-host'/a.mode;out.mkdir(parents=True,exist_ok=True)
ev=r/'evidence/ws-host'/a.mode;ev.mkdir(parents=True,exist_ok=True)
fixtures=out/'fixtures';fixtures.mkdir(exist_ok=True)
previews=(a.previews or out/'previews').resolve();previews.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMPDIR=str(fixtures),TMP=str(fixtures),TEMP=str(fixtures),PYTHONDONTWRITEBYTECODE='1')
checks=[]
def run(name,command,cwd=r):
 with (ev/(name+'.log')).open('w') as log:
  result=subprocess.run(list(map(str,command)),cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
 checks.append(dict(name=name,command=list(map(str,command)),exit=result.returncode))
 (ev/'RESULTS.json').write_text(json.dumps(checks,indent=2)+'\n')
 print(name, 'PASS' if not result.returncode else 'FAIL',flush=True)
 if result.returncode:raise SystemExit(result.returncode)
def hashes():
 return {str(f.relative_to(resources)):hashlib.sha256(f.read_bytes()).hexdigest() for f in resources.rglob('*') if f.is_file()}
before=hashes()
flags='-DDRAFTOR_BLOCK_BACKEND=1 -DHANIME_TEXT_CHAIN=1 -DDRAFTOR_SHARED_TEXT_VARIANT=3 -DDRAFTOR_PERFORMANCE_COMMON=1 -DDRAFTOR_TEXT_RESIDENT=0 -DDRAFTOR_INPUT_PRESENT_COORD=0 -DDRAFTOR_AUTH_HANDOFF=1 -DDRAFTOR_DISPLAY_TRANSACTION=1'
flags+=' -I'+str(product/'dependencies/core/src')+' -I'+str(r/'TestBuilds/menu-font')
# Prove the horizontal candidate painter is exactly the actual device block.
device=(product/'platform/device_main.cpp').read_text();begin=device.index(' if(imeVisible&&!vertical&&!app->findOpen()){');end=device.index('\n if(app&&app->findOpen()){',begin)
assert (r/'tools/ws_checks/ws_horizontal_product.inc').read_text().split('\n',1)[1]==device[begin:end]+'\n'
usb=(product/'tests/usb_product_flow.cpp').read_text()
fixture=(r/'tools/ws_checks/namespace_usb_fixture.hpp').read_text()
assert usb[usb.index('void exported('):usb.index('int main(')]==fixture[fixture.index('void exported('):fixture.index('int inheritedUsbMain(')],'USB product flow drift'
run('configure',['cmake','-S',product,'-B',out,'-DDRAFTOR_PUBLIC_VALIDATION=ON','-DDRAFTOR_RELEASE_BUILD=ON','-DDRAFTOR_USB_ENABLED=ON','-DSANITIZE='+('ON' if a.mode=='sanitize' else 'OFF'),'-DCMAKE_CXX_FLAGS='+flags,'-DHANIME_RESOURCE_ROOT='+str(resources),'-DHANIME_INDEX_ROOT='+str(resources),'-DPython3_EXECUTABLE='+sys.executable])
targets=['ws_namespace','ws_preview','ws_storage_regression','find_chinese','usb_product_flow','save_request_lifecycle']
run('build',['cmake','--build',out,'--parallel','2','--target',*targets])
for target in targets:
 case=fixtures/target;case.mkdir(exist_ok=True)
 extra=[previews] if target=='ws_preview' else []
 run(target,[out/target,*extra],case)
# Sweep allocation failures through the real namespace/VFS implementation and
# the exact startup gate; global allocator injection is only in this Host binary.
ns_begin=device.index(' const auto space=draftor::board::prepareNamespace(')
ns_end=device.index('\n#endif',ns_begin)
(out/'namespace_startup.inc').write_text(device[ns_begin:ns_end]+'\n')
ns=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(product/'platform'),'-I'+str(product/'src'),'-I'+str(product/'dependencies/common'),'-I'+str(out)]
if a.mode=='sanitize':ns+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
run('namespace_allocation_build',ns+[r/'tools/ws_checks/namespace_allocation.cpp',product/'platform/board_namespace.cpp',product/'platform/s3_vfs.cpp',product/'platform/operation_profile.cpp','-o',out/'namespace_allocation'])
run('namespace_allocation',[out/'namespace_allocation'],fixtures)
# Real ST7305 SPI driver with a protocol/pixel model and pinned source-word reads.
cmd=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-DDRAFTOR_DISPLAY_TRANSACTION=1']
if a.mode=='sanitize':cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
for directory in ['tools/ws_checks/display_stubs','integration/product/platform','integration/product/src','integration/product/dependencies/common']:cmd+=['-I'+str(r/directory)]
cmd += [r/'tools/ws_checks/display_contract.cpp',product/'platform/ST7305_Waveshare.cpp','-o',out/'display_contract']
run('display_build',cmd);run('display_contract',[out/'display_contract'],fixtures)
sd=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-DDRAFTOR_BOARD=2','-DDRAFTOR_SDMMC_HOST_TEST=1','-I'+str(r/'tools/ws_checks/sdmmc_stubs'),'-I'+str(product/'platform')]
if a.mode=='sanitize':sd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
run('sdmmc_build',sd+[r/'tools/ws_checks/sdmmc_contract.cpp',product/'platform/board_sdmmc.cpp','-o',out/'sdmmc_contract'])
for case in ['normal','drive','card_alloc','host_init','slot_init','card_init','vfs_alloc','vfs_conflict','fat_mount','context_alloc','context_alloc_retry','context_alloc_cleanup_error','unregister_error','admission_overrun','admission_cleanup_error','fat_detach_error','host_deinit_error','retry_timeout','owned_rejection']:
 run('sdmmc_'+case,[out/'sdmmc_contract',case],fixtures)
legacy=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(product/'platform')]
if a.mode=='sanitize':legacy+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
run('mount_recovery_build',legacy+[product/'tests/tf_mount_recovery.cpp','-o',out/'mount_recovery'])
run('mount_recovery',[out/'mount_recovery'],fixtures)
run('offline_contract',[sys.executable,'-B',r/'tools/ws_checks/offline_contract.py'],fixtures)
assert before==hashes(),'Shared static input changed'
(ev/'RESOURCE_UNCHANGED.json').write_text(json.dumps({'status':'PASS','files':before,'device_access':False},indent=2)+'\n')
print('PASS all requested finite host checks; real-device validation remains pending',flush=True)
