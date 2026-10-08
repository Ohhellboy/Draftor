"""Composition facts; unavailable page/segmentation identities remain unavailable."""
from .adapter import UINT32_UNKNOWN, enum_name
from .analysis_types import Reason as R


def selection_pairs(adapter, events):
    requests = adapter.kinds(events, 'SELECTION_REQUEST')
    results = adapter.kinds(events, 'SELECTION_RESULT')
    for request in requests:
        sid = request['fields']['selection_id']
        matches = [r for r in results if r['fields']['selection_id'] == sid]
        if len(matches) != 1 or sum(r['fields']['selection_id'] == sid for r in requests) != 1:
            continue
        result = matches[0]
        ended = any(e['event_type'] == 'COMPOSITION_END' and e['event_seq'] < result['event_seq'] for e in events)
        if (result['fields']['outcome'] == 1 and request['action_id'] != 0
                and not ended
                and all(request[k] == result[k] for k in (
                    'composition_id', 'composition_revision', 'candidate_snapshot_id', 'action_id'))
                and request['event_seq'] < result['event_seq'] and adapter.intact(request, result)):
            yield request, result


def presented_view(adapter, interval, ready):
    """Require existing high-level view/submission/successful acknowledgement endpoints."""
    for view in reversed(interval):
        if not (view['event_type'] == 'VIEW_READY' and view['fields']['reason'] == 2
                and all(view[k] == ready[k] for k in (
                    'composition_id', 'composition_revision', 'candidate_snapshot_id', 'action_id'))
                and view['layout_revision'] > ready['layout_revision']):
            continue
        keys = ('action_id', 'composition_id', 'composition_revision', 'candidate_snapshot_id', 'document_revision', 'layout_revision')
        submissions = [e for e in interval if e['event_type'] == 'DISPLAY_SUBMIT'
                       and e['event_seq'] > view['event_seq'] and all(e[k] == view[k] for k in keys)]
        for submit in submissions:
            acks = [e for e in interval if e['event_type'] == 'DISPLAY_ACK' and e['fields']['outcome'] == 1
                    and e['fields']['display_id'] == submit['fields']['display_id']
                    and e['event_seq'] > submit['event_seq'] and all(e[k] == view[k] for k in keys)]
            if len(acks) == 1:
                return [view, submit, acks[0]]
    return []


