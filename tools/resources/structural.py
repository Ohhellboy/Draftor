#!/usr/bin/env python3
"""Deterministically convert the frozen B/text resource set; never edit inputs.

HSH3 extends the authoritative 52-byte lexeme to 60 bytes by adjoining the
existing default descriptor's payload-relative offset/length. Every other
logical record, string, runtime reference and scheme/order byte is retained.
The old descriptor table remains the general-output path, not another default
authority: equality is checked for ALL records before any new pin is emitted.
SHA-256 domain encodings below are byte-exact ports of frozen packed_auth.cpp
and resource_publisher.cpp; original transports are regenerated and compared
byte for byte first. Runtime still uses the original portable verifier.
"""
from pathlib import Path
import argparse, hashlib, json, re, struct, sys

if not __debug__:
    raise RuntimeError('Resource publication requires validation; optimized Python is prohibited')

FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK = (1 << 64) - 1
ARMS = ('BASE_SHARED', 'LAYOUT_PROTO', 'RESIDENT_PROTO')
NAMES = ('shared', 'xiaohe', 'full_pinyin', 'wubi86', 'xiaohe_base', 'full_pinyin_base', 'wubi86_base')
EXT = {n: '.hns' if n == 'shared' else '.hbo' if n.endswith('_base') else '.hnm' for n in NAMES}
sha = lambda b: hashlib.sha256(b).digest()
u32 = lambda b, o=0: struct.unpack_from('<I', b, o)[0]
u64 = lambda b, o=0: struct.unpack_from('<Q', b, o)[0]


def fnv(b, seed=FNV_OFFSET):
    for c in b:
        seed = ((seed ^ c) * FNV_PRIME) & MASK
    return seed


