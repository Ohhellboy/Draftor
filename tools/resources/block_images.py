"""Actual B2 -> five immutable, valued, privately versioned query images.
Only physical code->(start,count,paths) changes; rank/member tail is byte exact.
Radix, output-pushed acyclic FST and LOUDS are finite screening implementations.
DAT uses unmodified upstream Darts-clone. No library Agent continuation claims.
"""
from pathlib import Path
from collections import deque
import struct,json,hashlib,re,subprocess,time,platform
from legacy_hbo import decode,proof
R=Path(__file__).resolve().parents[1]
S=R/'source/integration/resources/staged'
E=R/'evidence'
NONE=0xffffffff
sha=lambda b:hashlib.sha256(b).hexdigest()
u32=lambda x:struct.pack('<I',x)
def trie(rows):
    nodes=[{'c':{},'v':NONE}]
    for value,(key,*_) in enumerate(rows):
        n=0
        for c in key:
            if c not in nodes[n]['c']:
                nodes[n]['c'][c]=len(nodes);nodes.append({'c':{},'v':NONE})
            n=nodes[n]['c'][c]
        assert nodes[n]['v']==NONE;nodes[n]['v']=value
    return nodes
def radix(t):
    queue=[(0,b'')];records=[];labels=bytearray();i=0
    while i<len(queue):
        n,label=queue[i];first=len(queue)
        for c,child in sorted(t[n]['c'].items()):
            edge=bytes([c])
            while t[child]['v']==NONE and len(t[child]['c'])==1:
                c,child=next(iter(t[child]['c'].items()));edge+=bytes([c])
            queue.append((child,edge))
        records.append(struct.pack('<IHHII',first,len(t[n]['c']),len(label),len(labels),t[n]['v']))
        labels+=label;i+=1
    return [b''.join(records),bytes(labels)],dict(nodes=len(queue),edges=len(queue)-1,node_record_bytes=16,edge_label_bytes=len(labels),terminal_value_bytes_in_nodes=4*len(queue),uncompressed_nodes=len(t))
def fst(t):
    ids=[0]*len(t);mins=[0]*len(t);intern={};states=[]
    for n in range(len(t)-1,-1,-1):
        v=t[n]['v'];mins[n]=min(([v] if v!=NONE else [])+[mins[j] for j in t[n]['c'].values()])
        term=NONE if v==NONE else v-mins[n]
        sig=(term,tuple((c,ids[j],mins[j]-mins[n]) for c,j in sorted(t[n]['c'].items())))
        if sig not in intern:intern[sig]=len(states);states.append(sig)
        ids[n]=intern[sig]
    # BFS physical layout, keeping state numbers strictly physical.
    order=[ids[0]];mapping={ids[0]:0}
    for x in order:
        for _,child,_ in states[x][1]:
            if child not in mapping:mapping[child]=len(order);order.append(child)
    edges=[];records=[]
    for x in order:
        term,arcs=states[x];assert term in (NONE,0)
        records.append(struct.pack('<IHBx',len(edges),len(arcs),term==0))
        edges.extend(struct.pack('<BII',c,mapping[child],out) for c,child,out in arcs)
    assert mins[0]==0
    return [b''.join(records),b''.join(edges)],dict(states=len(states),arcs=len(edges),state_record_bytes=8,arc_record_bytes=9,arc_output_bytes=4*len(edges),final_output=0,output_semantics='sum of nonnegative arc outputs = physical descriptor ordinal, never score/identity',uncompressed_nodes=len(t))
