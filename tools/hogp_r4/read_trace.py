#!/usr/bin/env python3
"""Offline WDR4 decoder; no device I/O or automatic functional verdict."""
import argparse,json,struct
from pathlib import Path
MAGIC=b'WDR4'
STATS=['loop_begin','loop_end','main_point','main_point_ms','last_key','raw_count','rx_point','rx_point_ms','decoded','bad','await_drop','enqueued','queue_overflow','popped','admit_drop','route_drop','ble_state','epoch','handle','raw_drop','main_drop','heap_stop','key_done','notify_seen','conn_rejected','connected_rejected','actual_conn','expected_conn','notify_handle','host_entered','host_exited','host_type','host_point','host_point_ms','client_flags','other_input']
NAMES={1:'WorkerBeat',2:'RawHead',3:'RawSize',4:'KeyPop',5:'KeyDone',6:'ReportMapHeader',7:'ReportMapWord',8:'ReportReference',9:'ReportSelection',32:'R1Stage',10:'DiscoveryGeneration',11:'DescriptorEpoch',12:'RawGeneration',33:'CausalAction',40:'InputSubscription',41:'CccdReadback',42:'InputSubscriptionDetail',43:'InputSetupResult',127:'WindowEnd'}
NAMES.update({16+i:'RawWord'+str(i) for i in range(8)});NAMES.update({64+i:'Health_'+v for i,v in enumerate(STATS)})
def decode(data):
 rows=[];at=0;skipped=0
 while at+16<=len(data):
  if data[at:at+4]!=MAGIC:at+=1;skipped+=1;continue
  _,ms,seq,kind,aux,value=struct.unpack_from('<IIHBBI',data,at)
  rows.append(dict(sequence=seq,ms=ms,kind=kind,aux=aux,stage=NAMES.get(kind,'Unknown'),value=value));at+=16
 gaps=[{'after':a['sequence'],'next':b['sequence']} for a,b in zip(rows,rows[1:]) if (a['sequence']+1)&65535!=b['sequence']]
 health=[];sample=None;raw={};descriptor={};map_header=None;refs={};generation=0;descriptor_epoch=None;inputs={};setup=None
 for r in rows:
  k,v,a=r['kind'],r['value'],r['aux']
  if k==1:
   sample={'ms':r['ms'],'beat':v,'fields':{}};health.append(sample)
  elif 64<=k<64+len(STATS) and sample is not None:sample['fields'][STATS[k-64]]=v
  elif k==2:raw[a]={'id':a,'ms':r['ms'],'handle':v&65535,'head_bytes':v>>16,'words':{}}
  elif k==12 and a in raw:raw[a]['generation']=v
  elif k==10:generation=v;descriptor={};map_header=None;refs={};descriptor_epoch=None;inputs={};setup=None
  elif k==11:descriptor_epoch=v
  elif k==3 and a in raw:raw[a].update(total_bytes=v&65535,captured_bytes=(v>>16)&255)
  elif 16<=k<=23 and a in raw:raw[a]['words'][k-16]=v
  elif k==6:map_header={'original_bytes':v,'parsed_keyboard_fields':bool(a),'capture_limit':4096}
  elif k==7:descriptor[r['ms']]=struct.pack('<I',v)
  elif k==8:refs[a]={'handle':v&65535,'report_id':(v>>16)&255,'report_type':v>>24}
  elif k==9 and a in refs:refs[a].update(keyboard_bits=v&65535,selected_keyboard=bool(v>>16))
  elif k==40:inputs[a]={'handle':v&65535,'report_id':(v>>16)&255,'status':v>>24,'parts':[40]}
  elif k==41 and a in inputs:inputs[a].update(cccd_handle=v&65535,cccd_value=v>>16);inputs[a]['parts'].append(41)
  elif k==42 and a in inputs:inputs[a].update(read_bytes=v&255,keyboard=bool((v>>8)&1));inputs[a]['parts'].append(42)
  elif k==43:setup={'status':a,'failure_handle':v&65535,'input_count':v>>16,'generation':generation,'epoch':descriptor_epoch}
 for value in inputs.values():value['complete_in_trace']=value.pop('parts')==[40,41,42]
 reports=[]
 for item in raw.values():
  words=item.pop('words');n=item.get('captured_bytes',0);expected=(n+3)//4
  item['complete_in_trace']='captured_bytes' in item and all(i in words for i in range(expected))
  item['head_truncated']=item.get('total_bytes',item['head_bytes'])!=item['head_bytes']
  item['prefix_truncated']=n<item.get('total_bytes',item['head_bytes'])
  item['prefix_hex']=''.join(struct.pack('<I',words[i]).hex() for i in range(expected) if i in words)[:n*2];reports.append(item)
 map_complete=bool(map_header) and map_header['original_bytes']<=4096 and all(i in descriptor for i in range(0,map_header['original_bytes'],4))
 map_hex=''.join(descriptor[k].hex() for k in sorted(descriptor))[:2*min(4096,map_header['original_bytes'])] if map_header else ''
 complete_health=[h for h in health if len(h['fields'])==len(STATS)]
 summary={'records':len(rows),'skipped_bytes':skipped,'trailing_bytes':len(data)-at,'sequence_gaps':gaps,'first_sequence':rows[0]['sequence'] if rows else None,'last_sequence':rows[-1]['sequence'] if rows else None,'complete_health_samples':len(complete_health),'last_health':complete_health[-1] if complete_health else None,'raw_reports':reports,'report_references':list(refs.values()),'input_subscriptions':list(inputs.values()),'input_setup':setup,'discovery_generation':generation,'descriptor_epoch':descriptor_epoch,'report_map':dict(map_header or {},complete_in_trace=map_complete,hex=map_hex),'window_end':any(r['kind']==127 for r in rows),'functional_verdict':'NOT_ASSIGNED','limits':'Counters are sampled at separate instants. Missing events do not prove absence or root cause. Raw reports are first 128 notifications, first 32 packet bytes (across mbuf segments); original head length recorded separately.'}
 return rows,summary

