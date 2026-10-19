#!/usr/bin/env python3
"""Build the fixed Alpha 2 assembly; no device or network operation."""
from pathlib import Path
import os,sys,subprocess,shutil,json,time,hashlib,shlex
if not __debug__:raise SystemExit('Run without Python -O: validation is required')
R=Path(__file__).resolve().parents[3]
p=R/'integration/product';out=R/'TestBuilds/s3/DISPLAY_TX_PROTO'
evidence=R/'evidence/s3';evidence.mkdir(parents=True,exist_ok=True)
tmp=out/'tmp';tmp.mkdir(parents=True,exist_ok=True)
for name in ('TMPDIR','TMP','TEMP'):os.environ[name]=str(tmp)
os.environ['PYTHONDONTWRITEBYTECODE']='1'
pin_root=R/'release/alpha2/pins';resource_manifest=R/'release/alpha2/RESOURCE_MANIFEST.json'
resource_identity=json.loads(resource_manifest.read_text())
assert resource_identity['shared_bundle_id']==17304919664082782091
assert resource_identity['format']==4 and len(resource_identity['files'])==14
for row in resource_identity['pins']:
 assert hashlib.sha256((pin_root/row['name']).read_bytes()).hexdigest()==row['sha256'],row['name']
class Options:
 arm='DISPLAY_TX_PROTO'
 incremental=False
a=Options();is_block=True;observe=False;text_chain=True;variant=3;display_tx=1
resource_arm='SHORT_TEXT_PROTO';resource_root=resource_identity['resource_root']
usb_enabled=int(os.environ.get('DRAFTOR_USB_ENABLED','1'))
assert usb_enabled==1, 'Both exact arms retain the inherited USB assembly'
telemetry_profile=0
telemetry_capture=1
assert telemetry_profile in (0,1,2) and telemetry_capture in (0,1)
assert not telemetry_profile or telemetry_capture
assert (p/'dependencies/core/include/hanime/core.hpp').is_file()
stage=out/'stage/HanIME_S3_IME';build=out/'build';dist=out/'dist'
if stage.exists():shutil.rmtree(stage)
for d in [stage,build,dist]:d.mkdir(parents=True,exist_ok=True)
shutil.copy2(p/'partitions/B.csv',stage/'partitions.csv')
# ABI changes require all core/extension/sketch consumers to compile again.
# Reuse the board SDK core cache, never a prior HanimePinned or sketch object.
for owned in [build/'sketch',build/'libraries/HanimePinned',build/'libraries/BLE']:
 if owned.exists() and not a.incremental:shutil.rmtree(owned)
if (p/'dependencies').exists():
 common=p/'dependencies/common';core=p/'dependencies/core';ext=p/'dependencies/extension';base=p/'dependencies/s3'
else:raise SystemExit('Missing packaged product dependencies')
for d in [p/'platform',p/'src',common]:
 for f in d.iterdir():
  if f.is_file() and f.suffix in ['.cpp','.h','.hpp'] and (usb_enabled or not f.name.startswith(('usb_','wifi_'))):shutil.copy2(f,stage/f.name)
(stage/'HanIME_S3_IME.ino').write_text('#include "device_main.hpp"\nvoid setup(){deviceSetup();}\nvoid loop(){deviceLoop();}\n')
# Preserve the measured product assembly.
for name in ('candidate_window.cpp','device_ime.cpp','device_main.cpp','candidate_build.hpp','cp_observe.hpp'):
 source=p/('platform' if name=='device_main.cpp' else 'src')/name
 shutil.copy2(source,stage/name)
for private_name in ('base_order_internal.hpp','shared_structural.hpp'):
 private_header=p/'dependencies/core/src'/private_name
 if private_header.exists():shutil.copy2(private_header,stage/private_header.name)
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
pin=pin_root/'release_pin.hpp'
block_pin=pin_root/'block_release_pin.hpp'
wubi_pin=pin_root/'wubi_release_v1.hpp'
# Arduino flattens sketch headers: replace the inherited local Wubi pin too.
# Both sketch and library copies are identical selected-arm bytes and audited.
for selected_pin in (pin,block_pin,wubi_pin):
 shutil.copy2(selected_pin,src/selected_pin.name)
 shutil.copy2(selected_pin,stage/selected_pin.name)
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

qr=p.parent/'qr';qr_lib=out/'libraries/DraftorQr';qr_src=qr_lib/'src'
if qr_lib.exists():shutil.rmtree(qr_lib)
qr_src.mkdir(parents=True)
(qr_lib/'library.properties').write_text('name=DraftorQr\nversion=0.1.0\nauthor=Draftor\nmaintainer=Draftor\nsentence=Bounded generic QR encoding\nparagraph=Fixed Nayuki C encoder and caller-owned workspace\ncategory=Data Processing\narchitectures=*\n')
shutil.copytree(qr/'include/draftor_qr',qr_src/'draftor_qr')
for folder in [qr/'src',qr/'vendor/nayuki']:
 for f in folder.iterdir():
  if f.suffix in ('.c','.cpp','.h','.hpp'):shutil.copy2(f,qr_src/f.name)
