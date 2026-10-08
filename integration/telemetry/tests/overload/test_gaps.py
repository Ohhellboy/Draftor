import copy
import unittest
from support import *

class GapTests(unittest.TestCase):
    def test_h12_gap_then_snapshot(self):
        s=session('H12'); self.assertFalse(s['history_complete']); self.assertEqual(s['status'],'RELIABLE')
        gaps=[g for g in s['gaps'] if g['kind']=='event_seq_gap']
        self.assertGreaterEqual(len(gaps),1)
        self.assertTrue(all(g['recovered_by_seq'] is not None for g in gaps))
        self.assertGreater(s['health'][0]['fields']['dropped_queue'],0)
        self.assertFalse(s['health'][0]['reliable'])
        seq=[e['event_seq'] for e in s['events']]
        self.assertEqual(seq,sorted(set(seq)))
        self.assertEqual(s['metrics']['latencies']['input_to_range']['count'],1)

    def test_corruption_requires_sync_and_tail_is_unreliable(self):
        s=session('H13'); self.assertEqual(s['status'],'NEEDS_RESYNC')
        sync=next(e for e in s['events'] if e['event_type']=='SYNC_SNAPSHOT')
        self.assertTrue(sync['reliable']); self.assertFalse(s['history_complete'])
        self.assertFalse(next(e for e in s['events'] if e['event_type']=='DOCUMENT_MUTATION')['reliable'])

    def test_future_event_conservative_reliability(self):
        s=session('H14'); self.assertEqual(s['status'],'RELIABLE'); self.assertFalse(s['history_complete'])
        self.assertTrue(any(g['kind']=='unknown_event_semantics' for g in s['gaps']))
        self.assertEqual(s['metrics']['latencies']['input_to_range']['count'],0)

    def test_duplicate_out_of_order_snapshot_cannot_repair(self):
        records=events('H01'); duplicate=copy.deepcopy(records[0]); duplicate['event_type']='SYNC_SNAPSHOT'
        s=next(iter(summary(records+[duplicate])['sessions'].values()))
        self.assertEqual(s['status'],'NEEDS_RESYNC')
        self.assertEqual(s['last_seq'],records[-1]['event_seq'])

    def test_clock_regression_suppresses_latency(self):
        records=events('H01'); records[3]['device_monotonic_us']=0
        s=next(iter(summary(records)['sessions'].values()))
        self.assertEqual(s['status'],'NEEDS_RESYNC')
        self.assertTrue(any(g['kind']=='device_time_regression' for g in s['gaps']))

    def test_transport_gap_has_no_fabricated_events(self):
        records=events('H01'); inserted={'record_type':'transport_gap','error':'serial_disconnect','host_received_time':STAMP}
        s=next(iter(summary(records[:8]+[inserted]+records[8:])['sessions'].values()))
        self.assertEqual(len(s['events']),len(records)); self.assertEqual(s['status'],'NEEDS_RESYNC')
        self.assertEqual(s['metrics']['latencies']['view_to_display_ack']['count'],0)

    def test_duplicate_sequence_cannot_replace_action_evidence(self):
        records=events('H01')
        duplicate=copy.deepcopy(records[0])
        duplicate['event_seq']=records[2]['event_seq']
        s=next(iter(summary(records+[duplicate])['sessions'].values()))
        self.assertNotIn('ROUTE_DECISION',s['actions']['1']['missing'])
        self.assertEqual(s['status'],'NEEDS_RESYNC')
