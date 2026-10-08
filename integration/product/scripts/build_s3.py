#!/usr/bin/env python3
"""Reuse the installed 1.4.1/3.3.11 SDK; source stage and build live OUTSIDE this tree."""
from pathlib import Path
import os,sys,subprocess,shutil,json,time
p=Path(__file__).resolve().parents[1]
work=Path(os.environ.get('HANIME_WORKSPACE',str(p.parent.parent))).resolve()
out=Path(os.environ.get('HANIME_BUILD_ROOT',str(p.parent/'TestBuilds/product-s3'))).resolve()
if p==out or p in out.parents:raise SystemExit('Build root must be outside source')
evidence=Path(os.environ.get('HANIME_BUILD_EVIDENCE',str(out/'evidence'))).resolve()
# Generated outputs are explicitly selected and never written into product source.
tmp=out/'tmp'
tmp.mkdir(parents=True,exist_ok=True)
for name in ['TMPDIR','TMP','TEMP']:os.environ[name]=str(tmp)
os.environ['PYTHONDONTWRITEBYTECODE']='1'

usb_enabled=int(os.environ.get('DRAFTOR_USB_ENABLED','1'))
assert usb_enabled in (0,1)
telemetry_profile=0
telemetry_capture=1
assert telemetry_profile in (0,1,2) and telemetry_capture in (0,1)
assert not telemetry_profile or telemetry_capture
subprocess.run([sys.executable,str(p/'scripts/verify_dependencies.py'),str(work),str(p)],check=True)
stage=out/'stage/HanIME_S3_IME';build=out/'build';dist=out/'dist'
if stage.exists():shutil.rmtree(stage)
for d in [stage,build,dist]:d.mkdir(parents=True,exist_ok=True)
# ABI changes require all core/extension/sketch consumers to compile again.
# Reuse the board SDK core cache, never a prior HanimePinned or sketch object.
for owned in [build/'sketch',build/'libraries/HanimePinned',build/'libraries/BLE']:
 if owned.exists():shutil.rmtree(owned)
if (p/'dependencies').exists():
 common=p/'dependencies/common';core=p/'dependencies/core';ext=p/'dependencies/extension';base=p/'dependencies/s3'
else:
 common=work/'Draftor_EditorCore_v0_1/common';core=work/'HANIME_Independent_Core';ext=work/'HANIME_Input_Extension';base=work/'Draftor_EditorCore_v0_1/platforms/s3/HANIME_ESP32S3_EnglishWriting_v0_6_SharedCore'
for d in [p/'platform',p/'src',common]:
 for f in d.iterdir():
  if f.is_file() and f.suffix in ['.cpp','.h','.hpp'] and (usb_enabled or not f.name.startswith('usb_')):shutil.copy2(f,stage/f.name)
(stage/'HanIME_S3_IME.ino').write_text('#include "device_main.hpp"\nvoid setup(){deviceSetup();}\nvoid loop(){deviceLoop();}\n')
# Match the existing Arduino flat-header stage; nested non-src headers would
# mix source/build include identities and defeat pragma once.
for f in (p/'platform/draftor').glob('*.hpp'):shutil.copy2(f,stage/f.name)
for f in stage.iterdir():
 if f.is_file() and f.suffix in ['.cpp','.hpp']:
  text=f.read_text();normalized=text.replace('#include \"draftor/','#include \"')
  if normalized!=text:f.write_text(normalized)
for d in [common/'src',common/'hanime']:
 shutil.copytree(d,stage/d.name,dirs_exist_ok=True)
lib=out/'libraries/HanimePinned';src=lib/'src'
if lib.exists():shutil.rmtree(lib)
src.mkdir(parents=True)
(lib/'library.properties').write_text('name=HanimePinned\nversion=0.13.1\nauthor=HanIME\nmaintainer=HanIME\nsentence=RC0131-rc1 API0130 and Host084-rc1 MEM1 PY1 MR1\nparagraph=Fixed RC1 core and INPUT Host084-rc1 candidate consumption\ncategory=Other\narchitectures=esp32\n')
(src/'HanimePinned.h').write_text('#pragma once\n#include "hanime_extension/submission.hpp"\n')
shutil.copytree(core/'include/hanime',src/'hanime')
pin=p/'dependencies/release_resources/release_pin.hpp'
# Pin bytes are carried with this exact resource generation.
# Packaging checks the fixed B2 binding and unchanged dependency bytes.
assert pin.read_bytes()==(p.parents[1]/'release/integration_headers/release_pin.hpp').read_bytes()
shutil.copy2(pin,src/'release_pin.hpp')
shutil.copytree(ext/'include/hanime_extension',src/'hanime_extension')
shutil.copytree(p/'dependencies/wubi/include/hanime_wubi',src/'hanime_wubi')
shutil.copytree(p/'dependencies/wubi/src',src/'wubi')
for tag,folder,names in [('core',core/'src','core stable_key paged_scheme_index paged_shared_resource exact_ranker base_order member_eligibility bounded_output candidate_session composition_commit state_v3 bounded_user_state verified_resource packed_auth'.split()),('extension',ext/'src',['composition','submission','navigation','segment_planner'])]:
 dest=src/tag;dest.mkdir()
 for n in names:shutil.copy2(folder/(n+'.cpp'),dest/(n+'.cpp'))
 for f in folder.glob('*.hpp'):shutil.copy2(f,dest/f.name)
 if tag=='extension':shutil.copy2(folder/'navigation_syllables.inc',dest/'navigation_syllables.inc')
