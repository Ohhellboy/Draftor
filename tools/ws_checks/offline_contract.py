#!/usr/bin/env python3
"""Only workspace fixtures and compile-only hardware selection. No device APIs."""
from pathlib import Path
import importlib.util, tempfile, subprocess, hashlib
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('prepare_ws_namespace',root/'tools/prepare_ws_namespace.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
def hashes(p):
 return {str(f.relative_to(p)):hashlib.sha256(f.read_bytes()).hexdigest() for f in p.rglob('*') if f.is_file()}
with tempfile.TemporaryDirectory(prefix='offline-port-',dir=Path.cwd()) as folder:
 p=Path(folder).resolve(); card=p/'card';card.mkdir()
 for name in ['HANIME_S3_TEST/v01/CURRENT.A','HANIME/IME/resource','PICO_OLD/sentinel']:
  f=card/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b'protected '+name.encode())
 before=hashes(card);assert module.prepare(card)['status']=='PLAN' and hashes(card)==before
 assert module.prepare(card,True)['status']=='CREATED'
 after=hashes(card);assert all(after[k]==v for k,v in before.items())
 assert set(after)-set(before)=={'WAVESHARE_RLCD42/.draftor-namespace'}
 assert module.prepare(card,True)['status']=='COMPATIBLE_UNCHANGED' and hashes(card)==after
 assert not (card/'PICO2W').exists()
 for kind in ['directory','file','symlink','bad-marker','marker-symlink']:
  fixture=p/kind;fixture.mkdir();space=fixture/'WAVESHARE_RLCD42'
  if kind=='file':space.write_text('unknown')
  elif kind=='symlink':space.symlink_to(card/'HANIME_S3_TEST',target_is_directory=True)
  else:
   space.mkdir()
   if kind=='bad-marker':(space/'.draftor-namespace').write_text('unknown')
   if kind=='marker-symlink':(space/'.draftor-namespace').symlink_to(card/'WAVESHARE_RLCD42/.draftor-namespace')
  b=hashes(fixture)
  try:module.prepare(fixture,True)
  except ValueError:pass
  else:raise AssertionError('adopted '+kind)
  assert hashes(fixture)==b
 link=p/'alias';link.symlink_to(card,target_is_directory=True)
 try:module.prepare(link,True)
 except ValueError:pass
 else:raise AssertionError('root alias accepted')
 print('PASS prepare tool: plan makes no writes; apply creates marker only; idempotent; unknown namespace and symlinks rejected; original/static/Pico sentinel bytes unchanged')
 tool=root/'tools/ws_checks/card_snapshot.py';receipt=p/'protected.json'
 def snap(*args):return subprocess.run(['python3','-B',str(tool),'--card-root',str(card),*map(str,args)],capture_output=True,text=True)
 assert snap('--create',receipt).returncode==0
 assert snap('--check',receipt).returncode==0
 (card/'WAVESHARE_RLCD42/test-file').write_text('allowed test')
 assert snap('--check',receipt).returncode==0
 (card/'HANIME_S3_TEST/v01/CURRENT.A').write_text('injected change')
 result=snap('--check',receipt);assert result.returncode==1 and 'PROTECTED_TREE_CHANGED' in result.stderr
 assert snap('--create',card/'bad.json').returncode==1 and not (card/'bad.json').exists()
 print('PASS handoff snapshot tool: WS-only changes excluded; protected-tree change detected; receipt on card refused')
 for target in [None,'ESP32S3_OSPTEK_ST7306','WAVESHARE_RLCD42','PICO2W','UNKNOWN_BOARD','3']:
  ws=target=='WAVESHARE_RLCD42'
  code='#include "board_config.hpp"\nstatic_assert(draftor::board::Waveshare=='+('true' if ws else 'false')+');\n'
  if not ws:code+='static_assert(draftor::board::DisplaySck==12 && draftor::board::DisplayMosi==11 && draftor::board::DisplayCs==10 && draftor::board::DisplayDc==9 && draftor::board::DisplayReset==4);\n'
  cmd=['clang++','-x','c++','-std=c++17','-fsyntax-only','-I'+str(root/'integration/product/platform')]
  if target:cmd+=['-DDRAFTOR_BOARD='+target]
  result=subprocess.run(cmd+['-'],input=code,text=True,capture_output=True)
  assert (result.returncode==0)==(target in [None,'ESP32S3_OSPTEK_ST7306','WAVESHARE_RLCD42']),result.stderr
  if result.returncode:assert 'Unsupported DRAFTOR_BOARD' in result.stderr
  print('PASS compile selector',target or 'DEFAULT_ORIGINAL','accepted' if not result.returncode else 'rejected')
 for name in ['PICO2W','UNKNOWN_BOARD']:
  result=subprocess.run(['python3','-B',str(root/'integration/product/scripts/build_s3.py'),'--target',name],capture_output=True,text=True)
  assert result.returncode==2 and 'invalid choice' in result.stderr
 print('PASS unknown build target rejected before build/mutation')
