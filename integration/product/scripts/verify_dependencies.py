# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Ruotao Wang
from pathlib import Path
import sys,json
work,p=map(Path,sys.argv[1:]);r=p.parents[1]
required=['dependencies/core/include/hanime/core.hpp','dependencies/extension/include/hanime_extension/extension.hpp','dependencies/common/document.hpp','dependencies/s3/libraries/BLE/src/BLEDevice.h','dependencies/release_resources/release_pin.hpp']
for rel in required:assert (p/rel).is_file(),rel
pin=p/'dependencies/release_resources/release_pin.hpp'
assert pin.read_bytes()==(r/'release/integration_headers/release_pin.hpp').read_bytes()
if (r/'BUILD_PARENT.json').is_file():
 parent=Path(json.loads((r/'BUILD_PARENT.json').read_text())['parent'])/'integration/product'
 count=0
 for sub in ['core','extension','common','s3','wubi','release_resources']:
  a=parent/'dependencies'/sub;b=p/'dependencies'/sub
  files=lambda root:{str(f.relative_to(root)):f for f in root.rglob('*') if f.is_file() and '__pycache__' not in f.parts}
  old,new=files(a),files(b);assert old.keys()==new.keys(),sub
  for rel in old:assert old[rel].read_bytes()==new[rel].read_bytes(),sub+'/'+rel
  count+=len(old)
 print('Private release parent dependencies byte-identical:',count,'files; release pin matches.')
else:
 for row in json.loads((r/'EXTERNAL_INPUTS.json').read_text()):
  f=r/row['path'];assert f.is_file() and f.stat().st_size==row['bytes'],row['path']
 print('Public source input completeness and duplicate pin checked; this is not reviewed binary identity. Runtime authentication remains enabled.')
