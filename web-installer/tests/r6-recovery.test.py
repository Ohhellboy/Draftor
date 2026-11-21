"""Exact esptool and maintenance code with in-memory SLIP serial only.
Optional private context verifies the actual recovery image without publishing it.
"""
import contextlib,hashlib,importlib.util,io,json,os,struct,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('r6',ROOT/'tools/r6_maintenance.py');r6=importlib.util.module_from_spec(spec);spec.loader.exec_module(r6)

def enc(p):return b'\xc0'+p.replace(b'\xdb',b'\xdb\xdd').replace(b'\xc0',b'\xdb\xdc')+b'\xc0'
def words(*values):return struct.pack('<'+'I'*len(values),*values)
def word(b,off=0):return struct.unpack_from('<I',b,off)[0]

class MemoryPort:
    name=port='SYNTHETIC_R6_NO_OS_PORT';baudrate=115200;timeout=3;write_timeout=10;is_open=True
    def __init__(self,flash,fail_begin=None,fail_data=None,corrupt=False):
        self.flash=bytearray(flash);self.queue=bytearray();self.registers={};self.counts={};self.fail_begin=fail_begin;self.fail_data=fail_data;self.corrupt=corrupt;self.commands=[]
    def reset_input_buffer(self):self.queue.clear()
    def flushInput(self):self.queue.clear()
    def flushOutput(self):pass
    def flush(self):pass
    def close(self):self.is_open=False
    @property
    def in_waiting(self):return len(self.queue)
    def inWaiting(self):return len(self.queue)
    def setDTR(self,*a):raise AssertionError('No signals')
    def setRTS(self,*a):raise AssertionError('No signals')
    def read(self,n=1):
        b=bytes(self.queue[:n]);del self.queue[:n];return b
    def write(self,slip):
        assert slip[0]==192 and slip[-1]==192
        b=slip[1:-1].replace(b'\xdb\xdc',b'\xc0').replace(b'\xdb\xdd',b'\xdb');d,op,n,chk=struct.unpack('<BBHI',b[:8]);p=b[8:]
        assert d==0 and len(p)==n;self.counts[op]=self.counts.get(op,0)+1;self.commands.append(op)
        data=b'';value=0;status=0;responses=1
        if op==8:value=0x20120707;responses=8
        elif op==20:data=words(0)+b'\0'*8+words(9,0)
        elif op==9:
            assert len(p)==16;addr,v,mask,delay=struct.unpack('<IIII',p);self.registers[addr]=v
            if addr==0x60002000:
                cmd=self.registers.get(0x60002020,0)&255
                self.registers[0x60002058]=0x1840ef if cmd==0x9f else 0
                self.registers[addr]=0
        elif op==10:
            value=1 if word(p)==0x60007044 else 0x0200 if word(p)==0x60007048 else self.registers.get(word(p),0)
        elif op==13:assert p==words(0,0)
        elif op==11:assert word(p,4)==16777216
        elif op==14:
            off,size=struct.unpack('<II',p);data=bytes(self.flash[off:off+size]).ljust(64,b'\0')
        elif op==19:
            off,size,z1,z2=struct.unpack('<IIII',p);assert z1==z2==0;data=hashlib.md5(self.flash[off:off+size]).hexdigest().encode()
        elif op==2:
            size,blocks,bs,off,en=struct.unpack('<IIIII',p);assert bs==1024 and en==0 and blocks==(size+1023)//1024 and 0<=off<off+size<=len(self.flash)
            if self.fail_begin==self.counts[op]:status=1
            else:self.flash[off:off+((size+4095)//4096)*4096]=b'\xff'*(((size+4095)//4096)*4096)
            self.pending=(off,blocks);self.next=0
        elif op==3:
            size,seq,z1,z2=struct.unpack('<IIII',p[:16]);assert size==1024 and seq==self.next and z1==z2==0 and len(p)==1040
            checksum=239
            for v in p[16:]:checksum^=v
            assert checksum==chk
            if self.fail_data==self.counts[op]:status=1
            else:
                off=self.pending[0]+seq*1024;self.flash[off:off+1024]=p[16:];self.next+=1
                if self.corrupt and self.next==self.pending[1]:self.flash[self.pending[0]+500]^=1
        else:raise AssertionError(f'Forbidden opcode {op}')
        for _ in range(responses):self.queue+=b'\xc0\xc0'+enc(struct.pack('<BBHI',1,op,len(data)+4,value)+data+bytes([status,8 if status else 0,255,255]))
        return len(slip)

def assets():
    if os.environ.get('DRAFTOR_R6_PRIVATE_CONTEXT'):
        return r6.load_assets(json.loads(Path(os.environ['DRAFTOR_R6_PRIVATE_CONTEXT']).read_text()))
    lock=json.loads((ROOT/'release-lock.json').read_text());f=json.loads((ROOT/'factory-lock.json').read_text())
    rows=[(lock['files'][n]['offset'],(ROOT/'public/firmware'/n).read_bytes()) for n in ['bootloader.bin','partitions.bin','boot_app0.bin','app.bin']]
    image=bytearray(b'\xff'*r6.FLASH)
    for off,b in rows:image[off:off+len(b)]=b
    backup=image[:];backup[0x9000:0xe000]=b'\xa5'*0x5000;backup[0xa10000:0xa10100]=bytes(range(256))
    return {'backup':bytes(backup),'factory':(ROOT/'release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin').read_bytes(),'draftor':rows,'initialized':image,'lock':lock,'factory_lock':f}

def setup(image,**faults):
    port=MemoryPort(image,**faults);ROM=r6.make_rom_class()
    class SyntheticROM(ROM):
        def get_usb_vid_pid(self):return (0x303a,0x1001)
    rom=SyntheticROM(port);rom.connect(mode='no-reset',attempts=1)
    from esptool.cmds import attach_flash
    attach_flash(rom);rom.flash_set_parameters(r6.FLASH)
    return rom,port

class RecoveryTests(unittest.TestCase):
    def setUp(self):self.assets=assets();self.output=io.StringIO();self.redirect=contextlib.redirect_stdout(self.output);self.redirect.__enter__()
    def tearDown(self):self.redirect.__exit__(None,None,None)
    def test_full_recovery_exact_bytes_and_independent_digest_metadata(self):
        a=self.assets;rom,p=setup(a['factory']+b'\xff'*(r6.FLASH-len(a['factory'])))
        r6.operation(rom,'restore',a);self.assertEqual(bytes(p.flash),a['backup'])
        self.assertEqual(p.counts[2],1);self.assertEqual(p.counts[3],16384)
        independent,q=setup(p.flash);r6.operation(independent,'verify-backup',a)
        self.assertNotIn(2,q.counts);self.assertNotIn(3,q.counts);self.assertGreater(independent.empty_frames,0)
    def test_factory_rom_erase_then_official_write_and_independent_verify(self):
        a=self.assets;rom,p=setup(a['backup']);r6.operation(rom,'precheck',a);r6.operation(rom,'factory',a)
        expected=a['factory']+b'\xff'*(r6.FLASH-len(a['factory']));self.assertEqual(bytes(p.flash),expected)
        self.assertEqual(p.counts[2],2);self.assertEqual(p.counts[3],4442)
        independent,q=setup(p.flash);r6.operation(independent,'verify-factory',a);self.assertNotIn(2,q.counts)
    def test_draftor_four_file_independent_official_verify(self):
        rom,p=setup(self.assets['initialized']);r6.operation(rom,'verify-draftor',self.assets);self.assertNotIn(2,p.counts)
    def test_stale_backup_stops_before_erasure(self):
        a=self.assets;b=bytearray(a['backup']);b[0x9000]^=1;rom,p=setup(b)
        with self.assertRaises(r6.MaintenanceStop):r6.operation(rom,'factory',a)
        self.assertNotIn(2,p.counts)
    def test_unarmed_write_and_erase_rejected(self):
        rom,p=setup(self.assets['backup'])
        with self.assertRaises(r6.MaintenanceStop):rom.flash_begin(r6.FLASH,0)
        self.assertNotIn(2,p.counts)
    def test_restore_error_no_second_begin_or_retry(self):
        rom,p=setup(b'\xff'*r6.FLASH,fail_data=2)
        with self.assertRaises(r6.MaintenanceStop):r6.operation(rom,'restore',self.assets)
        self.assertEqual(p.counts[2],1);self.assertEqual(p.counts[3],2)
    def test_restore_verify_mismatch_cannot_succeed(self):
        from esptool.util import FatalError
        rom,p=setup(b'\xff'*r6.FLASH,corrupt=True)
        with self.assertRaises((r6.MaintenanceStop,FatalError)):r6.operation(rom,'restore',self.assets)
        self.assertEqual(p.counts[2],1)
    def test_factory_erase_error_stops_without_image_write(self):
        rom,p=setup(self.assets['backup'],fail_begin=1)
        with self.assertRaises(r6.MaintenanceStop):r6.operation(rom,'factory',self.assets)
        self.assertEqual(p.counts[2],1);self.assertNotIn(3,p.counts)
    def test_stub_reset_erase_flash_and_arbitrary_range_are_rejected(self):
        for op in (4,5,6,7,0xd0,0xd1,0xd2):
            rom,p=setup(self.assets['backup'])
            with self.assertRaises(r6.MaintenanceStop):rom.command(op)
            self.assertNotIn(op,p.counts)

if __name__=='__main__':unittest.main()