(stage/'HanIME_S3_IME.ino').write_text('#include <HanimePinned.h>\n#include "device_main.hpp"\nvoid setup(){deviceSetup();}\nvoid loop(){deviceLoop();}\n')


if telemetry_profile:
 telemetry=p.parent/'telemetry'
 shutil.copytree(telemetry/'include/draftor/telemetry',src/'draftor/telemetry')
 dest=src/'behavior_core';dest.mkdir()
 for f in (telemetry/'src').iterdir():
  if f.is_file() and f.suffix in ('.cpp','.inc'):shutil.copy2(f,dest/f.name)

usb_library=[]
if usb_enabled:
 module=p.parent/'module';shared=p.parent/'shared'
 transfer_lib=out/'libraries/DraftorTransfer';transfer_src=transfer_lib/'src'
 if transfer_lib.exists():shutil.rmtree(transfer_lib)
 transfer_src.mkdir(parents=True)
 (transfer_lib/'library.properties').write_text('name=DraftorTransfer\nversion=1.2.0\nauthor=Draftor\nmaintainer=Draftor\nsentence=Product USB transfer\nparagraph=Fixed shared TXT source\ncategory=Data Storage\narchitectures=esp32\n')
 for folder,sub in [(module/'include/draftor_usb_transfer','draftor_usb_transfer'),(shared/'include/draftor_exchange','draftor_exchange'),(module/'platform/shared',''),(module/'platform/esp32s3','')]:
  for header in folder.glob('*.hpp'):
   dest=transfer_src/sub/header.name;dest.parent.mkdir(parents=True,exist_ok=True)
   dest.write_text('#pragma once\n#include "'+str(header)+'"\n')
 for folder in [module/'src/common',module/'platform/shared',module/'platform/esp32s3',shared/'src']:
  for impl in folder.glob('*.cpp'):(transfer_src/impl.name).write_text('#include "'+str(impl)+'"\n')
 (transfer_src/'DraftorTransfer.h').write_text('#pragma once\n#include "draftor_exchange/export_session.hpp"\n#include "draftor_usb_transfer/inbox.hpp"\n')
 ino=stage/'HanIME_S3_IME.ino';ino.write_text('#include <DraftorTransfer.h>\n'+ino.read_text())
 usb_library=['--library',str(transfer_lib)]

cli=Path(os.environ.get('ARDUINO_CLI',str(work/'Toolchains/ArduinoPico/bin/arduino-cli')))
data=Path(os.environ.get('HANIME_ESP32_DATA',str(work/'HANIME/firmware/esp32s3/HANIME_ESP32S3_HardwareBringup_v0_1/.arduino')))
owned_sdk=p.parent.parent/'TestBuilds/arduino-data'
if data.resolve()!=owned_sdk.resolve():
 raise SystemExit('Release requires its inherited private SDK: set HANIME_ESP32_DATA='+str(owned_sdk))
if not (data/'data/packages/esp32/tools/esp32s3-libs/3.3.11/lib/libarduino_tinyusb.a').is_file():
 raise SystemExit('Restore the pinned T03 MSC-only component into this release private SDK')
