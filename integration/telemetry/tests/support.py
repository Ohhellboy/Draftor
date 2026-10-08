"""Test-only packet mutation helpers; production has exactly one encoder (C++)."""
import binascii
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'host'))
from draftor_telemetry.decoder import StreamDecoder
from draftor_telemetry.reconstruction import SessionReconstructor
STAMP='2026-09-24T12:00:00+00:00'

def slip_packet(packet):
    return b'\xc1'+packet.replace(b'\xdb',b'\xdb\xdd').replace(b'\xc0',b'\xdb\xdc').replace(b'\xc1',b'\xdb\xde')+b'\xc0'

def packets(data):
    result=[]
    for frame in data.split(b'\xc1'):
        if frame:
            frame=frame.removesuffix(b'\xc0')
            result.append(frame.replace(b'\xdb\xdc',b'\xc0').replace(b'\xdb\xde',b'\xc1').replace(b'\xdb\xdd',b'\xdb'))
    return result

def rewrite(packet, offset, format_, *values):
    value=bytearray(packet)
    struct.pack_into(format_,value,offset,*values)
    value[-2:]=struct.pack('<H',binascii.crc_hqx(value[:-2],0xffff))
    return bytes(value)

def decode(data, chunk_size=11):
    decoder=StreamDecoder(); records=[]
    for offset in range(0,len(data),chunk_size): records+=decoder.feed(data[offset:offset+chunk_size],STAMP)
    return records+decoder.finish(STAMP)

def fixture(name):
    group='vertical_compat' if name.startswith('V') else 'preintegration' if name.startswith('P') else 'horizontal'
    return (ROOT/'tests/fixtures'/group/(name+'.bin')).read_bytes()

def events(name): return [e for e in decode(fixture(name)) if e['record_type']=='event']

def summary(records):
    reconstructor=SessionReconstructor()
    for record in records: reconstructor.accept(record)
    return reconstructor.summary()

def session(name): return next(iter(summary(decode(fixture(name)))['sessions'].values()))
