"""Document acceptance and strictly bounded, spatially correlated edit patterns."""
from .adapter import enum_name
from .analysis_types import Reason as R
from .evidence import selection_pairs


def accepted_commits(adapter):
    mutations = adapter.kinds(adapter.events, 'DOCUMENT_MUTATION')
    for cid, events in adapter.by_comp.items():
        if not cid or not adapter.complete(events):
            continue
        end = adapter.kinds(events, 'COMPOSITION_END')[0]
        if end['fields']['outcome'] != 1:
            continue
        for request, result in selection_pairs(adapter, events):
            accepted = [e for e in mutations if e['action_id'] == result['action_id']
                        and e['composition_id'] == cid and result['event_seq'] < e['event_seq'] <= end['event_seq']
                        and e['composition_revision'] == result['fields']['new_composition_revision']
                        and e['fields']['inserted_bytes'] > 0 and e['fields']['removed_bytes'] == 0
                        and e['fields']['mutation_type'] == 1 and e['fields']['byte_start'] == e['fields']['byte_end']
                        and e['fields']['revision_after'] > e['fields']['revision_before']
                        and adapter.intact(request, end)]
            if len(accepted) == 1:
                yield request, result, accepted[0], end


def same_revision(first, second):
    return first['fields']['revision_after'] == second['fields']['revision_before']


def document_boundary(events):
    # There is no document identity in r2. Do not correlate across file replacement.
    return any(e['event_type'] in ('FILE_OP_BEGIN', 'FILE_OP_END')
               and e['fields']['operation'] in (2, 3, 5, 6, 7) for e in events)


def extract_document(adapter, records, options):
    mutations = adapter.kinds(adapter.events, 'DOCUMENT_MUTATION')
    positions = {id(e): i for i, e in enumerate(mutations)}
    for request, result, commit, end in accepted_commits(adapter):
        unit = adapter.unit(commit['composition_id'])
        normal = records.add('NORMAL_CANDIDATE_COMMIT', [request, result, commit, end], unit=unit,
                             metrics={'inserted_bytes': commit['fields']['inserted_bytes']},
                             reasons=[R.ACCEPTED_SELECTION, R.EXPLICIT_ENDPOINTS])
        index = positions[id(commit)]
        if index+1 >= len(mutations):
            continue
        delete = mutations[index+1]
        c, d = commit['fields'], delete['fields']
        delay = delete['device_monotonic_us']-commit['device_monotonic_us']
        actions = [e for e in adapter.by_action[delete['action_id']] if e['event_type'] == 'EDITOR_ACTION'
                   and e['fields']['semantic_action'] in (6, 7) and end['event_seq'] < e['event_seq'] < delete['event_seq']]
        if not (0 <= delay <= options.post_commit_us and same_revision(commit, delete)
                and end['event_seq'] < delete['event_seq'] and adapter.intact(commit, delete)
                and not document_boundary(adapter.between(commit, delete))
                and d['mutation_type'] == 2 and d['inserted_bytes'] == 0 and d['removed_bytes'] > 0
                and d['revision_after'] > d['revision_before']
                and d['byte_end']-d['byte_start'] == d['removed_bytes'] and len(actions) == 1):
            continue
        overlap = min(c['byte_start']+c['inserted_bytes'], d['byte_end'])-max(c['byte_start'], d['byte_start'])
        if overlap <= 0:
            continue
        removed = records.add('POST_COMMIT_DELETE', [request, result, commit, end, *actions, delete], unit=unit,
                              metrics=dict(time_delta_us=delay, overlap_bytes=overlap,
                                           whole_commit_removed=overlap == c['inserted_bytes'], threshold_us=options.post_commit_us),
                              reasons=[R.DOCUMENT_OVERLAP], supporting=[normal], missing=['user_intent'])
        if (index+2 >= len(mutations) or d['byte_start'] != c['byte_start']
                or d['removed_bytes'] != c['inserted_bytes']):
            continue  # A1 rewrite rule requires one complete deletion, not guessed partial replacement.
        replacement = mutations[index+2]
        b = replacement['fields']
        replacement_delay = replacement['device_monotonic_us']-delete['device_monotonic_us']
        routes = [e for e in adapter.by_action[replacement['action_id']]
                  if e['event_type'] == 'ROUTE_DECISION' and e['fields']['semantic_action'] in (2, 10)
                  and delete['event_seq'] < e['event_seq'] < replacement['event_seq']]
        intervening = adapter.between(delete, replacement)
        moved_caret = any(e['event_type'] == 'EDITOR_ACTION' and e['fields']['semantic_action'] == 4 for e in intervening)
        if (0 <= replacement_delay <= options.post_commit_us and same_revision(delete, replacement)
                and adapter.intact(delete, replacement) and not document_boundary(intervening)
                and b['revision_after'] > b['revision_before'] and b['mutation_type'] == 1 and b['removed_bytes'] == 0
                and b['inserted_bytes'] > 0 and b['byte_start'] == b['byte_end'] == d['byte_start']
                and len(routes) == 1 and not moved_caret):
            records.add('POST_COMMIT_REWRITE', [commit, delete, *routes, replacement], unit=unit,
                        metrics=dict(time_delta_us=replacement_delay, inserted_bytes=b['inserted_bytes'],
                                     replacement_relation='SAME_DOCUMENT_POSITION', content_changed=None,
                                     threshold_us=options.post_commit_us),
                        reasons=[R.SAME_POSITION_REPLACEMENT], supporting=[removed], missing=['user_intent', 'text_equality'])


def extract_routes(adapter, records, options):
    # Per-action route decisions are not a persistent mode. Compare only actual text-entry routes.
    routes = [e for e in adapter.events if e['event_type'] == 'ROUTE_DECISION'
              and ((e['fields']['route_target'] == 1 and e['fields']['semantic_action'] == 1)
                   or (e['fields']['route_target'] in (3, 4) and e['fields']['semantic_action'] == 10))]
    for before, after in zip(routes, routes[1:]):
        if before['fields']['route_target'] == after['fields']['route_target']:
            continue
        delta = after['device_monotonic_us']-before['device_monotonic_us']
        if not adapter.intact(before, after):
            continue
        interval = adapter.between(before, after)
        ends = [e for e in interval if e['event_type'] == 'COMPOSITION_END']
        preceding = ends[-1] if ends else None
        source = [before, after]+([preceding] if preceding else [])
        records.add('INPUT_ROUTE_CHANGED', source,
                    unit=adapter.unit(preceding['composition_id']) if preceding else None,
                    metrics=dict(from_route=enum_name('RouteTarget', before['fields']['route_target']),
                                 to_route=enum_name('RouteTarget', after['fields']['route_target']),
                                 preceding_composition=preceding['composition_id'] if preceding else None,
                                 time_delta_us=delta,
                                 termination_to_route_us=after['device_monotonic_us']-preceding['device_monotonic_us'] if preceding else None,
                                 relation='CONSECUTIVE_OBSERVED_TEXT_ENTRY_ROUTE_DECISIONS'),
                    reasons=[R.ROUTE_ENDPOINTS], missing=['persistent_mode_identity', 'cause_of_route_change'])