def extract_selections(adapter, records, events, unit):
    pairs = list(selection_pairs(adapter, events))
    paired = {request['event_seq'] for request, _ in pairs}
    for request in adapter.kinds(events, 'SELECTION_REQUEST'):
        if request['event_seq'] not in paired:
            records.insufficient('NONFIRST_CANDIDATE_SELECTED', [request], unit=unit,
                                 reasons=[R.INCOMPLETE_CHAIN], missing=['unique_reliable_accepted_selection_pair'])
    for request, result in pairs:
        ready = [e for e in adapter.kinds(events, 'CANDIDATE_DATA_READY')
                 if e['candidate_snapshot_id'] == request['candidate_snapshot_id']]
        if len(ready) != 1:
            records.insufficient('NONFIRST_CANDIDATE_SELECTED', [request, result], unit=unit,
                                 reasons=[R.INCOMPLETE_CHAIN], missing=['unique_candidate_snapshot'])
            continue
        ready = ready[0]
        if (ready['event_seq'] >= request['event_seq'] or not adapter.intact(ready, result)
                or ready['composition_revision'] != request['composition_revision']):
            records.insufficient('NONFIRST_CANDIDATE_SELECTED', [ready, request, result], unit=unit,
                                 reasons=[R.UNRELIABLE_INTERVAL], missing=['continuous_snapshot_selection_chain'])
            continue
        interval = adapter.between(ready, request)
        revisions = [e for e in adapter.between(ready, result) if e['composition_id'] == request['composition_id']
                     and e['event_type'] == 'COMPOSITION_REVISION']
        navigation = [e for e in interval if e['event_type'] == 'CANDIDATE_NAVIGATION'
                      and e['candidate_snapshot_id'] == ready['candidate_snapshot_id']
                      and e['composition_id'] == ready['composition_id']
                      and e['composition_revision'] == ready['composition_revision']
                      and e['fields']['outcome'] == 1]
        window = navigation[-1]['fields'] if navigation else ready['fields']
        start = window.get('window_start', window.get('visible_start'))
        count = window.get('window_count', window.get('visible_count'))
        index, reported_rank = request['fields']['global_index'], request['fields']['rank']
        rank = (index-start+1 if all(v != UINT32_UNKNOWN for v in (index, start, count))
                and count > 0 and start <= index < start+count and reported_rank == index+1 else None)
        metrics = dict(selected_local_rank=rank, local_rank=rank, rank_scope='OBSERVED_WINDOW',
                       reported_rank=reported_rank, candidate_page=None, global_ordinal=None,
                       query_generation=None, work_id=ready['fields']['work_id'],
                       candidate_snapshot_id=ready['candidate_snapshot_id'],
                       composition_revision=request['composition_revision'], window_start=start,
                       selection_id=request['fields']['selection_id'],
                       candidate_ready_to_selection_us=request['device_monotonic_us']-ready['device_monotonic_us'])
        sources = [ready, *navigation, request, result]
        if rank is None:
            records.insufficient('NONFIRST_CANDIDATE_SELECTED', sources, unit=unit, metrics=metrics,
                                 reasons=[R.RANK_UNAVAILABLE], missing=['consistent_observed_window_local_rank'])
        elif rank > 1:
            records.add('NONFIRST_CANDIDATE_SELECTED', sources, unit=unit, metrics=metrics,
                        reasons=[R.ACCEPTED_SELECTION], missing=[R.GLOBAL_ORDINAL_UNAVAILABLE, R.PAGE_IDENTITY_UNAVAILABLE])
        if revisions:
            records.insufficient('TOP1_REJECTED', sources+revisions, unit=unit, metrics=metrics,
                                 reasons=[R.COMPOSITION_REVISED], missing=['unchanged_composition_revision'])
            continue
        presentation = presented_view(adapter, interval, ready)
        raw_states = [e for e in events if e['event_type'] in ('COMPOSITION_BEGIN', 'COMPOSITION_REVISION')
                      and e['event_seq'] < ready['event_seq']]
        raw_state = raw_states[-1] if raw_states else None
        raw_known = (raw_state is not None and raw_state['composition_revision'] == ready['composition_revision']
                     and raw_state['fields']['raw_length'] > 0 and adapter.intact(raw_state, result))
        if index > 0 and rank is not None:
            if ready['fields']['visible_start'] == 0 and ready['fields']['visible_count'] > 0 and presentation and raw_known:
                stable = records.add('RAW_UNCHANGED', [raw_state, ready, request], unit=unit,
                                     metrics={'scope': 'candidate_ready_to_selection', 'composition_revision': request['composition_revision'],
                                              'candidate_snapshot_id': ready['candidate_snapshot_id'],
                                              'selection_id': request['fields']['selection_id']},
                                     reasons=[R.STABLE_REVISION])
                records.add('TOP1_REJECTED', sources+[raw_state, *presentation], unit=unit, metrics=metrics,
                            reasons=[R.PRESENTED_FIRST_WINDOW, R.STABLE_REVISION], supporting=[stable],
                            missing=[R.QUERY_GENERATION_UNAVAILABLE, R.GLOBAL_ORDINAL_UNAVAILABLE])
            else:
                records.insufficient('TOP1_REJECTED', sources, unit=unit, metrics=metrics,
                                     reasons=[R.NO_PRESENTATION if not presentation else R.INCOMPLETE_CHAIN],
                                     missing=['presented_rank_one_and_known_raw_in_same_snapshot'])


