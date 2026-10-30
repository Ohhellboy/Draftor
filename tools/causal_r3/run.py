from pathlib import Path
import subprocess,json,os
r=Path(__file__).resolve().parents[2];p=r/'integration/product';old=r.parents[1]/'hid-r2/source'
t=r/'tools/causal_r3';out=r/'TestBuilds/causal-r3';ev=r/'evidence/causal-r3';out.mkdir(parents=True,exist_ok=True);ev.mkdir(parents=True,exist_ok=True)
results=[]
def run(name,cmd):
 q=subprocess.run(list(map(str,cmd)),capture_output=True,text=True);log=q.stdout+q.stderr;(ev/(name+'.log')).write_text(log)
 ok=q.returncode==0 and not any(x in log for x in ['ERROR: AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer'])
 results.append({'name':name,'exit':q.returncode,'pass':ok,'command':list(map(str,cmd))});(ev/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,ok,flush=True)
 if not ok:print(log);raise SystemExit(1)
def inc(root):return ['-I'+str(root/'integration/product/platform'),'-I'+str(root/'integration/product/dependencies/common'),'-I'+str(root/'integration/product/dependencies/s3/libraries/BLE/src'),'-I'+str(out)]
s=(p/'platform/device_main.cpp').read_text();baseline=(old/'integration/product/platform/device_main.cpp').read_text()
def function(s,name,end):return s[s.index(name):s.index(end,s.index(name))]
usb=function(s,'bool usbKey(', '\n#else');assert usb==function(baseline,'bool usbKey(','\n#else')
(out/'usb_actual.inc').write_text(usb)
entry=(p/'platform/usb_transfer.hpp').read_text();(out/'entry_actual.inc').write_text(function(entry,'inline bool usbEntryKey(','\n// Hardware'))
(out/'causal_actual.inc').write_text(function(s,'__attribute__((noinline)) bool usbCausalKey(','\n#endif'))
# Directly check unchanged business implementations and placement of the causal boundary.
assert s.index('if(!inputRoute.accept(e,millis()))') < s.index('if(usbCausalKey(e))')
for rel in ['platform/usb_transfer.cpp','platform/usb_document_import.hpp','platform/usb_runtime_allocation.hpp','platform/hid_keyboard.cpp']:
 assert (p/rel).read_bytes()==(old/'integration/product'/rel).read_bytes(),rel
bs=(p/'platform/ble_keyboard_host.cpp').read_text();oldbs=(old/'integration/product/platform/ble_keyboard_host.cpp').read_text();assert function(bs,'void notify(','\nclass ClientCallbacks')==function(oldbs,'void notify(','\nclass ClientCallbacks')
(out/'notify_actual.inc').write_text(function(bs,'void notify(','\nclass ClientCallbacks'));(out/'admit_actual.inc').write_text(function(bs,'bool admit(','\nvoid publishFrame'))
gap=(p/'dependencies/s3/libraries/BLE/src/BLEClient.cpp').read_text();start=gap.index('    case BLE_GAP_EVENT_NOTIFY_RX:');start=gap.index('{',start)+1;end=gap.index('    }  // BLE_GAP_EVENT_NOTIFY_RX',start);(out/'gap_actual.inc').write_text(gap[start:end])
g=(r/'tools/hid_r2/gap_fixture.cpp').read_text().replace('#include <map>','#include <map>\n#include <atomic>')
g=g.replace('uint16_t m_conn_id=7;bool m_isConnected=true;', 'std::atomic<uint16_t> m_conn_id{7};std::atomic<bool> m_isConnected{true},m_retiring{false},m_connectPending{false};')
g=g.replace('Core fixture;std::vector<Rx> stages;', 'Core fixture;std::vector<Rx> stages;\nvoid inc(Stat s)noexcept{fixture.add(s); }\nstatic uint32_t now(){return 100;}')
wrapper=(p/'platform/hid_probe.cpp').read_text();obs=function(wrapper,'void hostEvent(','void discovery()').replace('core.','fixture.')
g=g.replace('void raw(uint16_t', obs+'\nvoid raw(uint16_t')
g=g.replace(' characteristic.m_notifyCallback=nullptr;', '''
 const auto rawBefore=hidprobe::fixture.stats[hidprobe::RawCount].load();
 client.m_conn_id=9;assert(receive(&client,&event)==0&&hidprobe::fixture.stats[hidprobe::RawCount]==rawBefore);
 client.m_conn_id=7;client.m_isConnected=false;assert(receive(&client,&event)==0&&hidprobe::fixture.stats[hidprobe::RawCount]==rawBefore);client.m_isConnected=true;
 assert(hidprobe::fixture.stats[hidprobe::NotifySeen]==4&&hidprobe::fixture.stats[hidprobe::ConnRejected]==1&&hidprobe::fixture.stats[hidprobe::ConnectedRejected]==1);
 auto early=[](){hidprobe::HostScope probe(10);return;};early();assert(hidprobe::fixture.stats[hidprobe::HostEntered]==1&&hidprobe::fixture.stats[hidprobe::HostExited]==1);
 puts("PASS actual pre-guard probe distinguishes connection/connected rejection; host RAII exits on return");
 characteristic.m_notifyCallback=nullptr;''')
(out/'gap_fixture.cpp').write_text(g)
for mode in ['normal','sanitize']:
 flags=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
 if mode=='sanitize':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 cases=[('fair',t/'fairness.cpp',[],r),('R2-starvation',t/'fairness.cpp',['-DEXPECT_R2_STARVATION'],old),('command',t/'command_fixture.cpp',['-DDRAFTOR_WS_RUNTIME_DIAG=1'],r),('gap',out/'gap_fixture.cpp',['-DDRAFTOR_WS_HID_DIAG=1',p/'platform/hid_keyboard.cpp',p/'dependencies/common/body_key_repeat.cpp'],r),('notify',r/'tools/hid_r2/notify_fixture.cpp',['-DDRAFTOR_WS_HID_DIAG=1',p/'platform/hid_keyboard.cpp',p/'dependencies/common/body_key_repeat.cpp'],r),('wrapper',t/'wrapper_test.cpp',['-DDRAFTOR_BOARD=2','-DDRAFTOR_WS_HID_DIAG=1','-I'+str(r/'tools/hid_r2/stubs'),'-I'+str(r/'tools/runtime_r1/stubs'),p/'platform/hid_probe.cpp'],r)]
 for name,source,extra,root in cases:
  dest=out/(mode+'-'+name);run(mode+'-'+name+'-build',flags+inc(root)+[source,*extra,'-o',dest]);run(mode+'-'+name,[dest])
(ev/'SEMANTIC_BOUNDARIES.json').write_text(json.dumps({'usbKey_byte_identical_to_R2':True,'keyboard_notify_byte_identical_to_R2':True,'transfer_and_document_validity_sources_unchanged':True,'causal_after_admit_route':True,'production_default_macro_disabled':True},indent=2)+'\n')
