# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Ruotao Wang
"""Controlled installer orchestration; synthetic images, no devices/network."""
import argparse, hashlib, json, struct, tempfile, unittest, zlib
from pathlib import Path
from install_firmware import BOARD, FILES, EXPECTED, install

def ota(seq=1):
    b=bytearray(b'\xff'*8192)
    struct.pack_into('<I',b,0,seq)
    struct.pack_into('<I',b,28,zlib.crc32(b[:4],0xffffffff))
    return bytes(b)

class Fixture:
    def __init__(self, root):
        self.root=root;self.package=root/'Firmware';self.package.mkdir()
        table=bytearray(b'\xff'*3072)
        for i,(name,kind,sub,offset,size) in enumerate(EXPECTED):
            struct.pack_into('<HBBII16sI',table,i*32,0x50aa,kind,sub,offset,size,name.encode(),0)
        image=bytearray(256);image[0]=0xe9;struct.pack_into('<H',image,12,9)
        image[48:48+len(b'SYNTHETIC_WS_ALPHA2_2')]=b'SYNTHETIC_WS_ALPHA2_2'
        data={'app.bin':image,'bootloader.bin':image,'partitions.bin':table,'boot_app0.bin':ota()}
        manifest={'board':BOARD,'chip':'esp32s3','flash_bytes':0x1000000,'firmware_label':'SYNTHETIC_WS_ALPHA2_2','files':{}}
        for name,payload in data.items():
            (self.package/name).write_bytes(payload)
            manifest['files'][name]={'offset':FILES[name],'bytes':len(payload),'sha256':hashlib.sha256(payload).hexdigest()}
        (self.package/'FIRMWARE_MANIFEST.json').write_text(json.dumps(manifest))
        self.calls=[];self.fail=None;self.device_mac='00:11:22:33:44:55';self.partition=bytes(table)+b'\xff'*1024;self.ota=ota();self.secure=False

    def args(self,**kw):
        values=dict(firmware=self.package,board=BOARD,port='/dev/synthetic-port',expected_mac='00:11:22:33:44:55',mode='update',output=self.root/'result',current_app=None,execute=False,backup_confirmed=False,allow_erase=False)
        values.update(kw);return argparse.Namespace(**values)

    def runner(self,args,output):
        fixture=self
        class Mock:
            def run(self,label,action):
                fixture.calls.append(action)
                if label==fixture.fail:raise ValueError(label+' injected failure')
                if action[0]=='version':return 'esptool v5.3.1\n5.3.1'
                if action[0]=='read-mac':return 'Connected to ESP32-S3\nMAC: '+fixture.device_mac+'\n'
                if action[0]=='flash-id':return 'Detected flash size: 16MB'
                if action[0]=='get-security-info':return 'Secure Boot: '+('Enabled' if fixture.secure else 'Disabled')+'\nFlash Encryption: Disabled'
                if action[0]=='read-flash':Path(action[3]).write_bytes(fixture.partition if action[1]=='0x8000' else fixture.ota)
                return 'PASS (controlled fixture only)'
        return Mock()

class InstallerTests(unittest.TestCase):
    def setUp(self):self.temp=tempfile.TemporaryDirectory();self.f=Fixture(Path(self.temp.name))
    def tearDown(self):self.temp.cleanup()
    def run_install(self,**kw):return install(self.f.args(**kw),self.f.runner)
    def no_write(self):self.assertFalse(any(x[0] in ('write-flash','erase-flash') for x in self.f.calls))
    def test_plan_is_readonly(self):
        self.assertEqual(self.run_install()['status'],'PLAN_READY');self.no_write()
    def test_actual_app1_not_assumed_app0(self):
        self.f.ota=ota(2);r=self.run_install(execute=True,backup_confirmed=True)
        self.assertEqual(r['app_offset'],'0x410000');self.assertEqual(r['status'],'VERIFIED_BOOT_PENDING')
        writes=[c for c in self.f.calls if c[0]=='write-flash'];self.assertEqual(len(writes),1);self.assertEqual(writes[0][1],'0x410000');self.assertEqual(len(writes[0]),3)
        self.assertFalse(any(c[0]=='erase-flash' for c in self.f.calls));self.assertEqual(self.f.calls[-1][0],'verify-flash')
    def test_identity_mismatch(self):
        self.f.device_mac='00:11:22:33:44:66'
        with self.assertRaisesRegex(ValueError,'identity mismatch'):self.run_install(execute=True,backup_confirmed=True)
        self.no_write()
    def test_board_mismatch_before_port(self):
        with self.assertRaisesRegex(ValueError,'board'):self.run_install(board='ESP32S3_OSPTEK_ST7306')
        self.assertEqual(self.f.calls,[])
    def test_bad_image_before_port(self):
        (self.f.package/'app.bin').write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError,'identity mismatch'):self.run_install()
        self.assertEqual(self.f.calls,[])
    def test_partition_mismatch(self):
        data=bytearray(self.f.partition);struct.pack_into('<I',data,2*32+8,0x200000);self.f.partition=bytes(data)
        with self.assertRaises(ValueError):self.run_install(execute=True,backup_confirmed=True)
        self.no_write()
    def test_ambiguous_or_pending_ota(self):
        self.f.ota=b'\xff'*8192
        with self.assertRaisesRegex(ValueError,'OTA'):self.run_install(execute=True,backup_confirmed=True)
        self.no_write()
    def test_security_refusal(self):
        self.f.secure=True
        with self.assertRaisesRegex(ValueError,'security'):self.run_install(execute=True,backup_confirmed=True)
        self.no_write()
    def test_write_failure_stops_without_verify_retry(self):
        self.f.fail='08-write'
        with self.assertRaisesRegex(ValueError,'injected'):self.run_install(execute=True,backup_confirmed=True)
        self.assertEqual(self.f.calls[-1][0],'write-flash');self.assertEqual(sum(c[0]=='write-flash' for c in self.f.calls),1)
        self.assertEqual(json.loads((self.f.root/'result/RESULT.json').read_text())['status'],'STOP')
    def test_verify_failure_stops(self):
        self.f.fail='09-independent-verify'
        with self.assertRaisesRegex(ValueError,'injected'):self.run_install(execute=True,backup_confirmed=True)
        self.assertEqual(self.f.calls[-1][0],'verify-flash');self.assertEqual(sum(c[0]=='write-flash' for c in self.f.calls),1)
    def test_fresh_requires_explicit_ack(self):
        with self.assertRaisesRegex(ValueError,'erase'):self.run_install(mode='fresh',execute=True,backup_confirmed=True)
        self.assertEqual(self.f.calls,[])
    def test_fresh_four_files_and_independent_verify(self):
        r=self.run_install(mode='fresh',execute=True,backup_confirmed=True,allow_erase=True)
        self.assertTrue(r['erased'] and r['independently_verified']);self.assertEqual(self.f.calls[-3][0],'erase-flash')
        self.assertEqual(self.f.calls[-2][1::2],['0x0','0x8000','0xe000','0x10000']);self.assertEqual(self.f.calls[-1][1:],self.f.calls[-2][1:])

if __name__=='__main__':unittest.main()
