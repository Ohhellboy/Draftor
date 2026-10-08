#!/usr/bin/env python3
"""Reproducible Phase 2 Step 0 validation; writes evidence only inside this task root."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/'evidence'

def run(command, log):
    log.write('$ '+' '.join(map(str,command))+'\n'); log.flush()
    result=subprocess.run(list(map(str,command)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                          text=True,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
    log.write(result.stdout); log.write(f'Exit: {result.returncode}\n\n'); log.flush()
    if result.returncode: raise RuntimeError('validation command failed: '+str(command))
    return result.stdout

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--skip-sanitizers',action='store_true')
    parser.add_argument('--baseline',type=Path,help='pre-change fixture/schema directory for byte compatibility evidence')
    args=parser.parse_args()
    EVIDENCE.mkdir(exist_ok=True); (ROOT/'_tmp').mkdir(exist_ok=True)
    status={'timestamp_utc':datetime.now(timezone.utc).isoformat(),'platform':platform.platform(),
            'python':sys.version,'machine':platform.machine(),'phase':'2 / Step 0','schema_revision':2,'evidence_levels':{'SOURCE':'IMPLEMENTED_REVIEWED','HOST':'PENDING',
            'SIMULATED_SERIAL':'PENDING','P23_INTEGRATION':'NOT_STARTED','DEVICE':'NOT_RUN'}}
    with (EVIDENCE/'validation.log').open('w') as log:
        run([sys.executable,'scripts/generate_schema.py','--check'],log)
        configurations=[('normal',[])]
        if not args.skip_sanitizers: configurations += [('asan',['-DTELEMETRY_ASAN=ON']),('ubsan',['-DTELEMETRY_UBSAN=ON'])]
        for name,flags in configurations:
            build=Path('_build')/name
            run(['cmake','-S','.','-B',build,'-DCMAKE_BUILD_TYPE=Release' if name=='normal' else '-DCMAKE_BUILD_TYPE=Debug',*flags],log)
            run(['cmake','--build',build,'-j','4'],log)
            run(['ctest','--test-dir',build,'--output-on-failure'],log)
            output='tests/fixtures' if name=='normal' else '_tmp/'+name+'_fixtures'
            run([build/'synthetic_workflows',output],log)
            status[name]='PASS'
        run([sys.executable,'scripts/complete_fixtures.py'],log)
        if not args.skip_sanitizers:
            # Sanitizers must produce byte-identical retained event streams.
            for name in ('asan','ubsan'):
                for source in (ROOT/'_tmp'/(name+'_fixtures')).rglob('*.bin'):
                    target=ROOT/'tests/fixtures'/source.relative_to(ROOT/'_tmp'/(name+'_fixtures'))
                    if source.read_bytes()!=target.read_bytes(): raise RuntimeError('sanitizer fixture drift: '+str(source))
            log.write('PASS sanitizer fixture byte equivalence\n\n')
        if args.baseline:
            baseline=args.baseline
            fixtures=list((ROOT/'tests/fixtures').rglob('*.bin'))
            for target in fixtures:
                original=baseline/target.relative_to(ROOT/'tests/fixtures')
                if original.read_bytes()!=target.read_bytes(): raise RuntimeError('pre-hardening fixture drift: '+str(target))
            original_schema=json.loads((baseline/'events.json').read_text())
            current_schema=json.loads((ROOT/'schema/events.json').read_text())
            for schema in (original_schema,current_schema):
                for event in schema['events']: del event['required_envelope']
            if original_schema!=current_schema: raise RuntimeError('out-of-scope wire/schema change')
            compatibility={'schema_revision':2,'wire_schema_unchanged':True,'fixtures_byte_identical':len(fixtures),
                           'comparison':'Pre-hardening Phase 1.1 bytes vs regenerated Step 0; no hash',
                           'fixtures':[str(p.relative_to(ROOT/'tests/fixtures')) for p in sorted(fixtures)]}
            (EVIDENCE/'wire_compatibility.json').write_text(json.dumps(compatibility,indent=2)+'\n')
            log.write(f'PASS all {len(fixtures)} pre-hardening fixtures byte-identical; wire schema unchanged\n\n')
        run([Path('_build/normal/hardening_tests'),EVIDENCE/'hardening_stream.bin'],log)
        resources=json.loads(run([Path('_build/normal/resource_benchmark')],log))
        resources['host_environment']={'platform':platform.platform(),'machine':platform.machine(),'python':sys.version}
        (EVIDENCE/'resources.json').write_text(json.dumps(resources,indent=2)+'\n')
        host_output=run([sys.executable,'scripts/run_host_tests.py'],log)
        import re
        status['host_test_count']=int(re.search(r'Ran (\d+) tests',host_output).group(1))
        run([sys.executable,'scripts/check_consumer.py'],log)
        run([sys.executable,'scripts/check_consumer.py','--sanitizer-options'],log)
        status['external_consumer']='PASS (normal and sanitizer options)'
        status['host_tests']='PASS'; status['evidence_levels']['HOST']='TESTED'
        status['evidence_levels']['SIMULATED_SERIAL']='TESTED_POSIX_PTY_AND_CALLBACK'
        run([sys.executable,'scripts/build_evidence.py'],log)
    (EVIDENCE/'validation.json').write_text(json.dumps(status,indent=2)+'\n')
    print(json.dumps(status,indent=2))
if __name__=='__main__': main()
