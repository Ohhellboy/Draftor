#!/usr/bin/env python3
"""Reproducible core validation; explicit paths, reusable build, no downloads."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--logs', type=Path, required=True)
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--publish', type=Path, help='New output directory; must not already exist')
    args = p.parse_args()
    root, build, logs = args.root.resolve(), args.build.resolve(), args.logs.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    cmake = shutil.which('cmake')
    if not cmake:
        raise SystemExit('CMake 3.20+ required; no automatic toolchain installation')
    ctest = str(Path(cmake).with_name('ctest'))
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    results = []

    def run(label, argv):
        with (logs / (label + '.log')).open('w') as out:
            proc = subprocess.run([str(x) for x in argv], stdout=out, stderr=subprocess.STDOUT, env=env)
        results.append({'label': label, 'argv': [str(x) for x in argv], 'exit': proc.returncode})
        (logs / 'COMMANDS.json').write_text(json.dumps({'sanitize': args.sanitize,
            'leak_detection': False if args.sanitize else 'not run', 'commands': results}, indent=2))
        if proc.returncode:
            raise SystemExit(label + ' failed: ' + str(logs / (label + '.log')))

    run('CONFIGURE', [cmake, '-S', root, '-B', build, '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
        '-DPython3_EXECUTABLE=' + sys.executable,
        '-DHANIME_ENABLE_SANITIZERS=' + ('ON' if args.sanitize else 'OFF')])
    run('BUILD', [cmake, '--build', build, '--parallel', '4'])
    run('CTEST', [ctest, '--test-dir', build, '--output-on-failure', '-V', '-j', '4'])
    if args.publish:
        data = root / 'resources/production/order_a'
        run('PUBLISH', [build / 'hanime_publish_verified', '--shared', data / 'shared.hns',
            '--namespace', '0x48414e494d450001', '--scheme', 'xiaohe', data / 'xiaohe.hnm',
            '--scheme', 'full_pinyin', data / 'full_pinyin.hnm', '--out', args.publish.resolve()])
        run('INDEX_ORACLE', [sys.executable, root / 'tools/verify_release_index.py', data, args.publish.resolve()])
    print('PASS core validation; evidence:', logs)


if __name__ == '__main__':
    main()
