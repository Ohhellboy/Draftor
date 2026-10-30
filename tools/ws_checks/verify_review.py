#!/usr/bin/env python3
"""Hash a saved review package and optional shared resources. No device APIs."""
from pathlib import Path
import argparse,hashlib,json
def sha(p):
 if p.is_symlink() or not p.is_file():raise ValueError('Ordinary file required: '+str(p))
 h=hashlib.sha256()
 with p.open('rb') as f:
  while data:=f.read(1024*1024):h.update(data)
 return h.hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--review',type=Path,required=True);p.add_argument('--resources',type=Path)
 a=p.parse_args()
 try:
  root=a.review.resolve();manifest=json.loads((root/'MANIFEST.json').read_text())
  if manifest['status']!='WS_RLCD42_A3_SOFTWARE_READY_FOR_REVIEW_DEVICE_PENDING':raise ValueError('Review not complete')
  for name,digest in manifest['files'].items():
   path=root/name
   if not path.resolve().is_relative_to(root) or sha(path)!=digest:raise ValueError('Review mismatch: '+name)
  if a.resources:
   resources=a.resources.resolve();expected=json.loads((root/'Evidence/RESOURCE_IDENTITIES.json').read_text())
   for name,digest in expected['dictionary_files'].items():
    if sha(resources/name)!=digest:raise ValueError('Resource mismatch: '+name)
   if sha(resources.parent/'R1_API080/matched28.hbf1')!=expected['font_sha256']:raise ValueError('Font mismatch')
  print('PASS review SHA256'+(' and exact shared resource/font bytes' if a.resources else '')+'; no device qualification implied')
 except (ValueError,OSError,KeyError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':main()
