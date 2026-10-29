#!/usr/bin/env python3
"""Finite R1 USB Serial/JTAG reader. No commands, reset, flashing or TF access.
Capture only AFTER separate device-test approval. --decode and --self-test are offline.
"""
import argparse,json,struct,time
from pathlib import Path
WIRE=struct.Struct('<IIHBBI')
MAGIC=b'WDR1'
NAMES=['F9Pop','AdmissionDrop','RouteDrop','UsbKey','IntegrityBegin','IntegrityPass',
 'IntegrityFail','AllocateBegin','AllocateDenied','AllocateReady','DispatchBegin',
 'DispatchEnd','TickBegin','TickEnd','PaintBegin','PaintEnd','PresentBegin',
 'PresentEnd','RoutePublished','Heartbeat','TransportEnd','AllocationException',
 'OperationBegin','OperationTotalUs','VfsReadUs','VfsWriteUs','VfsSyncUs',
 'VfsOtherUs','VerifyUs','InstallUs','LayoutUs','DrawUs','VfsReadCalls',
 'VfsWriteCalls','VfsSyncCalls','LcdUs','LcdResult']
def decode(data):
 records=[];offset=0;skipped=0;gaps=[];last=None
 while offset+WIRE.size<=len(data):
  if data[offset:offset+4]!=MAGIC:offset+=1;skipped+=1;continue
  magic,us,sequence,stage,lost,value=WIRE.unpack_from(data,offset)
  if not 1<=stage<=len(NAMES):offset+=1;skipped+=1;continue
  if last is not None and sequence!=last+1:gaps.append({'before':last,'after':sequence})
  row=dict(sequence=sequence,us_mod32=us,stage=NAMES[stage-1],lost_total=lost,value=value)
  if row['stage']=='OperationBegin':row['operation_prefix']=value.to_bytes(4,'little').rstrip(b'\0').decode('ascii',errors='replace')
  records.append(row);last=sequence;offset+=WIRE.size
 summary=dict(records=len(records),skipped_bytes=skipped,trailing_bytes=len(data)-offset,sequence_gaps=gaps,
  dropped_reported=max((r['lost_total'] for r in records),default=0),
  integrity_failure_marker=any(r['stage']=='IntegrityFail' for r in records),
  route_published=any(r['stage']=='RoutePublished' for r in records),
  transport_end=any(r['stage']=='TransportEnd' for r in records),
  conclusion='Evidence only: missing markers do not prove where execution stopped.')
 return records,summary
def self_test():
 data=WIRE.pack(0x31524457,10,0,1,0,0)+WIRE.pack(0x31524457,20,1,7,0,0)
 rows,s=decode(b'noise'+data+b'WD');assert len(rows)==2 and s['integrity_failure_marker'] and s['skipped_bytes']==5 and s['trailing_bytes']==2
 _,s=decode(data[:16]+WIRE.pack(0x31524457,30,3,20,1,1));assert s['sequence_gaps'] and s['dropped_reported']==1
 _,s=decode(b'');assert s['records']==0 and not s['integrity_failure_marker']
 print('PASS trace decoder resynchronization, partial tail, loss/gap and empty/inconclusive cases')
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 mode=ap.add_mutually_exclusive_group(required=True);mode.add_argument('--port');mode.add_argument('--decode',type=Path);mode.add_argument('--self-test',action='store_true')
 ap.add_argument('--out',type=Path);ap.add_argument('--seconds',type=int,default=300)
 a=ap.parse_args()
 if a.self_test:self_test();return
 if a.decode:
  rows,summary=decode(a.decode.read_bytes());print(json.dumps(dict(summary=summary,records=rows),ensure_ascii=False,indent=2));return
 if a.out is None or not 1<=a.seconds<=600:ap.error('--port needs a NEW --out directory and --seconds 1..600')
 import serial
 a.out.mkdir(parents=True,exist_ok=False)
 # Explicit deassertion is set before open; no intentional DTR/RTS pulses,
 # reset command, serial write, break, flush/reset-input or auto-reconnect.
 s=serial.Serial(port=None,baudrate=115200,timeout=.25,exclusive=True)
 s.dtr=False;s.rts=False;s.port=a.port
 failure=None;opened=False;started=time.monotonic()
 try:
  s.open();opened=True
  print('CAPTURE_READY: perform the continuous guide; no bytes will be sent.',flush=True)
  with (a.out/'raw.bin').open('xb') as f:
   while time.monotonic()-started<a.seconds:
    b=s.read(4096)
    if b:f.write(b);f.flush()
 except (OSError,serial.SerialException) as e:failure=str(e)
 except KeyboardInterrupt:failure='operator stopped capture'
 finally:
  if s.is_open:s.close()
 raw=a.out/'raw.bin';rows,summary=decode(raw.read_bytes() if raw.exists() else b'')
 summary.update(port=a.port,elapsed_seconds=round(time.monotonic()-started,2),reader_error=failure,capture_attempted=True,serial_opened=opened,device_functional_verdict="NOT_ASSIGNED")
 (a.out/'events.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rows))
 (a.out/'SUMMARY.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(summary,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
