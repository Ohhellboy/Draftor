#!/usr/bin/env python3
"""Bounded offline Input Report inspection; never emits OS keys or device I/O.
Numeric page/usage descriptions only. Vendor meanings are deliberately unknown.
"""
from pathlib import Path
import argparse,json

def parse(data):
 if len(data)>4096:raise ValueError('map too large')
 g={'page':0,'size':0,'count':0,'id':0,'min':0,'max':0};stack=[];local={};reports={};at=0
 def usage(raw):return ((raw>>16)&65535,raw&65535) if raw>65535 else (g['page'],raw)
 while at<len(data):
  prefix=data[at];at+=1
  if prefix==254:raise ValueError('long items unsupported')
  n=prefix&3;n=4 if n==3 else n
  if at+n>len(data):raise ValueError('truncated item')
  b=data[at:at+n];at+=n;v=int.from_bytes(b,'little');kind=(prefix>>2)&3;tag=prefix>>4
  if kind==1:
   if tag in {0,7,8,9}:g[{0:'page',7:'size',8:'id',9:'count'}[tag]]=v
   elif tag==1:g['min']=int.from_bytes(b,'little',signed=True)
   elif tag==2:g['max']=int.from_bytes(b,'little',signed=g['min']<0)
   elif tag==10:
    if len(stack)==4:raise ValueError('global stack limit')
    stack.append(dict(g))
   elif tag==11:
    if not stack:raise ValueError('global stack underflow')
    g=stack.pop()
   if not 0<=g['id']<=255:raise ValueError('report id')
  elif kind==2:
   if tag==0:local.setdefault('usages',[]).append(usage(v))
   elif tag==1:local['lo']=usage(v)
   elif tag==2:local['hi']=usage(v)
  elif kind==0:
   if tag==8:
    bits=g['size']*g['count'];r=reports.setdefault(g['id'],{'bits':0,'fields':[]})
    if g['size']>32 or g['count']>256 or bits+r['bits']>4096:raise ValueError('input bounds')
    r['fields'].append(dict(g,offset=r['bits'],constant=bool(v&1),variable=bool(v&2),null_state=bool(v&64),local=dict(local)));r['bits']+=bits
   local={}
 if stack:raise ValueError('unbalanced global stack')
 for r in reports.values():r['bytes']=(r['bits']+7)//8;r['pages']=sorted({f['page'] for f in r['fields'] if not f['constant']})
 return reports

def decode(reports,report_id,payload):
 r=reports[report_id]
 if len(payload)!=r['bytes']:raise ValueError('payload length mismatch')
 v=int.from_bytes(payload,'little');active=[]
 for f in r['fields']:
  if f['constant']:continue
  usages=f['local'].get('usages',[]);lo=f['local'].get('lo');hi=f['local'].get('hi')
  def resolve(index):
   if 0<=index<len(usages):return usages[index]
   if lo and hi and lo[0]==hi[0] and 0<=index<=hi[1]-lo[1]:return (lo[0],lo[1]+index)
   return (f['page'],None)
  for i in range(f['count']):
   value=(v>>(f['offset']+i*f['size']))&((1<<f['size'])-1)
   if f['min']<0 and f['size'] and value&(1<<(f['size']-1)):value-=1<<f['size']
   if value==0:continue
   page,u=resolve(i if f['variable'] else value-f['min'])
   active.append({'page':f'0x{page:04X}','usage':None if u is None else f'0x{u:04X}','value':value,'offset':f['offset']+i*f['size']})
 return {'report_id':report_id,'payload_hex':payload.hex(),'active_fields':active,'contains_standard_keyboard_F9':any(v['page']=='0x0007' and v['usage']=='0x0042' for v in active),'vendor_semantics':'not inferred'}

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--map',type=Path,required=True);ap.add_argument('--report-id',type=int);ap.add_argument('--hex');a=ap.parse_args();r=parse(a.map.read_bytes())
 print(json.dumps(decode(r,a.report_id,bytes.fromhex(a.hex)) if a.report_id is not None else r,indent=2))