def extract_navigation(adapter, records, events, unit):
    # Group by actual snapshot/revision, never by assumed page size or screen geometry.
    groups = {}
    for event in adapter.kinds(events, 'CANDIDATE_NAVIGATION'):
        groups.setdefault((event['composition_revision'], event['candidate_snapshot_id'],
                           event['reliability_epoch']), []).append(event)
    for (revision, snapshot, _), navigation in groups.items():
        ready = [e for e in adapter.kinds(events, 'CANDIDATE_DATA_READY')
                 if e['composition_revision'] == revision and e['candidate_snapshot_id'] == snapshot
                 and e['event_seq'] < navigation[0]['event_seq']]
        if len(ready) != 1 or not adapter.intact(ready[0], navigation[-1]):
            records.insufficient('PAGE_NAVIGATION_OCCURRED', navigation, unit=unit,
                                 reasons=[R.INCOMPLETE_CHAIN], missing=['continuous_candidate_navigation_chain'])
            continue
        ready = ready[0]
        moved = [e for e in navigation if e['fields']['outcome'] == 1]
        windows = [ready['fields']['visible_start']]
        forward = backward = 0
        for event in moved:
            start = event['fields']['window_start']
            if start == UINT32_UNKNOWN:
                continue
            forward += start > windows[-1]
            backward += start < windows[-1]
            windows.append(start)
        selections = [(q, r) for q, r in selection_pairs(adapter, events)
                      if q['event_seq'] > navigation[-1]['event_seq'] and q['candidate_snapshot_id'] == snapshot
                      and q['composition_revision'] == revision and adapter.intact(navigation[-1], r)]
        metrics = dict(page_forward_count=None, page_backward_count=None, candidate_pages_seen=None,
                       window_forward_count=forward, window_backward_count=backward,
                       observed_window_starts=list(dict.fromkeys(windows)), navigation_count=len(moved),
                       boundary_hit_count=sum(e['fields']['outcome'] == 2 for e in navigation),
                       selection_after_navigation=bool(selections), candidate_snapshot_id=snapshot,
                       composition_revision=revision,
                       selection_ids_after_navigation=[q['fields']['selection_id'] for q, _ in selections])
        sources = [ready, *navigation, *[e for pair in selections for e in pair]]
        if moved:
            records.add('CANDIDATE_NAVIGATION_OCCURRED', sources, unit=unit, metrics=metrics,
                        reasons=[R.OBSERVED_NAVIGATION])
        records.insufficient('PAGE_NAVIGATION_OCCURRED', sources, unit=unit, metrics=metrics,
                             reasons=[R.PAGE_IDENTITY_UNAVAILABLE], missing=[R.PAGE_IDENTITY_UNAVAILABLE])


def extract_revisions(adapter, records, events, unit):
    revisions = [e for e in events if e['event_type'] in ('COMPOSITION_BEGIN', 'COMPOSITION_REVISION')]
    for before, after in zip(revisions, revisions[1:]):
        cause = after['fields']['cause']
        if cause == 2:  # A candidate selection is not a user raw edit.
            continue
        if not adapter.intact(before, after) or after['composition_revision'] <= before['composition_revision']:
            records.insufficient('RAW_REVISED', [before, after], unit=unit,
                                 reasons=[R.UNRELIABLE_INTERVAL], missing=['ordered_revision_chain'])
            continue
        a, b = adapter.full_raw(before), adapter.full_raw(after)
        delta = after['fields']['raw_length']-before['fields']['raw_length']
        kind = ('BACKSPACE' if cause == 3 else 'REOPEN_MEMBER' if cause == 4 else
                'RAW_INSERT' if cause == 1 and delta > 0 else
                'RAW_REPLACEMENT' if a is not None and b is not None and a != b else
                'RAW_LENGTH_CHANGE' if delta else None)
        if kind:
            records.add('RAW_REVISED', [before, after], unit=unit,
                        metrics=dict(revision_kind=kind, cause=enum_name('RevisionCause', cause),
                                     raw_length_before=before['fields']['raw_length'], raw_length_after=after['fields']['raw_length'],
                                     revision_before=before['composition_revision'], revision_after=after['composition_revision']),
                        reasons=[R.OBSERVED_REVISION])
        else:
            records.insufficient('RAW_REVISED', [before, after], unit=unit,
                                 reasons=[R.INCOMPLETE_CHAIN], missing=['raw_edit_vs_focus_or_member_change'])
    ranges = adapter.kinds(events, 'IME_RANGE_READY')
    if len(ranges) > 1:
        # A span is the selected query range, not a syllable/segmentation trace.
        records.insufficient('SEGMENTATION_CHANGED', ranges, unit=unit,
                             reasons=[R.SEGMENTATION_UNAVAILABLE], missing=[R.SEGMENTATION_UNAVAILABLE])
    plans = adapter.kinds(events, 'IME_PLAN_BEGIN')
    if len(plans) > 1:
        records.insufficient('QUERY_REPEATED', plans, unit=unit,
                             metrics={'query_attempt_count': len(plans)},
                             reasons=[R.QUERY_EQUIVALENCE_UNAVAILABLE], missing=['equivalent_logical_query_identity'])


