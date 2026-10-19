#!/usr/bin/env python3
"""Offline Alpha 2 file checks. Never opens a port, disk, network or device.

Inputs must be ordinary saved files/directories. This is a compatibility check,
not a flashing program, backup creator, or substitute for runtime authentication.
"""
from pathlib import Path
import argparse,json,struct,zlib,hashlib

EXPECTED=[('nvs',1,2,0x9000,0x5000),('otadata',1,0,0xe000,0x2000),
 ('app0',0,0x10,0x10000,0x400000),('app1',0,0x11,0x410000,0x400000),
 ('ime_ro',0x40,0,0x810000,0x200000),('ffat',1,0x81,0xa10000,0x5e0000),
 ('coredump',1,3,0xff0000,0x10000)]

def partitions(path):
 data=path.read_bytes();rows=[]
 if len(data) not in (3072,4096):raise ValueError('Partition file must be a saved 3072/4096-byte table')
 for at in range(0,3072,32):
  magic=struct.unpack_from('<H',data,at)[0]
  if magic in (0xffff,0xebeb):break
  if magic!=0x50aa:raise ValueError('Bad partition magic')
  _,typ,sub,off,size,name,flags=struct.unpack_from('<HBBII16sI',data,at)
  if flags:raise ValueError('Encrypted/flagged partitions are outside this package')
  if not size or off<0x9000 or off+size>0x1000000:raise ValueError('Invalid partition range')
  name=name.split(b'\0')[0].decode('ascii');rows.append((name,typ,sub,off,size))
 for a,b in zip(sorted(rows,key=lambda x:x[3]),sorted(rows,key=lambda x:x[3])[1:]):
  if a[3]+a[4]>b[3]:raise ValueError('Overlapping partitions')
 if not rows:raise ValueError('No partitions')
 return rows

def active_slot(path):
 data=path.read_bytes()
 if len(data)!=8192:raise ValueError('Saved OTA data must be exactly 8192 bytes')
 entries=[]
 for at in (0,4096):
  entry=data[at:at+32]
  if entry==b'\xff'*32:continue
  seq=struct.unpack_from('<I',entry)[0];state,crc=struct.unpack_from('<II',entry,24)
  if not 0<seq<0xffffffff or crc!=zlib.crc32(entry[:4],0xffffffff) or state not in (2,0xffffffff):
   raise ValueError('Invalid, pending, rollback or unsupported OTA state')
  entries.append(seq)
 # The tested installation has one valid entry and one erased entry. Do not
 # guess boot choice for other states, sequence wrap, or rollback transitions.
 if len(entries)!=1:raise ValueError('OTA selection is not the supported unambiguous one-valid/one-erased pattern')
 return (entries[0]-1)%2

def resources(root,manifest):
 if manifest.get('format')!=4 or manifest.get('shared_bundle_id')!=17304919664082782091:
  raise ValueError('Wrong release resource manifest')
 if (root/'STAGING_INCOMPLETE').exists() or not (root/'CANDIDATE_READY.json').is_file():
  raise ValueError('Incomplete resource installation')
 for row in manifest['files']:
  f=root/row['name']
  if f.is_symlink() or not f.is_file() or f.stat().st_size!=row['bytes']:raise ValueError('Missing/wrong-size resource: '+row['name'])
 with (root/'shared.hns').open('rb') as f:head=f.read(40)
 if struct.unpack_from('<II',head)!= (0x34485348,4) or struct.unpack_from('<Q',head,16)[0]!=manifest['shared_bundle_id']:
  raise ValueError('format2/format3 or wrong shared bundle')
 for name in ('xiaohe','full_pinyin','wubi86'):
  with (root/(name+'.hnm')).open('rb') as f:head=f.read(24)
  if struct.unpack_from('<Q',head,16)[0]!=manifest['shared_bundle_id']:raise ValueError('Mixed scheme/shared bundle')
 return {'files':14,'bytes':sum(x['bytes'] for x in manifest['files']),'check':'sizes, format, shared/scheme pairing; full byte authentication remains in compiled runtime pins'}

def check(firmware,resource_root,mode,partition=None,ota=None):
 pair=json.loads((firmware/'RELEASE_PAIR.json').read_text());app=firmware/pair['app']['name']
 if app.is_symlink() or app.stat().st_size!=pair['app']['bytes']:raise ValueError('Wrong App length')
 data=app.read_bytes()
 if data[0]!=0xe9 or struct.unpack_from('<H',data,12)[0]!=9:raise ValueError('App is not an ESP32-S3 image')
 # The quiet build can discard the diagnostic-only printable pin digest.
 # Verify the actual distributed App, not a log string. Compile receipts bind
 # that App to all three consumed pins; runtime authentication is unchanged.
 if hashlib.sha256(data).hexdigest()!=pair['app']['sha256']:
  raise ValueError('App bytes do not match this release manifest')
 if pair['firmware_label'].encode() not in data or pair['resources']['resource_root'].encode() not in data:
  raise ValueError('App identity/resource metadata mismatch')
 rows=partitions(partition or firmware/'partitions.bin')
 slots=[r for r in rows if r[1]==0]
 if any(r[4]==0x200000 for r in slots):raise ValueError('2 MiB layout: App does not fit; separate migration required, not App-only')
 if rows!=EXPECTED:raise ValueError('Not the exact supported dual-4 MiB layout')
 res=resources(resource_root,pair['resources'])
 if mode=='update':
  if partition is None or ota is None:raise ValueError('Update requires saved current partition and OTA files; do not use package defaults as device evidence')
  slot=active_slot(ota)
 else:
  slot=0
  boot=(firmware/'bootloader.bin').read_bytes()
  if len(boot)>=0x8000 or boot[0]!=0xe9 or struct.unpack_from('<H',boot,12)[0]!=9:raise ValueError('Bootloader mismatch')
  if (firmware/'boot_app0.bin').stat().st_size!=8192:raise ValueError('Invalid SDK OTA initializer')
 appslot=next(r for r in rows if r[1]==0 and r[2]==0x10+slot)
 if len(data)>appslot[4]:raise ValueError('App exceeds slot')
 return {'result':'COMPATIBLE_FILES_ONLY','mode':mode,'app_slot':slot,'app_offset':hex(appslot[3]),'slot_end_exclusive':hex(appslot[3]+appslot[4]),'app_bytes':len(data),'resources':res,'device_access':False,'device_qualification':False}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--firmware',type=Path,required=True);p.add_argument('--resources',type=Path,required=True)
 p.add_argument('--mode',choices=['fresh','update'],required=True);p.add_argument('--partition',type=Path);p.add_argument('--otadata',type=Path)
 a=p.parse_args()
 try:result=check(a.firmware,a.resources,a.mode,a.partition,a.otadata)
 except (ValueError,OSError,KeyError,struct.error) as e:raise SystemExit('REFUSED: '+str(e))
 print(json.dumps(result,indent=2))
if __name__=='__main__':main()
