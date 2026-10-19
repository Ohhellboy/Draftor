#!/usr/bin/env python3
"""Publish the one HSH4 short-text prototype from the measured HSH3 image.

No source mutation, no test-code special cases, no new identity, no descriptor
or payload deletion. The retained structural publisher defines the unchanged
SHA-256 proof domains and pin grammar. Only new/actually consumed source bytes
are hashed; untouched order/proof fingerprints are inherited, not recomputed.
"""
from pathlib import Path
import argparse
import collections
import importlib.util
import json
import struct

if not __debug__:
    raise RuntimeError('Publication requires validation; optimized Python is prohibited')

PAGE = 4096
BLOCK = 512
WIDTH = 64
CAPACITY = 10
PER_PAGE = 63
NAMES = ('shared', 'xiaohe', 'full_pinyin', 'wubi86', 'xiaohe_base', 'full_pinyin_base', 'wubi86_base')
EXT = {n: '.hns' if n == 'shared' else '.hbo' if n.endswith('_base') else '.hnm' for n in NAMES}


def inherited(path):
    spec = importlib.util.spec_from_file_location('frozen_structural_resources', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def per_page(section, width, version):
    return PER_PAGE if version == 4 and section == 0 else (PAGE - 16) // width


def record_offset(info, section, index):
    off, count, width, _ = info['sections'][section]
    assert 0 <= index < count
    per = per_page(section, width, info['version'])
    page, local = divmod(index, per)
    within = 16 + local * width
    if info['version'] == 4 and section == 0 and local >= 7:
        within = 512 + (local - 7) * 64
    return off + page * PAGE + within


def record(data, info, section, index):
    off = record_offset(info, section, index)
    return data[off:off + info['sections'][section][2]]


def parse_shared(data, g):
    if g.u32(data, 4) == 3:
        return g.parse_shared(data)
    assert g.u32(data) == 0x34485348 and g.u32(data, 4) == 4
    assert g.u32(data, 40) == 0x34444853
    assert g.u32(data, 44) == 160 and g.u32(data, 48) == 1
    assert g.u32(data, 52) == 32 and g.u32(data, 56) == PAGE and g.u32(data, 60) == 5
    assert len(data) - 40 == g.u64(data, 32)
    assert not any(data[200:512])
    sections, expected = [], 512
    widths = (64, 16, 8, 8, 4)
    for section in range(5):
        off, count, width, pages, reserved = struct.unpack_from('<Q4I', data, 64 + 24 * section)
        per = per_page(section, width, 4)
        assert off == expected and off % BLOCK == 0 and width == widths[section] and reserved == 0
        assert pages == (count + per - 1) // per
        sections.append((off, count, width, pages))
        expected += pages * PAGE
    payload_offset, payload_bytes = struct.unpack_from('<QQ', data, 184)
    assert payload_offset == expected and payload_offset + payload_bytes == len(data)
    info = {'version': 4, 'page_bytes': PAGE, 'sections': sections,
            'payload_offset': payload_offset, 'payload_bytes': payload_bytes,
            'namespace': g.u64(data, 8), 'shared_bundle_id': g.u64(data, 16)}
    for section, (off, count, width, pages) in enumerate(sections):
        per = per_page(section, width, 4)
        for p in range(pages):
            n = min(per, count - p * per)
            start = off + p * PAGE
            assert struct.unpack_from('<4I', data, start) == (0x34504753, section, p, n)
            used = bytearray(PAGE)
            used[:16] = b'\1' * 16
            for local in range(n):
                at = record_offset(info, section, p * per + local) - start
                assert not any(used[at:at + width]) and at + width <= PAGE
                used[at:at + width] = b'\1' * width
            assert all(mark or value == 0 for mark, value in zip(used, data[start:start + PAGE]))
    return info


def decode_lexeme(raw, g):
    assert len(raw) == WIDTH
    inline_len = raw[49]
    offset = g.u32(raw, 50)
    assert inline_len <= CAPACITY
    if inline_len:
        size = inline_len
        text = raw[54:54 + size]
        text.decode('utf-8', 'strict')
        assert not any(raw[54 + size:64])
    else:
        size = g.u32(raw, 54)
        text = None
        assert size > CAPACITY and not any(raw[58:64])
    return offset, size, text


def make_short(old, oldinfo, g):
    sections, at = [], 512
    for section, (_, count, width, _) in enumerate(oldinfo['sections']):
        if section == 0:
            width = WIDTH
        per = per_page(section, width, 4)
        pages = (count + per - 1) // per
        sections.append((at, count, width, pages))
        at += pages * PAGE
    out = bytearray(at + oldinfo['payload_bytes'])
    out[:200] = old[:200]
    struct.pack_into('<II', out, 0, 0x34485348, 4)
    struct.pack_into('<I', out, 40, 0x34444853)
    struct.pack_into('<Q', out, 32, len(out) - 40)
    struct.pack_into('<QQ', out, 184, at, oldinfo['payload_bytes'])
    info = {'version': 4, 'sections': sections}
    for section, (off, count, width, pages) in enumerate(sections):
        struct.pack_into('<Q4I', out, 64 + 24 * section, off, count, width, pages, 0)
        per = per_page(section, width, 4)
        for p in range(pages):
            struct.pack_into('<4I', out, off + p * PAGE, 0x34504753, section, p, min(per, count - p * per))
        for index in range(count):
            src = record(old, oldinfo, section, index)
            if section == 0:
                dst = bytearray(WIDTH)
                dst[:49] = src[:49]
                payload_off, size = struct.unpack_from('<II', src, 52)
                struct.pack_into('<I', dst, 50, payload_off)
                if size <= CAPACITY:
                    assert size > 0
                    dst[49] = size
                    payload = oldinfo['payload_offset'] + payload_off
                    text = old[payload:payload + size]
                    text.decode('utf-8', 'strict')
                    dst[54:54 + size] = text
                else:
                    struct.pack_into('<I', dst, 54, size)
                src = bytes(dst)
            pos = record_offset(info, section, index)
            out[pos:pos + width] = src
    out[at:] = old[oldinfo['payload_offset']:]
    struct.pack_into('<Q', out, 16, g.fnv(out[40:], g.fnv(out[8:16])))
    struct.pack_into('<Q', out, 24, g.fnv(out[40:]))
    return bytes(out)


def compare_all(old, oi, new, ni, g):
    assert oi['namespace'] == ni['namespace']
    assert [s[1] for s in oi['sections']] == [s[1] for s in ni['sections']]
    assert old[oi['payload_offset']:] == new[ni['payload_offset']:]
    assert g.fnv(new[40:]) == g.u64(new, 24)
    assert g.fnv(new[40:], g.fnv(new[8:16])) == ni['shared_bundle_id']
    distribution = collections.Counter()
    inline_count = inline_bytes = external_count = 0
    crossings = collections.Counter()
    offsets = []
    for section, (_, count, _, _) in enumerate(oi['sections']):
        for index in range(count):
            a, b = record(old, oi, section, index), record(new, ni, section, index)
            if section:
                assert a == b, (section, index)
                continue
            assert a[:49] == b[:49] and a[49:52] == b'\0\0\0', index
            old_off, old_len = struct.unpack_from('<II', a, 52)
            offset, size, inline = decode_lexeme(b, g)
            assert (old_off, old_len) == (offset, size), index
            assert offset + size <= ni['payload_bytes']
            distribution[size] += 1
            text = old[oi['payload_offset'] + offset:oi['payload_offset'] + offset + size]
            if size <= CAPACITY:
                assert inline == text, index
                inline_count += 1
                inline_bytes += size
            else:
                assert inline is None
                external_count += 1
            pos = record_offset(ni, 0, index)
            assert pos // BLOCK == (pos + WIDTH - 1) // BLOCK
            assert (pos - ni['sections'][0][0]) // PAGE == (pos + WIDTH - 1 - ni['sections'][0][0]) // PAGE
            offsets.append(pos % BLOCK)
            crossings['baseline_record_512'] += (record_offset(oi, 0, index) % BLOCK + 60 > BLOCK)
            crossings['prototype_record_512'] += (pos % BLOCK + WIDTH > BLOCK)
            crossings['baseline_default_payload_512'] += ((oi['payload_offset'] + offset) % BLOCK + size > BLOCK)
            crossings['prototype_external_default_payload_512'] += (size > CAPACITY and (ni['payload_offset'] + offset) % BLOCK + size > BLOCK)
    return {'default_utf8_byte_distribution': dict(sorted(distribution.items())),
            'inline_records': inline_count, 'external_records': external_count,
            'inline_fraction': inline_count / oi['sections'][0][1],
            'inline_complete_text_copy_bytes': inline_bytes,
            'all_default_text_bytes': sum(k * v for k, v in distribution.items()),
            'record_512_offsets': dict(sorted(collections.Counter(offsets).items())),
            'crossings': dict(crossings)}


def transport_shape(size):
    blocks = (size + 511) // 512
    depth = (blocks - 1).bit_length()
    return {'logical_bytes': size, 'data_blocks': blocks, 'proof_depth': depth,
            'packet_bytes': 512 + 32 * depth, 'transport_bytes': blocks * (512 + 32 * depth),
            'transport_data_with_tail_padding_bytes': blocks * 512,
            'transport_proof_bytes': blocks * 32 * depth,
            'last_data_block_padding_bytes': blocks * 512 - size}


