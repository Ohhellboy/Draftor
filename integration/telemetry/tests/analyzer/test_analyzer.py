import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from support import ROOT, summary, decode, fixture
from draftor_telemetry.analyzer import Options, iter_analysis
from draftor_telemetry.analyzer.analysis_types import Reason
from fixtures import Trace, reconstruct, cases


def analyze(trace):
    return list(iter_analysis(reconstruct(trace.events)))


def rows(result, kind, status=None):
    return [r for r in result if (r['evidence_type'] or r['diagnostic_type']) == kind
            and (status is None or r['status'] == status)]


class AnalyzerTests(unittest.TestCase):
    def test_first_candidate_has_normal_commit_without_rejection(self):
        result = analyze(cases()['first_candidate'])
        self.assertEqual(len(rows(result, 'NORMAL_CANDIDATE_COMMIT', 'CONFIRMED')), 1)
        self.assertFalse(rows(result, 'TOP1_REJECTED'))
        self.assertFalse(rows(result, 'PROBABLE_RANKING_ISSUE'))

    def test_nonfirst_alone_is_not_ranking_issue(self):
        result = analyze(cases()['second_candidate'])
        selected = rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'CONFIRMED')[0]
        self.assertEqual(selected['metrics']['local_rank'], 2)
        self.assertIsNone(selected['metrics']['global_ordinal'])
        self.assertEqual(len(rows(result, 'TOP1_REJECTED', 'CONFIRMED')), 1)
        self.assertEqual(rows(result, 'PROBABLE_RANKING_ISSUE')[0]['status'], 'INSUFFICIENT_EVIDENCE')

    def test_browsed_stable_third_candidate_is_only_probable(self):
        result = analyze(cases()['third_candidate_browsed'])
        hypothesis = rows(result, 'PROBABLE_RANKING_ISSUE')[0]
        self.assertEqual((hypothesis['status'], hypothesis['confidence']), ('PROBABLE', 'MEDIUM'))
        self.assertGreaterEqual(len(hypothesis['supporting_evidence']), 4)
        self.assertIn('SEGMENTATION_UNAVAILABLE', hypothesis['missing_evidence'])

    def test_window_is_not_page_or_global_ordinal(self):
        result = analyze(cases()['window_navigation_selection'])
        local = rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'CONFIRMED')[0]['metrics']
        self.assertEqual(local['local_rank'], 2)
        for key in ('global_ordinal', 'candidate_page', 'query_generation'):
            self.assertIsNone(local[key])
        page = rows(result, 'PAGE_NAVIGATION_OCCURRED')[0]
        self.assertEqual(page['status'], 'INSUFFICIENT_EVIDENCE')
        self.assertEqual(page['metrics']['window_forward_count'], 1)
        self.assertIsNone(page['metrics']['page_forward_count'])
        self.assertTrue(page['metrics']['selection_after_navigation'])
        kinds = {reconstruct(cases()['window_navigation_selection'].events)['sessions']['101']['events'][int(r['reconstruction_ref'].split('/')[-1])]['event_type'] for r in page['source_refs']}
        self.assertTrue({'CANDIDATE_DATA_READY', 'CANDIDATE_NAVIGATION', 'SELECTION_REQUEST', 'SELECTION_RESULT'} <= kinds)

    def test_backward_window_and_boundary_hit(self):
        trace = Trace().begin().query().navigate(5, 6).navigate(0, 1, 6).navigate(0, 1, 1, 2).select(1)
        m = rows(analyze(trace), 'PAGE_NAVIGATION_OCCURRED')[0]['metrics']
        self.assertEqual((m['window_forward_count'], m['window_backward_count'], m['boundary_hit_count']), (1, 1, 1))

    def test_revision_prevents_ranking_hypothesis(self):
        result = analyze(cases()['raw_revision_then_commit'])
        self.assertEqual(rows(result, 'RAW_REVISED', 'CONFIRMED')[0]['metrics']['revision_kind'], 'BACKSPACE')
        self.assertFalse(rows(result, 'PROBABLE_RANKING_ISSUE', 'PROBABLE'))
        self.assertTrue(rows(result, 'SEGMENTATION_CHANGED', 'INSUFFICIENT_EVIDENCE'))

    def test_delete_and_rewrite_require_document_chain(self):
        result = analyze(cases()['post_commit_rewrite'])
        self.assertEqual(len(rows(result, 'POST_COMMIT_DELETE', 'CONFIRMED')), 1)
        self.assertEqual(len(rows(result, 'POST_COMMIT_REWRITE', 'CONFIRMED')), 1)
        self.assertFalse(any(r['diagnostic_type'] and r['status'] == 'PROBABLE' for r in result))

    def test_unrelated_delete_not_post_commit_delete(self):
        result = analyze(Trace().begin().query().select().delete(10, 12))
        self.assertFalse(rows(result, 'POST_COMMIT_DELETE'))

    def test_delete_after_timeout_not_reported(self):
        trace = cases()['post_commit_delete']
        for e in trace.events:
            if not e['composition_id'] and e['event_seq'] > 2:
                e['device_monotonic_us'] += 3_000_000
        self.assertFalse(rows(analyze(trace), 'POST_COMMIT_DELETE'))

    def test_partial_delete_not_whole_rewrite(self):
        result = analyze(Trace().begin().query().select().delete(3, 6).direct(3))
        self.assertTrue(rows(result, 'POST_COMMIT_DELETE'))
        self.assertFalse(rows(result, 'POST_COMMIT_REWRITE'))

    def test_document_revision_break_prevents_delete_pairing(self):
        trace = cases()['post_commit_delete']
        trace.events[-1]['fields']['revision_before'] = 99
        self.assertFalse(rows(analyze(trace), 'POST_COMMIT_DELETE'))

    def test_explicit_abandon_and_unknown_end(self):
        result = analyze(cases()['composition_abandon'])
        aborted = rows(result, 'COMPOSITION_ABORTED', 'CONFIRMED')[0]
        self.assertEqual(aborted['metrics']['termination_reason'], 'CANCELLED')
        unknown = rows(analyze(Trace().begin().query().abort(0)), 'COMPOSITION_ABORTED')[0]
        self.assertEqual(unknown['status'], 'INSUFFICIENT_EVIDENCE')
        self.assertEqual(unknown['metrics']['termination_reason'], 'UNKNOWN')
        self.assertFalse(rows(analyze(Trace().begin().query()), 'COMPOSITION_ABORTED'))

    def test_segmentation_fixture_cannot_invent_missing_spans(self):
        result = analyze(cases()['segmentation_unobservable'])
        self.assertTrue(rows(result, 'SEGMENTATION_CHANGED', 'INSUFFICIENT_EVIDENCE'))
        self.assertTrue(rows(result, 'PROBABLE_SEGMENTATION_ISSUE', 'INSUFFICIENT_EVIDENCE'))
        self.assertFalse(rows(result, 'SEGMENTATION_CHANGED', 'CONFIRMED'))

    def test_route_semantics_and_probable_not_confirmed_causation(self):
        result = analyze(cases()['route_change'])
        route = rows(result, 'INPUT_ROUTE_CHANGED', 'CONFIRMED')[0]
        self.assertEqual((route['metrics']['from_route'], route['metrics']['to_route']), ('IME', 'Editor'))
        self.assertTrue(rows(result, 'PROBABLE_INPUT_REROUTE', 'PROBABLE'))
        self.assertFalse(rows(result, 'PROBABLE_LEXICON_MISS', 'PROBABLE'))

    def test_unknown_route_and_candidate_view_not_input_reroute(self):
        trace = cases()['route_change']
        next(e for e in reversed(trace.events) if e['event_type'] == 'ROUTE_DECISION')['fields']['route_target'] = 0
        self.assertFalse(rows(analyze(trace), 'INPUT_ROUTE_CHANGED'))
        self.assertFalse(rows(analyze(cases()['third_candidate_browsed']), 'INPUT_ROUTE_CHANGED'))

    def test_candidate_change_not_instability(self):
        result = analyze(cases()['candidate_result_change'])
        self.assertTrue(rows(result, 'CANDIDATE_RESULT_CHANGED', 'CONFIRMED'))
        self.assertTrue(rows(result, 'PROBABLE_CANDIDATE_INSTABILITY', 'INSUFFICIENT_EVIDENCE'))
        self.assertFalse(rows(result, 'PROBABLE_CANDIDATE_INSTABILITY', 'PROBABLE'))
        self.assertTrue(rows(result, 'QUERY_REPEATED', 'INSUFFICIENT_EVIDENCE'))

    def test_privacy_and_changed_mind_negative(self):
        for name in ('basic_privacy_insufficient', 'detailed_lexicon_still_insufficient', 'changed_mind'):
            result = analyze(cases()[name])
            lexicon = rows(result, 'PROBABLE_LEXICON_MISS')[0]
            self.assertEqual(lexicon['status'], 'INSUFFICIENT_EVIDENCE')
            self.assertEqual('INSUFFICIENT_EVIDENCE_DUE_TO_PRIVACY_PROFILE' in lexicon['missing_evidence'], name.startswith('basic'))
            self.assertNotIn('秘密', json.dumps(result, ensure_ascii=False))

    def test_off_empty_is_not_evidence_of_no_input(self):
        result = list(iter_analysis(summary([])))
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]['status'], 'INSUFFICIENT_EVIDENCE')
        self.assertIn('NO_EVENTS', result[0]['reason_codes'])

    def test_missing_view_and_rejected_selection_do_not_reject_top1(self):
        self.assertFalse(rows(analyze(Trace().begin().query(view=False).select(1)), 'TOP1_REJECTED', 'CONFIRMED'))
        self.assertFalse(rows(analyze(Trace().begin().query(ack=False).select(1)), 'TOP1_REJECTED', 'CONFIRMED'))
        self.assertFalse(rows(analyze(Trace().begin().query().select(1, accepted=False)), 'NONFIRST_CANDIDATE_SELECTED', 'CONFIRMED'))

    def test_transport_loss_blocks_correlated_facts(self):
        trace = cases()['third_candidate_browsed']
        trace.events.insert(7, dict(record_type='transport_gap', error='test_loss'))
        result = analyze(trace)
        self.assertFalse(rows(result, 'TOP1_REJECTED', 'CONFIRMED'))
        self.assertFalse(rows(result, 'PROBABLE_RANKING_ISSUE', 'PROBABLE'))
        self.assertFalse(rows(result, 'NORMAL_CANDIDATE_COMMIT', 'CONFIRMED'))

    def test_recovery_does_not_repair_prior_selection_chain(self):
        trace = Trace().begin().query()
        trace.events.append(dict(record_type='transport_gap', error='lost'))
        trace.emit('SYNC_SNAPSHOT', writing_mode=1, base_input_mode=1, input_scheme=1,
                   edit_context=2, raw_length=2, candidate_valid=1)
        trace.select(1)
        result = analyze(trace)
        self.assertFalse(rows(result, 'TOP1_REJECTED', 'CONFIRMED'))
        self.assertFalse(rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'CONFIRMED'))

    def test_missing_result_and_duplicate_identity_are_insufficient(self):
        for duplicate in (False, True):
            trace = cases()['second_candidate']
            index = next(i for i, e in enumerate(trace.events) if e['event_type'] == 'SELECTION_RESULT')
            if duplicate:
                trace.events.insert(index, copy.deepcopy(trace.events[index]))
            else:
                trace.events.pop(index)
            for number, e in enumerate(trace.events, 1):
                e['event_seq'] = number
                e['device_monotonic_us'] = number*10_000
            result = analyze(trace)
            self.assertFalse(rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'CONFIRMED'))
            self.assertTrue(rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'INSUFFICIENT_EVIDENCE'))

    def test_revision_between_ready_and_selection_is_not_top1_rejection(self):
        result = analyze(Trace().begin().query().revise().select(1))
        self.assertFalse(rows(result, 'TOP1_REJECTED', 'CONFIRMED'))

    def test_unknown_rank_or_inconsistent_slot_is_not_local_rank(self):
        for rank in (0, 0xffffffff, 20):
            trace = cases()['second_candidate']
            next(e for e in trace.events if e['event_type'] == 'SELECTION_REQUEST')['fields']['rank'] = rank
            result = analyze(trace)
            self.assertFalse(rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'CONFIRMED'))
            self.assertTrue(rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'INSUFFICIENT_EVIDENCE'))

    def test_detail_length_checked_and_basic_detail_ignored(self):
        detailed = Trace('DETAILED').begin().query().revise('na', cause=1)
        result = analyze(detailed)
        self.assertEqual(rows(result, 'RAW_REVISED', 'CONFIRMED')[0]['metrics']['revision_kind'], 'RAW_REPLACEMENT')
        detailed.events[-1]['detail']['text'] = 'n'  # Truncated, not a full raw value.
        self.assertFalse(rows(analyze(detailed), 'RAW_REVISED', 'CONFIRMED'))
        for e in detailed.events:
            e['privacy'] = 'BASIC'  # Even a malformed BASIC input must not use attached body text.
        self.assertFalse(rows(analyze(detailed), 'RAW_REVISED', 'CONFIRMED'))

    def test_file_boundary_blocks_spatial_pairing(self):
        trace = Trace().begin().query().select()
        trace.emit('FILE_OP_BEGIN', file_operation_id=1, operation=2)
        trace.delete()
        self.assertFalse(rows(analyze(trace), 'POST_COMMIT_DELETE'))

    def test_rewrite_different_position_is_not_related(self):
        result = analyze(Trace().begin().query().select().delete().direct(10))
        self.assertTrue(rows(result, 'POST_COMMIT_DELETE'))
        self.assertFalse(rows(result, 'POST_COMMIT_REWRITE'))

    def test_completed_without_selection_is_not_abandonment(self):
        trace = Trace().begin().query()
        trace.mutate(1, 0, 0, 6, 0)
        trace.emit('COMPOSITION_END', outcome=1)
        result = analyze(trace)
        self.assertFalse(rows(result, 'COMPOSITION_ABORTED'))
        self.assertFalse(rows(result, 'NORMAL_CANDIDATE_COMMIT'))

    def test_activity_or_selection_after_end_does_not_complete_lifecycle(self):
        self.assertFalse(rows(analyze(Trace().begin().abort().query()), 'COMPOSITION_ABORTED', 'CONFIRMED'))
        result = analyze(Trace().begin().query().abort().select(1))
        self.assertFalse(rows(result, 'TOP1_REJECTED', 'CONFIRMED'))
        self.assertFalse(rows(result, 'NONFIRST_CANDIDATE_SELECTED', 'CONFIRMED'))

    def test_no_runtime_identity_does_not_become_candidate_instability(self):
        trace = cases()['candidate_result_change']
        result = analyze(trace)
        change = rows(result, 'CANDIDATE_RESULT_CHANGED')[0]
        self.assertEqual(change['metrics']['runtime_identity_refs'], [None, None])
        self.assertFalse(rows(result, 'PROBABLE_CANDIDATE_INSTABILITY', 'PROBABLE'))

    def test_changed_opaque_candidate_id_does_not_prove_changed_content(self):
        trace = cases()['candidate_result_change']
        for e in trace.events:
            if e['event_type'] == 'CANDIDATE_ITEM_DETAIL':
                e['detail']['text'] = 'same'
        self.assertFalse(rows(analyze(trace), 'CANDIDATE_RESULT_CHANGED'))
        details = [e for e in trace.events if e['event_type'] == 'CANDIDATE_ITEM_DETAIL']
        details[-1]['detail']['text'] = 'same extended'
        self.assertFalse(rows(analyze(trace), 'CANDIDATE_RESULT_CHANGED'))

    def test_all_synthetic_fields_exist_in_unchanged_r2_schema(self):
        from draftor_telemetry.schema import EVENTS, valid_enum
        for trace in cases().values():
            for event in trace.events:
                spec = EVENTS[event['event_id']]
                self.assertEqual(set(event['fields']), {f['name'] for f in spec['fields']})
                for field in spec['fields']:
                    self.assertTrue(valid_enum(field, event['fields'][field['name']]))
                self.assertEqual(event['schema_revision'], 2)

    def test_all_records_have_resolving_provenance_and_separate_layers(self):
        for trace in cases().values():
            reconstruction = reconstruct(trace.events)
            original = copy.deepcopy(reconstruction)
            result = list(iter_analysis(reconstruction))
            self.assertEqual(reconstruction, original)
            self.assertEqual(result, list(iter_analysis(reconstruction)))
            identifiers = {r['record_id'] for r in result}
            for row in result:
                for key in ('session_id','analysis_unit_id','start_ts','end_ts','evidence_type','diagnostic_type',
                            'status','confidence','source_refs','metrics','reason_codes','missing_evidence',
                            'supporting_evidence','contradicting_evidence'):
                    self.assertIn(key, row)
                for ref in row['source_refs']:
                    current = reconstruction
                    for part in ref['reconstruction_ref'].strip('/').split('/'):
                        current = current[int(part)] if isinstance(current, list) else current[part]
                    self.assertEqual(current['event_seq'], ref['event_seq'])
                self.assertTrue(set(row['supporting_evidence']+row['contradicting_evidence']) <= identifiers)
                if row['diagnostic_type']:
                    self.assertNotEqual(row['status'], 'CONFIRMED')
                    self.assertTrue(row['missing_evidence'])
                    if row['status'] == 'PROBABLE':
                        self.assertTrue(row['supporting_evidence'])
                self.assertTrue(set(row['reason_codes']) <= {r.value for r in Reason})

    def test_existing_wire_fixtures_all_analyze(self):
        names = [f'H{i:02d}' for i in range(1,18)]+[f'V{i:02d}' for i in range(1,4)]+[f'P{i:02d}' for i in range(1,9)]
        for name in names:
            before = summary(decode(fixture(name)))
            original = copy.deepcopy(before)
            list(iter_analysis(json.loads(json.dumps(before))))
            self.assertEqual(before, original)

    def test_limits_and_schema_fail_closed(self):
        reconstruction = reconstruct(cases()['first_candidate'].events)
        with self.assertRaises(ValueError):
            list(iter_analysis(reconstruction, Options(max_session_events=2)))
        reconstruction['schema_revision'] = 3
        with self.assertRaises(ValueError):
            list(iter_analysis(reconstruction))
        with self.assertRaises(ValueError):
            Options(post_commit_us=0)

    def test_cli_determinism_no_overwrite_and_no_partial_failure(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'_tmp') as folder:
            root = Path(folder)
            source = root/'input.json'
            source.write_text(json.dumps(reconstruct(cases()['third_candidate_browsed'].events)))
            command = [sys.executable, str(ROOT/'host/analyze_behavior.py'), '--input', str(source)]
            for name in ('a', 'b'):
                done = subprocess.run(command+['--output', str(root/name)], capture_output=True, text=True)
                self.assertEqual(done.returncode, 0, done.stderr)
            for name in ('analysis.jsonl', 'summary.json', 'summary.md'):
                self.assertEqual((root/'a'/name).read_bytes(), (root/'b'/name).read_bytes())
            self.assertEqual(subprocess.run(command+['--output', str(root/'a')], capture_output=True).returncode, 2)
            failed = subprocess.run(command+['--output', str(root/'bad'), '--max-session-events', '2'], capture_output=True)
            self.assertEqual(failed.returncode, 2)
            self.assertFalse((root/'bad').exists())


if __name__ == '__main__':
    unittest.main()
