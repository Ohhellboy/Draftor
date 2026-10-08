"""Fact-based metrics from reliable intervals with explicit correlation keys."""
from collections import Counter
from .schema import ENUMS
from .workload import summarize_workload

SELECTION_ACCEPTED = ENUMS['SelectionOutcome']['Accepted']
DISPLAY_SUCCEEDED = ENUMS['OperationOutcome']['Succeeded']
COMPOSITION_BACKSPACE = ENUMS['RevisionCause']['CompositionBackspace']
REOPEN_MEMBER = ENUMS['RevisionCause']['ReopenMember']
DOCUMENT_BACKSPACE = ENUMS['SemanticAction']['DocumentBackspace']

LATENCIES = {
    'input_to_route': ('INPUT_RECEIVED', 'ROUTE_DECISION'),
    'input_to_range': ('INPUT_RECEIVED', 'IME_RANGE_READY'),
    'range_to_candidate': ('IME_RANGE_READY', 'CANDIDATE_DATA_READY'),
    'candidate_to_view': ('CANDIDATE_DATA_READY', 'VIEW_READY'),
    'view_to_display_ack': ('VIEW_READY', 'DISPLAY_ACK'),
    'input_to_display_ack': ('INPUT_RECEIVED', 'DISPLAY_ACK'),
    'selection_request_to_result': ('SELECTION_REQUEST', 'SELECTION_RESULT'),
    'selection_result_to_next_range': ('SELECTION_RESULT', 'IME_RANGE_READY'),
    'selection_result_to_next_candidate': ('SELECTION_RESULT', 'CANDIDATE_DATA_READY'),
    'selection_result_to_next_display_ack': ('SELECTION_RESULT', 'DISPLAY_ACK'),
    'document_to_view': ('DOCUMENT_MUTATION', 'VIEW_READY'),
    'document_to_display_ack': ('DOCUMENT_MUTATION', 'DISPLAY_ACK'),
}


def matches(name, start, end):
    if start['reliability_epoch'] != end['reliability_epoch']:
        return False
    if name.startswith('input_'):
        return start['action_id'] != 0 and start['action_id'] == end['action_id']
    if name == 'selection_request_to_result':
        return (start['composition_id'] == end['composition_id'] and
                start['candidate_snapshot_id'] == end['candidate_snapshot_id'] and
                start['fields']['selection_id'] == end['fields']['selection_id'])
    if name.startswith('selection_result_to_next_'):
        return (start['fields']['outcome'] == SELECTION_ACCEPTED and start['composition_id'] != 0 and
                start['composition_id'] == end['composition_id'] and
                end['composition_revision'] >= start['fields']['new_composition_revision'])
    if name == 'range_to_candidate':
        return all(start[key] == end[key] for key in ('composition_id', 'composition_revision')) and (
            start['fields']['work_id'] != 0 and start['fields']['work_id'] == end['fields']['work_id'])
    if name == 'candidate_to_view':
        return (start['candidate_snapshot_id'] != 0 and start['action_id'] != 0
                and end['fields']['reason']==ENUMS['ViewReason']['CandidateUpdate']
                and end['layout_revision']>start['layout_revision']
                and all(start[key] == end[key] for key in ('action_id','composition_id', 'composition_revision', 'candidate_snapshot_id')))
    if name == 'view_to_display_ack':
        return (start['action_id'] == end['action_id'] and start['layout_revision'] == end['layout_revision']
                and start['document_revision'] == end['document_revision'])
    if name=='document_to_view' and (end['fields']['reason']!=ENUMS['ViewReason']['DocumentEdit']
                                    or end['layout_revision']<=start['layout_revision']):
        return False
    if name.startswith('document_'):
        return start['fields']['revision_after'] == end['document_revision'] and start['action_id'] == end['action_id']
    return False


