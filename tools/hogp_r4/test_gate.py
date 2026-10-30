from pathlib import Path
import importlib.util,struct,json
p=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('g',p/'check_baseline.py');g=importlib.util.module_from_spec(sp);sp.loader.exec_module(g)
inspector=p.parents[1]/'TestBuilds/hogp-r4/inspect_hid'
desc=bytes.fromhex('05010906a101050719e029e7750195088102750895018101190029ff750895068100c0')
def fixture(case='good'):
 packets=[]
 def emit(k,v,ms=10,aux=0):packets.append([0x34524457,ms,len(packets),k,aux,v])
 emit(10,1);emit(11,99 if case=='stale_epoch' else 13);emit(6,len(desc),aux=1)
 for i in range(0,len(desc),4):
  if case=='missing_map' and i==4:continue
  emit(7,int.from_bytes(desc[i:i+4].ljust(4,b'\0'),'little'),ms=i)
 emit(8,42|(1<<24));emit(9,64|(1<<16))
 if case!='missing_subscription':
  emit(40,42|((2 if case=='write_only' else 3)<<24));emit(41,44|((0 if case=='cccd_disabled' else 1)<<16))
  if case!='partial_subscription':emit(42,2|(1<<8))
 if case!='missing_setup':emit(43,1<<16,aux=11 if case=='setup_failed' else 1)

 def health(ms,beat):
  emit(1,beat,ms)
  for i,n in enumerate(g.trace.STATS):
   value={'loop_begin':ms,'loop_end':ms-1,'raw_count':8,'decoded':8,'enqueued':10,'popped':10,'key_done':10,'ble_state':261,'epoch':13}.get(n,0)
   if case=='drop' and n=='main_drop':value=1
   emit(64+i,value,ms)
 health(100,1)
 states=[bytes([0,0,4,0,0,0,0,0]),bytes(8),bytes([0,0,24,0,0,0,0,0]),bytes(8),bytes([0,0,41,0,0,0,0,0]),bytes(8),bytes([3,0,0,0,0,0,0,0]),bytes(8)]
 if case=='missing_keys':states=[bytes(8)]*8
 for i,b in enumerate(states):
  emit(2,42|(8<<16),200+i,i);emit(3,(40 if case=='truncated' else 8)|(8<<16),200+i,i);emit(12,1,200+i,i)
  for w in range(2):emit(16+w,int.from_bytes(b[w*4:w*4+4],'little'),200+i,i)
 health(90 if case=='old_health' else 1000,2)
 if case=='heartbeat_only':packets=[x for x in packets if x[3]==1 or x[3]>=64]
 for i,x in enumerate(packets):x[2]=i
 if case=='gap':packets[-2][2]+=1
 return b''.join(struct.pack('<IIHBBI',*x) for x in packets)
results=[]
for case in ['good','missing_map','stale_epoch','drop','missing_keys','truncated','old_health','heartbeat_only','gap','missing_subscription','write_only','cccd_disabled','partial_subscription','missing_setup','setup_failed']:
 r=g.check(fixture(case),inspector);expected=case=='good';assert (r['status']=='BASELINE_READY')==expected,(case,r['reasons']);results.append({'case':case,'status':r['status'],'reasons':r['reasons']});print(case,r['status'])
(p.parents[1]/'evidence/hogp-r4/BASELINE_GATE_TEST.json').write_text(json.dumps(results,indent=2)+'\n')