def write_exact(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.is_symlink() or path.read_bytes() != data:
            raise RuntimeError('Refuse to overwrite differing existing output: ' + str(path))
    else:
        path.write_bytes(data)


def json_exact(path, data):
    write_exact(path, (json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + '\n').encode())


def link_exact(path, src):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink():
        assert path.resolve() == src.resolve(), path
    elif path.exists():
        assert path.read_bytes() == src.read_bytes(), path
    else:
        path.symlink_to(src.resolve())


def legacy_proof(b):
    """Original 1024-byte leaf / 32-way, 1024-byte page Merkle encoding."""
    hashes = [sha(b'\0' + struct.pack('<QI', i, len(b[o:o+1024])) + b[o:o+1024])
              for i, o in enumerate(range(0, len(b), 1024))]
    parts, level = [], 0
    while True:
        pages = [b''.join(hashes[o:o+32]).ljust(1024, b'\0') for o in range(0, len(hashes), 32)]
        parts.extend(pages)
        hashes = [sha(b'\1' + struct.pack('<II', level, i) + p) for i, p in enumerate(pages)]
        if len(hashes) == 1:
            return b''.join(parts), hashes[0]
        level += 1


def packed_proof(b):
    """Original 512-byte packed data plus binary authentication path."""
    count = (len(b) + 511) // 512
    leaves = 1 << (count - 1).bit_length()
    depth = leaves.bit_length() - 1
    tree = [b''] * (2 * leaves)
    for i in range(leaves):
        part = b[512*i:512*(i+1)]
        tree[leaves+i] = sha(b'\2' + struct.pack('<QI', i, len(part)) + part)
    first, level = leaves // 2, 0
    while first:
        for node in range(first):
            at = first + node
            tree[at] = sha(b'\3' + struct.pack('<IQ', level, node) + tree[2*at] + tree[2*at+1])
        first //= 2
        level += 1
    out = bytearray(count * (512 + 32 * depth))
    for i in range(count):
        at = i * (512 + 32 * depth)
        part = b[512*i:512*(i+1)]
        out[at:at+len(part)] = part
        node = leaves + i
        for j in range(depth):
            out[at+512+32*j:at+512+32*(j+1)] = tree[node ^ 1]
            node //= 2
    return bytes(out), tree[1]


def pin_section(text, name):
    result = re.findall(r'inline (?:const )?hanime::ResourceReleaseSpec ' + re.escape(name) + r' = \[\] \{.*?\}\(\);', text, re.S)
    assert len(result) == 1, name
    return result[0]


def pin_number(text, field, default=None):
    result = re.search(r's\.' + field + r'=(\d+)', text)
    if result:
        return int(result[1])
    assert default is not None, field
    return default


def pin_digest(text, field):
    return bytes(map(int, re.search(r's\.' + field + r'=\{\{([^}]+)', text)[1].split(',')))


def patch_pin(text, number_fields, digest_fields):
    for field, value in number_fields.items():
        text, n = re.subn(r'(s\.' + field + r'=)\d+', lambda m: m[1] + str(value), text)
        assert n == 1, field
    for field, value in digest_fields.items():
        text, n = re.subn(r's\.' + field + r'=\{\{[^}]+\}\}',
                          's.' + field + '={{' + ','.join(map(str, value)) + '}}', text)
        assert n == 1, field
    return text


def header(namespace, sections):
    return ('#pragma once\n#include "hanime/verified_resource.hpp"\n'
            '// TEST_ONLY. Generated only after full-record equivalence and original transport checks.\n'
            '// These compiled pins are trusted; on-resource JSON is never a trust source.\n'
            'namespace ' + namespace + ' {\n' + '\n'.join(sections) + '\n}\n').encode()


def parse_shared(b):
    version = u32(b, 4)
    assert version in (2, 3)
    assert u32(b) == (0x32485348 if version == 2 else 0x33485348)
    assert u32(b, 40) == (0x32444853 if version == 2 else 0x33444853)
    assert u32(b, 44) == 160 and u32(b, 48) == 1 and u32(b, 52) == 32 and u32(b, 60) == 5
    assert len(b) - 40 == u64(b, 32)
    page_bytes = u32(b, 56)
    assert 76 <= page_bytes <= 65536
    sections, pos = [], 200
    widths = (52 if version == 2 else 60, 16, 8, 8, 4)
    for s in range(5):
        offset, count, width, pages, zero = struct.unpack_from('<Q4I', b, 64 + 24*s)
        per = (page_bytes - 16) // width
        assert offset == pos and width == widths[s] and not zero
        assert pages == (count + per - 1) // per
        for p in range(pages):
            assert struct.unpack_from('<4I', b, offset+p*page_bytes) == (
                0x32504753 if version == 2 else 0x33504753, s, p, min(per, count-p*per))
            used = 16 + min(per, count-p*per)*width
            assert not any(b[offset+p*page_bytes+used:offset+(p+1)*page_bytes])
        sections.append((offset, count, width, pages))
        pos += pages * page_bytes
    payload_offset, payload_bytes = struct.unpack_from('<QQ', b, 184)
    assert payload_offset == pos and payload_offset + payload_bytes == len(b)
    assert sections[0][1] == sections[4][1]
    return {'version': version, 'page_bytes': page_bytes, 'sections': sections,
            'payload_offset': payload_offset, 'payload_bytes': payload_bytes,
            'namespace': u64(b, 8), 'shared_bundle_id': u64(b, 16)}


def record(b, info, section, index):
    offset, count, width, _ = info['sections'][section]
    assert index < count
    per = (info['page_bytes'] - 16) // width
    at = offset + (index // per) * info['page_bytes'] + 16 + (index % per) * width
    return b[at:at+width]


def validate_shared(b, info):
    """All normalized records, including unencoded identities and every form."""
    assert fnv(b[40:]) == u64(b, 24)
    assert fnv(b[40:], fnv(b[8:16])) == info['shared_bundle_id']
    nlex, nrel, nsel, nstr, _ = [s[1] for s in info['sections']]
    payload = b[info['payload_offset']:]
    strings, spans = [], []
    at = 0
    for i in range(nstr):
        off, size = struct.unpack('<II', record(b, info, 3, i))
        assert size and off == at and off + size <= len(payload)
        part = payload[off:off+size]
        part.decode('utf-8', 'strict')
        strings.append(part); spans.append((off, size)); at += size
    assert at == len(payload)
    next_rel, keys, semantic = 0, [], hashlib.sha256()
    for i in range(nlex):
        r = record(b, info, 0, i)
        key, weight, default, first, count, kind = struct.unpack('<32siIIIB', r[:49])
        assert any(key) and kind in (1, 2) and not any(r[49:52])
        assert default < nstr and first == next_rel and first + count <= nrel
        if info['version'] == 3:
            assert struct.unpack('<II', r[52:60]) == spans[default]
        semantic.update(struct.pack('<I', i+1)); semantic.update(r[:52])
        semantic.update(struct.pack('<I', len(strings[default]))); semantic.update(strings[default])
        keys.append(key); next_rel += count
    assert next_rel == nrel and len(set(keys)) == nlex
    unresolved = conditional = named = 0
    for i in range(nrel):
        r = record(b, info, 1, i)
        selector, target, condition, ck, availability, admission, preferred = struct.unpack('<IIIBBBB', r)
        assert selector < nsel and ck <= 1 and admission <= 1 and preferred <= 1
        assert availability and not availability & ~7
        assert target < nstr or admission == 1 and target == 0xffffffff
        assert (not ck and condition == 0xffffffff) or ck and condition < nstr
        unresolved += admission == 1; conditional += ck == 1; named += 1
        semantic.update(r)
    for i in range(nsel):
        r = record(b, info, 2, i); name, basis = struct.unpack('<II', r)
        assert name < nstr and (basis < nstr or basis == 0xffffffff)
        semantic.update(r)
    for i, span in enumerate(spans):
        semantic.update(struct.pack('<II', *span)); semantic.update(strings[i])
    previous = None
    for i in range(nlex):
        ref = u32(record(b, info, 4, i)); assert 1 <= ref <= nlex
        key = keys[ref-1]; assert previous is None or previous < key; previous = key
        semantic.update(struct.pack('<I', ref))
    return {'all_runtime_refs': nlex, 'all_relations': nrel, 'all_selectors': nsel,
            'all_strings': nstr, 'named_relations_preserved': named,
            'unresolved_relations': unresolved, 'conditional_relations': conditional,
            'maximum_utf8_bytes': max(map(len, strings)),
            'all_record_semantic_sha256': semantic.hexdigest()}


def make_layout(old, info):
    page = info['page_bytes']; sections, at = [], 200
    for s, (_, count, width, _) in enumerate(info['sections']):
        width = 60 if s == 0 else width
        per = (page - 16) // width; pages = (count + per - 1) // per
        sections.append((at, count, width, pages)); at += pages*page
    out = bytearray(at + info['payload_bytes']); out[:200] = old[:200]
    struct.pack_into('<II', out, 0, 0x33485348, 3)
    struct.pack_into('<I', out, 40, 0x33444853)
    struct.pack_into('<Q', out, 32, len(out)-40)
    struct.pack_into('<QQ', out, 184, at, info['payload_bytes'])
    for s, (offset, count, width, pages) in enumerate(sections):
        struct.pack_into('<Q4I', out, 64+24*s, offset, count, width, pages, 0)
        per = (page-16)//width
        for p in range(pages):
            struct.pack_into('<4I', out, offset+p*page, 0x33504753, s, p, min(per, count-p*per))
        for i in range(count):
            r = record(old, info, s, i)
            if s == 0:
                r += record(old, info, 3, u32(r, 36))
            dst = offset + (i//per)*page + 16 + (i%per)*width
            out[dst:dst+width] = r
    out[at:] = old[info['payload_offset']:]
    struct.pack_into('<Q', out, 16, fnv(out[40:], fnv(out[8:16])))
    struct.pack_into('<Q', out, 24, fnv(out[40:]))
    return bytes(out)


def scheme_refs(b, name, max_ref):
    assert u32(b) == 0x324d4353 and u32(b, 4) == 2 and len(b)-40 == u64(b, 32)
    assert fnv(b[40:]) == u64(b, 24)
    magic, page, pages, root, depth, count, leaves, namebytes = struct.unpack_from('<8I', b, 40)
    assert magic == 0x32584749
    assert b[104:104+namebytes].decode() == name
    begin = 40 + u64(b, 72); refs = set(); seen = 0; digest = hashlib.sha256(); previous = None
    for i in range(leaves):
        p = b[begin+i*page:begin+(i+1)*page]
        assert u32(p) == 0x32475049 and p[4] == 1 and p[5] == 0
        records = struct.unpack_from('<H', p, 6)[0]; at = 16
        for j in range(records):
            n = struct.unpack_from('<H', p, at)[0]; at += 2; code = p[at:at+n]; at += n
            ref, weight, ordinal = struct.unpack_from('<IiI', p, at); at += 12
            assert 1 <= ref <= max_ref and code and (previous is None or previous <= (code, ref))
            previous = (code, ref); refs.add(ref); seen += 1
            digest.update(struct.pack('<I', n)+code+struct.pack('<IiI', ref, weight, ordinal))
        assert at == u32(p, 12)
    assert seen == count
    return refs, {'mappings': count, 'distinct_runtime_refs': len(refs), 'logical_mapping_sha256': digest.hexdigest(),
                  'scheme_body_sha256': sha(b[40:]).hex(), 'tree_depth': depth, 'pages': pages}


def crossing(info, block=512):
    offset, count, width, _ = info['sections'][0]; per = (info['page_bytes']-16)//width
    return sum((offset+(i//per)*info['page_bytes']+16+(i%per)*width)%block+width > block for i in range(count))


