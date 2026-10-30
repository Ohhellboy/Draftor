from pathlib import Path
import argparse,subprocess,json,sys,os,tarfile
r=Path(__file__).resolve().parents[2];p=r/'integration/product';ap=argparse.ArgumentParser();ap.add_argument('--baseline',type=Path,default=r/'tools/hid_r2/R1_ble_keyboard_host.cpp.reference.txt');a=ap.parse_args();out=r/'TestBuilds/hid-r2';ev=r/'evidence/hid-r2';out.mkdir(parents=True,exist_ok=True);ev.mkdir(parents=True,exist_ok=True);results=[]
def run(name,cmd):
 q=subprocess.run([str(x) for x in cmd],capture_output=True,text=True,env=dict(os.environ,HID_R2_TRACE_OUT=str(ev/(name+'.bin'))));text=q.stdout+q.stderr;(ev/(name+'.log')).write_text(text);ok=q.returncode==0 and not any(w in text for w in ['AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer']);results.append({'name':name,'exit':q.returncode,'pass':ok,'command':[str(x) for x in cmd]});(ev/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,ok,flush=True)
 if not ok:print(text);raise SystemExit(1)
for mode in ['normal','sanitize']:
 flags=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pthread']
 if mode=='sanitize':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 inc=['-I'+str(p/'platform'),'-I'+str(p/'dependencies/common'),'-I'+str(p/'dependencies/s3/libraries/BLE/src')]
 for label,source in [('R1',a.baseline),('R2',p/'platform/ble_keyboard_host.cpp')]:
  folder=out/(mode+'-'+label);folder.mkdir(exist_ok=True);s=source.read_text();(folder/'notify_actual.inc').write_text(s[s.index('void notify('):s.index('\nclass ClientCallbacks')]);start=s.index('bool admit(');end=s.index('\nvoid publishFrame',start);(folder/'admit_actual.inc').write_text(s[start:end]);cmd=flags+(['-DDRAFTOR_WS_HID_DIAG=1'] if label=='R2' else [])+inc+['-I'+str(folder),r/'tools/hid_r2/notify_fixture.cpp',p/'platform/hid_keyboard.cpp',p/'dependencies/common/body_key_repeat.cpp','-o',folder/'test'];run(mode+'-'+label+'-build',cmd);run(mode+'-'+label,[folder/'test'])
 for label,source in [('R1',r/'tools/hid_r2/R1_BLEClient.cpp.reference.txt'),('R2',p/'dependencies/s3/libraries/BLE/src/BLEClient.cpp')]:
  folder=out/(mode+'-'+label+'-gap');folder.mkdir(exist_ok=True);s=source.read_text();start=s.index('    case BLE_GAP_EVENT_NOTIFY_RX:');start=s.index('{',start)+1;end=s.index('    }  // BLE_GAP_EVENT_NOTIFY_RX',start);(folder/'gap_actual.inc').write_text(s[start:end]);cmd=flags+(['-DDRAFTOR_WS_HID_DIAG=1'] if label=='R2' else [])+inc+['-I'+str(folder),r/'tools/hid_r2/gap_fixture.cpp',p/'platform/hid_keyboard.cpp',p/'dependencies/common/body_key_repeat.cpp','-o',folder/'test'];run(mode+'-'+label+'-gap-build',cmd);run(mode+'-'+label+'-gap',[folder/'test'])
 binary=out/(mode+'-core');run(mode+'-core-build',flags+inc+[r/'tools/hid_r2/core_test.cpp','-o',binary]);run(mode+'-core',[binary])
 wrapper=out/(mode+'-wrapper');run(mode+'-wrapper-build',flags+inc+['-DDRAFTOR_BOARD=2','-DDRAFTOR_WS_HID_DIAG=1','-I'+str(r/'tools/hid_r2/stubs'),'-I'+str(r/'tools/runtime_r1/stubs'),r/'tools/hid_r2/wrapper_test.cpp',p/'platform/hid_probe.cpp','-o',wrapper]);run(mode+'-wrapper',[wrapper])
run('decoder-self-test',[sys.executable,'-B',r/'tools/hid_r2/read_trace.py','--self-test'])
run('decoder-actual-core',[sys.executable,'-B',r/'tools/hid_r2/read_trace.py','--decode',ev/'normal-core.bin','--out',ev/'decoded-core'])
print('PASS directed actual-notify fixtures and diagnostic core; no device access')
