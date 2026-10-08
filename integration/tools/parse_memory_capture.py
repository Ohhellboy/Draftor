#!/usr/bin/env python3
"""Offline T05 memory/event extraction from the existing mixed raw archive.

This assigns no DEVICE_PASS and makes no leak/root-cause classification.
"""
from pathlib import Path
import argparse,base64,json,re,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'telemetry/host'))
from draftor_telemetry.decoder import StreamDecoder
FIELDS='seq ms kind a b c sampled i_free i_largest i_min p_free p_largest p_min loop_hwm_bytes ble_worker_hwm_bytes integrity dropped'.split()
IDENTITY='DRAFTOR_S3_LEXICON_R3_R1_20261006_BASIC_TEST_ONLY'

class MemoryReader:
    def __init__(self):
        self.decoder=StreamDecoder(mixed=True);self.line=bytearray();self.boot=None
        self.records=[];self.issues=[];self.boots=[];self.last=None;self.sample=None;self.firmware=None
    def issue(self,reason):self.issues.append({'boot':self.boot,'reason':reason})
    def parse_line(self,line):
        if line.startswith(b'[BT_BUILD] '):
            m=re.search(rb'firmware=([^ ]+)',line);self.firmware=m.group(1).decode('ascii') if m else None
            if self.firmware!=IDENTITY:self.issue('unexpected or missing firmware identity')
        elif line.startswith(b'[ST_INIT] '):
            self.boot=None;self.last=None;self.sample=None;self.firmware=None
            if not re.search(rb'\bok=1\b',line):self.issue('stability buffer unavailable')
        elif line.startswith(b'[BT_BOOT] '):
            m=re.search(rb'\bboot=([0-9a-fA-F]{16})\b',line)
            self.boot=m.group(1).decode('ascii') if m else None
            if not self.boot or self.boot in self.boots:self.issue('missing or repeated boot identity')
            self.boots.append(self.boot);self.last=None;self.sample=None
        elif line.startswith(b'[ST_STOP]'):
            self.issue('product stop marker observed')
        elif line.startswith(b'[ST] '):
            pairs=re.findall(rb'(\w+)=(\d+)',line)
            values={k.decode():int(v) for k,v in pairs}
            if len(pairs)!=len(FIELDS) or set(values)!=set(FIELDS) or any(n>0xffffffff for n in values.values()):
                self.issue('malformed ST record');return
            if not self.boot or self.firmware!=IDENTITY:self.issue('record lacks verified startup identity')
            if self.last is None:
                if values['seq']!=1:self.issue('initial ST output sequence missing: expected 1')
            elif values['seq']!=(self.last+1)&0xffffffff:self.issue('ST output sequence gap/reorder')
            self.last=values['seq']
            if values['dropped']:self.issue('device buffer drop counter nonzero')
            if values['integrity']==2:self.issue('heap integrity failure')
            if values['kind']==13:
                if values['sampled']!=1 or values['integrity']!=0 or values['loop_hwm_bytes'] or values['ble_worker_hwm_bytes']:self.issue('periodic schema mismatch')
                if self.sample is None and values['a']!=1:self.issue('initial periodic admission sequence missing: expected 1')
                if self.sample is not None:
                    seq,ms=self.sample
                    if values['a']!=(seq+1)&0xffffffff:self.issue('periodic admission sequence gap/reorder')
                    if (values['ms']-ms)&0xffffffff<2000:self.issue('record timestamp interval below 2000 ms: clock-read delay/preemption possible; not proof of admission cadence violation or lost records')
                self.sample=(values['a'],values['ms'])
                if values['c']:self.issue('skipped sampling intervals')
            self.records.append({'boot':self.boot,**values})
        elif b'Guru Meditation' in line or b'panic' in line or b'abort()' in line:self.issue('panic text observed')
    def consume(self,records):
        for record in records:
            if record['record_type']=='diagnostic':
                self.line.extend(base64.b64decode(record['bytes_base64'],validate=True))
                while b'\n' in self.line:
                    line,_,rest=self.line.partition(b'\n');self.line=bytearray(rest);self.parse_line(bytes(line).rstrip(b'\r'))
                if len(self.line)>2048:self.issue('overlong or truncated diagnostic line');self.line.clear()
            else:
                if self.line:self.issue('partial diagnostic interrupted by binary frame');self.line.clear()
                if record['record_type']!='event':self.issue('mixed decoder: '+record.get('error',record['record_type']))
    def feed(self,data):self.consume(self.decoder.feed(data,'OFFLINE_RAW_ORDER'))
    def finish(self):
        self.consume(self.decoder.finish('OFFLINE_RAW_ORDER','input_eof'))
        if self.line:self.issue('truncated final diagnostic line')
        samples=[r for r in self.records if r['kind']==13]
        if not samples:self.issue('no periodic memory samples')
        return {'status':'CAPTURE_NEEDS_REVIEW' if self.issues else 'PARSED_NO_DETECTED_GAPS',
                'device_verdict':'NOT_ASSIGNED','scope':'Observed capture only; no leak, fragmentation or historic root-cause verdict',
                'caps':{'internal':0x804,'psram':0x404},'unit':'bytes','boots':self.boots,'samples':samples,
                'events':[r for r in self.records if r['kind']!=13],'issues':self.issues}

