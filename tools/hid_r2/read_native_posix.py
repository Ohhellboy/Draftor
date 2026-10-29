#!/usr/bin/env python3
"""Read-only Linux tty attach: no DTR/RTS ioctl, no tcflush, no reset/write.
The kernel can still affect modem state on open: record USB devnum before/after
and treat disconnect, re-enumeration or ROM reset text as capture interference.
"""
from pathlib import Path
import argparse,datetime,fcntl,importlib.util,json,os,select,termios,time
import serial.tools.list_ports
ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('r1_trace',ROOT/'read_trace.py')
decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
def utc():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def usb_identity():
 items=[]
 for x in Path('/sys/bus/usb/devices').iterdir():
  try:
   if (x/'serial').read_text().strip().lower().replace(':','')=='a4cb8fcfcd60':items.append({'sysfs':str(x),'serial':(x/'serial').read_text().strip(),'busnum':(x/'busnum').read_text().strip(),'devnum':(x/'devnum').read_text().strip()})
  except (FileNotFoundError,NotADirectoryError,PermissionError):pass
 return items
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--seconds',type=int,default=30);a=ap.parse_args()
 assert 1<=a.seconds<=600
 matches=[p for p in serial.tools.list_ports.comports() if p.vid==0x303a and p.pid==0x1001 and (p.serial_number or '').lower().replace(':','')=='a4cb8fcfcd60']
 assert len(matches)==1 and 'JTAG' in (matches[0].product or '')
 a.out.mkdir(exist_ok=False);port=matches[0].device;before=usb_identity();assert len(before)==1
 status={'started_utc':utc(),'port':port,'pid':os.getpid(),'before_usb':before,'reader':'POSIX_RDONLY_NO_MODEM_IOCTL_NO_FLUSH','capture_attempted':True}
 (a.out/'OPEN.json').write_text(json.dumps(status,indent=2)+'\n')
 fd=None;error=None;last_print=-1;started=time.monotonic()
 try:
  fd=os.open(port,os.O_RDONLY|os.O_NONBLOCK|os.O_NOCTTY)
  fcntl.ioctl(fd,termios.TIOCEXCL)
  # Preserve speed; set a raw receiver. No TCIFLUSH/TCSAFLUSH and no modem ioctl.
  cfg=termios.tcgetattr(fd);cfg[0]=0;cfg[1]=0;cfg[3]=0
  cfg[2]&=~(termios.CSIZE|termios.PARENB|termios.CSTOPB|termios.HUPCL|getattr(termios,'CRTSCTS',0))
  cfg[2]|=termios.CS8|termios.CLOCAL|termios.CREAD
  cfg[6][termios.VMIN]=0;cfg[6][termios.VTIME]=0
  termios.tcsetattr(fd,termios.TCSANOW,cfg)
  print('POSIX_CAPTURE_OPEN_NO_MODEM_IOCTL',flush=True)
  with (a.out/'raw.bin').open('xb') as log:
   while time.monotonic()-started<a.seconds:
    if usb_identity()!=before:raise RuntimeError('USB identity/devnum changed: stop, do not mix reset evidence')
    ready,_,_=select.select([fd],[],[],.25)
    if ready:
     try:b=os.read(fd,4096)
     except BlockingIOError:continue
     if not b:raise RuntimeError('EOF/disconnect on tty; stop without reconnect')
     log.write(b);log.flush()
    tick=int(time.monotonic()-started)
    if tick>=last_print+5:
     last_print=tick;rows,summary=decoder.decode((a.out/'raw.bin').read_bytes())
     print(json.dumps({'seconds':tick,'bytes':log.tell(),'records':len(rows),'last_stages':[r['stage'] for r in rows[-10:]]}),flush=True)
 except KeyboardInterrupt:error='operator stopped capture'
 except (OSError,RuntimeError) as e:error=str(e)
 finally:
  if fd is not None:os.close(fd)
 raw=(a.out/'raw.bin').read_bytes() if (a.out/'raw.bin').exists() else b''
 rows,summary=decoder.decode(raw)
 summary.update(status);summary.update(ended_utc=utc(),after_usb=usb_identity(),reader_error=error,rom_reset_text=b'ESP-ROM:' in raw or b'USB_UART_CHIP_RESET' in raw,device_functional_verdict='NOT_ASSIGNED')
 (a.out/'events.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows))
 (a.out/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2),flush=True)
if __name__=='__main__':main()