(qr_src/'DraftorQr.h').write_text('#pragma once\n#include "draftor_qr/qr.hpp"\n')
ino=stage/'HanIME_S3_IME.ino';ino.write_text('#include <DraftorQr.h>\n'+ino.read_text())
usb_library=['--library',str(qr_lib)]
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
 usb_library+=['--library',str(transfer_lib)]

cli=Path(os.environ.get('ARDUINO_CLI',shutil.which('arduino-cli') or 'arduino-cli'))
data=R/'TestBuilds/arduino-data'
assert (data/'data/packages/esp32/tools/esp32s3-libs/3.3.11/lib/libarduino_tinyusb.a').is_file(), 'Run tools/build_candidate.py first'
version=subprocess.check_output([str(cli),'version'],text=True)
assert '1.4.1' in version, 'Use pinned Arduino CLI 1.4.1'
env=dict(os.environ,ARDUINO_DIRECTORIES_DATA=str(data/'data'),ARDUINO_DIRECTORIES_DOWNLOADS=str(out/'downloads'),ARDUINO_DIRECTORIES_USER=str(out/'user'),ARDUINO_BUILD_CACHE_PATH=str(out/'sdk-cache'))
fqbn='esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,FlashMode=qio,USBMode=hwcdc,CDCOnBoot=default,UploadSpeed=460800,DebugLevel=info'
if usb_enabled:fqbn=fqbn.replace('USBMode=hwcdc','USBMode=default')
if telemetry_capture:fqbn=fqbn.replace('DebugLevel=info','DebugLevel=none')
tf_hz=int(os.environ.get('HANIME_TF_RUNTIME_HZ','20000000'))
if tf_hz != 20000000:raise SystemExit('T06 TF20 candidate requires 20 MHz runtime; device qualification pending')
profile=int(os.environ.get('HANIME_OUTPUT_PROFILE','2'))
if profile!=2:raise SystemExit('Both exact arms retain OutputProfile=2 and its memory policy')
save_diagnostic=int(os.environ.get('HANIME_SAVE_SUBSTAGE_DIAGNOSTIC','0'))
assert save_diagnostic==0
# Own executable helpers; the installed SDK remains read-only. No historical
# research checkout or author-specific path is an input to this entry point.
original_helpers=data/'data/packages/esp32/tools/esp-x32/2601'
helper_root=out/'toolchain_helpers'
helper_names=['libexec/gcc/xtensa-esp-elf/14.2.0/'+n for n in ('cc1','cc1plus','collect2','liblto_plugin.so')]+['xtensa-esp-elf/bin/'+n for n in ('as','ld','ar')]
for name in helper_names:
 source=original_helpers/name;target=helper_root/name
 target.parent.mkdir(parents=True,exist_ok=True)
 data_bytes=source.read_bytes();target.write_bytes(data_bytes);shutil.copymode(source,target)
 assert target.read_bytes()==data_bytes,name
