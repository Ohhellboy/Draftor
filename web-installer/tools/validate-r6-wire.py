#!/usr/bin/env python3
"""Offline only: reconstruct recorded R6 transactions and compare exact bytes.
Never imports serial or opens any device. Private digests/identity stay private.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('r6',ROOT/'tools/r6_maintenance.py');r6=importlib.util.module_from_spec(spec);spec.loader.exec_module(r6)

def parse_python(path):
    b=path.read_bytes();i=0;packets=[[],[]];empty=0
    while i<len(b):
        assert i+11<=len(b);t,d,n=struct.unpack_from('<dBH',b,i);assert d in (0,1) and i+11+n<=len(b)
        q=b[i+11:i+11+n];i+=11+n
        if q:packets[d].append((t*1000,q))
        else:empty+=1
    return packets,empty,[]

def parse_browser(path):
    rows=[json.loads(l) for l in path.read_text().splitlines()];packets=[];empty=0
    for kind in ('tx','rx'):
        out=[];buf=bytearray();esc=False;started=False
        for row in rows:
            if row['type']!=kind:continue
            for v in bytes.fromhex(row['hex']):
                if v==192:
                    assert not esc
                    if started and buf:out.append((row['t'],bytes(buf)))
                    elif started:empty+=1
                    buf.clear();started=True
                elif started:
                    if esc:assert v in (220,221);buf.append(192 if v==220 else 219);esc=False
                    elif v==219:esc=True
                    else:buf.append(v)
        assert not buf and not esc;packets.append(out)
    return packets,empty,rows

def validate(context,directory,stage):
    a=r6.load_assets(context);browser=stage=='browser'
    (tx,rx),empty,events=(parse_browser(directory/'browser-events.private.jsonl') if browser else parse_python(directory/'wire.private.bin'))
    factory=a['factory']+b'\xff'*(r6.FLASH-len(a['factory']))
    initial=a['backup'] if stage in ('precheck','factory','verify-backup') else a['initialized'] if stage=='verify-draftor' else factory
    flash=bytearray(initial);erased=False;seen_reads={};counts={};j=0;pending=None;digests=[];identities={};security=0;write_rows=[]
    if stage=='factory':expected=[(0,None,r6.FLASH),(0,a['factory'],len(a['factory']))]
    elif stage=='restore':expected=[(0,a['backup'],r6.FLASH)]
    elif browser:expected=[(0,None,r6.FLASH)]+[(off,data,len(data)) for off,data in a['draftor']]
    else:expected=[]
    for t,p in tx:
        d,op,n,checksum=struct.unpack('<BBHI',p[:8]);payload=p[8:]
        assert d==0 and n==len(payload) and op in {2,3,8,9,10,11,13,14,15,19,20};counts[hex(op)]=counts.get(hex(op),0)+1
        if op==2:
            assert pending is None or pending['next']==pending['blocks']
            off,image,size=expected[len(write_rows)];actual=struct.unpack('<IIIII',payload)
            assert actual==(size,(size+1023)//1024,1024,off,0)
            flash[off:off+(size+4095)//4096*4096]=b'\xff'*((size+4095)//4096*4096)
            pending=None if image is None else {'offset':off,'image':image,'blocks':(size+1023)//1024,'next':0}
            write_rows.append({'offset':hex(off),'bytes':size,'kind':'erase-only' if image is None else 'write-image'})
            if image is None:erased=True
        if op==3:
            assert pending is not None
            size,seq,z1,z2=struct.unpack('<IIII',payload[:16]);assert (size,seq,z1,z2)==(1024,pending['next'],0,0)
            block=payload[16:];assert block==pending['image'][seq*1024:(seq+1)*1024].ljust(1024,b'\xff')
            check=239
            for v in block:check^=v
            assert check==checksum;start=pending['offset']+seq*1024;flash[start:start+1024]=block;pending['next']+=1
        for _ in range(8 if op==8 else 1):
            rt,q=rx[j];j+=1;direction,rop,rn,val=struct.unpack('<BBHI',q[:8])
            assert direction==1 and rop==op and rn==len(q)-8 and rn=={14:68,19:36,20:24}.get(op,4) and q[-4]==0
            if op==8:assert val!=0
            if op==20:
                flags=struct.unpack_from('<I',q,8)[0];chip=struct.unpack_from('<I',q,20)[0];assert flags==0 and q[12]==0 and chip==9;security+=1
            if op==10:
                addr=struct.unpack('<I',payload)[0];identities.setdefault(addr,[]).append(val)
            if op==14:
                off,size=struct.unpack('<II',payload);assert size==64;data=q[8:72]
                if browser and not erased and 0x9000<=off<0x10000:
                    if off in seen_reads:assert seen_reads[off]==data
                    seen_reads[off]=data;flash[off:off+64]=data
                assert data==flash[off:off+64]
            if op==19:
                off,size,z1,z2=struct.unpack('<IIII',payload);assert z1==z2==0
                got=q[8:40].decode('ascii');assert got==hashlib.md5(flash[off:off+size]).hexdigest()
                digests.append({'offset':hex(off),'bytes':size,'matched':True,'elapsed_ms':round(rt-t,3)})
    assert j==len(rx) and len(write_rows)==len(expected) and counts.get('0x8')==1
    assert pending is None or pending['next']==pending['blocks']
    lo=identities[0x60007044];hi=identities[0x60007048];assert len(lo)==len(hi)>0
    for l,h in zip(lo,hi):
        mac=':'.join(f'{v:02x}' for v in [(h>>8)&255,h&255,l>>24,(l>>16)&255,(l>>8)&255,l&255]);assert mac==context['expected_mac'].lower()
    if browser:
        assert sum(r['type']=='opened' for r in events)==1 and sum(r['type']=='closed' for r in events)==1
        assert not any(r['type'] in ('open-error','read-error','write-error','close-error') for r in events)
        ui=[r for r in events if r['type']=='ui'];assert any('VERIFIED_BOOT_PENDING' in r['status'] for r in ui)
        plan=json.loads([r['plan'] for r in ui if r.get('plan')][-1]);assert plan['mode']=='fresh' and plan['initialState']=='waveshare-factory-20260120' and plan['fullErase'] and plan['hardwareWriteAllowed']
    else:assert json.loads((directory/'result.json').read_text())['status']=='PASS'
    result={'status':'PASS','stage':stage,'collection':'actual recorded bytes, offline independent reconstruction','tx_packets':len(tx),'rx_packets':len(rx),'empty_delimiters_accepted':empty,'op_counts':counts,'exact_write_payloads_sequences_checksums':True,'all_ROM_status_success':True,'same_expected_identity_samples':len(lo),'security_checks':security,'write_ranges':write_rows,'digest_checks':digests,'metadata_reads_match':True,'no_stub':True,'no_finish_or_reset_command':True,'no_TF_access':True}
    (directory/'wire-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    return {k:result[k] for k in ['status','stage','tx_packets','rx_packets','op_counts','same_expected_identity_samples','write_ranges']}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('stage');p.add_argument('--context',type=Path,required=True);p.add_argument('--directory',type=Path,required=True);a=p.parse_args()
    print(json.dumps(validate(json.loads(a.context.read_text()),a.directory,a.stage),indent=2))
