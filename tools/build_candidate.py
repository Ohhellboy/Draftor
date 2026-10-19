from pathlib import Path
import os,subprocess,sys,json
r=Path(__file__).resolve().parents[1]
assert r.is_dir()
if not (r/'TestBuilds/arduino-data/data').exists():subprocess.run([sys.executable,'-B',str(r/'tools/restore_sdk.py')],check=True)
private=r/'TestBuilds/arduino-data/data/packages/esp32'
paths=['hardware/esp32/3.3.11/cores/esp32/esp32-hal-tinyusb.c','tools/esp32s3-libs/3.3.11/lib/libarduino_tinyusb.a','tools/esp32s3-libs/3.3.11/include/arduino_tinyusb/include/tusb_config.h','tools/esp32s3-libs/3.3.11/qio_opi/include/sdkconfig.h','tools/esp32s3-libs/3.3.11/sdkconfig']
for rel in paths:
 reference=r/'vendor/libarduino_tinyusb.a' if rel.endswith('/libarduino_tinyusb.a') else r/'vendor/overlay'/rel
 if reference.is_file():assert (private/rel).read_bytes()==reference.read_bytes(),rel
 else:assert rel.endswith('/libarduino_tinyusb.a') and (r/'evidence/tinyusb/COMMANDS.json').is_file()
 # Parent component is inherited through the exact vendor copy.
(r/'evidence').mkdir(exist_ok=True)
(r/'evidence/INHERITED_SDK.json').write_text(json.dumps(dict(status='PASS',origin='inherited reviewed T03 SDK via Lexicon Round3 R1',private=str(private),overlay_checked=paths,archive_origin='inherited binary if present; otherwise rebuilt fixed sources with recorded commands',hashes='NOT_COMPUTED'),indent=2)+'\n')
env=dict(os.environ,HANIME_ESP32_DATA=str(r/'TestBuilds/arduino-data'),HANIME_BUILD_ROOT=str(r/'TestBuilds/product-s3'),HANIME_BUILD_EVIDENCE=str(r/'evidence/product-s3'),PYTHONDONTWRITEBYTECODE='1')
subprocess.run([sys.executable,'-B',str(r/'integration/product/scripts/build_s3.py')],env=env,check=True)
