#!/usr/bin/env python3
"""Build a copied, outside-core public consumer and run differential/bounded modes.

Explicit reusable build directory, no downloads. Every command and its actual
exit is recorded. This does not compile or claim DeviceIme/BasicHost K1 changes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--logs', type=Path, required=True)
    p.add_argument('--sanitize', action='store_true')
    args = p.parse_args()
    root, build, logs = args.root.resolve(), args.build.resolve(), args.logs.resolve()
    assert root != build and root not in build.parents, 'consumer must be outside core'
    logs.mkdir(parents=True, exist_ok=True)
    source = build / 'external consumer source'
    shutil.copytree(root / 'examples/query_consumer', source, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('__pycache__', '.DS_Store'))
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    cmake = shutil.which('cmake')
    assert cmake, 'CMake required; no automatic install'
    commands = []

    def run(label, argv):
        argv = list(map(str, argv))
        start = time.monotonic()
        with (logs / (label + '.log')).open('w') as out:
            result = subprocess.run(argv, env=env, stdout=out, stderr=subprocess.STDOUT)
        commands.append(dict(label=label, argv=argv, exit=result.returncode,
                             host_wall_seconds=time.monotonic() - start))
        (logs / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert result.returncode == 0, label + ' failed: ' + str(logs)

    run('COMPILER', ['c++', '--version'])
    run('GXX_IDENTITY', ['g++', '--version'])
    run('CONFIGURE', [cmake, '-S', source, '-B', build / 'objects', '-DCMAKE_BUILD_TYPE=Release',
        '-DHANIME_CORE_PATH=' + str(root), '-DHANIME_RELEASE_PIN_DIR=' + str(root / 'resources/base_order_v1'),
        '-DQUERY_SANITIZERS=' + ('ON' if args.sanitize else 'OFF')])
    run('BUILD', [cmake, '--build', build / 'objects', '--parallel', '4'])
    command = [build / 'objects/query_consumer', root / 'resources/production/order_a', root / 'resources/base_order_v1']
    run('DIFFERENTIAL', command)
    run('BOUNDED', command + ['--bounded'])
    costs = {}
    for label in ('DIFFERENTIAL', 'BOUNDED'):
        output = (logs / (label + '.log')).read_text()
        assert 'PASS ' in output and 'gate=FAIL' not in output
        costs[label] = [dict(part.split('=', 1) for part in line.split()[1:])
                        for line in output.splitlines() if line.startswith('COST ')]
    (logs / 'COSTS.json').write_text(json.dumps(costs, indent=2) + '\n')
    stack = logs / 'stack'
    stack.mkdir(exist_ok=True)
    for file in (build / 'objects').rglob('*.su'):
        if file.name in {'main.cpp.su', 'base_order.cpp.su', 'member_eligibility.cpp.su'}:
            shutil.copy2(file, stack / file.name)
    dep = next((build / 'objects/CMakeFiles/query_consumer.dir').rglob('main.cpp.o.d')).read_text()
    assert not any('/HANIME_Independent_Core/' + prefix in dep for prefix in ('src/', 'tests/', 'dependencies/'))
    (logs / 'PUBLIC_DEPENDENCIES.txt').write_text(dep)
    source_hashes = {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in source.iterdir() if f.is_file()}
    (logs / 'RESULT.json').write_text(json.dumps(dict(status='PASS', source=str(source),
        source_sha256=source_hashes, sanitizer=args.sanitize, leak_detection=False,
        core_only=True, device=False, actual_DeviceIme_integration='OPEN'), indent=2) + '\n')
    print('PASS query public consumers:', logs)


if __name__ == '__main__':
    main()
