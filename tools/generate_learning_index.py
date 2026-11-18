#!/usr/bin/env python3
"""Recreate the two generic learning index files from the pinned format4 resources."""
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
from collections import defaultdict
import argparse
import hashlib
import json
import struct
import tempfile

SCHEMES = ['xiaohe', 'full_pinyin', 'wubi86']

def source_map(folder, name):
    data = (folder / (name + '.hnm')).read_bytes()
    magic, version, ns, bundle, oldhash, bodybytes = struct.unpack_from('<IIQQQQ', data)
    assert magic == 0x324d4353 and version == 2 and bodybytes == len(data)-40
    h = struct.unpack_from('<8IQ', data, 40)
    _, size, pages, root, depth, count, leaves, namesize, offset = h
    assert data[104:104+namesize].decode() == name
    result = defaultdict(list)
    actual = 0
    for i in range(leaves):
        page = data[40+offset+i*size:40+offset+(i+1)*size]
        pm, kind, reserved, n, nxt, used = struct.unpack_from('<IBBHII', page)
        assert pm == 0x32475049 and kind == 1 and not reserved
        assert nxt == (i+1 if i+1<leaves else 0xffffffff)
        at = 16
        for _ in range(n):
            length = struct.unpack_from('<H', page, at)[0]; at += 2
            code = page[at:at+length]; at += length
            ref, weight, ordinal = struct.unpack_from('<IiI', page, at); at += 12
            result[(code, ref)].append((weight, ordinal)); actual += 1
        assert at == used
    assert actual == count
    return result, dict(bytes=len(data), raw_paths=count, leaves=leaves, pages=pages,
                        depth=depth, namespace=ns, bundle=bundle)

def base(folder, name):
    data = (folder / (name + '_base.hbo')).read_bytes()
    h = struct.unpack_from('<32I', data)
    entries = []
    if h[:2] == (0x31535142, 5):
        for i in range(h[5]):
            page = data[h[10]+1024*i:h[10]+1024*(i+1)]
            n, restarts, first = struct.unpack_from('<HHI', page)
            assert first == len(entries) and restarts == (n+15)//16
            at = 8+2*restarts; code = b''
            for j in range(n):
                pre, suf = page[at:at+2]
                start, count, paths = struct.unpack_from('<III', page, at+2)
                assert pre <= len(code) and (j%16 or pre==0)
                code = code[:pre]+page[at+14:at+14+suf]; at += 14+suf
                entries.append((code, start, count, paths))
        rank, groups = h[18], h[3]
    elif h[:2] == (0x324f4248, 2):
        width, codes, groups, maxcode = h[2:6]
        rank = struct.unpack_from('<Q', data, 40)[0]
        for i in range(h[7]):
            page = data[1024*(i+1):1024*(i+2)]
            level, n = struct.unpack_from('<II', page)
            if level: continue
            for j in range(n):
                at = 16+j*width
                length, start, count, paths = struct.unpack_from('<4I', page, at)
                entries.append((page[at+16:at+16+length], start, count, paths))
        entries.sort()
        assert len(entries) == codes
    else: raise AssertionError(h[:2])
    assert len(entries) == h[2] if h[0]==0x31535142 else len(entries)==h[3]
    return data, entries, rank, groups

