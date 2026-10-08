#!/usr/bin/env python3
# Offline checks only. Does not call logger main, open a device or invoke esptool flashing.
from pathlib import Path
import sys, tempfile, json, hashlib, codecs
p=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(p/'tools'))
import flash, serial_logger
real=flash.verify(); assert len([r for r in real['files'] if r.get('default_flash')])==4
with tempfile.TemporaryDirectory(prefix='HANIME tools with spaces ') as folder:
 root=Path(folder);(root/'dist').mkdir();raw=b'offline image';(root/'dist'/'firmware with spaces.bin').write_bytes(raw)
 manifest={'firmware':flash.VERSION,'files':[{'file':'firmware with spaces.bin','size':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'default_flash':True,'address':'0x10000'}]}
 (root/'dist/FLASH_MANIFEST.json').write_text(json.dumps(manifest));old=flash.ROOT;flash.ROOT=root
 try:
  m=flash.verify();cmd=flash.command('/tool path/esptool','/dev/cu.EXPLICIT_TEST_ONLY',m)
  assert cmd[-1]==str(root/'dist/firmware with spaces.bin') and cmd[-2]=='0x10000'
  assert flash.choose_port('/dev/cu.EXPLICIT_TEST_ONLY')=='/dev/cu.EXPLICIT_TEST_ONLY'
  (root/'dist/firmware with spaces.bin').write_bytes(b'damaged')
  try:flash.verify();raise AssertionError('corruption accepted')
  except ValueError:pass
 finally:flash.ROOT=old
pending=False;got='';decoder=codecs.getincrementaldecoder('utf-8')('replace')
for chunk in [b'A\r',b'\nB\rC\n\xc2',b'\xa3\r',b'\n']:
 text,pending=serial_logger.normalize_newlines(decoder.decode(chunk),pending);got+=text
assert got=='A\nB\nC\n£\n' and not pending and serial_logger.BAUD==115200
print('PASS artifact hashes, damage refusal, paths with spaces, explicit port, 115200, split UTF-8/CRLF/CR/LF; no serial opened')