def selftest():
    assert IDENTITY in (ROOT/'product/src/candidate_build.hpp').read_text(), 'Parser identity must match this candidate source'
    def row(seq=1,ms=2000,sample=1,drop=0,skip=0):
        v=[seq,ms,13,sample,0,skip,1,321,123,111,654,456,222,0,0,0,drop]
        return ('[ST] '+' '.join(f'{k}={n}' for k,n in zip(FIELDS,v))+'\n').encode()
    boot=b'[ST_INIT] ok=1 bytes=1496 hook=0 reset=1\n[BT_BUILD] firmware='+IDENTITY.encode()+b' profile=1 capture=1\n[BT_BOOT] boot=0123456789abcdef profile=1\n'
    r=MemoryReader()
    for byte in boot+row()+row(2,4000,2):r.feed(bytes([byte]))
    good=r.finish();assert not good['issues'] and len(good['samples'])==2
    for foreign in [b'DRAFTOR_S3_ALPHA_CLOSEOUT_T05_R2_20261006_BASIC_TEST_ONLY',b'DRAFTOR_S3_ALPHA_CLOSEOUT_T05_R1_20261005_BASIC_TEST_ONLY',b'DRAFTOR_S3_ALPHA_CLOSEOUT_T05_20261005_BASIC_TEST_ONLY',b'DRAFTOR_S3_ALPHA_CLOSEOUT_T04_20261005_BASIC_TEST_ONLY',b'DRAFTOR_S3_ALPHA_CLOSEOUT_T03_20261005_BASIC_TEST_ONLY',b'DRAFTOR_S3_ALPHA_CLOSEOUT_T01_20261004_BASIC_TEST_ONLY',b'UNREVIEWED_OTHER_FIRMWARE']:
        r=MemoryReader();r.feed(boot.replace(IDENTITY.encode(),foreign)+row())
        assert any('firmware identity' in x['reason'] for x in r.finish()['issues'])
    for bad in [row(seq=2),row(seq=2,ms=4000,sample=2),row(sample=2),row()+row(3,4000,3),row(drop=1),row(skip=1),row()+b'[ST_STOP] integrity=2 scope=product_loop\n',row()+row(2,2500,2),row()[:-1],b'[ST] bad\n',row()+bytes([0xc1,0,0xc0])]:
        r=MemoryReader();r.feed(boot+bad);assert r.finish()['issues']
    r=MemoryReader();r.feed(row());assert r.finish()['issues']
    # Wire-only controls: each positively identified boot restarts both sequences.
    second=boot.replace(b'0123456789abcdef',b'fedcba9876543210')
    r=MemoryReader();r.feed(boot+row()+row(2,4000,2)+second+row()+row(2,4000,2))
    two=r.finish();assert not two['issues'] and len(two['boots'])==2
    r=MemoryReader();r.feed(boot+row()+second+row(seq=3,sample=2))
    two=r.finish();assert any('initial ST output sequence' in x['reason'] for x in two['issues'])
    assert any('initial periodic admission sequence' in x['reason'] for x in two['issues'])
    # An explicitly missing middle window remains a gap. Wrapping counters and
    # monotonic milliseconds afterward must not add a false reorder/short-time issue.
    r=MemoryReader();r.feed(boot+row(ms=0xfffff000)+row(0xffffffff,0xfffffff0,0xffffffff))
    assert len(r.issues)==2
    r.feed(row(0,1984,0)+row(1,3984,1));wrapped=r.finish();assert len(wrapped['issues'])==2
    r=MemoryReader();r.feed(boot+row(seq=0,sample=0));assert len(r.finish()['issues'])==2
    # Two actual record clocks can be 1999 ms apart although admission was 2000 ms.
    r=MemoryReader();r.feed(boot+row(ms=2001)+row(2,4000,2));timing=r.finish()
    assert timing['status']=='CAPTURE_NEEDS_REVIEW' and timing['device_verdict']=='NOT_ASSIGNED'
    assert len(timing['issues'])==1 and 'not proof of admission cadence violation or lost records' in timing['issues'][0]['reason']
    print('MEMORY_CAPTURE_PARSER_PASS initial and interior gaps, complete and multi-boot starts, counter/time wrap, conservative clock note, drops/stop/missing identity/truncated/binary errors')

def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path);p.add_argument('--output',type=Path);p.add_argument('--self-test',action='store_true');a=p.parse_args()
    if a.self_test:selftest();return
    if not a.capture or not a.output:p.error('--capture and --output are required')
    folder=a.capture.resolve();manifest=json.loads((folder/'manifest.json').read_text());assert manifest['stream_mode']=='mixed'
    assert not a.output.exists(),'Refuse to overwrite an earlier interpretation'
    reader=MemoryReader();size=0
    for part in manifest['files']:
        f=(folder/part['file']).resolve();assert f.parent==folder and f.stat().st_size==part['bytes']
        with f.open('rb') as stream:
            while chunk:=stream.read(65536):
                size+=len(chunk);assert size<=128*1024*1024;reader.feed(chunk)
    assert size==manifest['bytes'];result=reader.finish();result['capture_manifest']=str(folder/'manifest.json')
    result['archive_stop_reason']=manifest['stop_reason']
    if manifest['stop_reason'] not in ['ctrl_c','input_eof']:result['issues'].append({'boot':None,'reason':'archive stopped: '+manifest['stop_reason']});result['status']='CAPTURE_NEEDS_REVIEW'
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(result['status'],'samples='+str(len(result['samples'])),'DEVICE_VERDICT_NOT_ASSIGNED')
if __name__=='__main__':main()
