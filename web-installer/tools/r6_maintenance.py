#!/usr/bin/env python3
"""R6 local maintenance, exact esptool 5.3.1, no stub/reset/retry.

Requires private context and explicit final authorization for mutations. No MAC,
backup bytes or backup digests are emitted to stdout. Raw evidence stays private.
This is maintenance/recovery, never evidence of a browser first installation.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse, hashlib, json, struct, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH = 16777216

class MaintenanceStop(BaseException):
    pass

def require(ok, message):
    if not ok:
        raise MaintenanceStop(message)

def digest(data, algorithm='md5'):
    return hashlib.new(algorithm, data).hexdigest()

def load_assets(context):
    lock = json.loads((ROOT/'release-lock.json').read_text())
    factory_lock = json.loads((ROOT/'factory-lock.json').read_text())
    factory = Path(context['factory_image']).read_bytes()
    require(len(factory) == factory_lock['bytes'] and digest(factory,'sha256') == factory_lock['sha256'], 'Factory asset mismatch')
    require(digest(b'blob '+str(len(factory)).encode()+b'\0'+factory,'sha1') == factory_lock['git_blob_sha1'], 'Factory Git blob mismatch')
    receipt = json.loads(Path(context['backup_receipt']).read_text())
    backup = Path(context['backup_image']).read_bytes()
    require(len(backup) == FLASH and digest(backup,'sha256') == receipt['files']['full_flash.bin']['sha256'], 'Backup SHA-256 mismatch')
    require(digest(backup) == receipt['files']['full_flash.bin']['md5'] and receipt['rom_md5_match'] is True, 'Backup ROM receipt mismatch')
    require(receipt['device_mac'].lower() == context['expected_mac'].lower(), 'Backup identity mismatch')
    files=[]
    for name in ['bootloader.bin','partitions.bin','boot_app0.bin','app.bin']:
        pin=lock['files'][name]; data=(ROOT/'public/firmware'/name).read_bytes()
        require(len(data)==pin['bytes'] and digest(data,'sha256')==pin['sha256'],'Draftor asset mismatch')
        files.append((pin['offset'],data))
    initialized=bytearray(b'\xff'*FLASH)
    for off,data in files: initialized[off:off+len(data)]=data
    require(digest(initialized)==factory_lock['draftorInitializedFullMd5'],'Initialized image lock mismatch')
    return {'factory':factory,'backup':backup,'draftor':files,'initialized':initialized,'lock':lock,'factory_lock':factory_lock}

def make_rom_class():
    import esptool
    from esptool.targets.esp32s3 import ESP32S3ROM
    require(esptool.__version__=='5.3.1','Requires exact esptool 5.3.1')
    class MaintenanceROM(ESP32S3ROM):
        WRITE_FLASH_ATTEMPTS=1
        def __init__(self, port, trace=None):
            self.audit=trace; self.counts={}; self.syncs=0; self.expected_op=None
            self.allowed_begin=[]; self.pending=None; self.failed=False; self.empty_frames=0
            super().__init__(port, baud=115200)
        def record(self,d,packet):
            if self.audit:
                self.audit.write(struct.pack('<dBH',time.monotonic(),d,len(packet))+packet)
        def arm(self,steps):
            require(not self.allowed_begin and (self.pending is None or self.pending['remaining']==0),'Unfinished operation')
            self.allowed_begin=list(steps)
        def write(self,packet):
            require(not self.failed and len(packet)>=8,'Terminal or short command')
            direction,op,n,checksum=struct.unpack('<BBHI',packet[:8]);data=packet[8:]
            require(direction==0 and n==len(data) and op in {2,3,8,9,10,11,13,14,15,19,20},'Forbidden command')
            if op==8:
                self.syncs+=1; require(self.syncs==1,'No SYNC retry')
            if op==9:
                regs={0x600080b0,0x60008098,0x600080b8,0x600080b4,0x60002000,0x60002004,0x60002018,0x6000201c,0x60002020,0x60002028,0x60002058}
                require(len(data)==16 and struct.unpack_from('<I',data)[0] in regs,'Volatile-register allowlist')
            if op==2:
                require(self.allowed_begin and len(data)==20,'Unarmed Flash begin')
                size,blocks,block_size,offset,encrypt=struct.unpack('<IIIII',data)
                step=self.allowed_begin.pop(0)
                require((offset,size)==(step['offset'],step['size']) and block_size==1024 and blocks==(size+1023)//1024 and encrypt==0,'Flash begin differs from frozen plan')
                self.pending={'data':step.get('data'),'remaining':blocks if step.get('data') is not None else 0,'sequence':0,'offset':offset}
            if op==3:
                require(self.pending and self.pending['remaining']>0 and len(data)==1040,'Unarmed Flash data')
                size,seq,z1,z2=struct.unpack('<IIII',data[:16]);p=self.pending
                require((size,seq,z1,z2)==(1024,p['sequence'],0,0),'Flash sequence mismatch')
                expected=p['data'][seq*1024:(seq+1)*1024].ljust(1024,b'\xff')
                require(data[16:]==expected and checksum==self.checksum(expected),'Flash data differs from authorized image')
                p['remaining']-=1;p['sequence']+=1
            if op==14:
                off,size=struct.unpack('<II',data)
                require(size==64 and (0x8000<=off<0x9000 or 0xe000<=off<0x10000),'Metadata-only slow read')
            if op==19:
                off,size,a,b=struct.unpack('<IIII',data)
                require(0<=off and 0<size and off+size<=FLASH and a==b==0,'MD5 range')
            self.expected_op=op;self.counts[hex(op)]=self.counts.get(hex(op),0)+1
            self.record(0,packet)
            return super().write(packet)
        def read(self):
            # Inherited R3 framing/status rules. Official esptool consumes empty
            # SLIP delimiters; the two reserved status bytes are not error flags.
            p=super().read();self.record(1,p)
            if not p:
                self.empty_frames+=1;return p
            require(len(p)>=12,'Short ROM response')
            d,op,n,v=struct.unpack('<BBHI',p[:8])
            require(d==1 and op==self.expected_op and n==len(p)-8,'ROM framing/opcode mismatch')
            require(n=={14:68,19:36,20:24}.get(op,4),'ROM length mismatch')
            require(p[-4]==0,'ROM error status')
            if op==8:require(v!=0,'Stub forbidden')
            return p
        def command(self,*args,**kwargs):
            try:return super().command(*args,**kwargs)
            except BaseException as error:
                self.failed=True
                if isinstance(error,MaintenanceStop):raise
                raise MaintenanceStop('Command failed; no retry') from error
        def run_spiflash_command(self, command, data=b'', read_bits=0, **kwargs):
            # Same attach/read-ID allowance as R3; no WREN/program/erase/status write.
            valid=(command==0x9f and read_bits==24 and not kwargs) or (command==0x5a and read_bits in (8,32) and kwargs.get('addr') in (0,16) and kwargs.get('addr_len')==24 and kwargs.get('dummy_len')==8 and set(kwargs)=={'addr','addr_len','dummy_len'}) or (command in (0x66,0x99) and read_bits==0 and not kwargs)
            require(valid and data==b'','Forbidden SPI opcode')
            return super().run_spiflash_command(command,data,read_bits,**kwargs)
    return MaintenanceROM

def identity(esp,expected_mac):
    s=esp.get_security_info(cache=False)
    require(s['chip_id']==9 and s['flags'] & ~0x7ff == 0 and s['flags'] & 5 == 0 and s['flash_crypt_cnt']<=7 and s['flash_crypt_cnt'].bit_count()%2==0,'Unsafe security/chip')
    mac=':'.join(f'{b:02x}' for b in esp.read_mac())
    require(mac==expected_mac.lower(),'Wrong private identity')
    from esptool.cmds import detect_flash_size
    require(detect_flash_size(esp)=='16MB','Not 16 MiB')
    return {'same_expected_identity':True,'chip':'ESP32-S3','flash_bytes':FLASH,'security_known_disabled':True}

def verify_metadata(esp,image):
    for off,size in [(0x8000,4096),(0xe000,8192)]:
        b=esp.read_flash(off,size)
        require(len(b)==size and b==image[off:off+size],'Metadata readback mismatch')
        require(esp.flash_md5sum(off,size)==digest(b),'Metadata read/ROM digest mismatch')

def operation(esp,stage,assets):
    from esptool.cmds import erase_region,write_flash,verify_flash
    if stage=='precheck':
        require(esp.flash_md5sum(0,FLASH)==digest(assets['backup']),'CURRENT_FLASH_DIFFERS_FROM_RESTORE_BACKUP')
        verify_metadata(esp,assets['backup'])
    elif stage=='factory':
        # Preserve original state if B0 has changed between sessions.
        require(esp.flash_md5sum(0,FLASH)==digest(assets['backup']),'CURRENT_FLASH_DIFFERS_FROM_RESTORE_BACKUP')
        image=assets['factory']
        esp.arm([{'offset':0,'size':FLASH}]);erase_region(esp,0,FLASH)
        require(esp.flash_md5sum(0,FLASH)==digest(b'\xff'*FLASH),'Erase verification failed')
        esp.arm([{'offset':0,'size':len(image),'data':image}])
        write_flash(esp,[(0,image)],flash_freq='keep',flash_mode='keep',flash_size='keep',compress=False,no_compress=True,no_progress=True,erase_all=False,force=False)
        require(esp.flash_md5sum(0,FLASH)==digest(image+b'\xff'*(FLASH-len(image))),'Factory full Flash mismatch')
    elif stage=='restore':
        image=assets['backup'];esp.arm([{'offset':0,'size':FLASH,'data':image}])
        write_flash(esp,[(0,image)],flash_freq='keep',flash_mode='keep',flash_size='keep',compress=False,no_compress=True,no_progress=True,erase_all=False,force=False)
        require(esp.flash_md5sum(0,FLASH)==digest(image),'Restore full Flash mismatch')
        verify_metadata(esp,image)
    elif stage in ('verify-factory','verify-backup','verify-draftor'):
        if stage=='verify-factory':image=assets['factory'];rows=[(0,image)];whole=image+b'\xff'*(FLASH-len(image))
        elif stage=='verify-backup':whole=assets['backup'];rows=[(0,whole)]
        else:whole=assets['initialized'];rows=assets['draftor']
        verify_flash(esp,rows,flash_freq='keep',flash_mode='keep',flash_size='keep')
        require(esp.flash_md5sum(0,FLASH)==digest(whole),'Independent full Flash mismatch')
        verify_metadata(esp,whole)
    else:raise MaintenanceStop('Unknown stage')
    require(not esp.allowed_begin and (esp.pending is None or esp.pending['remaining']==0),'Incomplete write sequence')

def main():
    parser=argparse.ArgumentParser();parser.add_argument('stage',choices=['precheck','factory','verify-factory','verify-draftor','restore','verify-backup'])
    parser.add_argument('--context',type=Path,required=True);parser.add_argument('--esptool-site',type=Path,required=True)
    args=parser.parse_args();sys.path.insert(0,str(args.esptool_site.resolve()))
    context=json.loads(args.context.read_text());assets=load_assets(context)
    evidence=Path(context['evidence']);stage_dir=evidence/args.stage
    # An existing stage means this authorized attempt has already been consumed.
    require(not stage_dir.exists(),'Stage already attempted; do not retry automatically')
    if args.stage in ('factory','restore'):
        approval=json.loads((evidence/'FINAL_DESTRUCTIVE_APPROVAL.private.json').read_text())
        require(approval.get('user_confirmed') is True and approval.get('scope')==['factory-once','browser-first-install-once','restore-backup-once'],'Final destructive approval missing')
        if args.stage=='factory':
            require(json.loads((evidence/'precheck/result.json').read_text())['status']=='PASS','Precheck not passed')
    import serial.tools.list_ports,subprocess
    ports=[p for p in serial.tools.list_ports.comports() if p.vid==0x303a and p.pid==0x1001 and '/cu.' in p.device]
    require(len(ports)==1 and ports[0].device==context['port'],'Unique USB Serial/JTAG device not found')
    owners=subprocess.run(['lsof','-t',context['port'],context['port'].replace('/cu.','/tty.')],capture_output=True,text=True)
    require(not owners.stdout.strip(),'Serial port occupied')
    stage_dir.mkdir(mode=0o700,parents=False)
    rom=None;result={'status':'STOP','stage':args.stage,'esptool':'5.3.1','stub':False,'reset':False,'retry':False}
    from contextlib import redirect_stdout,redirect_stderr
    with (stage_dir/'wire.private.bin').open('xb') as trace,(stage_dir/'console.private.log').open('x') as console,redirect_stdout(console),redirect_stderr(console):
        try:
            ROM=make_rom_class();rom=ROM(context['port'],trace)
            rom.connect(mode='no-reset',attempts=1)
            from esptool.cmds import attach_flash
            attach_flash(rom);rom.flash_set_parameters(FLASH)
            result.update(identity(rom,context['expected_mac']))
            operation(rom,args.stage,assets)
            result.update(identity(rom,context['expected_mac']));result['status']='PASS'
        except BaseException as error:
            result['error']=str(error);import traceback;traceback.print_exc()
        finally:
            if rom:
                result.update(op_counts=rom.counts,empty_slip_frames=rom.empty_frames,sync_count=rom.syncs)
                rom._port.close();result['serial_closed']=not rom._port.is_open
    (stage_dir/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result));return 0 if result['status']=='PASS' else 1

if __name__=='__main__':sys.exit(main())
