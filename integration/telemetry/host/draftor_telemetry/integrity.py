"""Report unobserved counterparts without inventing state or declaring user intent."""

def missing_counterparts(events):
    observations=[e for e in events if e['record_type']=='event']
    missing=[]
    def key(event, identity):
        if identity=='work_id':
            return event['composition_id'],event['composition_revision'],event['fields'][identity]
        if identity=='display_id':
            return event['action_id'],event['layout_revision'],event['fields'][identity]
        return event['fields'][identity],
    for first,second,identity in (
        ('IME_PLAN_BEGIN','IME_RANGE_READY','work_id'),
        ('IME_RANGE_READY','CANDIDATE_DATA_READY','work_id'),
        ('DISPLAY_SUBMIT','DISPLAY_ACK','display_id'),
    ):
        starts={key(e,identity):e for e in observations if e['event_type']==first}
        ends={key(e,identity):e for e in observations if e['event_type']==second}
        for identity_key,event in starts.items():
            partner=ends.get(identity_key)
            if partner is None or partner['event_seq']<=event['event_seq']:
                missing.append(dict(kind=second+'_not_observed_after',at_seq=event['event_seq']))
        for identity_key,event in ends.items():
            partner=starts.get(identity_key)
            if partner is None or partner['event_seq']>=event['event_seq']:
                missing.append(dict(kind=first+'_not_observed_before',at_seq=event['event_seq']))
    return missing
