#!/usr/bin/env python3
"""Package source and a fully self-hosted LOCKED static site; no publication."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
source_name = 'Draftor_Web_Installer_R1_source.zip'
site_name = 'Draftor_Web_Installer_R1_site.zip'
source = root / 'public/sources' / source_name
delivery = root / 'delivery'
exclude = {'node_modules', 'dist', 'dist-device-test', '.test-cache', 'test-results',
           'playwright-report', 'delivery', '__pycache__', '.git'}
optional_inputs = {'Draftor_v0.1.0-alpha.2.1_Resources.zip',
                   'Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip'}

def archive(target, files, base):
    with zipfile.ZipFile(str(target), 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(files, key=lambda x: x.relative_to(base).as_posix()):
            assert f.is_file() and not f.is_symlink(), str(f)
            name = f.relative_to(base).as_posix()
            info = zipfile.ZipInfo(name, date_time=(2026, 11, 19, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, f.read_bytes())

def source_files():
    result = []
    for f in root.rglob('*'):
        rel = f.relative_to(root)
        if any(part in exclude for part in rel.parts) or f.name == '.DS_Store':
            continue
        if f == source or (rel.parts[0] == 'release-inputs' and f.name in optional_inputs):
            continue
        if f.is_file():
            result.append(f)
    return result

def verify_site(path):
    with zipfile.ZipFile(str(path)) as z:
        assert z.testzip() is None
        names = z.namelist()
        assert all(not n.startswith('/') and '..' not in Path(n).parts for n in names)
        build = json.loads(z.read('BUILD.json'))
        assert build['hardwareUpdateWrites'] is False and build['hardwareFreshWrites'] is False
        assert build['release'] == 'v0.1.0-alpha.3'
        checks = z.read('SITE_SHA256SUMS').decode().splitlines()
        for line in checks:
            sha, name = line.split('  ', 1)
            assert hashlib.sha256(z.read(name)).hexdigest() == sha, name
        assert set(names) == {line.split('  ', 1)[1] for line in checks} | {'SITE_SHA256SUMS'}
        required = ['index.html', 'simulation.html', 'style.css', 'assets/app.js', 'assets/simulation.js',
                    'firmware/app.bin', 'firmware/bootloader.bin', 'firmware/partitions.bin',
                    'firmware/boot_app0.bin', 'firmware/FIRMWARE_MANIFEST.json',
                    'docs/TF_GUIDE.html', 'docs/SOURCE.html', 'sources/' + source_name]
        assert all(name in names for name in required)
        assert hashlib.sha256(z.read('firmware/app.bin')).hexdigest() == '165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd'
        return dict(result='PASS', files=len(names), all_site_sha256_match=True,
                    complete_self_hosted=True, hardware_update_writes=False,
                    hardware_fresh_writes=False, real_device_access=False)

if '--verify' in sys.argv:
    result = verify_site(delivery / site_name)
else:
    source.parent.mkdir(parents=True, exist_ok=True)
    if '--source-only' in sys.argv:
        archive(source, source_files(), root)
        print('Corresponding source archive created: ' + str(source))
        sys.exit(0)
    delivery.mkdir(exist_ok=True)
    # Two packaging passes include the validation receipt in corresponding source.
    # The receipt carries invariant properties/counts, not a recursive self-hash.
    for cycle in range(2):
        archive(source, source_files(), root)
        subprocess.run(['node', 'tools/build.mjs'], cwd=str(root), check=True)
        archive(delivery / site_name, [p for p in (root / 'dist').rglob('*') if p.is_file()], root / 'dist')
        result = verify_site(delivery / site_name)
        (root / 'evidence/package-verification.json').write_text(json.dumps(result, indent=2) + '\n')
    shutil.copyfile(str(source), str(delivery / source_name))
    sums = []
    for name in [site_name, source_name]:
        p = delivery / name
        sums.append(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + name)
    (delivery / 'SHA256SUMS').write_text('\n'.join(sums) + '\n')
(root / 'evidence/package-verification.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
