from pathlib import Path
import subprocess,json,shutil,tarfile,os
r=Path(__file__).resolve().parents[2];p=r/'integration/product';t=r/'tools/hogp_r4';out=r/'TestBuilds/hogp-r4';ev=r/'evidence/hogp-r4';out.mkdir(parents=True,exist_ok=True);ev.mkdir(parents=True,exist_ok=True)
root=r.parents[2];base=Path(os.environ.get('DRAFTOR_R3_BASELINE',str(root/'outputs/WS_RLCD42_F9_R2_RECOVERY_R3_CAUSAL_REVIEW')))
with tarfile.open(base/'Source.tar.gz') as ar:old={m.name.removeprefix('DraftorSource/'):ar.extractfile(m).read() for m in ar.getmembers() if m.isfile()}
s=(p/'platform/ble_keyboard_host.cpp').read_text();bs=old['integration/product/platform/ble_keyboard_host.cpp'].decode()
def span(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
for name,start,end in [('notify_actual.inc','void notify(','\nclass ClientCallbacks'),('invalidate_actual.inc','void invalidate(','\nvoid notify('),('admit_actual.inc','bool admit(','\nvoid publishFrame')]:
 body=span(s,start,end);assert body==span(bs,start,end);(out/name).write_text(body)
(out/'hogp_actual.inc').write_text(span(s,'// DRAFTOR_HOGP_INPUTS_BEGIN','// DRAFTOR_HOGP_INPUTS_END'))
(out/'subscribe_R3.inc').write_text(span(bs,'bool subscribe(','std::atomic<bool> scanFound').replace('bool subscribe(','bool subscribeR3(',1))
prime=span(s,'     s3hid::State initial[8];','     ble_gap_conn_desc identity{};');assert prime==span(bs,'     s3hid::State initial[8];','     ble_gap_conn_desc identity{};');(out/'prime_actual.inc').write_text(prime)
clear=span(s,'   portENTER_CRITICAL(&stateMux);\n   for(auto& sub:subscriptions)','   delete client;');(out/'clear_actual.inc').write_text(clear)
mapfile=t/'HID_REPORT_MAP_R3.bin';assert mapfile.stat().st_size==253
original_map=root/'outputs/WS_RLCD42_R3_CAUSAL_DEVICE_VALIDATION/Evidence/HID_REPORT_MAP.bin'
if original_map.is_file():assert mapfile.read_bytes()==original_map.read_bytes()
# Keep the command chain and every unrelated baseline product file byte-identical.
modified=[rel for rel,b in old.items() if rel.startswith('integration/product/') and (r/rel).read_bytes()!=b]
allowed={'integration/product/platform/ble_keyboard_host.cpp','integration/product/platform/hid_probe_core.hpp','integration/product/platform/hid_probe.cpp','integration/product/dependencies/s3/libraries/BLE/src/hanime_hid_probe.hpp','integration/product/scripts/build_s3.py'}
assert set(modified)==allowed,modified
# Strip only the explicit R4 guarded additions; the default/original body must match R3.
normalized=s
start=normalized.index('#if DRAFTOR_WS_HOGP_INPUTS\n// Same pinned IDF export')
end=normalized.index('#endif\n',start)+len('#endif\n');normalized=normalized[:start]+normalized[end:]
start=normalized.index('// DRAFTOR_HOGP_INPUTS_BEGIN');end=normalized.index('// DRAFTOR_HOGP_INPUTS_END\n',start)+len('// DRAFTOR_HOGP_INPUTS_END\n');normalized=normalized[:start]+normalized[end:]
normalized=normalized.replace('#if DRAFTOR_WS_HOGP_INPUTS\n return subscribeAllInputs(client);\n#else\n','')
normalized=normalized.replace(' return true;\n#endif\n}\nstd::atomic<bool> scanFound',' return true;\n}\nstd::atomic<bool> scanFound')
normalized=normalized.replace('#if DRAFTOR_WS_HOGP_INPUTS\n   clearInputSubscriptions();\n#endif\n','')
assert normalized==bs,'Unexpected change outside R4 target guard'
refs=json.loads((t/'REPORT_REFERENCES_R3.json').read_text());assert [(v['handle'],v['report_id'],v['report_type']) for v in refs]==[(42,1,1),(52,3,1),(56,4,1),(60,5,1),(64,17,1)]
(ev/'SEMANTIC_BOUNDARIES.json').write_text(json.dumps({'modified_product_files':modified,'original_target_default_subscribe_and_lifecycle_byte_identical_after_guard_removal':True,'all_other_R3_product_files_byte_identical':True,'notify_invalidate_admit_and_initial_release_priming_byte_identical':True,'device_main_USB_causal_dispatch_and_protections_byte_identical':True,'SDK_display_SDMMC_IME_document_sources_unchanged':True},indent=2)+'\n')
results=[]
def run(name,cmd):
 q=subprocess.run(list(map(str,cmd)),capture_output=True,text=True);text=q.stdout+q.stderr;(ev/(name+'.log')).write_text(text);ok=q.returncode==0 and not any(v in text for v in ['ERROR: AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer']);results.append({'name':name,'pass':ok,'exit':q.returncode,'command':list(map(str,cmd))});(ev/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,ok,flush=True)
 if not ok:print(text);raise SystemExit(1)
includes=['-I'+str(p/'platform'),'-I'+str(p/'dependencies/common'),'-I'+str(p/'dependencies/s3/libraries/BLE/src'),'-I'+str(out)]
common=[p/'platform/hid_keyboard.cpp',p/'dependencies/common/body_key_repeat.cpp']
for mode in ['normal','sanitize']:
 flags=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
 if mode=='sanitize':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 for name,src,extra in [('subscription',t/'subscription_fixture.cpp',['-DDRAFTOR_WS_HOGP_INPUTS=1','-DDRAFTOR_WS_HID_DIAG=1',*common]),('notify',r/'tools/hid_r2/notify_fixture.cpp',['-DDRAFTOR_WS_HID_DIAG=1',*common]),('fairness',t/'fairness.cpp',[])]:
  dst=out/(mode+'-'+name);run(mode+'-'+name+'-build',flags+includes+[src,*extra,'-o',dst]);run(mode+'-'+name,[dst,*([mapfile] if name=='subscription' else [])])
