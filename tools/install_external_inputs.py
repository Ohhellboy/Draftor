# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Ruotao Wang
from pathlib import Path
import argparse,json,shutil
p=argparse.ArgumentParser();p.add_argument('--from',dest='origin',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];origin=Path(a.origin).resolve()
manifest=json.loads((r/'EXTERNAL_INPUTS.json').read_text())
missing=[x['path'] for x in manifest if not (origin/x['path']).is_file() or (origin/x['path']).stat().st_size!=x['bytes']]
if missing:raise SystemExit('Missing or size-mismatched external inputs: '+str(missing))
for x in manifest:
 rel=Path(x['path']);assert not rel.is_absolute() and '..' not in rel.parts
 dst=r/rel;assert not dst.exists(),str(dst)
 dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(origin/rel,dst)
print('Installed complete same-release companion build inputs; sizes checked. Runtime resource authentication remains mandatory.')
