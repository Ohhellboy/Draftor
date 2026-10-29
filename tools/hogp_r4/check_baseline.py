#!/usr/bin/env python3
"""Offline readiness gate over actual received bytes; never writes to device."""
from pathlib import Path
import importlib.util,json,argparse,subprocess,tempfile
root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('trace',root/'read_trace.py');trace=importlib.util.module_from_spec(spec);spec.loader.exec_module(trace)
def check(raw,inspector,after_ms=0,full=True):
 rows,s=trace.decode(raw);reasons=[];decoded=[]
 if s['sequence_gaps'] or s['skipped_bytes']:reasons.append('TRACE_GAP_OR_NON_PROTOCOL_BYTES')
 if s['window_end']:reasons.append('WINDOW_ENDED')
 if not s['report_map']['complete_in_trace'] or not s['report_map'].get('parsed_keyboard_fields') or not s['report_map'].get('original_bytes'):reasons.append('MAP_NOT_COMPLETE')
 if not s['report_references']:reasons.append('REFERENCES_MISSING')
 setup=s.get('input_setup');subs=s.get('input_subscriptions',[])
 if not setup or setup['status'] not in (1,2):reasons.append('INPUT_SETUP_NOT_VERIFIED')
 elif setup['generation']!=s['discovery_generation'] or setup['epoch']!=s['descriptor_epoch']:reasons.append('INPUT_SETUP_GENERATION_MISMATCH')
 expected={(r['handle'],r['report_id']) for r in s['report_references'] if r['report_type']==1}
 actual={(r['handle'],r['report_id']) for r in subs}
 if not expected or actual!=expected or len(subs)!=len(actual) or not setup or setup['input_count']!=len(subs):reasons.append('INPUT_SUBSCRIPTIONS_INCOMPLETE')
 if any(not r['complete_in_trace'] or r['status']!=3 or r.get('read_bytes')!=2 or r.get('cccd_value')!=1 for r in subs):reasons.append('CCCD_NOT_VERIFIED')

 health=[]
 for row in rows:
  if row['kind']==1:health.append({'ms':row['ms'],'fields':{}})
  elif 64<=row['kind']<64+len(trace.STATS) and health:health[-1]['fields'][trace.STATS[row['kind']-64]]=row['value']
 health=[h for h in health if len(h['fields'])==len(trace.STATS)]
 if len(health)<2:reasons.append('TWO_COMPLETE_HEALTH_SAMPLES_REQUIRED')
 else:
  a,b=health[-2:];f=b['fields']
  if f['loop_end']<=a['fields']['loop_end']:reasons.append('MAIN_LOOP_NOT_ADVANCING')
  if f['ble_state']!=261 or f['epoch']!=s['descriptor_epoch']:reasons.append('CURRENT_READY_EPOCH_NOT_BOUND_TO_MAP')
  if any(f[k] for k in ['bad','await_drop','queue_overflow','admit_drop','route_drop','raw_drop','main_drop','heap_stop']):reasons.append('ERROR_OR_DROP_COUNTER')
  if f['raw_count']>=112:reasons.append('RAW_BUDGET_LOW')
  if b['ms']-health[0]['ms']>=480000:reasons.append('CAPTURE_WINDOW_LOW')
 if not reasons:
  refs={r['handle']:r for r in s['report_references'] if r.get('selected_keyboard')}
  with tempfile.TemporaryDirectory(prefix='r4-map-') as temp:
   path=Path(temp)/'map.bin';path.write_bytes(bytes.fromhex(s['report_map']['hex']))
   for report in s['raw_reports']:
    if report.get('generation')!=s['discovery_generation'] or report['ms']<after_ms:continue
    if report['handle'] not in refs:continue
    if not report['complete_in_trace'] or report['prefix_truncated']:reasons.append('RAW_REPORT_NOT_COMPLETE');continue
    ref=refs[report['handle']];q=subprocess.run([str(inspector),str(path),str(ref['report_id']),report['prefix_hex']],capture_output=True,text=True)
    if q.returncode:reasons.append('ACTUAL_PRODUCT_HID_DECODE_FAILED');continue
    state=json.loads(q.stdout);decoded.append(dict(report,state=state,report_id=ref['report_id']))
  if decoded and (health[-1]['ms']<decoded[-1]['ms'] or health[-1]['fields']['raw_count']<decoded[-1]['id']+1):reasons.append('POST_BASELINE_HEALTH_NOT_RECEIVED')
  keys={u for r in decoded for u in r['state']['keys']};required={4,24,41} if full else {4,41}
  if not required<=keys:reasons.append('SPECIFIED_BASELINE_KEYS_NOT_RECEIVED')
  if full and not any((r['state']['mods']&0x11) and (r['state']['mods']&0x22) for r in decoded):reasons.append('CTRL_SHIFT_REPORT_NOT_RECEIVED')
  if not decoded or decoded[-1]['state']['mods'] or decoded[-1]['state']['keys']:reasons.append('FINAL_ALL_RELEASE_NOT_RECEIVED')
 return {'status':'BASELINE_READY' if not reasons else 'NOT_READY','reasons':sorted(set(reasons)),'decoded_reports':decoded,'trace_summary':s,'after_ms':after_ms,'full_baseline':full,'requires_unchanged_usb_identity':True,'physical_actions_authorized_by_this_tool':False}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--raw',type=Path,required=True);p.add_argument('--inspector',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--after-ms',type=int,default=0);p.add_argument('--post-menu',action='store_true');a=p.parse_args();result=check(a.raw.read_bytes(),a.inspector,a.after_ms,not a.post_menu);a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'status':result['status'],'reasons':result['reasons'],'decoded_reports':len(result['decoded_reports'])}));raise SystemExit(0 if result['status']=='BASELINE_READY' else 3)