helper_flags='-B'+str(helper_root/'libexec/gcc/xtensa-esp-elf/14.2.0')+'/ -B'+str(helper_root/'xtensa-esp-elf/bin')+'/'
flags=helper_flags+' -DHANIME_BUILD_HOST_ADAPTERS=0 -DHANIME_BUILD_TOOLS=0 -DHANIME_BUILD_FIXTURES=0 -DHANIME_BUILD_TESTS=0 -DHANIME_OUTPUT_PROFILE='+str(profile)+' -DHANIME_TF_RUNTIME_HZ='+str(tf_hz)+' -std=gnu++17 -fexceptions -Werror -fstack-usage -DHANIME_SAVE_SUBSTAGE_DIAGNOSTIC='+str(save_diagnostic)+' -DHANIME_BASE_ORDER_LOCATE_ENTRIES=8'
flags+=' -DDRAFTOR_PERFORMANCE_COMMON=1 -DDRAFTOR_TEXT_RESIDENT=0 -DDRAFTOR_BLOCK_BACKEND=1 -DHANIME_TEXT_CHAIN=1 -DHANIME_STRUCTURAL_HOST_OBSERVE=0 -DDRAFTOR_INPUT_PRESENT_COORD=0 -DDRAFTOR_COORD_OBSERVE=0 -DDRAFTOR_SHARED_TEXT_VARIANT='+str(variant)
pin_sha=hashlib.sha256(pin.read_bytes()+block_pin.read_bytes()+wubi_pin.read_bytes()).hexdigest()
label='DRAFTOR_ALPHA2_20261019_RELEASE'
(stage/'block_build_metadata.hpp').write_text('#pragma once\n#define DRAFTOR_B_FIRMWARE_LABEL "'+label+'"\n#define DRAFTOR_B_PIN_SHA "'+pin_sha+'"\n#define DRAFTOR_LAYOUT_ARM "'+a.arm+'"\n#define DRAFTOR_LAYOUT_RESOURCE_ROOT "'+resource_root+'"\n')
flags+=' -DDRAFTOR_DISPLAY_TRANSACTION='+str(display_tx)+' -DDRAFTOR_DISPLAY_TX_OBSERVE=0'
flags+=' -DDRAFTOR_CP_OBSERVE='+str(int(observe))
flags+=' -Wno-error=misleading-indentation'
flags+=' -DDRAFTOR_AUTH_HANDOFF=1 -DHANIME_SHA256_BATCHED_FINAL=0 -DDRAFTOR_AUTH_OBSERVE=0'
flags+=' -DDRAFTOR_RELEASE_BUILD=1 -DDRAFTOR_USB_ENABLED='+str(usb_enabled)
mapping=' '.join(chr(34)+flag+prefix+'=.'+chr(34) for prefix in (str(R),) for flag in ('-ffile-prefix-map=','-fmacro-prefix-map='))
flags+=' '+mapping
flags+=' -DDRAFTOR_TELEMETRY_PROFILE='+str(telemetry_profile)+' -DDRAFTOR_TELEMETRY_CAPTURE='+str(telemetry_capture)
sd=p/'libraries/SD' if (p/'libraries/SD').is_dir() else base/'libraries/SD'
evidence.mkdir(parents=True,exist_ok=True)
command=[str(cli),'compile','--jobs','2','--fqbn',fqbn,'--warnings','all','--build-property','compiler.c.extra_flags='+helper_flags+' '+mapping,'--build-property','compiler.cpp.extra_flags='+flags,'--build-property','compiler.cpp.flags=-MMD -c "@{compiler.sdk.path}/flags/cpp_flags" {compiler.warning_flags} {compiler.optimization_flags} {compiler.common_werror_flags} -std=gnu++17','--build-property','compiler.c.elf.extra_flags='+helper_flags+' -Wl,--wrap=_Znwj,--wrap=_Znaj,--wrap=_ZnwjRKSt9nothrow_t,--wrap=_ZnajRKSt9nothrow_t','--library',str(lib),*usb_library,'--library',str(sd),'--library',str(base/'libraries/BLE'),'--build-path',str(build),'--output-dir',str(dist),str(stage)]
command[2:2]=['--build-property','upload.maximum_size=4194304']
command[2:2]=['--build-property','compiler.path='+str(data/'data/packages/esp32/tools/esp-x32/2601/bin')+'/']
(evidence/'effective_build.py').write_bytes(Path(__file__).read_bytes())
(evidence/'BUILD_METADATA.hpp').write_bytes((stage/'block_build_metadata.hpp').read_bytes())
(evidence/'s3_command.json').write_text(json.dumps({'command':command,'fqbn':fqbn,'firmware_label':label,'combined_pin_sha256':pin_sha,'product_source':str(p),'sdk_source':os.environ.get('DRAFTOR_SDK_SOURCE'),'pin_sources':[str(pin),str(block_pin),str(wubi_pin)]},indent=2)+'\n')
started_ns=time.time_ns()
with (evidence/'s3_build.log').open('w') as log:
 result=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
(evidence/'BUILD_RESULT.json').write_text(json.dumps({'exit':result.returncode,'wall_seconds':(time.time_ns()-started_ns)/1e9},indent=2)+'\n')
print('S3 build exit:',result.returncode,'log:',evidence/'s3_build.log',flush=True)
if result.returncode:raise SystemExit(result.returncode)
database=json.loads((build/'compile_commands.json').read_text())
objects={f.resolve() for folder in (build/'sketch',build/'libraries/HanimePinned') for f in folder.rglob('*.o')}
compiled=[]
sdk_input=Path(os.environ['DRAFTOR_SDK_SOURCE']).resolve()
sdk_components={name:str((sdk_input/path).resolve()) for name,path in {
 'installed_data':'.',
 'esp32_hardware_3.3.11':'packages/esp32/hardware/esp32/3.3.11',
 'esp_x32_2601':'packages/esp32/tools/esp-x32/2601',
 'esp32s3_libs_3.3.11':'packages/esp32/tools/esp32s3-libs/3.3.11'}.items()}
