"""Read-only adapter over SessionReconstructor.summary(); no parallel reconstruction."""
from collections import defaultdict
from ..schema import EVENTS, SCHEMA_REVISION

SPECS = {s['name']: s for s in EVENTS.values()}
UINT32_UNKNOWN = 0xffffffff


def enum_name(enum, value):
    from ..schema import ENUMS
    return next((name for name, number in ENUMS[enum].items() if number == value), 'UNKNOWN')


def reliable(events):
    return bool(events) and all(e.get('reliable') is True for e in events) and len({
        e['reliability_epoch'] for e in events}) == 1


def validate_summary(summary):
    if summary.get('schema_version') != 1 or summary.get('schema_revision') != SCHEMA_REVISION:
        raise ValueError('Expected reconstruction summary schema version 1 / revision 2')
    if not isinstance(summary.get('sessions'), dict):
        raise ValueError('Expected reconstruction sessions mapping')


class SessionAdapter:
    def __init__(self, session_id, session, options):
        self.session_id = str(session_id)
        self.session = session
        self.events = session['events']  # Borrow the graph; never deepcopy it.
        if len(self.events) > options.max_session_events:
            raise ValueError('Session exceeds --max-session-events; split at session boundaries')
        self.by_comp = defaultdict(list)
        self.by_action = defaultdict(list)
        self.positions = {}
        for index, event in enumerate(self.events):
            for key in ('event_seq', 'device_monotonic_us', 'composition_id', 'composition_revision',
                        'candidate_snapshot_id', 'action_id', 'document_revision', 'layout_revision',
                        'reliability_epoch'):
                if type(event.get(key)) is not int or event[key] < 0:
                    raise ValueError('Invalid reconstructed event: '+key)
            if type(event.get('reliable')) is not bool or event.get('privacy') not in ('BASIC', 'DETAILED'):
                raise ValueError('Missing reconstruction reliability/privacy metadata')
            if event['event_type'] in SPECS:
                for field in SPECS[event['event_type']]['fields']:
                    if type(event.get('fields', {}).get(field['name'])) is not int:
                        raise ValueError('Missing reconstructed field: '+field['name'])
            self.positions[id(event)] = index
            self.by_comp[event['composition_id']].append(event)
            self.by_action[event['action_id']].append(event)

    def ref(self, event):
        return dict(event_seq=event['event_seq'], reconstruction_ref=(
            f'/sessions/{self.session_id}/events/{self.positions[id(event)]}'))

    def unit(self, composition_id):
        return f'{self.session_id}:composition:{composition_id}'

    def between(self, first, last):
        return self.events[self.positions[id(first)]:self.positions[id(last)]+1]

    def intact(self, first, last):
        events = self.between(first, last)
        return (reliable(events) and all(
            b['event_seq'] == a['event_seq']+1 and b['device_monotonic_us'] >= a['device_monotonic_us']
            for a, b in zip(events, events[1:])))

    def kinds(self, events, kind):
        return [e for e in events if e['event_type'] == kind]

    def complete(self, events):
        begins = self.kinds(events, 'COMPOSITION_BEGIN')
        ends = self.kinds(events, 'COMPOSITION_END')
        return (len(begins) == len(ends) == 1 and begins[0]['event_seq'] < ends[0]['event_seq']
                and self.intact(begins[0], ends[0]))

    def full_raw(self, event):
        if event['privacy'] != 'DETAILED':
            return None
        value = event.get('detail', {}).get('text')
        # Detail text may be truncated. No truncated prefix is a raw identity.
        if isinstance(value, str) and len(value.encode('utf-8')) == event['fields'].get('raw_length'):
            return value
        return None