def bits(values):
    b=bytearray((len(values)+7)//8)
    for i,v in enumerate(values):
        if v:b[i//8]|=1<<(i%8)
    return bytes(b)
def louds(t):
    order=[0];top=[];labels=bytearray();terms=[];outputs=[];select=[]
    for i,n in enumerate(order):
        for c,j in sorted(t[n]['c'].items()):order.append(j);top.append(1);labels.append(c)
        if i%32==0:select.append(len(top))
        top.append(0);terms.append(t[n]['v']!=NONE)
        if terms[-1]:outputs.append(t[n]['v'])
    ranks=[];count=0
    for i,v in enumerate(terms):
        if i%256==0:ranks.append(count)
        count+=v
    # Section5 contains rank samples followed by 24-bit terminal output IDs.
    out=b''.join(x.to_bytes(3,'little') for x in outputs)
    return [bits(top),bytes(labels),b''.join(map(u32,select)),bits(terms),b''.join(map(u32,ranks))+out],dict(nodes=len(order),topology_bits=len(top),labels=len(labels),select0_stride=32,select0_samples=len(select),rank_stride_bits=256,rank_samples=len(ranks),terminal_bits=len(terms),terminal_value_bytes=len(out),terminal_values=len(outputs),marisa_library='not ported; this is an actual LOUDS representation with explicit auxiliaries')
def common(a,b):
    i=0
    while i<min(len(a),len(b)) and a[i]==b[i]:i+=1
    return i
def blocks(rows):
    # 1 KiB authentication locality; 16-key restarts bound local decode.
    pages=[];fences=[];maxscan=0;restartcount=0
    at=0
    while at<len(rows):
        first=at;records=[];offsets=[];previous=b'';payload=0
        while at<len(rows):
            key,start,count,paths=rows[at];n=len(records);restart=n%16==0
            prefix=0 if restart else common(previous,key)
            record=struct.pack('<BBIII',prefix,len(key)-prefix,start,count,paths)+key[prefix:]
            restarts=(n+16)//16
            if 8+2*restarts+payload+len(record)>1024:break
            if restart:offsets.append(payload)
            records.append(record);payload+=len(record);previous=key;at+=1
        assert records
        head=8+2*len(offsets)
        page=struct.pack('<HHI',len(records),len(offsets),first)+b''.join(struct.pack('<H',head+x) for x in offsets)+b''.join(records)
        pages.append(page.ljust(1024,b'\0'));fences.append(rows[first][0]);restartcount+=len(offsets);maxscan=max(maxscan,min(16,len(records)))
    offsets=[];b=bytearray()
    for key in fences:offsets.append(len(b));b+=bytes([len(key)])+key
    nav=b''.join(u32(4*len(fences)+v) for v in offsets)+b
    return [bytes(nav),b''.join(pages)],dict(blocks=len(pages),navigation_entries=len(fences),navigation_resident_bytes=len(nav),restart_interval=16,restart_points=restartcount,max_scan_records=maxscan,descriptors_inline_bytes=12*len(rows),restart_offset_bytes=2*restartcount,block_header_bytes=8*len(pages),encoded_records_and_padding_bytes=len(pages)*1024-8*len(pages)-2*restartcount-12*len(rows))
def parse_pin(name):
    pin=(R/'source/integration/product/dependencies/release_resources/release_pin.hpp').read_text()
    return re.search(r'inline const hanime::ResourceReleaseSpec '+name+r'_base = \[\] \{(.*?)\}\(\);',pin,re.S).group(0)
def pin_digest(section,field):return bytes(map(int,re.search(r's\.'+field+r'=\{\{([^}]+)',section).group(1).split(',')))
def write_image(name,kind,parts,stats,old,h,rows):
    header=[0]*32;header[:7]=[0x31535142,kind,len(rows),h[4],h[5],stats.get('nodes',stats.get('states',stats.get('slots',stats.get('blocks')))),0]
    image=bytearray(1024);components={};sections=[]
    for index,part in enumerate(parts):
        image+=bytes((-len(image))%1024);offset=len(image);image+=part
        header[8+2*index]=offset;header[9+2*index]=len(part);sections.append(dict(offset=offset,bytes=len(part)))
        components['section'+str(index+1)]=len(part)
    if kind!=5:
        image+=bytes((-len(image))%1024);header[7]=len(image)
        desc=b''.join(struct.pack('<III',*row[1:]) for row in rows);image+=desc;components['descriptors']=len(desc)
    else:components['descriptors']=0
    image+=bytes((-len(image))%1024);header[18]=len(image);header[19]=header[18]+64*h[4]
    tail=old[h[9]:];image+=tail;header[20]=len(image)
    if kind==4:header[21]=stats['rank_samples']
    if kind==5:header[21]=stats['navigation_entries']
    image[:128]=struct.pack('<32I',*header);image[128:160]=hashlib.sha256(old).digest()
    assert bytes(image[header[18]:])==tail
    p,root=proof(image)
    label={1:'radix',2:'dat',3:'fst',4:'louds',5:'block'}[kind]
    out=R/'resources'/label;out.mkdir(exist_ok=True)
    (out/(name+'.bqs')).write_bytes(image);(out/(name+'.hnv')).write_bytes(p)
    # Small reconstructable prefix, unchanged rank/member tail excluded.
    (out/(name+'.prefix')).write_bytes(image[:header[18]])
    sourcepin=parse_pin(name)
    newpin=sourcepin.replace('inline const hanime::ResourceReleaseSpec '+name+'_base', 'inline hanime::ResourceReleaseSpec '+label+'_'+name)
    newpin=re.sub(r's.resource_format=\d+',f's.resource_format={100+kind}',newpin)
    newpin=re.sub(r's.file_bytes=\d+ULL',f's.file_bytes={len(image)}ULL',newpin)
    for field,value in [('file_sha256',hashlib.sha256(image).digest()),('merkle_root',root)]:
        newpin=re.sub(r's\.'+field+r'=\{\{[^}]+\}\}', 's.'+field+'={{'+','.join(map(str,value))+'}}',newpin)
    components.update(header=1024,alignment_padding=header[18]-1024-sum(components.values()),rank=64*h[4],member=8*h[4],proof=len(p),file=len(image),total_tf=len(image)+len(p))
    return dict(scheme=name,kind=label,stats=stats,sections=sections,components=components,sha256=sha(image),merkle_root=root.hex(),tail_sha256=sha(tail),source_sha256=sha(old)),newpin
