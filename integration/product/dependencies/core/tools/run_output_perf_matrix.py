#!/usr/bin/env python3
"""Finite same-consumer comparisons; no downloads or copies of the whole core."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--build',type=Path,required=True)
p.add_argument('--logs',type=Path,required=True)
p.add_argument('--baseline',type=Path,help='Frozen API0.11 source tree; optional, never inferred')
p.add_argument('--sanitize',action='store_true')
a=p.parse_args(); root=a.root.resolve(); logs=a.logs.resolve(); logs.mkdir(parents=True,exist_ok=True)
jobs=[('balanced','balanced','all'),('balanced_reuse','balanced','reuse')]
if not a.sanitize:
    jobs=[('off','off','all'),('output','output','all'),('anchors','anchors','all'),
          *jobs,('extra','extra','all'),('off_reuse','off','reuse')]
if a.baseline:
    jobs=[('baseline','off','all'),('baseline_reuse','off','reuse'),*jobs]
commands=[]
for name,profile,case in jobs:
    command=[sys.executable,str(root/'tools/run_visible_text_consumer.py'),'--root',str(root),
             '--release',str(root/'resources/packed_auth_v2'),'--build',str(a.build.resolve()),
             '--logs',str(logs/name),'--case',case,'--profile',profile]
    if name.startswith('baseline'): command+=['--core',str(a.baseline.resolve())]
    if a.sanitize: command+=['--sanitize']
    with (logs/(name+'.log')).open('w') as out:
        r=subprocess.run(command,stdout=out,stderr=subprocess.STDOUT)
    commands.append(dict(name=name,argv=command,exit=r.returncode))
    (logs/'COMMANDS.json').write_text(json.dumps(commands,indent=2)+'\n')
    if r.returncode: raise SystemExit('FAIL '+name+'; see '+str(logs))
    print('PASS',name,flush=True)
