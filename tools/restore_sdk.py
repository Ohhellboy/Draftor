"""Create an owned Arduino 3.3.11 overlay from the existing fixed installation.

Unchanged SDK/tool directories are read-only inputs through symlinks. Every
patched file and the rebuilt archive live in this task, never in that input.
"""
from pathlib import Path
import os, shutil, json, subprocess, sys
r=Path(__file__).resolve().parents[1]
installed=Path(os.environ['DRAFTOR_SDK_SOURCE']).resolve()
private=r/'TestBuilds/arduino-data/data'
assert installed.is_dir()
assert not private.exists(), 'Use a fresh delivery copy/build directory; do not mutate an existing reviewed SDK.'
sdk0=installed/'packages/esp32/tools/esp32s3-libs/3.3.11'
assert (sdk0/'versions.txt').read_bytes()==(r/'vendor/versions.txt').read_bytes()
private.mkdir(parents=True)
for f in installed.iterdir():
 if f.is_file() and f.suffix in ('.json','.sig'):shutil.copy2(f,private/f.name)
pack=private/'packages/esp32';(pack/'hardware/esp32').mkdir(parents=True);(pack/'tools').mkdir()
for f in (installed/'packages').iterdir():
 if f.name!='esp32':
  if f.is_dir():shutil.copytree(f,private/'packages'/f.name,symlinks=True)
  else:shutil.copy2(f,private/'packages'/f.name)
# Preserve a private path only where the fixed overlay writes. The rest of
# the installed hardware package is a declared read-only SDK input, just like
# the tools below. This avoids copying thousands of unchanged examples/files.
hardware_source=installed/'packages/esp32/hardware/esp32/3.3.11'
hardware_private=pack/'hardware/esp32/3.3.11'
hardware_overlay=r/'vendor/overlay/hardware/esp32/3.3.11'
changed={f.relative_to(hardware_overlay) for f in hardware_overlay.rglob('*') if f.is_file()}
assert changed, 'Missing fixed hardware overlay'
def private_scaffold(source,destination,relative=Path()):
 destination.mkdir()
 for f in source.iterdir():
  if f.name in ('__pycache__','.DS_Store'):continue
  rel=relative/f.name;dst=destination/f.name
  if f.is_dir() and any(rel in c.parents for c in changed):private_scaffold(f,dst,rel)
  elif rel in changed:dst.write_bytes(f.read_bytes())
  else:dst.symlink_to(f,target_is_directory=f.is_dir())
private_scaffold(hardware_source,hardware_private)
for f in (installed/'packages/esp32/tools').iterdir():
 if f.name!='esp32s3-libs':(pack/'tools'/f.name).symlink_to(f,target_is_directory=True)
sdk=pack/'tools/esp32s3-libs/3.3.11';sdk.mkdir(parents=True)
for f in sdk0.iterdir():
 if f.name in ('include','lib','qio_opi'):continue
 if f.is_dir():(sdk/f.name).symlink_to(f,target_is_directory=True)
 else:shutil.copy2(f,sdk/f.name)
for folder,changed in [('include','arduino_tinyusb'),('lib','libarduino_tinyusb.a'),('qio_opi','include')]:
 (sdk/folder).mkdir()
 for f in (sdk0/folder).iterdir():
  if f.name==changed:
   if f.is_dir():shutil.copytree(f,sdk/folder/f.name)
  else:(sdk/folder/f.name).symlink_to(f,target_is_directory=f.is_dir())
headers=0
for f in (sdk/'include/arduino_tinyusb/tinyusb/src').rglob('*'):
 if f.is_file():
  rel=f.relative_to(sdk/'include/arduino_tinyusb/tinyusb/src')
  assert f.read_bytes()==(r/'vendor/tinyusb/src'/rel).read_bytes(),rel
  headers+=1
assert headers==111
print('Owned SDK ready, pinned headers compared:',headers)

# Inherit the reviewed T03 component and HAL/configuration byte for byte.
overlay=r/'vendor/overlay'
for f in overlay.rglob('*'):
 if f.is_file():
  dest=pack/f.relative_to(overlay);dest.parent.mkdir(parents=True,exist_ok=True)
  assert not dest.is_symlink(),dest
  shutil.copy2(f,dest)
if (r/'vendor/libarduino_tinyusb.a').is_file():
 shutil.copy2(r/'vendor/libarduino_tinyusb.a',pack/'tools/esp32s3-libs/3.3.11/lib/libarduino_tinyusb.a')
else:
 subprocess.run([sys.executable,'-B',str(r/'tools/rebuild_tinyusb.py')],check=True)
print('Restored reviewed T03 MSC-only archive and private HAL/config; no component changes')