def extract_abort(adapter, records, events, unit):
    ends = adapter.kinds(events, 'COMPOSITION_END')
    revisions = [e for e in events if e['event_type'] in ('COMPOSITION_BEGIN', 'COMPOSITION_REVISION')]
    activity = [e for e in events if e['event_type'] in ('IME_PLAN_BEGIN', 'CANDIDATE_DATA_READY')]
    if not ends or not revisions or not activity or not any(e['fields']['raw_length'] > 0 for e in revisions):
        return
    end = ends[-1]
    activity = [e for e in activity if revisions[0]['event_seq'] < e['event_seq'] < end['event_seq']]
    revisions = [e for e in revisions if e['event_seq'] < end['event_seq']]
    if not activity or not revisions:
        return
    # Only explicit non-completed terminal semantics establish an abort. EOF is not cancellation.
    if end['fields']['outcome'] == 1 or list(selection_pairs(adapter, events)):
        return
    accepted = [e for e in adapter.kinds(events, 'SELECTION_RESULT') if e['fields']['outcome'] == 1]
    mutations = [e for e in adapter.kinds(adapter.events, 'DOCUMENT_MUTATION')
                 if e['composition_id'] == end['composition_id'] and e['fields']['inserted_bytes'] > 0]
    sources = [*revisions, *activity, *adapter.kinds(events, 'CANDIDATE_NAVIGATION'), end]
    metrics = dict(raw_length=revisions[-1]['fields']['raw_length'],
                   duration_us=end['device_monotonic_us']-revisions[0]['device_monotonic_us'],
                   candidate_activity=len(activity), navigation_count=len(adapter.kinds(events, 'CANDIDATE_NAVIGATION')),
                   raw_revisions=len(revisions)-1,
                   termination_reason=enum_name('CompositionOutcome', end['fields']['outcome']).upper())
    if adapter.complete(events) and end['fields']['outcome'] in (2, 3, 4) and not accepted and not mutations:
        records.add('COMPOSITION_ABORTED', sources, unit=unit, metrics=metrics,
                    reasons=[R.NO_NORMAL_COMMIT, R.EXPLICIT_ENDPOINTS], missing=['user_intent'])
    else:
        records.insufficient('COMPOSITION_ABORTED', sources+accepted+mutations, unit=unit, metrics=metrics,
                             reasons=[R.INCOMPLETE_CHAIN], missing=[R.SELECTION_COVERAGE_UNAVAILABLE])


def extract_composition(adapter, records, composition_id, events):
    unit = adapter.unit(composition_id)
    extract_revisions(adapter, records, events, unit)
    extract_selections(adapter, records, events, unit)
    extract_navigation(adapter, records, events, unit)
    extract_abort(adapter, records, events, unit)
