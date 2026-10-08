"""Explanatory hypotheses consume facts. No diagnosis in A1 is CONFIRMED."""
from .analysis_types import Reason as R


def extract_candidate_changes(adapter, records, events, unit):
    snapshots = adapter.kinds(events, 'CANDIDATE_DATA_READY')
    for before, after in zip(snapshots, snapshots[1:]):
        if before['candidate_snapshot_id'] == after['candidate_snapshot_id'] or not adapter.intact(before, after):
            continue
        def items(ready):
            matches = [e for e in events if e['event_type'] == 'CANDIDATE_ITEM_DETAIL'
                       and e['candidate_snapshot_id'] == ready['candidate_snapshot_id']
                       and e['composition_revision'] == ready['composition_revision'] and e['privacy'] == 'DETAILED'
                       and e['event_seq'] > ready['event_seq'] and e.get('detail', {}).get('text')
                       and ready['fields']['visible_start'] <= e['fields']['global_index'] < ready['fields']['visible_start']+ready['fields']['visible_count']
                       and adapter.intact(ready, e)]
            # A repeated slot is ambiguous, not a replacement for the first observation.
            unique = {}
            for e in matches:
                slot = e['fields']['global_index']
                unique[slot] = None if slot in unique else e
            return {slot: e for slot, e in unique.items() if e is not None}
        a, b = items(before), items(after)
        def differs(slot):
            first, second = a[slot]['detail']['text'], b[slot]['detail']['text']
            # IDs can be snapshot-local; prefixes can be truncated by a producer.
            # Only a differing observed character proves content differs.
            return not first.startswith(second) and not second.startswith(first)
        changes = [slot for slot in sorted(a.keys() & b.keys())
                   if differs(slot) and a[slot]['event_seq'] < after['event_seq']]
        if changes:
            records.add('CANDIDATE_RESULT_CHANGED', [before, after, *[e for slot in changes for e in (a[slot], b[slot])]],
                        unit=unit, metrics=dict(changed_observed_slots=changes,
                            comparison_scope='OBSERVED_SLOTS_ONLY', comparison_basis='DIFFERING_OBSERVED_TEXT_CHARACTERS',
                            candidate_snapshot_ids=[before['candidate_snapshot_id'], after['candidate_snapshot_id']],
                            work_ids=[before['fields']['work_id'], after['fields']['work_id']],
                            composition_revisions=[before['composition_revision'], after['composition_revision']],
                            document_revisions=[before['document_revision'], after['document_revision']],
                            runtime_identity_refs=[before.get('runtime_identity_seq'), after.get('runtime_identity_seq')]),
                        reasons=[R.EXPLICIT_ENDPOINTS], missing=[R.QUERY_EQUIVALENCE_UNAVAILABLE])


