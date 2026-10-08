"""Reconstruction preserves missing evidence and never fabricates business events."""
from .analysis import summarize_metrics
from .schema import STATE_EVENTS, ENUMS, SCHEMA_REVISION
from .integrity import missing_counterparts


class SessionReconstructor:
    def __init__(self):
        self.sessions = {}
        self.current = None
        self.unassigned_errors = []

    @staticmethod
    def _session(boot_id):
        return dict(boot_id=boot_id, status='NEEDS_RESYNC', history_complete=True,
                    last_seq=0, last_device_us=None, reliability_epoch=0, state={},
                    gaps=[], events=[], actions={}, compositions={}, document_mutations=[],
                    runtime_identities=[], current_runtime_identity_seq=None,
                    view_display_events=[], file_operations={}, health=[], missing=[])

    def _gap(self, session, kind, **details):
        session['status']='NEEDS_RESYNC'; session['history_complete']=False
        session['reliability_epoch'] += 1
        session['current_runtime_identity_seq']=None
        session['gaps'].append(dict(kind=kind, recovered_by_seq=None, **details))

    def accept(self, record):
        if record['record_type'] in ('decode_error', 'transport_gap'):
            if self.current is None:
                self.unassigned_errors.append(record.copy())
            else:
                self._gap(self.current, record.get('error', 'transport_gap'), evidence=record.copy())
            return
        if record['record_type'] not in ('event', 'unknown_event'):
            return
        event = record.copy()
        session = self.sessions.setdefault(str(event['boot_id']), self._session(event['boot_id']))
        self.current = session
        if self.unassigned_errors:
            for error in self.unassigned_errors:
                self._gap(session, 'unassigned_prior_transport_error', evidence=error)
            self.unassigned_errors.clear()
        sequence = event['event_seq']
        if not session['events'] and event['event_type'] != 'SESSION_START':
            self._gap(session, 'session_start_not_observed', at_seq=sequence)
        if sequence <= session['last_seq']:
            self._gap(session, 'non_monotonic_sequence', received=sequence, previous=session['last_seq'])
            event.update(reliable=False, reliability_epoch=session['reliability_epoch'])
            session['events'].append(event)
            return # Never apply stale/repeated state snapshots.
        if sequence != session['last_seq']+1:
            self._gap(session, 'event_seq_gap', first_missing=session['last_seq']+1, last_missing=sequence-1)
        if session['last_device_us'] is not None and event['device_monotonic_us'] < session['last_device_us']:
            self._gap(session, 'device_time_regression', at_seq=sequence)
        session['last_seq']=sequence; session['last_device_us']=event['device_monotonic_us']
        name = event['event_type']
        if name == 'UNKNOWN':
            self._gap(session, 'unknown_event_semantics', event_id=event['event_id'], at_seq=sequence)
        if name == 'SESSION_START' and sequence != 1:
            self._gap(session, 'reused_boot_identity', at_seq=sequence)
        if name == 'SYNC_SNAPSHOT' or (name=='SESSION_START' and sequence==1 and not session['gaps']):
            recovering=session['status']!='RELIABLE'
            session['status']='RELIABLE'
            # A recovery starts a new segment; it cannot retroactively repair latency.
            if recovering: session['reliability_epoch'] += 1
            for gap in session['gaps']:
                if gap['recovered_by_seq'] is None: gap['recovered_by_seq']=sequence
        event.update(reliable=session['status']=='RELIABLE', reliability_epoch=session['reliability_epoch'])
        session['events'].append(event)
        if name in STATE_EVENTS:
            session['state']={**event['fields'], **{key:event[key] for key in (
                'composition_id','composition_revision','candidate_snapshot_id','document_revision','layout_revision')},
                'source_seq':sequence}
        elif event['record_type']=='event' and event['reliable']:
            # Apply only explicit endpoint facts. The integration-supplied snapshot
            # remains necessary to restore a complete state after any gap.
            state=session['state']
            if name in ('COMPOSITION_BEGIN','COMPOSITION_REVISION'):
                state.update({key:event['fields'][key] for key in ('raw_length','focus','selected_member_count')})
                state.update(composition_id=event['composition_id'],composition_revision=event['composition_revision'],candidate_valid=0)
            elif name=='CANDIDATE_DATA_READY':
                state.update(candidate_snapshot_id=event['candidate_snapshot_id'],candidate_valid=1)
            elif name=='DOCUMENT_MUTATION':
                state['document_revision']=event['fields']['revision_after']
            elif name=='VIEW_READY':
                state.update(writing_mode=event['fields']['writing_mode'],layout_revision=event['layout_revision'])
        event['input_scheme']=session['state'].get('input_scheme',ENUMS['InputScheme']['Unknown']) if event['reliable'] else ENUMS['InputScheme']['Unknown']
        if name=='RUNTIME_IDENTITY':
            session['runtime_identities'].append(event)
            if event['reliable']: session['current_runtime_identity_seq']=sequence
        event['runtime_identity_seq']=session['current_runtime_identity_seq']
        action_id = event['action_id']
        if action_id:
            action = session['actions'].setdefault(str(action_id), dict(event_sequences=[], routes=[], composition_ids=[], missing=[]))
            action['event_sequences'].append(sequence)
            if name=='ROUTE_DECISION': action['routes'].append(event)
            if event['composition_id'] and event['composition_id'] not in action['composition_ids']:
                action['composition_ids'].append(event['composition_id'])
        composition_id = event['composition_id']
        if composition_id:
            composition = session['compositions'].setdefault(str(composition_id), dict(
                revisions=[], ime_phases=[], candidate_snapshots={}, navigation=[], selections={},
                lifecycle=[], action_ids=[], missing=[]))
            if action_id and action_id not in composition['action_ids']: composition['action_ids'].append(action_id)
            if name in ('COMPOSITION_BEGIN','COMPOSITION_END'): composition['lifecycle'].append(event)
            if name in ('COMPOSITION_BEGIN','COMPOSITION_REVISION'): composition['revisions'].append(event)
            if name in ('IME_PLAN_BEGIN','IME_RANGE_READY','CANDIDATE_DATA_READY'): composition['ime_phases'].append(event)
            if name=='CANDIDATE_DATA_READY':
                composition['candidate_snapshots'][str(event['candidate_snapshot_id'])] = dict(ready=event, items=[])
            if name=='CANDIDATE_ITEM_DETAIL':
                snapshot=composition['candidate_snapshots'].get(str(event['candidate_snapshot_id']))
                if snapshot is None: composition['missing'].append(dict(kind='candidate_ready_missing',at_seq=sequence))
                else: snapshot['items'].append(event)
            if name=='CANDIDATE_NAVIGATION': composition['navigation'].append(event)
            if name in ('SELECTION_REQUEST','SELECTION_RESULT'):
                selection=composition['selections'].setdefault(str(event['fields']['selection_id']), {})
                key='request' if name=='SELECTION_REQUEST' else 'result'
                if key in selection: composition['missing'].append(dict(kind='duplicate_selection_identity',at_seq=sequence))
                selection[key]=event
                if name=='SELECTION_RESULT' and 'request' in selection:
                    request=selection['request']
                    if request['candidate_snapshot_id'] != event['candidate_snapshot_id']:
                        composition['missing'].append(dict(kind='selection_snapshot_mismatch',at_seq=sequence))
        if name=='DOCUMENT_MUTATION': session['document_mutations'].append(event)
        if name in ('VIEW_READY','DISPLAY_SUBMIT','DISPLAY_ACK'): session['view_display_events'].append(event)
        if name in ('FILE_OP_BEGIN','FILE_OP_END'):
            operation=session['file_operations'].setdefault(str(event['fields']['file_operation_id']), {})
            operation['begin' if name=='FILE_OP_BEGIN' else 'end']=event
        if name=='TELEMETRY_HEALTH': session['health'].append(event)

    def summary(self):
        # Work on a copy so repeated summaries cannot mutate or duplicate findings.
        import copy
        sessions=copy.deepcopy(self.sessions)
        for session in sessions.values():
            session['missing'].extend(missing_counterparts(session['events']))
            by_seq={}
            for event in session['events']:
                by_seq.setdefault(event['event_seq'],event)
            for action in session['actions'].values():
                names={by_seq[seq]['event_type'] for seq in action['event_sequences']}
                for required in ('INPUT_RECEIVED','ROUTE_DECISION'):
                    if required not in names: action['missing'].append(required)
            for composition in session['compositions'].values():
                names={e['event_type'] for e in composition['lifecycle']}
                if 'COMPOSITION_BEGIN' not in names: composition['missing'].append({'kind':'composition_begin_missing'})
                if 'COMPOSITION_END' not in names: composition['missing'].append({'kind':'composition_end_not_observed'})
                for selection in composition['selections'].values():
                    selection['missing']=[key for key in ('request','result') if key not in selection]
                    request=selection.get('request')
                    if request and str(request['candidate_snapshot_id']) not in composition['candidate_snapshots']:
                        selection['missing'].append('candidate_snapshot')
            for operation in session['file_operations'].values():
                operation['missing']=[key for key in ('begin','end') if key not in operation]
            if not any(e['event_type']=='SESSION_START' for e in session['events']): session['missing'].append('SESSION_START')
            session['metrics']=summarize_metrics(session['events'], len(session['gaps']))
        return {'schema_version':1, 'schema_revision':SCHEMA_REVISION, 'sessions':sessions, 'unassigned_errors':self.unassigned_errors.copy()}
