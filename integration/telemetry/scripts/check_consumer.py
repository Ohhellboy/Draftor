#!/usr/bin/env python3
"""Build a real external consumer without inheriting this library's private policy."""
import argparse
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--sanitizer-options',action='store_true'); args=parser.parse_args()
    build=ROOT/'_build'/('consumer_with_sanitizer_options' if args.sanitizer_options else 'consumer')
    flags=['-DTELEMETRY_ASAN=ON','-DTELEMETRY_UBSAN=ON'] if args.sanitizer_options else []
    commands=[['cmake','-S',str(ROOT/'tests/consumer'),'-B',str(build),'-DTELEMETRY_SOURCE='+str(ROOT),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',*flags],
              ['cmake','--build',str(build),'--target','external_consumer','-j','4'],
              ['ctest','--test-dir',str(build),'-R','^consumer_semantics$','--output-on-failure']]
    for command in commands:
        print('$ '+' '.join(command),flush=True); subprocess.run(command,check=True)
    entries=json.loads((build/'compile_commands.json').read_text())
    consumer=[entry for entry in entries if Path(entry['file']).resolve()==ROOT/'tests/consumer/main.cpp']
    assert len(consumer)==1
    command=consumer[0]['command']
    for forbidden in ('-Wall','-Wextra','-Wpedantic','-Werror','-fno-exceptions','-fno-rtti','-fsanitize'):
        if forbidden in command: raise SystemExit('consumer inherited policy: '+forbidden)
    # The public library also remains uninstrumented when test sanitizer options are enabled.
    for entry in entries:
        if 'draftor_telemetry.dir' in entry['command']:
            assert '-fsanitize' not in entry['command']
    print('PASS external consumer: exceptions, RTTI, own warning policy, no sanitizer propagation',flush=True)
if __name__=='__main__': main()