def diagnose(adapter, records, options):
    observed = list(records.rows)
    by_unit = {}
    for row in observed:
        by_unit.setdefault(row['analysis_unit_id'], []).append(row)
    for cid, events in adapter.by_comp.items():
        if not cid:
            continue
        unit = adapter.unit(cid)
        rows = by_unit.get(unit, [])
        def facts(kind):
            return [r for r in rows if r['evidence_type'] == kind and r['status'] == 'CONFIRMED']
        def sources(support):
            seqs = {ref['event_seq'] for row in support for ref in row['source_refs']}
            return [e for e in adapter.events if e['event_seq'] in seqs]
        selections = facts('NONFIRST_CANDIDATE_SELECTED')
        for selection in selections:
            selected = selection['metrics']
            def matching(kind):
                return [r for r in facts(kind)
                        if all(r['metrics'].get(k) == selected[k] for k in ('candidate_snapshot_id', 'composition_revision'))
                        and (selected['selection_id'] in r['metrics'].get('selection_ids_after_navigation', [])
                             if kind == 'CANDIDATE_NAVIGATION_OCCURRED'
                             else r['metrics'].get('selection_id') == selected['selection_id'])]
            top1, stable, navigation = matching('TOP1_REJECTED'), matching('RAW_UNCHANGED'), matching('CANDIDATE_NAVIGATION_OCCURRED')
            support = [selection]+top1+stable+navigation
            revisions = facts('RAW_REVISED')
            # A selection alone never activates a probable ranking rule.
            probable = (adapter.complete(events) and facts('NORMAL_CANDIDATE_COMMIT')
                        and top1 and stable and navigation
                        and not revisions and not facts('CANDIDATE_RESULT_CHANGED'))
            records.add('PROBABLE_RANKING_ISSUE', sources(support+revisions), unit=unit, diagnostic=True,
                        status='PROBABLE' if probable else 'INSUFFICIENT_EVIDENCE',
                        confidence='MEDIUM' if probable else 'NONE', supporting=support,
                        contradicting=revisions+facts('CANDIDATE_RESULT_CHANGED'),
                        reasons=[R.BROWSED_STABLE_NONFIRST if probable else R.INCOMPLETE_CHAIN],
                        missing=[R.SEGMENTATION_UNAVAILABLE, R.USER_INTENT_UNKNOWN],
                        metrics={'rule': 'stable_presented_nonfirst_with_browsing_and_document_acceptance',
                                 'repetition_confidence_boost': False})
        if len(adapter.kinds(events, 'IME_RANGE_READY')) > 1:
            support = facts('RAW_REVISED')+[r for r in rows if r['evidence_type'] == 'SEGMENTATION_CHANGED']
            records.insufficient('PROBABLE_SEGMENTATION_ISSUE', sources(support) or events[:1], unit=unit,
                                 diagnostic=True, supporting=support,
                                 reasons=[R.SEGMENTATION_UNAVAILABLE], missing=[R.SEGMENTATION_UNAVAILABLE])
        aborts = facts('COMPOSITION_ABORTED')
        if aborts or any(r['evidence_type'] == 'COMPOSITION_ABORTED' for r in rows):
            support = [r for r in rows if r['evidence_type'] == 'COMPOSITION_ABORTED']+facts('CANDIDATE_NAVIGATION_OCCURRED')+facts('INPUT_ROUTE_CHANGED')
            missing = [R.TARGET_TEXT_UNAVAILABLE, 'exhaustive_candidate_coverage', 'same_intended_target_relation']
            if any(e['privacy'] == 'BASIC' for e in events):
                missing.append(R.INSUFFICIENT_EVIDENCE_DUE_TO_PRIVACY_PROFILE)
            records.insufficient('PROBABLE_LEXICON_MISS', sources(support) or events[:1], unit=unit,
                                 diagnostic=True, supporting=support, missing=missing,
                                 reasons=[R.USER_INTENT_UNKNOWN, *missing[:1]]+
                                 ([R.INSUFFICIENT_EVIDENCE_DUE_TO_PRIVACY_PROFILE] if missing[-1] == R.INSUFFICIENT_EVIDENCE_DUE_TO_PRIVACY_PROFILE else []))
        if len(adapter.kinds(events, 'CANDIDATE_DATA_READY')) > 1:
            changed = facts('CANDIDATE_RESULT_CHANGED')
            support = changed+[r for r in rows if r['evidence_type'] == 'QUERY_REPEATED']
            missing = [R.QUERY_EQUIVALENCE_UNAVAILABLE, R.SEGMENTATION_UNAVAILABLE]
            if any(e['privacy'] == 'BASIC' for e in events):
                missing.append(R.INSUFFICIENT_EVIDENCE_DUE_TO_PRIVACY_PROFILE)
            elif not changed:
                missing.append(R.CANDIDATE_CONTENT_UNAVAILABLE)
            records.insufficient('PROBABLE_CANDIDATE_INSTABILITY', sources(support) or adapter.kinds(events, 'CANDIDATE_DATA_READY'),
                                 unit=unit, diagnostic=True, supporting=support, reasons=missing,
                                 missing=missing, contradicting=facts('RAW_REVISED'))
        for route in facts('INPUT_ROUTE_CHANGED'):
            metrics = route['metrics']
            delay = metrics['termination_to_route_us']
            probable = (bool(aborts) and metrics['from_route'] == 'IME' and metrics['to_route'] in ('Editor', 'Literal')
                        and delay is not None and 0 <= delay <= options.reroute_us)
            support = aborts+[route]
            records.add('PROBABLE_INPUT_REROUTE', sources(support), unit=unit, diagnostic=True,
                        status='PROBABLE' if probable else 'INSUFFICIENT_EVIDENCE',
                        confidence='LOW' if probable else 'NONE', supporting=support,
                        reasons=[R.ABANDON_THEN_DIRECT_INPUT if probable else R.INCOMPLETE_CHAIN],
                        missing=['failure_causation', R.USER_INTENT_UNKNOWN],
                        metrics={'threshold_us': options.reroute_us})