env=dict(os.environ,ARDUINO_DIRECTORIES_DATA=str(data/'data'),ARDUINO_DIRECTORIES_DOWNLOADS=str(out/'downloads'),ARDUINO_DIRECTORIES_USER=str(out/'user'),ARDUINO_BUILD_CACHE_PATH=str(out/'sdk-cache'))
fqbn='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=fatflash,PSRAM=opi,FlashMode=qio,USBMode=hwcdc,CDCOnBoot=default,UploadSpeed=460800,DebugLevel=info'
if usb_enabled:fqbn=fqbn.replace('USBMode=hwcdc','USBMode=default')
if telemetry_capture:fqbn=fqbn.replace('DebugLevel=info','DebugLevel=none')
tf_hz=int(os.environ.get('HANIME_TF_RUNTIME_HZ','20000000'))
if tf_hz != 20000000:raise SystemExit('T06 TF20 candidate requires 20 MHz runtime; device qualification pending')
profile=int(os.environ.get('HANIME_OUTPUT_PROFILE','2'))
if profile not in (0,1,2):raise SystemExit('Unknown output profile')
save_diagnostic=int(os.environ.get('HANIME_SAVE_SUBSTAGE_DIAGNOSTIC','0'))
assert save_diagnostic in (0,1)
flags='-DHANIME_BUILD_HOST_ADAPTERS=0 -DHANIME_BUILD_TOOLS=0 -DHANIME_BUILD_FIXTURES=0 -DHANIME_BUILD_TESTS=0 -DHANIME_OUTPUT_PROFILE='+str(profile)+' -DHANIME_TF_RUNTIME_HZ='+str(tf_hz)+' -std=gnu++17 -fexceptions -Werror -fstack-usage -DHANIME_SAVE_SUBSTAGE_DIAGNOSTIC='+str(save_diagnostic)+' -DHANIME_BASE_ORDER_LOCATE_ENTRIES=8'
flags+=' -DDRAFTOR_RELEASE_BUILD=1 -DDRAFTOR_USB_ENABLED='+str(usb_enabled)
mapping=' '.join(chr(34)+flag+str(work)+'=.'+chr(34) for flag in ('-ffile-prefix-map=','-fmacro-prefix-map='))
flags+=' '+mapping
flags+=' -DDRAFTOR_TELEMETRY_PROFILE='+str(telemetry_profile)+' -DDRAFTOR_TELEMETRY_CAPTURE='+str(telemetry_capture)
sd=p/'libraries/SD' if (p/'libraries/SD').is_dir() else base/'libraries/SD'
evidence.mkdir(parents=True,exist_ok=True)
command=[str(cli),'compile','--jobs','2','--fqbn',fqbn,'--warnings','all','--build-property','compiler.c.extra_flags='+mapping,'--build-property','compiler.cpp.extra_flags='+flags,'--build-property','compiler.cpp.flags=-MMD -c "@{compiler.sdk.path}/flags/cpp_flags" {compiler.warning_flags} {compiler.optimization_flags} {compiler.common_werror_flags} -std=gnu++17','--build-property','compiler.c.elf.extra_flags=-Wl,--wrap=_Znwj,--wrap=_Znaj,--wrap=_ZnwjRKSt9nothrow_t,--wrap=_ZnajRKSt9nothrow_t','--library',str(lib),*usb_library,'--library',str(sd),'--library',str(base/'libraries/BLE'),'--build-path',str(build),'--output-dir',str(dist),str(stage)]
(evidence/'s3_command.json').write_text(json.dumps({'command':command,'compiler_cpp_extra_flags':flags,'fqbn':fqbn,'sdk':'Arduino-ESP32 3.3.11','cli':'1.4.1','tf_runtime_requested_hz':tf_hz,'output_profile':profile,'scope':'T06 Alpha release candidate; real USB='+str(usb_enabled)+'; BASIC and benchmark OFF, safety diagnostics retained; DEVICE_NOT_RUN','active_sd':str(sd),'active_sd_files':{str(f.relative_to(sd)):f.stat().st_size for f in sorted(sd.rglob('*')) if f.is_file()}},indent=2)+'\n')
started_ns=time.time_ns()
with (evidence/'s3_build.log').open('w') as log:
 result=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
finished_ns=time.time_ns()
(evidence/'BUILD_RESULT.json').write_text(json.dumps({'exit':result.returncode,'started_ns':started_ns,'finished_ns':finished_ns,'wall_seconds':(finished_ns-started_ns)/1e9},indent=2)+'\n')
print('S3 build exit:',result.returncode,'log:',evidence/'s3_build.log')
if result.returncode:raise SystemExit(result.returncode)
# The successful compiler database is evidence of the command used for every
# consumer. Check the actual objects, not only the final link exit code.
database=json.loads((build/'compile_commands.json').read_text())
shutil.copy2(build/'compile_commands.json',evidence/'compile_commands.json')
consumer_roots=(build/'sketch',build/'libraries/HanimePinned')
objects={f.resolve() for root in consumer_roots for f in root.rglob('*.o')}
compiled=[]
for row in database:
 argv=row['arguments']
 if '-o' not in argv:continue
 obj=Path(argv[argv.index('-o')+1]).resolve()
 if obj not in objects:continue
 source=Path(row['file'])
 assert obj.stat().st_mtime_ns>=started_ns,'Old consumer object: '+str(obj)
 assert source.is_file() and obj.read_bytes()[:4]==b'\x7fELF',str(obj)
 assert '-fexceptions' in argv and '-Werror' in argv
 assert [v for v in argv if v.startswith('-std=')][-1]=='-std=gnu++17'
 compiled.append({'source':str(source),'object':str(obj),'mtime_ns':obj.stat().st_mtime_ns})
assert {Path(r['object']) for r in compiled}==objects and len(compiled)==len(objects)
assert any(r['source'].endswith('/extension/segment_planner.cpp') for r in compiled)
(evidence/'CONSUMER_REBUILD.json').write_text(json.dumps({'count':len(compiled),'all_objects_from_this_build':True,'sdk_cache_policy':'permitted; all product objects forced fresh','objects':compiled},indent=2)+'\n')
manifest={f.name:{'bytes':f.stat().st_size} for f in dist.iterdir() if f.is_file()}
(evidence/'s3_artifacts.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))

# R1 critical rendering stack regression gate (actual emitted target .su).
subprocess.run([sys.executable,str(p/"scripts/check_vertical_stack.py"),str(build/"sketch"),"--output",str(evidence/"STACK_GATE.json")],check=True)
