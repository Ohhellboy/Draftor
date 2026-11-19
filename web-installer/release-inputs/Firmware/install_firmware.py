#!/usr/bin/env python3
"""Explicit-port installer. Defaults to read-only checks and a displayed plan.

Use the guide shipped with the target firmware. No board discovery, TF access,
automatic reset, retry, eFuse changes or force flags. Results contain device
identity: keep the output directory private.
"""
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Ruotao Wang
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
from check_install import EXPECTED, active_slot, partitions

BOARD = 'WAVESHARE_RLCD42'
FILES = {'bootloader.bin': 0, 'partitions.bin': 0x8000,
         'boot_app0.bin': 0xe000, 'app.bin': 0x10000}

def require(condition, message):
    if not condition:
        raise ValueError(message)

def mac(value):
    value = value.lower().replace(':', '').replace('-', '')
    require(re.fullmatch('[0-9a-f]{12}', value), 'Expected a complete device MAC')
    return value

def package_plan(folder, board):
    manifest = json.loads((folder / 'FIRMWARE_MANIFEST.json').read_text())
    require(board == BOARD and manifest['board'] == board, 'Wrong physical-board declaration or firmware target')
    require(manifest['chip'] == 'esp32s3' and manifest['flash_bytes'] == 0x1000000, 'Unsupported chip/flash configuration')
    require(set(manifest['files']) == set(FILES), 'Incomplete target four-file manifest')
    for name, offset in FILES.items():
        item = manifest['files'][name]
        path = folder / name
        require(path.is_file() and not path.is_symlink(), 'Use ordinary package files: '+name)
        data = path.read_bytes()
        require(item['offset'] == offset and len(data) == item['bytes'] and
                hashlib.sha256(data).hexdigest() == item['sha256'], 'Firmware identity mismatch: '+name)
        if name in ('app.bin', 'bootloader.bin'):
            require(len(data) >= 24 and data[0] == 0xe9 and struct.unpack_from('<H', data, 12)[0] == 9, 'Not an ESP32-S3 image')
    app = (folder/'app.bin').read_bytes()
    require(0 < len(app) <= 0x400000, 'App exceeds the fixed 4 MiB slot')
    require(manifest['firmware_label'].encode() in app, 'App target/version label mismatch')
    require(len((folder/'bootloader.bin').read_bytes()) < 0x8000, 'Bootloader overlaps partition table')
    require(partitions(folder/'partitions.bin') == EXPECTED, 'Package partition layout is incompatible')
    require(active_slot(folder/'boot_app0.bin') == 0, 'Fresh-install OTA initializer must select app0')
    return manifest

class Runner:
    def __init__(self, args, output):
        self.args, self.output, self.commands = args, output, []

    def run(self, label, action):
        cmd = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3',
               '--port', self.args.port, '--baud', '460800', '--before', 'no-reset',
               '--after', 'no-reset', '--no-stub', *action]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        (self.output/(label+'.log')).write_text(result.stdout+result.stderr)
        self.commands.append({'name': label, 'command': cmd, 'exit': result.returncode})
        (self.output/'commands.json').write_text(json.dumps(self.commands, indent=2)+'\n')
        require(result.returncode == 0, label+' failed; preserve this result and stop, no automatic retry')
        return result.stdout+result.stderr

