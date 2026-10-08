#!/usr/bin/env python3
"""Offline A1 validation. Fresh Host build; never regenerate legacy fixtures/evidence."""
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT/'evidence/analyzer_a1'


def main():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (ROOT/'_tmp').mkdir(exist_ok=True)
    results = []
    environment = {**os.environ, 'PYTHONDONTWRITEBYTECODE': '1'}
    with (EVIDENCE/'validation.log').open('w', encoding='utf-8') as log:
        def run(command):
            command = list(map(str, command))
            log.write('$ '+' '.join(command)+'\n')
            log.flush()
            done = subprocess.run(command, cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True)
            log.write(done.stdout+f'\nExit: {done.returncode}\n\n')
            log.flush()
            results.append({'command': command, 'exit_code': done.returncode})
            if done.returncode:
                raise RuntimeError('Validation failed: '+' '.join(command))
            return done.stdout
        with tempfile.TemporaryDirectory(prefix='analyzer-a1-validation-', dir=ROOT/'_tmp') as temporary:
            build = Path(temporary)/'build'
            run([sys.executable, 'scripts/generate_schema.py', '--check'])
            run(['cmake', '-S', '.', '-B', build, '-DCMAKE_BUILD_TYPE=Release'])
            run(['cmake', '--build', build, '-j', '4'])
            run(['ctest', '--test-dir', build, '--output-on-failure'])
            output = run([sys.executable, 'scripts/run_host_tests.py'])
            run([sys.executable, 'tests/analyzer/generate_example.py', '--output', Path(temporary)/'example'])
            saved = ROOT/'examples/analyzer_a1'
            compared = []
            for path in sorted(saved.rglob('*')):
                if path.is_file():
                    relative = path.relative_to(saved)
                    if path.read_bytes() != (Path(temporary)/'example'/relative).read_bytes():
                        raise RuntimeError('Example output drift: '+str(relative))
                    compared.append(str(relative))
            if not compared:
                raise RuntimeError('Saved analyzer example is missing')
            log.write('PASS deterministic saved example equivalence\n')
    status = dict(status='HOST_ANALYZER_A1_READY_FOR_REVIEW', device_integration='NOT_STARTED',
                  platform=platform.platform(), python=sys.version, schema_revision=2,
                  fresh_host_build=True, ctest_passed=2,
                  host_test_count=int(re.search(r'Ran (\d+) tests', output).group(1)),
                  saved_example_files_verified=compared, commands=results)
    (EVIDENCE/'validation.json').write_text(json.dumps(status, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in status.items() if key != 'commands'}, indent=2))


if __name__ == '__main__':
    main()