def merkle(data):
    sha = hashlib.sha256
    children = [sha(b'\0'+struct.pack('<QI', i, len(data[i*1024:(i+1)*1024]))+
                    data[i*1024:(i+1)*1024]).digest() for i in range((len(data)+1023)//1024)]
    levels=[]; level=0
    while True:
        page=b''.join(children); page+=bytes((-len(page))%1024); levels.append(page)
        parents=[sha(b'\1'+struct.pack('<II',level,i)+page[i*1024:(i+1)*1024]).digest()
                 for i in range(len(page)//1024)]
        if len(parents)==1: return b''.join(levels), parents[0]
        children=parents; level+=1

def build(folder, receipt):
    refs = 242979
    associations = [[] for _ in range(refs+1)]
    stats = {}
    for sid, name in enumerate(SCHEMES):
        mappings, stat = source_map(folder,name)
        data, ranges, rank, groups = base(folder,name)
        expected = 0; seen=set()
        for code, start, count, paths in ranges:
            assert start == expected and count and paths>=count
            expected += count; totalpaths=0; previous=None
            for pos in range(count):
                off=rank+64*(start+pos)
                key, score, ref, weight, pathcount, ordinal, pad = struct.unpack_from('<32sqIiIIQ', data, off)
                assert 0<ref<=refs and key!=bytes(32) and pad==0
                raw=mappings[(code,ref)]
                # Original exact grouping: max scheme_weight, lowest ordinal among max-weight paths.
                w=max(x[0] for x in raw)
                assert pathcount==len(raw) and weight==w and ordinal==min(x[1] for x in raw if x[0]==w)
                pair=(code,ref); assert pair not in seen; seen.add(pair); totalpaths+=pathcount
                sortkey=(-score,key)
                assert previous is None or previous<sortkey
                previous=sortkey
                scope=(sid<<30)|start
                associations[ref].append((scope,start+pos))
            assert totalpaths==paths
        assert expected==groups and seen==set(mappings)
        stat.update(ranges=len(ranges), groups=groups, rank_offset=rank, base_bytes=len(data),
                    all_source_pairs_checked=len(seen))
        stats[name]=stat
    # Dense CSR directory: exactly two adjacent u32 offsets per ref; no key strings/ref table scan.
    offsets=[0]; edges=[]
    for ref in range(1,refs+1):
        edges.extend(sorted(associations[ref])); offsets.append(len(edges))
    index=struct.pack('<%dI'%len(offsets),*offsets)
    payload=b''.join(struct.pack('<II',*e) for e in edges)
    header=bytearray(1024)
    struct.pack_into('<8sIIQQIIII',header,0,b'BGR301\0\0',1,refs,5206528720952950785,
                     receipt['shared_bundle_id'],len(edges),1024,1024+len(index),len(stats))
    for i,n in enumerate(SCHEMES):
        struct.pack_into('<4I',header,64+i*16,stats[n]['rank_offset'],stats[n]['groups'],stats[n]['ranges'],0)
    new=bytes(header)+index+payload
    proof, root=merkle(new)
    return new,proof,root


def main():
    if not __debug__:
        raise SystemExit('Run with validation enabled (omit -O).')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resources',required=True,type=Path,help='The B_SHORT_TEXT_20261018 directory')
    parser.add_argument('--output',required=True,type=Path,help='New directory for the two generic index files')
    args=parser.parse_args()
    repo=Path(__file__).resolve().parents[1]
    receipt=json.loads((repo/'release/alpha2/RESOURCE_MANIFEST.json').read_text())
    manifest=json.loads((repo/'release/alpha3/TF_RESOURCES.json').read_text())
    if args.output.exists():
        raise SystemExit('Use a new output directory.')
    for row in receipt['files']:
        with (args.resources/row['name']).open('rb') as f:
            if hashlib.file_digest(f,'sha256').hexdigest()!=row['sha256']:
                raise SystemExit('Wrong input: '+row['name'])
    data,proof,root=build(args.resources,receipt)
    expected={Path(row['tf_path']).name:row for row in manifest['files']}
    if root.hex()!=manifest['relation_merkle_root']:
        raise SystemExit('Generated proof root differs from the release.')
    files={'relations.bgr':data,'relations.hnv':proof}
    for name,content in files.items():
        if len(content)!=expected[name]['bytes'] or hashlib.sha256(content).hexdigest()!=expected[name]['sha256']:
            raise SystemExit('Generated index differs from the release: '+name)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as staging:
        output=Path(staging)/'index'
        output.mkdir()
        for name,content in files.items():
            (output/name).write_bytes(content)
        output.rename(args.output)
    print('Verified',len(data)+len(proof),'bytes of generic learning indexes in',args.output)

if __name__=='__main__':
    main()
