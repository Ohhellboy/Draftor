"""B1 TEST_ONLY: derive a compact directory from pinned HBO2, never from lexicon.

HBO3 pages: level/count/zero64, uint16 offsets[count+1], packed records.
Leaf = length/start/count/paths/key; branch = length/child/key.
Offsets are page-relative. Each record stays inside its authenticated 1024B page.
The complete rank/member tail is copied verbatim. No cache or new trust source.
"""
from pathlib import Path
import hashlib, json, re, struct, time
R=Path(__file__).resolve().parents[1]
BASE=R/'source/BASE'
OUT=R/'resources/PROBE'
u32=lambda b,o=0:struct.unpack_from('<I',b,o)[0]
def digest(b):return hashlib.sha256(b).digest()
def proof(b):
    hashes=[digest(b'\0'+struct.pack('<QI',i,len(b[o:o+1024]))+b[o:o+1024]) for i,o in enumerate(range(0,len(b),1024))]
    result=b'';level=0
    while True:
        pages=[b''.join(hashes[o:o+32]).ljust(1024,b'\0') for o in range(0,len(hashes),32)]
        result+=b''.join(pages)
        hashes=[digest(b'\1'+struct.pack('<II',level,i)+p) for i,p in enumerate(pages)]
        if len(hashes)==1:return result,hashes[0]
        level+=1
def decode(b,compact=False):
    h=struct.unpack('<8I4Q',b[:64]);assert h[0]==(0x334f4248 if compact else 0x324f4248) and h[1]==(3 if compact else 2)
    assert h[2]==h[5]+16 and h[6]==64 and h[8]==1024 and h[9]==1024*(h[7]+1)
    assert h[10]==h[9]+64*h[4] and h[11]==h[10]+8*h[4]==len(b)
    assert not any(b[64:1024]);seen=set();rows=[];levels={}
    def visit(p,expect=None):
        assert p<h[7] and p not in seen;seen.add(p)
        page=b[1024*(p+1):1024*(p+2)];lev,n,zero=struct.unpack('<IIQ',page[:16])
        assert lev<=31 and (expect is None or lev==expect) and n and not zero
        levels[lev]=levels.get(lev,0)+1
        if compact:
            assert n<=(1024-18)//(2+(8 if lev else 16)+1)
            offsets=struct.unpack_from('<'+'H'*(n+1),page,16)
            assert offsets[0]==16+2*(n+1) and offsets[-1]<=1024
            records=[page[a:z] for a,z in zip(offsets,offsets[1:])]
            assert all(a<z for a,z in zip(offsets,offsets[1:]))
            end=offsets[-1]
        else:
            assert n<=(1024-16)//h[2]
            records=[page[16+i*h[2]:16+(i+1)*h[2]] for i in range(n)];end=16+n*h[2]
        assert not any(page[end:]);last=None
        for row in records:
            length,start=struct.unpack_from('<II',row);meta=8 if compact and lev else 16
            assert 0<length<=h[5] and len(row)==(meta+length if compact else h[2])
            key=row[meta:meta+length];assert last is None or last<key;last=key
            if lev:
                if not compact:assert not any(row[8:16]) and not any(row[16+length:])
                assert start>p and visit(start,lev-1)==key
            else:
                count,paths=struct.unpack_from('<II',row,8);assert count and paths>=count
                if not compact:assert not any(row[16+length:])
                rows.append((key,start,count,paths))
        return last
    visit(0);assert len(seen)==h[7] and len(rows)==h[3]
    pos=0;last=None
    for key,start,count,paths in rows:
        assert start==pos and (last is None or last<key);pos+=count;last=key
    assert pos==h[4]
    return h,rows,levels
def build(old,rows):
    nodes=[]
    def group(items,level):
        batch=[];used=18;out=[]
        for item in items:
            key=item['key'] if level else item[0]
            size=2+(8 if level else 16)+len(key)
            if used+size>1024:
                node={'level':level,'items':batch,'key':batch[-1]['key'] if level else batch[-1][0]};nodes.append(node);out.append(node);batch=[];used=18
            batch.append(item);used+=size
        if batch:
            node={'level':level,'items':batch,'key':batch[-1]['key'] if level else batch[-1][0]};nodes.append(node);out.append(node)
        return out
    level=group(rows,0);height=1
    while len(level)>1:level=group(level,height);height+=1;assert height<=32
    ordered=sorted(nodes,key=lambda n:-n['level'])
    for i,node in enumerate(ordered):node['page']=i
    pages=[]
    for node in ordered:
        rec=[]
        for item in node['items']:
            if node['level']:rec.append(struct.pack('<II',len(item['key']),item['page'])+item['key'])
            else:
                key,start,count,paths=item;rec.append(struct.pack('<IIII',len(key),start,count,paths)+key)
        offset=16+2*(len(rec)+1);offsets=[offset]
        for x in rec:offset+=len(x);offsets.append(offset)
        page=struct.pack('<IIQ',node['level'],len(rec),0)+struct.pack('<'+'H'*len(offsets),*offsets)+b''.join(rec)
        assert len(page)<=1024;pages.append(page.ljust(1024,b'\0'))
    h=list(struct.unpack('<8I4Q',old[:64]));tail=old[h[9]:];h[0]=0x334f4248;h[1]=3;h[7]=len(pages);h[9]=1024*(len(pages)+1);h[10]=h[9]+64*h[4];h[11]=h[9]+len(tail)
    return struct.pack('<8I4Q',*h).ljust(1024,b'\0')+b''.join(pages)+tail