def self_test():
 def p(kind,value,seq=0,aux=0,ms=10):return struct.pack('<IIHBBI',0x34524457,ms,seq,kind,aux,value)
 assert decode(b'')[1]['functional_verdict']=='NOT_ASSIGNED'
 rows,s=decode(b'junk'+p(1,1)+p(2,(8<<16)|23,1)+b'WDR');assert s['skipped_bytes']==4 and s['trailing_bytes']==3 and len(rows)==2
 assert decode(p(1,1,0)+p(1,2,2))[1]['sequence_gaps']
 raw=p(2,(8<<16)|23,0)+p(3,(8<<16)|8,1)+p(16,0x42000000,2)+p(17,0,3)
 s=decode(raw)[1];assert s['raw_reports'][0]['complete_in_trace'] and s['raw_reports'][0]['prefix_hex']=='0000004200000000'
 assert not decode(raw[:-16])[1]['raw_reports'][0]['complete_in_trace']
 assert not decode(p(6,4,0))[1]['report_map']['complete_in_trace']
 assert decode(p(6,4,0)+p(7,0x04030201,1,ms=0))[1]['report_map']['hex']=='01020304'
 print('PASS decoder resync, gaps, empty/partial data, raw grouping and map completeness')
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--decode',type=Path);ap.add_argument('--out',type=Path);ap.add_argument('--self-test',action='store_true');a=ap.parse_args()
 if a.self_test:self_test()
 elif a.decode:
  rows,summary=decode(a.decode.read_bytes())
  if a.out:
   a.out.mkdir(parents=True,exist_ok=True);(a.out/'events.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows));(a.out/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n')
   if summary['report_map']['complete_in_trace']:(a.out/'report_map.bin').write_bytes(bytes.fromhex(summary['report_map']['hex']))
  print(json.dumps(summary,indent=2))
 else:ap.error('choose --decode or --self-test')
