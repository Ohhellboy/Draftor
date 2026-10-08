"""Incremental bounded SLIP decoder. Errors are records, never silent drops."""
import base64
import binascii
import struct
from .schema import (EVENTS, WIDTHS, HEADER_BYTES, MAX_PACKET_BYTES, MAX_PAYLOAD_BYTES,
                     START, END, ESC, ENVELOPE_NAMES, STATE_EVENTS, WIRE, SCHEMA_REVISION, valid_enum)


def error_record(kind, received_time, **extra):
    return dict(record_type='decode_error', error=kind, host_received_time=received_time, **extra)


def decode_packet(packet, received_time):
    if len(packet) < HEADER_BYTES + 2:
        return error_record('short_packet', received_time, packet_bytes=len(packet))
    if packet[:2] != b'DT':
        return error_record('bad_magic', received_time)
    if binascii.crc_hqx(packet[:-2], 0xFFFF) != int.from_bytes(packet[-2:], 'little'):
        return error_record('crc_mismatch', received_time)
    version, flags, event_id, payload_size = struct.unpack_from('<BBHH', packet, 2)
    if version != 1:
        return error_record('unsupported_wire_version', received_time, version=version)
    privacy, revision = flags & 0x0F, flags >> 4
    if revision != SCHEMA_REVISION:
        return error_record('unsupported_schema_revision', received_time, schema_revision=revision)
    if privacy not in (1, 2) or payload_size > MAX_PAYLOAD_BYTES or len(packet) != HEADER_BYTES + payload_size + 2:
        return error_record('invalid_header', received_time)
    envelope = dict(zip(ENVELOPE_NAMES, struct.unpack_from('<QQQIIIIII', packet, 8)))
    result = dict(record_type='event', schema_version=version, schema_revision=revision, privacy='BASIC' if privacy==1 else 'DETAILED',
                  event_id=event_id, host_received_time=received_time, **envelope)
    if event_id not in EVENTS:
        return dict(result, record_type='unknown_event', event_type='UNKNOWN')
    spec = EVENTS[event_id]
    result['event_type'] = spec['name']
    if privacy == 1 and spec['privacy'] == 'DETAILED':
        return error_record('privacy_violation', received_time)
    payload, offset = packet[HEADER_BYTES:-2], 0
    fields = {}
    for field in spec['fields']:
        size = WIDTHS[field['type']]
        if offset + size > len(payload):
            return error_record('short_payload', received_time)
        fields[field['name']] = int.from_bytes(payload[offset:offset+size], 'little')
        offset += size
        if not valid_enum(field, fields[field['name']]):
            return error_record('invalid_enum', received_time, event_type=spec['name'], field=field['name'], enum=field['enum'], value=fields[field['name']])
    if spec['name'] in STATE_EVENTS:
        if (fields['focus'] > fields['raw_length']
                or (fields['candidate_valid'] and not envelope['candidate_snapshot_id'])):
            return error_record('invalid_snapshot', received_time)
    result['fields'] = fields
    if privacy == 2:
        if len(payload) < offset + 14:
            return error_record('short_detail', received_time)
        codepoint, identity = struct.unpack_from('<IQ', payload, offset); offset += 12
        text_length = payload[offset]; offset += 1
        if text_length > WIRE['text_capacity'] or offset + text_length >= len(payload):
            return error_record('invalid_detail_length', received_time)
        text_bytes = payload[offset:offset+text_length]; offset += text_length
        path_length = payload[offset]; offset += 1
        if path_length > WIRE['path_capacity'] or offset + path_length != len(payload):
            return error_record('invalid_detail_length', received_time)
        try:
            text = text_bytes.decode('utf-8')
            path = payload[offset:offset+path_length].decode('utf-8')
        except UnicodeDecodeError:
            return error_record('invalid_detail_utf8', received_time)
        offset += path_length
        result['detail'] = dict(printable_codepoint=codepoint, candidate_identity=identity,
                                text=text, path=path)
    if offset != len(payload):
        return error_record('unexpected_payload_bytes', received_time)
    return result


class StreamDecoder:
    """Record-level mixed mode; START/END cannot occur in valid UTF-8 diagnostics.

    All bytes within a started behavior frame remain binary, even if printable.
    A new START explicitly abandons a truncated frame, preventing diagnostic text
    from hiding CRC/escape/truncation failures. No byte-level writer arbitration.
    """
    def __init__(self, mixed=False):
        self.mixed=mixed
        self.packet=bytearray()
        self.diagnostic=bytearray()
        self.escape=False
        self.discard=False
        self.in_frame=False
        self.unframed_error=False

    def _diagnostic_record(self, received_time):
        data=bytes(self.diagnostic)
        self.diagnostic.clear()
        return dict(record_type='diagnostic',host_received_time=received_time,bytes=len(data),
                    bytes_base64=base64.b64encode(data).decode('ascii'),text=data.decode('utf-8',errors='replace'))

    def feed(self, chunk, received_time):
        records=[]
        for byte in chunk:
            if byte==START:
                if self.diagnostic: records.append(self._diagnostic_record(received_time))
                if self.in_frame:
                    records.append(error_record('truncated_frame',received_time))
                self.packet.clear(); self.escape=False; self.discard=False
                self.in_frame=True; self.unframed_error=False
                continue
            if not self.in_frame:
                if self.mixed:
                    self.diagnostic.append(byte)
                    if len(self.diagnostic)==WIRE['diagnostic_chunk_bytes']:
                        records.append(self._diagnostic_record(received_time))
                elif not self.unframed_error:
                    records.append(error_record('unframed_bytes',received_time))
                    self.unframed_error=True
                continue
            if byte==END:
                if self.escape:
                    records.append(error_record('truncated_escape',received_time))
                elif not self.discard:
                    records.append(decode_packet(bytes(self.packet),received_time))
                self.packet.clear(); self.escape=False; self.discard=False; self.in_frame=False
                continue
            if self.discard:
                continue
            if self.escape:
                self.escape=False
                if byte not in (0xDC,0xDD,0xDE):
                    records.append(error_record('invalid_escape',received_time))
                    self.packet.clear(); self.discard=True
                    continue
                byte={0xDC:END,0xDD:ESC,0xDE:START}[byte]
            elif byte==ESC:
                self.escape=True
                continue
            self.packet.append(byte)
            if len(self.packet)>MAX_PACKET_BYTES:
                records.append(error_record('oversize_frame',received_time))
                self.packet.clear(); self.discard=True
        # Keep timestamps at transport-read granularity and diagnostics bounded;
        # raw base64 preserves UTF-8 bytes even across fragmented text chunks.
        if self.diagnostic: records.append(self._diagnostic_record(received_time))
        return records

    def finish(self, received_time, reason='input_eof'):
        records=[]
        if self.diagnostic: records.append(self._diagnostic_record(received_time))
        if self.in_frame:
            records.append(error_record('truncated_final_frame',received_time,reason=reason))
        self.packet.clear(); self.escape=False; self.discard=False; self.in_frame=False; self.unframed_error=False
        return records
