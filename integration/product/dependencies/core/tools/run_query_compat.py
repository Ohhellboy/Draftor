#!/usr/bin/env python3
"""Incremental unchanged Basic Host plus legacy P/X public compatibility runs."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--root', type=Path, required=True)
p.add_argument('--core-build', type=Path, required=True)
p.add_argument('--startup-build', type=Path, required=True)
p.add_argument('--work', type=Path, required=True)
p.add_argument('--logs', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
root, core, work, logs = a.root.resolve(), a.core_build.resolve(), a.work.resolve(), a.logs.resolve()
logs.mkdir(parents=True, exist_ok=True)
work.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
if a.sanitize:
    env.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
commands = []


def run(label, cmd, cwd=None):
    cmd = list(map(str, cmd))
    with (logs / (label + '.log')).open('w') as out:
        result = subprocess.run(cmd, cwd=cwd, env=env, stdout=out, stderr=subprocess.STDOUT)
    commands.append(dict(label=label, argv=cmd, cwd=str(cwd or Path.cwd()), exit=result.returncode))
    (logs / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
    assert result.returncode == 0, label + ' failed: ' + str(logs)


cmake = shutil.which('cmake')
assert cmake
run('BASIC_CONFIGURE', [cmake, '-S', root / 'examples/startup_consumer', '-B', a.startup_build.resolve(),
    '-DHANIME_CORE_PATH=' + str(root), '-DBASIC_HOST_PATH=' + str(root / 'dependencies/basic_host_v0_5'),
    '-DHANIME_RELEASE_PIN_DIR=' + str(root / 'resources/verified_v1'), '-DCMAKE_BUILD_TYPE=Release',
    '-DSTARTUP_SANITIZERS=' + ('ON' if a.sanitize else 'OFF')])
run('BASIC_BUILD', [cmake, '--build', a.startup_build.resolve(), '--parallel', '4'])
run('BASIC_RUN', [a.startup_build.resolve() / 'startup_consumer', root / 'resources/production/order_a', root / 'resources/verified_v1'])
for example in ('public_consumer', 'composition_consumer'):
    source = work / (example + '.cpp')
    shutil.copy2(root / 'examples' / example / 'main.cpp', source)
    flags = ['-std=c++17', '-O2', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-fstack-usage']
    if a.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    run(example + '_BUILD', ['c++', *flags, '-I' + str(root / 'include'), source,
        core / 'libhanime_host_adapters.a', core / 'libhanime_core.a', '-o', work / example])
empty = Path(tempfile.mkdtemp(prefix='disabled-empty-', dir=work))
run('X5', [work / 'composition_consumer', root / 'resources/production/order_a/shared.hns'], empty)
assert not list(empty.iterdir()), 'Disabled example created state'
state = Path(tempfile.mkdtemp(prefix='single-state-', dir=work)) / 'state'
run('P_DEMO', [work / 'public_consumer', 'demo', core / 'step2f/order_a', state])
run('P_VERIFY', [work / 'public_consumer', 'verify', core / 'step2f/order_b', state])
print('PASS unchanged Basic Host and legacy public P6/X5:', logs)
