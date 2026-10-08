"""Deterministic schema-r2 decoded fixtures; all pass through the real reconstructor.

No fixture invents page, generation, segmentation, target intent or context fields.
"""
import copy
import json
from draftor_telemetry.schema import EVENTS
from draftor_telemetry.reconstruction import SessionReconstructor

SPECS = {e['name']: (number, e) for number, e in EVENTS.items()}


class Trace:
    def __init__(self, privacy='BASIC'):
        self.events = []
        self.privacy = privacy
        self.action = self.comp = self.rev = self.snapshot = self.layout = 0
        self.doc = 1
        self.raw = 'ni'
        self.emit('SESSION_START', writing_mode=1, base_input_mode=1, input_scheme=1,
                  edit_context=1, raw_length=0, focus=0, selected_member_count=0, candidate_valid=0)

    def emit(self, name, detail=None, **fields):
        number, spec = SPECS[name]
        payload = {field['name']: 0 for field in spec['fields']}
        payload.update(fields)
        event = dict(record_type='event', schema_version=1, schema_revision=2, privacy=self.privacy,
                     event_id=number, event_type=name, host_received_time='2026-09-29T00:00:00Z',
                     boot_id=101, event_seq=len(self.events)+1, device_monotonic_us=(len(self.events)+1)*10_000,
                     action_id=self.action, composition_id=self.comp, composition_revision=self.rev,
                     candidate_snapshot_id=self.snapshot, document_revision=self.doc, layout_revision=self.layout,
                     fields=payload)
        if self.privacy == 'DETAILED':
            event['detail'] = dict(printable_codepoint=0, candidate_identity=0, text='', path='')
            if name in ('COMPOSITION_BEGIN', 'COMPOSITION_REVISION'):
                event['detail']['text'] = self.raw
            event['detail'].update(detail or {})
        self.events.append(event)
        return event

    def route(self, target, action):
        self.action += 1
        self.emit('INPUT_RECEIVED', physical_key=1)
        self.emit('ROUTE_DECISION', route_target=target, semantic_action=action)

    def begin(self):
        self.route(1, 1)
        self.comp = self.rev = 1
        self.emit('COMPOSITION_BEGIN', raw_length=len(self.raw.encode()), cause=1)
        return self

    def query(self, detail_identity=None, view=True, range_end=None, ack=True):
        self.snapshot += 1
        size = len(self.raw.encode()) if range_end is None else range_end
        self.emit('IME_PLAN_BEGIN', work_id=self.snapshot, range_start=0, range_end=size)
        self.emit('IME_RANGE_READY', work_id=self.snapshot, range_start=0, range_end=size)
        self.emit('CANDIDATE_DATA_READY', work_id=self.snapshot, range_start=0, range_end=size,
                  candidate_count_known=5, visible_start=0, visible_count=5, completion_state=2)
        if detail_identity is not None:
            self.emit('CANDIDATE_ITEM_DETAIL', global_index=0, rank=1, provenance=1,
                      detail={'candidate_identity': detail_identity, 'text': '秘密'+str(detail_identity)})
        if view:
            self.layout += 1
            self.emit('VIEW_READY', writing_mode=1, reason=2)
            self.emit('DISPLAY_SUBMIT', display_id=self.layout)
            if ack:
                self.emit('DISPLAY_ACK', display_id=self.layout, outcome=1)
        return self

    def navigate(self, start=0, index=1, previous=0, outcome=1):
        self.route(2, 3)
        self.emit('CANDIDATE_NAVIGATION', previous_index=previous, global_index=index,
                  window_start=start, window_count=5, spatial_direction=2, outcome=outcome)
        return self

    def revise(self, raw='n', cause=3):
        self.route(1, 5 if cause == 3 else 1)
        self.rev += 1
        self.raw = raw
        self.emit('COMPOSITION_REVISION', raw_length=len(raw.encode()), cause=cause)
        return self

    def select(self, index=0, accepted=True):
        self.route(2, 2)
        self.emit('SELECTION_REQUEST', selection_id=1, global_index=index, rank=index+1)
        self.emit('SELECTION_RESULT', selection_id=1, outcome=1 if accepted else 2,
                  consumed_start=0, consumed_end=len(self.raw.encode()), new_focus=len(self.raw.encode()),
                  new_composition_revision=self.rev+1, confirmed_output_count=2, output_count_unit=1)
        if not accepted:
            return self
        self.rev += 1
        self.emit('COMPOSITION_REVISION', raw_length=len(self.raw.encode()), focus=len(self.raw.encode()),
                  selected_member_count=1, cause=2)
        self.mutate(1, 0, 0, 6, 0)
        self.emit('COMPOSITION_END', outcome=1)
        return self

    def mutate(self, kind, start, end, inserted, removed):
        previous = self.doc
        self.doc += 1
        self.emit('DOCUMENT_MUTATION', revision_before=previous, revision_after=self.doc,
                  mutation_type=kind, byte_start=start, byte_end=end, inserted_bytes=inserted,
                  removed_bytes=removed, cursor_before=end, cursor_after=start+inserted)

    def delete(self, start=0, end=6):
        self.comp = self.rev = self.snapshot = 0
        self.route(3, 6)
        self.emit('EDITOR_ACTION', semantic_action=6, cursor_before=end, cursor_after=start)
        self.mutate(2, start, end, 0, end-start)
        return self

    def direct(self, start=0):
        self.comp = self.rev = self.snapshot = 0
        self.route(3, 10)
        self.mutate(1, start, start, 3, 0)
        return self

    def abort(self, outcome=2):
        self.route(1, 11)
        self.emit('COMPOSITION_END', outcome=outcome)
        return self


def reconstruct(events):
    reconstructor = SessionReconstructor()
    for event in events:
        reconstructor.accept(event)
    # Exercises the public on-disk contract as well as the in-process output.
    return json.loads(json.dumps(reconstructor.summary()))


def cases():
    return {
        'first_candidate': Trace().begin().query().select(),
        'second_candidate': Trace().begin().query().select(1),
        'third_candidate_browsed': Trace().begin().query().navigate(index=2).select(2),
        'window_navigation_selection': Trace().begin().query().navigate(start=5, index=6).select(6),
        'raw_revision_then_commit': Trace().begin().query().revise().query().select(1),
        'post_commit_delete': Trace().begin().query().select().delete(),
        'post_commit_rewrite': Trace().begin().query().select().delete().direct(),
        'composition_abandon': Trace().begin().query().navigate().abort(),
        'segmentation_unobservable': Trace('DETAILED').begin().query().revise("n'i", cause=1).query().select(),
        'route_change': Trace().begin().query().abort().direct(),
        'candidate_result_change': Trace('DETAILED').begin().query(101).query(102).select(),
        'basic_privacy_insufficient': Trace().begin().query().query().abort(),
        'detailed_lexicon_still_insufficient': Trace('DETAILED').begin().query(101).abort().direct(),
        'changed_mind': Trace('DETAILED').begin().query(101).abort(),
    }
