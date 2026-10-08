import random
import struct
import unittest
from support import *
from draftor_telemetry.decoder import decode_packet
from draftor_telemetry.schema import MAX_PACKET_BYTES

class DecoderTests(unittest.TestCase):
    def test_actual_cpp_all_chunk_boundaries(self):
        data=fixture('H16'); expected=decode(data,len(data))
        self.assertGreater(len(expected),20)
        self.assertFalse(any(e['record_type']=='decode_error' for e in expected))
        for size in (1,2,3,7,56,97,4096): self.assertEqual(decode(data,size),expected)

    def test_corrupt_truncated_recover(self):
        records=decode(fixture('H13'))
        errors=[r['error'] for r in records if r['record_type']=='decode_error']
        self.assertIn('crc_mismatch',errors); self.assertIn('truncated_escape',errors)
        self.assertIn('truncated_final_frame',errors)
        self.assertTrue(any(e.get('event_type')=='SYNC_SNAPSHOT' for e in records))
        self.assertEqual(records[-1]['error'],'truncated_final_frame')

    def test_unknown_then_known(self):
        records=decode(fixture('H14'))
        self.assertEqual(records[1]['record_type'],'unknown_event')
        self.assertEqual(records[1]['event_id'],60000)
        self.assertEqual(records[-1]['event_type'],'SYNC_SNAPSHOT')
        self.assertFalse(any(r['record_type']=='decode_error' for r in records))

    def test_crc_version_length_and_detail_validation(self):
        packet=packets(fixture('H16'))[0]
        self.assertEqual(decode_packet(rewrite(packet,2,'<B',2),STAMP)['error'],'unsupported_wire_version')
        self.assertEqual(decode_packet(rewrite(packet,6,'<H',224),STAMP)['error'],'invalid_header')
        self.assertEqual(decode_packet(rewrite(packet,56,'<B',99),STAMP)['error'],'invalid_enum')
        self.assertEqual(decode_packet(rewrite(packet,56+17+12,'<B',97),STAMP)['error'],'invalid_detail_length')
        self.assertEqual(decode_packet(rewrite(packet,3,'<B',0x21),STAMP)['error'],'unexpected_payload_bytes')

    def test_oversize_noise_stays_bounded(self):
        decoder=StreamDecoder(); rng=random.Random(23)
        for _ in range(500):
            decoder.feed(bytes(rng.randrange(256) for _ in range(100)),STAMP)
            self.assertLessEqual(len(decoder.packet),MAX_PACKET_BYTES)
        decoder.feed(b'\xc1'+b'x'*10000,STAMP)
        self.assertTrue(decoder.discard); self.assertEqual(len(decoder.packet),0)
        records=decoder.feed(fixture('H01'),STAMP)
        self.assertEqual(records[-1]['event_type'],'STATE_SNAPSHOT')

    def test_each_truncation_keeps_prior_history(self):
        data=fixture('H01')
        for cut in range(1,len(data),17):
            records=decode(data[:cut])
            self.assertTrue(all('host_received_time' in event for event in records))
            good=[r for r in records if r['record_type']=='event']
            self.assertEqual([r['event_seq'] for r in good],list(range(1,len(good)+1)))

    def test_literal_protocol_golden(self):
        # Header/endian contract independent of schema-generating code.
        packet=packets(fixture('H01'))[0]
        self.assertEqual(packet[:8],bytes.fromhex('4454012101001100'))
        self.assertEqual(packet[8:16],(101).to_bytes(8,'little'))
        self.assertEqual(packet[16:24],(1).to_bytes(8,'little'))
        self.assertEqual(packet[24:32],(10).to_bytes(8,'little'))
        self.assertEqual(len(packet),75)

    def test_every_v1_event_uses_cpp_generated_trace(self):
        from draftor_telemetry.schema import EVENTS
        seen=set()
        for name in ('H12','H16','H11','H08','V03','H05','H07','P02'):
            seen.update(e['event_id'] for e in events(name))
        self.assertEqual(seen,set(EVENTS))

    def test_single_bit_packet_corruption_detected(self):
        packet=packets(fixture('H01'))[0]
        for offset in range(len(packet)):
            for bit in range(8):
                damaged=bytearray(packet); damaged[offset]^=1<<bit
                self.assertEqual(decode_packet(bytes(damaged),STAMP)['record_type'],'decode_error')
