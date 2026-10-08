#!/usr/bin/env python3
"""Independent stdlib SHA-256 oracle for final-byte verification sidecars.

This is NOT a second semantic validator or a trust-source importer. It verifies
the publisher's Merkle format independently of the C++ implementation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def check(resource_dir, release_dir):
    release = json.loads((release_dir / 'release_manifest.json').read_text())
    assert release['verification_index_format'] in (1, 2)
    assert release['verification_rule_version'] == 1
    results = []
    for item in release['files']:
        packed = item.get('verification_format', 1) == 2
        block = 512 if packed else 1024
        directory = release_dir if item['role'] == 'base_order' else resource_dir
        data = (directory / item['source_file']).read_bytes()
        assert len(data) == item['file_bytes']
        assert hashlib.sha256(data).hexdigest() == item['file_sha256']
        if packed:
            packets = (release_dir / item['index_file']).read_bytes()
            assert hashlib.sha256(packets).hexdigest() == item['index_sha256']
            count = (len(data) + 511) // 512
            depth = (count - 1).bit_length()
            stride = 512 + 32 * depth
            assert stride == item['packet_bytes'] and len(packets) == count * stride == item['index_bytes']
            for number in range(count):
                packet = packets[number * stride:(number + 1) * stride]
                original = data[number * 512:(number + 1) * 512]
                assert packet[:len(original)] == original
                assert not any(packet[len(original):512])
                value = hashlib.sha256(b'\2' + struct.pack('<QI', number, len(original)) + original).digest()
                for level in range(depth):
                    sibling = packet[512 + 32 * level:544 + 32 * level]
                    pair = sibling + value if (number >> level) & 1 else value + sibling
                    value = hashlib.sha256(b'\3' + struct.pack('<IQ', level, number >> (level + 1)) + pair).digest()
                assert value.hex() == item['merkle_root'], (item['logical_name'], number)
            results.append({'file':item['source_file'], 'bytes':len(data), 'transport_bytes':len(packets),
                            'depth':depth, 'packets':count, 'result':'PASS'})
            continue
        level = b''.join(hashlib.sha256(b'\0' + struct.pack('<QI', i // block, len(data[i:i + block]))
                       + data[i:i + block]).digest() for i in range(0, len(data), block))
        expected = bytearray()
        depth = 0
        while True:
            level += bytes((-len(level)) % 1024)
            expected.extend(level)
            parents = [hashlib.sha256(b'\1' + struct.pack('<II', depth, i // 1024)
                                     + level[i:i + 1024]).digest()
                       for i in range(0, len(level), 1024)]
            depth += 1
            if len(parents) == 1:
                root = parents[0].hex()
                break
            level = b''.join(parents)
        actual = (release_dir / item['index_file']).read_bytes()
        assert actual == expected, item['logical_name'] + ' index bytes differ'
        assert hashlib.sha256(actual).hexdigest() == item['index_sha256']
        assert root == item['merkle_root'] and len(actual) == item['index_bytes']
        results.append({'file': item['source_file'], 'bytes': len(data),
                        'index_bytes': len(actual), 'levels': depth, 'result': 'PASS'})
    print(json.dumps({'scope': 'independent SHA/index oracle; not semantic qualification',
                      'files': results}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('resources', type=Path)
    parser.add_argument('release', type=Path)
    args = parser.parse_args()
    check(args.resources, args.release)
