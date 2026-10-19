#!/usr/bin/env python3
"""Finite packaging checks and the real synchronous display driver contract."""
from pathlib import Path
import json,os,subprocess,struct,zlib
from check_install import EXPECTED,partitions,active_slot
R=Path(__file__).resolve().parents[1]
out=R/'TestBuilds/alpha2-checks';out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMPDIR=str(out),TMP=str(out),TEMP=str(out),PYTHONDONTWRITEBYTECODE='1')
def rejected(fn):
 try:fn()
 except ValueError:return
 raise AssertionError('Unsafe case unexpectedly accepted')
manifest=json.loads((R/'release/alpha2/RESOURCE_MANIFEST.json').read_text())
assert manifest['format']==4 and manifest['shared_bundle_id']==17304919664082782091
assert len(manifest['files'])==14 and sum(x['bytes'] for x in manifest['files'])==132153747
for row in manifest['pins']:
 name=row['name'];expected=(R/'release/alpha2/pins'/name).read_bytes()
 duplicate=R/'integration/product/src'/name if name=='wubi_release_v1.hpp' else R/'integration/product/dependencies/release_resources'/name
 assert expected==duplicate.read_bytes()

# Exercise both active slots, invalid CRC and ambiguous boot selection without
# accessing real metadata. These examples contain no device or document state.
for slot in (0,1):
 data=bytearray(b'\xff'*8192);struct.pack_into('<I',data,0,slot+1)
 struct.pack_into('<II',data,24,2,zlib.crc32(data[:4],0xffffffff))
 f=out/'synthetic-ota.bin';f.write_bytes(data);assert active_slot(f)==slot
 data[28]^=1;f.write_bytes(data);rejected(lambda:active_slot(f))
f.write_bytes(b'\xff'*8192);rejected(lambda:active_slot(f))
table=b''.join(struct.pack('<HBBII16sI',0x50aa,t,s,o,n,name.encode(),0) for name,t,s,o,n in EXPECTED)
f=out/'synthetic-partition.bin';f.write_bytes(table.ljust(3072,b'\xff'));assert partitions(f)==EXPECTED
bad=bytearray(table);struct.pack_into('<I',bad,32*3+4,0x10000);f.write_bytes(bad.ljust(3072,b'\xff'));rejected(lambda:partitions(f))
p=R/'integration/product';fixture=R/'tools/alpha2_checks';exe=out/'display-driver'
cmd=[os.environ.get('CXX','clang++'),'-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DDRAFTOR_DISPLAY_TX_OBSERVE=0','-DDRAFTOR_DISPLAY_TRANSACTION=1','-I'+str(fixture/'display_stubs'),'-I'+str(p/'platform'),'-I'+str(p/'src'),'-I'+str(p/'dependencies/common'),str(fixture/'display_driver_contract.cpp'),str(p/'platform/ST7306_OsptekLpm8.cpp'),str(p/'src/idle_log.cpp'),str(p/'src/device_metrics.cpp'),'-o',str(exe)]
subprocess.run(cmd,env=env,check=True);subprocess.run([str(exe)],env=env,check=True)
receipt={'result':'PASS','active_pin_copies':3,'partition_overlap_rejected':True,'ota_both_slots_checked':True,'invalid_or_ambiguous_ota_rejected':True,'real_display_driver':'ASan/UBSan, synchronous delta configuration','device_access':False,'scope':'finite package wiring and driver check; inherited full-session evidence is separate'}
(out/'RESULT.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