sdk_roots=[Path(path) for path in sdk_components.values()]
for row in database:
 argv=row['arguments']
 if '-o' not in argv:continue
 obj=Path(argv[argv.index('-o')+1]).resolve()
 if obj not in objects:continue
 source=Path(row['file'])
 assert obj.stat().st_mtime_ns>=started_ns and obj.read_bytes()[:4]==b'\x7fELF'
 assert '-fexceptions' in argv and '-Werror' in argv
 assert '-DDRAFTOR_DISPLAY_TRANSACTION=1' in argv and '-DDRAFTOR_SHARED_TEXT_VARIANT=3' in argv
 assert [v for v in argv if v.startswith('-std=')][-1]=='-std=gnu++17'
 dep=obj.with_suffix('.d').read_text().replace('\\\n',' ')
 dependencies=[Path(v) for v in shlex.split(dep.split(':',1)[1])]
 assert source in dependencies
 for d in dependencies:
  assert d.is_file() and d.stat().st_mtime_ns<=obj.stat().st_mtime_ns,str(d)
  resolved=d.resolve()
  assert R==resolved or R in resolved.parents or any(root==resolved or root in resolved.parents for root in sdk_roots), 'Undeclared input: '+str(d)
 compiled.append({'source':str(source),'object':str(obj),'dependencies':list(map(str,dependencies))})
assert {Path(r['object']) for r in compiled}==objects
assert any(r['source'].endswith('/extension/segment_planner.cpp') for r in compiled)
included_pins={}
for name in ('release_pin.hpp','block_release_pin.hpp','wubi_release_v1.hpp'):
 paths=sorted({d for row in compiled for d in row['dependencies'] if Path(d).name==name})
 assert paths, 'Missing active pin: '+name
 expected=(pin_root/name).read_bytes()
 for path in paths:
  data_bytes=Path(path).read_bytes();directive=('#line 1 "'+str(stage/name)+'"\n').encode()
  if data_bytes.startswith(directive):data_bytes=data_bytes[len(directive):]
  assert data_bytes==expected,'Wrong actual pin include: '+path
 included_pins[name]=paths
(evidence/'PIN_INCLUDE_BINDING.json').write_text(json.dumps({'result':'PASS','pins':included_pins,'normalization':'exact Arduino stage #line 1 only'},indent=2)+'\n')
(evidence/'CONSUMER_REBUILD.json').write_text(json.dumps({'fresh_objects':len(compiled),'all_dependencies_declared':True,'declared_sdk_components':sdk_components,'objects':compiled},indent=2)+'\n')
stack=subprocess.run([sys.executable,str(p/'scripts/check_vertical_stack.py'),str(build/'sketch'),'--output',str(evidence/'STACK_GATE.json')],capture_output=True,text=True)
(evidence/'STACK_GATE.log').write_text(stack.stdout+stack.stderr)
if stack.returncode:raise SystemExit('Rendering stack check failed')
compiler=Path(next(row['arguments'][0] for row in database if row['file'].endswith('candidate_window.cpp')))
elf=dist/'HanIME_S3_IME.ino.elf';binary=dist/'HanIME_S3_IME.ino.bin'
for tool,filename,arguments in [('size','ELF_SECTIONS.txt',['-A']),('nm','ELF_SYMBOLS.txt',['-S','-C'])]:
 (evidence/filename).write_text(subprocess.check_output([str(compiler.with_name('xtensa-esp32s3-elf-'+tool)),*arguments,str(elf)],text=True))
assert binary.stat().st_size<=4194304
assert label.encode() in binary.read_bytes() and resource_root.encode() in binary.read_bytes()
artifacts={f.name:{'bytes':f.stat().st_size} for f in dist.iterdir() if f.is_file()}
(evidence/'s3_artifacts.json').write_text(json.dumps(artifacts,indent=2)+'\n')
print('Full S3 build, actual pins and stack checks complete. Validation output:',dist,flush=True)

# The author explicitly requested removing composition/input-timing observation
# from the release. Check actual linked output, not just the requested macro.
for forbidden in (b'[SW] n=',b'[SW_VIEW]',b'[AUTH_DONE]',b'[SW_CONFIG]',b'[UI_TEXT]',b'[UI_FLOW]'):
 assert forbidden not in binary.read_bytes(), 'Input observation leaked: '+repr(forbidden)
symbols=(evidence/'ELF_SYMBOLS.txt').read_text()
for forbidden in ('formatServiceEndpoint','snapshotServiceEndpoint','traceServiceFeedback'):
 assert forbidden not in symbols, 'Linked input observer: '+forbidden
(evidence/'QUIET_RELEASE.json').write_text(json.dumps({'result':'PASS_LINKED_INPUT_OBSERVER_ABSENCE','auth_observe':0,'telemetry_profile':0,'telemetry_capture':1,'input_endpoint_strings_absent':True,'input_observer_symbols_absent':True,'ordinary_boot_health_error_logs_retained':True,'device_tested':False},indent=2)+'\n')
print('Release input-code/timing observation absent from linked image.',flush=True)
