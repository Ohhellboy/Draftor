# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Ruotao Wang
"""Compile the fixed 25 TinyUSB units using the already-owned MSC-only SDK."""
from pathlib import Path
import os,json,re,subprocess,sys,time
r=Path(__file__).resolve().parents[1]
data=r/'TestBuilds/arduino-data/data/packages/esp32'
sdk=data/'tools/esp32s3-libs/3.3.11'
core=data/'hardware/esp32/3.3.11/cores/esp32'
out=r/'TestBuilds/tinyusb';out.mkdir(parents=True,exist_ok=True)
e=r/'evidence/tinyusb';e.mkdir(parents=True,exist_ok=True)
tmp=r/'TestBuilds/.tmp';tmp.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMPDIR=str(tmp),TMP=str(tmp),TEMP=str(tmp))
compiler=data/'tools/esp-x32/2601/bin/xtensa-esp32s3-elf-gcc'
src=r/'vendor/tinyusb/src'
sources=re.findall(r'"\$\{COMPONENT_DIR\}/tinyusb/src/([^\"]+\.c)"',(r/'vendor/CMakeLists.upstream.txt').read_text())
assert len(sources)==25, len(sources)
commands=[];objects=[]
for rel in sources:
    source=src/rel;obj=out/(source.name+'.obj');objects.append(obj)
    cmd=[str(compiler),'-c','@'+str(sdk/'flags/c_flags'),'-Os','-ffile-prefix-map='+str(r)+'=.','-fmacro-prefix-map='+str(r)+'=.','-Werror=return-type','-Wno-type-limits','-DCFG_TUSB_MCU=OPT_MCU_ESP32S3','-DCFG_TUSB_DEBUG=0','-I'+str(src),'-I'+str(sdk/'include/arduino_tinyusb/include'),'@'+str(sdk/'flags/defines'),'-iprefix',str(sdk/'include')+'/', '@'+str(sdk/'flags/includes'),'-I'+str(sdk/'qio_opi/include'),'-I'+str(core),str(source),'-o',str(obj)]
    run=subprocess.run(cmd,env=env,capture_output=True,text=True);(e/(source.name+'.log')).write_text(run.stdout+run.stderr)
    commands.append(dict(command=cmd,exit=run.returncode,source=rel,object=str(obj)))
    if run.returncode:
        print(run.stdout+run.stderr);(e/'COMMANDS.json').write_text(json.dumps(commands,indent=2)+'\n');sys.exit(run.returncode)
archive=sdk/'lib/libarduino_tinyusb.a'
assert not archive.is_symlink()
fresh=out/('libarduino_tinyusb.'+str(time.time_ns())+'.a')
assert not fresh.exists()
cmd=[str(compiler.with_name('xtensa-esp32s3-elf-ar')),'rcs',str(fresh),*map(str,objects)]
subprocess.run(cmd,env=env,check=True);commands.append(dict(command=cmd,exit=0,installed_as=str(archive)))
fresh.replace(archive)
(e/'COMMANDS.json').write_text(json.dumps(commands,indent=2)+'\n')
print('Pinned TinyUSB rebuilt: 25 units, MSC-only, 4096B buffer.')
