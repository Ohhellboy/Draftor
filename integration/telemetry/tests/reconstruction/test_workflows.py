import copy
import unittest
from support import *

class WorkflowTests(unittest.TestCase):
    def test_h01_full_chain_and_latency_endpoints(self):
        s=session('H01')
        self.assertEqual(s['status'],'RELIABLE'); self.assertTrue(s['history_complete'])
        composition=s['compositions']['1']
        self.assertEqual(len(composition['selections']),1)
        self.assertEqual(composition['selections']['1']['missing'],[])
        self.assertEqual(len(s['document_mutations']),1)
        expected={'input_to_route':2,'input_to_range':1,'range_to_candidate':1,'candidate_to_view':1,
                  'view_to_display_ack':2,'input_to_display_ack':2,'selection_request_to_result':1,
                  'selection_result_to_next_display_ack':1,'document_to_view':1,'document_to_display_ack':1}
        for name,count in expected.items(): self.assertEqual(s['metrics']['latencies'][name]['count'],count,name)
        self.assertEqual(s['metrics']['latencies']['input_to_route']['mean_us'],10)
        self.assertEqual(s['metrics']['latencies']['view_to_display_ack']['mean_us'],20)
        self.assertEqual(s['metrics']['latencies']['document_to_view']['mean_us'],20)

    def test_h02_growth(self):
        c=session('H02')['compositions']['1']
        self.assertEqual(len(c['candidate_snapshots']),4)
        self.assertEqual([e['fields']['raw_length'] for e in c['revisions']],[2,3,4,5,5])

    def test_h03_partial_selection_next_ime(self):
        s=session('H03'); self.assertEqual(len(s['compositions']['1']['selections']),2)
        self.assertEqual(len(s['document_mutations']),1)
        for name in ('selection_result_to_next_range','selection_result_to_next_candidate'):
            self.assertEqual(s['metrics']['latencies'][name]['count'],1)
        self.assertEqual(s['metrics']['accepted_selection_count_per_composition'],{'1':2})

    def test_h04_digit_is_routed_selection(self):
        s=session('H04'); action=s['actions']['2']
        self.assertEqual(action['routes'][0]['fields'],{'route_target':2,'semantic_action':2})
        self.assertEqual(s['document_mutations'][0]['fields']['inserted_bytes'],6)
        self.assertEqual(s['metrics']['selected_rank_distribution'],{'1':1})

    def test_h05_window_crossing(self):
        e=session('H05')['compositions']['1']['navigation'][0]
        self.assertEqual(e['fields']['global_index'],5); self.assertEqual(e['fields']['window_start'],5)
        self.assertEqual(session('H05')['metrics']['candidate_navigation_count'],1)

    def test_h06_h08_backspace_separation(self):
        self.assertEqual(session('H06')['metrics']['composition_backspace_count'],1)
        self.assertEqual(session('H06')['metrics']['document_backspace_count'],0)
        self.assertEqual(session('H08')['metrics']['composition_backspace_count'],0)
        self.assertEqual(session('H08')['metrics']['document_backspace_count'],1)
        self.assertEqual(len(session('H08')['compositions']),0)
        self.assertEqual(len(session('H08')['document_mutations']),2)

    def test_h07_reopen_snapshot_staleness(self):
        s=session('H07'); self.assertEqual(s['metrics']['reopen_count'],1)
        selections=list(s['compositions']['1']['selections'].values())
        self.assertEqual([x['result']['fields']['outcome'] for x in selections],[1,2,1])
        self.assertNotEqual(selections[1]['request']['candidate_snapshot_id'],selections[2]['request']['candidate_snapshot_id'])
        self.assertEqual(s['metrics']['selected_rank_distribution'],{'1':2})

    def test_h09_h10_no_candidate_path(self):
        for name,action in [('H09',9),('H10',10)]:
            s=session(name)
            self.assertEqual(len(s['compositions']),0)
            self.assertEqual(s['actions']['1']['routes'][0]['fields']['semantic_action'],action)
            self.assertEqual(len(s['document_mutations']),1)
            self.assertEqual(s['metrics']['latencies']['input_to_range']['count'],0)

    def test_h11_file_overlap_not_ime(self):
        s=session('H11'); operation=s['file_operations']['1']
        self.assertEqual(operation['missing'],[])
        range_ready=next(e for e in s['events'] if e['event_type']=='IME_RANGE_READY')
        self.assertLess(operation['begin']['device_monotonic_us'],range_ready['device_monotonic_us'])
        self.assertGreater(operation['end']['device_monotonic_us'],range_ready['device_monotonic_us'])
        self.assertEqual(s['metrics']['latencies']['range_to_candidate']['mean_us'],10)

    def test_vertical_navigation_semantic_separation(self):
        a=session('V01'); b=session('V02')
        ra=a['actions']['1']['routes'][0]; rb=b['actions']['2']['routes'][0]
        self.assertEqual(ra['fields'],{'route_target':3,'semantic_action':4})
        self.assertEqual(rb['fields'],{'route_target':2,'semantic_action':3})
        editor=next(e for e in a['events'] if e['event_type']=='EDITOR_ACTION')
        self.assertEqual(editor['fields']['spatial_direction'],1)
        self.assertNotEqual(editor['fields']['cursor_before'],editor['fields']['cursor_after'])

    def test_v03_mode_change_preserves_business_state(self):
        s=session('V03'); change=next(e for e in s['events'] if e['event_type']=='STATE_CHANGE')
        previous=s['events'][s['events'].index(change)-1]
        for key in ('composition_id','composition_revision','candidate_snapshot_id','document_revision'):
            self.assertEqual(change[key],previous[key])
        self.assertEqual(change['fields']['writing_mode'],2)
        self.assertGreater(change['layout_revision'],previous['layout_revision'])

    def test_missing_route_never_inferred(self):
        records=events('H01'); records=[e for e in records if not (e['event_type']=='ROUTE_DECISION' and e['action_id']==1)]
        s=next(iter(summary(records)['sessions'].values()))
        self.assertEqual(s['actions']['1']['routes'],[])
        self.assertIn('ROUTE_DECISION',s['actions']['1']['missing'])
        self.assertFalse(s['history_complete'])

    def test_actions_and_compositions_are_shared_references(self):
        s=session('H02')
        self.assertGreater(len(s['compositions']['1']['action_ids']),1)
        self.assertTrue(all(a['composition_ids']==[1] for a in s['actions'].values()))

    def test_missing_ime_and_display_counterparts_explicit(self):
        records=events('H01')
        # Retain contiguous sequence numbers to isolate semantic incompleteness
        # from transport loss (e.g. a misconfigured future integration probe).
        records=[e for e in records if e['event_type'] not in ('IME_PLAN_BEGIN','DISPLAY_SUBMIT')]
        for index,event in enumerate(records,1): event['event_seq']=index
        s=next(iter(summary(records)['sessions'].values()))
        kinds={item['kind'] for item in s['missing'] if isinstance(item,dict)}
        self.assertIn('IME_PLAN_BEGIN_not_observed_before',kinds)
        self.assertIn('DISPLAY_SUBMIT_not_observed_before',kinds)
        self.assertEqual(s['metrics']['latencies']['view_to_display_ack']['count'],0)

    def test_snapshot_restores_structure_without_fabricating_history(self):
        records=events('H01')
        snapshot=copy.deepcopy(records[-1]); snapshot['event_type']='SYNC_SNAPSHOT'
        s=next(iter(summary([snapshot])['sessions'].values()))
        self.assertEqual(s['status'],'RELIABLE'); self.assertFalse(s['history_complete'])
        self.assertEqual(s['state']['document_revision'],snapshot['document_revision'])
        self.assertEqual(s['compositions']['1']['candidate_snapshots'],{})
        self.assertIn({'kind':'composition_begin_missing'},s['compositions']['1']['missing'])

    def test_no_latency_spans_gap_and_recovery(self):
        records=events('H01')
        snapshot=copy.deepcopy(records[0]); snapshot['event_type']='SYNC_SNAPSHOT'
        # Input before loss, route after recovery: same action ID is insufficient.
        snapshot['event_seq']=4; snapshot['device_monotonic_us']=25
        tail=copy.deepcopy(records[2:])
        for i,event in enumerate(tail,5): event['event_seq']=i
        s=next(iter(summary(records[:2]+[snapshot]+tail)['sessions'].values()))
        samples=s['metrics']['latencies']['input_to_route']['samples']
        self.assertEqual(len(samples),1)
        self.assertNotEqual(samples[0]['start_seq'],2)
