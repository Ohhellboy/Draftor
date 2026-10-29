#!/usr/bin/env python3
"""Read-only protected TF tree snapshot for a later authorized device round.
Writes only the new, explicit computer-side receipt; ignores the WS namespace.
This tool is not run on a physical card during software review.
"""
from pathlib import Path
import argparse, hashlib, json
def snapshot(root):
 root=Path(root).absolute()
 if not root.is_dir() or root==Path('/') or any(p.is_symlink() for p in [root,*root.parents]):
  raise ValueError('An explicit directory without symlink ancestors is required')
 entries={}
 def visit(folder):
  for p in sorted(folder.iterdir()):
   rel=p.relative_to(root).as_posix()
   if rel=='WAVESHARE_RLCD42':continue
   if p.is_symlink():raise ValueError('Symlink refused: '+rel)
   if p.is_dir():entries[rel]={'directory':True};visit(p)
   elif p.is_file():
    h=hashlib.sha256()
    with p.open('rb') as stream:
     while data:=stream.read(1024*1024):h.update(data)
    entries[rel]={'bytes':p.stat().st_size,'sha256':h.hexdigest()}
   else:raise ValueError('Non-ordinary file refused: '+rel)
 visit(root)
 return {'schema':1,'excluded_root':'WAVESHARE_RLCD42','entries':entries}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--card-root',type=Path,required=True)
 action=p.add_mutually_exclusive_group(required=True);action.add_argument('--create',type=Path);action.add_argument('--check',type=Path)
 a=p.parse_args()
 try:
  current=snapshot(a.card_root)
  if a.create:
   output=a.create.absolute();card=a.card_root.absolute()
   if output.is_relative_to(card):raise ValueError('Receipt must be outside the card')
   with output.open('x') as f:json.dump(current,f,indent=2,ensure_ascii=False);f.write('\n')
   print('SNAPSHOT_CREATED protected_entries='+str(len(current['entries'])))
  else:
   before=json.loads(a.check.read_text())
   if before!=current:
    keys=set(before.get('entries',{}))|set(current['entries'])
    changed=[k for k in sorted(keys) if before.get('entries',{}).get(k)!=current['entries'].get(k)]
    raise ValueError('PROTECTED_TREE_CHANGED '+json.dumps(changed,ensure_ascii=False))
   print('PASS protected tree byte-identical; only WAVESHARE_RLCD42 excluded')
 except (ValueError,OSError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':main()