def summarize_metrics(events, gap_count):
    reliable = [e for e in events if e['record_type']=='event' and e['reliable']]
    # A display acknowledgement needs its matching submission. An unrelated or
    # unobserved display operation cannot complete any latency chain.
    submissions = {(e['reliability_epoch'], e['fields']['display_id']):e for e in reliable
                   if e['event_type']=='DISPLAY_SUBMIT'}
    views = [event for event in reliable if event['event_type']=='VIEW_READY']
    def view_for_ack(event):
        submit=submissions.get((event['reliability_epoch'],event['fields']['display_id']))
        if submit is None:
            return None
        matching=[view for view in views if view['event_seq']<submit['event_seq']
                  and all(view[key]==event[key] for key in ('reliability_epoch','action_id','document_revision','layout_revision'))]
        return matching[-1] if matching else None
    def valid_end(event):
        if event['event_type'] != 'DISPLAY_ACK':
            return True
        submit = submissions.get((event['reliability_epoch'],event['fields']['display_id']))
        return (submit is not None and event['fields']['outcome']==DISPLAY_SUCCEEDED and submit['event_seq']<event['event_seq']
                and view_for_ack(event) is not None
                and all(submit[k]==event[k] for k in ('action_id','document_revision','layout_revision')))
    def causal_match(name,start,event):
        if not matches(name,start,event):
            return False
        if event['event_type']=='DISPLAY_ACK':
            view=view_for_ack(event)
            if name=='document_to_display_ack':
                return view['fields']['reason']==ENUMS['ViewReason']['DocumentEdit'] and view['layout_revision']>start['layout_revision']
            if name=='view_to_display_ack':
                return view['event_seq']==start['event_seq']
        return True
    latency_samples = {}
    for name, (start_type, end_type) in LATENCIES.items():
        pending, samples = [], []
        for event in reliable:
            pending=[start for start in pending if start['reliability_epoch']==event['reliability_epoch']]
            if event['event_type'] == end_type and valid_end(event):
                eligible = [s for s in pending if causal_match(name, s, event)]
                if eligible:
                    # One result per starting endpoint; no unrelated file/IME pairing.
                    for start in eligible:
                        delta = event['device_monotonic_us'] - start['device_monotonic_us']
                        if delta >= 0:
                            samples.append({'us':delta,'start_seq':start['event_seq'],'end_seq':event['event_seq']})
                        pending.remove(start)
            if event['event_type'] == start_type:
                pending.append(event)
        latency_samples[name] = samples
    def select(kind): return [e for e in reliable if e['event_type']==kind]
    accepted_ids = {(e['reliability_epoch'], e['composition_id'], e['candidate_snapshot_id'], e['fields']['selection_id'])
                    for e in select('SELECTION_RESULT') if e['fields']['outcome']==SELECTION_ACCEPTED}
    selected_ranks = Counter(str(e['fields']['rank']) for e in select('SELECTION_REQUEST')
                             if (e['reliability_epoch'],e['composition_id'],e['candidate_snapshot_id'],e['fields']['selection_id']) in accepted_ids)
    compositions = {str(e['composition_id']):0 for e in reliable if e['composition_id']}
    for e in select('SELECTION_RESULT'):
        if e['fields']['outcome']==SELECTION_ACCEPTED:
            key=str(e['composition_id']); compositions[key]=compositions.get(key,0)+1
    return {
        'basis':'observed reliable intervals only; no inferred user intent',
        'latencies':{name:{'count':len(samples), 'samples':samples,
                          'mean_us':sum(s['us'] for s in samples)/len(samples) if samples else None}
                     for name,samples in latency_samples.items()},
        **summarize_workload(events),
        'accepted_selection_count_per_composition':compositions,
        'candidate_navigation_count':len(select('CANDIDATE_NAVIGATION')),
        'selected_rank_distribution':dict(selected_ranks),
        'composition_backspace_count':sum(e['fields']['cause']==COMPOSITION_BACKSPACE for e in select('COMPOSITION_REVISION')),
        'document_backspace_count':sum(e['fields']['semantic_action']==DOCUMENT_BACKSPACE for e in select('EDITOR_ACTION')),
        'reopen_count':sum(e['fields']['cause']==REOPEN_MEMBER for e in select('COMPOSITION_REVISION')),
        'telemetry_gaps':gap_count,
    }
