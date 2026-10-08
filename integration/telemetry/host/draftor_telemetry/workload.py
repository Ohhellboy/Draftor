"""Workload distributions with explicit observational units and scheme attribution."""
from collections import Counter, defaultdict
from .schema import ENUMS

UNKNOWN_SCHEME=ENUMS['InputScheme']['Unknown']
SCHEME_NAMES={value:name for name,value in ENUMS['InputScheme'].items()}
ACCEPTED=ENUMS['SelectionOutcome']['Accepted']
DISTRIBUTIONS=('revision_raw_length_distribution','composition_peak_raw_length_distribution',
               'composition_terminal_raw_length_distribution','selection_consumed_raw_length_distribution')
DENOMINATORS={
    DISTRIBUTIONS[0]:'observed reliable COMPOSITION_BEGIN/REVISION events (one sample per revision observation)',
    DISTRIBUTIONS[1]:'compositions with observed begin and end in one reliable epoch and one input scheme (one peak per composition)',
    DISTRIBUTIONS[2]:'the same complete observed compositions (last observed raw length before terminal event, one per composition)',
    DISTRIBUTIONS[3]:'observed reliable accepted SELECTION_RESULT with a nonnegative consumed raw byte range (one per result)',
}

def summarize_workload(events):
    reliable=[e for e in events if e['record_type']=='event' and e['reliable']]
    totals={name:Counter() for name in DISTRIBUTIONS}
    by_scheme=defaultdict(lambda:{name:Counter() for name in DISTRIBUTIONS})
    compositions=defaultdict(list)
    excluded=Counter()
    def add(name,value,scheme):
        totals[name][str(value)]+=1
        by_scheme[SCHEME_NAMES.get(scheme,'Unknown')][name][str(value)]+=1
    for event in reliable:
        kind=event['event_type']; scheme=event.get('input_scheme',UNKNOWN_SCHEME)
        if kind in ('COMPOSITION_BEGIN','COMPOSITION_REVISION'):
            add(DISTRIBUTIONS[0],event['fields']['raw_length'],scheme)
        if kind in ('COMPOSITION_BEGIN','COMPOSITION_REVISION','COMPOSITION_END'):
            compositions[event['composition_id']].append(event)
        if kind=='SELECTION_RESULT' and event['fields']['outcome']==ACCEPTED:
            length=event['fields']['consumed_end']-event['fields']['consumed_start']
            if length>=0: add(DISTRIBUTIONS[3],length,scheme)
    for history in compositions.values():
        begins=[e for e in history if e['event_type']=='COMPOSITION_BEGIN']
        ends=[e for e in history if e['event_type']=='COMPOSITION_END']
        if len(begins)!=1 or len(ends)!=1 or begins[0]['event_seq']>=ends[0]['event_seq']:
            excluded['incomplete_or_ambiguous_lifecycle']+=1; continue
        epochs={e['reliability_epoch'] for e in history}
        if len(epochs)!=1:
            excluded['crosses_gap_or_resync']+=1; continue
        schemes={e.get('input_scheme',UNKNOWN_SCHEME) for e in history}
        if len(schemes)!=1:
            excluded['input_scheme_changed_within_composition']+=1; continue
        revisions=[e for e in history if e['event_type']!='COMPOSITION_END'
                   and begins[0]['event_seq']<=e['event_seq']<ends[0]['event_seq']]
        scheme=next(iter(schemes))
        lengths=[e['fields']['raw_length'] for e in revisions]
        add(DISTRIBUTIONS[1],max(lengths),scheme)
        add(DISTRIBUTIONS[2],lengths[-1],scheme)
    def distributions(counters):
        return {name:dict(counts) for name,counts in counters.items()}
    def sample_counts(counters):
        return {name:sum(counts.values()) for name,counts in counters.items()}
    return {**distributions(totals),
            'workload_denominators':{name:{'unit':DENOMINATORS[name],'sample_count':sum(totals[name].values())} for name in DISTRIBUTIONS},
            'workload_by_input_scheme':{name:{**distributions(counts),'sample_counts':sample_counts(counts)} for name,counts in by_scheme.items()},
            'composition_workload_exclusions':dict(excluded)}