def install(args, runner_factory=Runner):
    # Verify local inputs before opening any device or creating an execution plan.
    require(args.port and args.port.strip() == args.port, 'An explicit serial port is required')
    expected_mac = mac(args.expected_mac)
    manifest = package_plan(args.firmware, args.board)
    require(args.mode != 'update' or not args.allow_erase, 'Update never accepts erase permission')
    if args.execute:
        require(args.backup_confirmed, 'Verify and keep your existing backups before executing')
        require(args.mode != 'fresh' or args.allow_erase, 'Fresh installation requires explicit internal-flash erase acknowledgement')
    args.output.mkdir(parents=True, exist_ok=False, mode=0o700)
    result = {'status': 'CHECKING', 'mode': args.mode, 'board': args.board,
              'written': False, 'erased': False, 'reset': False}
    def save():
        (args.output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    runner = runner_factory(args, args.output)
    try:
        version = runner.run('00-version', ['version'])
        require(re.search(r'\b5\.3\.1\b', version), 'Use pinned esptool 5.3.1')
        identity = runner.run('01-identity', ['read-mac'])
        reported = re.findall(r'(?im)^\s*MAC:\s*([0-9a-f:]{17})\s*$', identity)
        require(reported and all(mac(v) == expected_mac for v in reported) and 'ESP32-S3' in identity, 'Actual device identity mismatch')
        flash = runner.run('02-flash', ['flash-id'])
        require(re.search(r'Detected flash size:\s*16MB\b', flash), 'Expected 16 MiB Flash')
        security = runner.run('03-security', ['get-security-info'])
        require(re.search(r'Secure Boot:\s*Disabled', security) and
                re.search(r'Flash Encryption:\s*Disabled', security), 'Unsupported or unknown security configuration')
        result['device_mac'] = expected_mac
        slot = 0
        if args.mode == 'update':
            table, ota = args.output/'actual-partitions.bin', args.output/'actual-otadata.bin'
            runner.run('04-read-partitions', ['read-flash', '0x8000', '0x1000', str(table)])
            require(partitions(table) == EXPECTED, 'Actual partition layout is not compatible; no App write')
            expected_table = (args.firmware/'partitions.bin').read_bytes()
            require(table.read_bytes()[:len(expected_table)] == expected_table,
                    'Actual partition bytes/checksum differ from the compatible package')
            runner.run('05-read-otadata', ['read-flash', '0xe000', '0x2000', str(ota)])
            slot = active_slot(ota)  # Refuse ambiguous/pending/rollback states.
        app_offset = 0x10000 if slot == 0 else 0x410000
        if args.current_app is not None:
            require(args.mode == 'update' and args.current_app.is_file() and not args.current_app.is_symlink(), 'Current App verification requires an ordinary saved App in update mode')
            require(24 <= args.current_app.stat().st_size <= 0x400000, 'Invalid current App length')
            runner.run('06-verify-current-app', ['verify-flash', hex(app_offset), str(args.current_app)])
        writes = [(app_offset, args.firmware/'app.bin')] if args.mode == 'update' else [(off, args.firmware/name) for name, off in FILES.items()]
        result.update(status='PLAN_READY', app_slot='app'+str(slot), app_offset=hex(app_offset),
                      plan=[{'offset':hex(off),'file':str(path),'bytes':path.stat().st_size} for off,path in writes],
                      app_sha256=manifest['files']['app.bin']['sha256'])
        save()
        if not args.execute:
            return result
        if args.mode == 'fresh':
            result['erase_started'] = True; save()
            runner.run('07-erase-internal-flash', ['erase-flash'])
            result['erased'] = True; save()
        result['write_started'] = True; save()
        pairs = [x for off,path in writes for x in (hex(off), str(path))]
        runner.run('08-write', ['write-flash', *pairs])
        result['written'] = True; save()
        runner.run('09-independent-verify', ['verify-flash', *pairs])
        result.update(status='VERIFIED_BOOT_PENDING', independently_verified=True)
        save(); return result
    except Exception as error:
        result.update(status='STOP', error=str(error)); save(); raise

def parser():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--firmware',type=Path,required=True)
    p.add_argument('--board',required=True,choices=[BOARD])
    p.add_argument('--port',required=True)
    p.add_argument('--expected-mac',required=True,help='MAC read from the intended physical board; no default')
    p.add_argument('--mode',choices=['fresh','update'],required=True)
    p.add_argument('--output',type=Path,required=True,help='New private result directory; never reused automatically')
    p.add_argument('--current-app',type=Path,help='Optional exact previous App for additional independent verification')
    p.add_argument('--execute',action='store_true',help='Perform the checked plan; default only reads/checks')
    p.add_argument('--backup-confirmed',action='store_true',help='Existing backups have been checked and kept privately')
    p.add_argument('--allow-erase',action='store_true',help='Fresh only: erase all internal Flash, including pairing and settings')
    return p

if __name__ == '__main__':
    try:
        print(json.dumps(install(parser().parse_args()),indent=2))
    except (ValueError,OSError,KeyError,struct.error,subprocess.SubprocessError) as e:
        raise SystemExit('REFUSED/STOP: '+str(e))
