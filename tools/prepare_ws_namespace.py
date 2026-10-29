#!/usr/bin/env python3
"""Plan by default. Create only a new compatible Waveshare namespace with --apply.
No formatting, deletion, migration, resource copying, or original-data access.
"""
from pathlib import Path
import argparse, os, stat, json
IDENTITY=b'DRAFTOR_WS_RLCD42_NAMESPACE_V1\nlogical_root=/HANIME_S3_TEST/v01\n'
def prepare(root,apply=False):
 root=Path(root).absolute()
 # Do not traverse symlink aliases or accept a device file as a card root.
 for p in [root,*root.parents]:
  if p.is_symlink():raise ValueError('Symlink root/ancestor refused: '+str(p))
 if not root.is_dir():raise ValueError('Explicit mounted directory required')
 space=root/'WAVESHARE_RLCD42';marker=space/'.draftor-namespace'
 if space.is_symlink():raise ValueError('Namespace symlink refused')
 if space.exists():
  if not space.is_dir() or marker.is_symlink() or not marker.is_file() or marker.read_bytes()!=IDENTITY:
   raise ValueError('Existing unknown or damaged namespace: leave unchanged; inspect manually')
  return {'status':'COMPATIBLE_UNCHANGED','namespace':str(space),'writes':[]}
 plan={'status':'PLAN','namespace':str(space),'writes':[str(space),str(marker)],'resources':'REUSE_EXISTING_READ_ONLY','pico':'RESERVED_NOT_CREATED'}
 if apply:
  space.mkdir() # no exist_ok: another writer cannot make us adopt its directory
  fd=os.open(marker,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o644)
  try:
   if os.write(fd,IDENTITY)!=len(IDENTITY):raise OSError('Short marker write')
   os.fsync(fd)
  finally:os.close(fd)
  plan['status']='CREATED'
 return plan
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--card-root',required=True,type=Path)
 parser.add_argument('--apply',action='store_true',help='Explicitly allow creation only inside WAVESHARE_RLCD42')
 args=parser.parse_args()
 try:print(json.dumps(prepare(args.card_root,args.apply),indent=2))
 except (OSError,ValueError) as e:parser.exit(1,str(e)+'\n')
