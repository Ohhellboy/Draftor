"""Small, content-free session summaries with at most three representative cases."""
from collections import Counter
from dataclasses import asdict


class Summary:
    def __init__(self, reconstruction, options):
        self.data = dict(analysis_version='A1', schema_revision=2, options=asdict(options),
                         evidence_level='HOST / OFFLINE', sessions={},
                         unassigned_error_count=len(reconstruction.get('unassigned_errors', [])),
                         limitations=['Counts cover observed eligible events, not all user input.',
                                      'Window-local rank is not page-local rank or global ordinal.',
                                      'No rendering CPU/SPI/LCD breakdown or inferred user intent.'])
        for sid, session in reconstruction['sessions'].items():
            self.data['sessions'][sid] = dict(compositions=len(session['compositions']),
                history_complete=session['history_complete'], gap_count=len(session['gaps']),
                confirmed=Counter(), probable=Counter(), insufficient=Counter(), examples=[])

    def accept(self, row):
        session = self.data['sessions'].get(row['session_id'])
        if session is None:
            return
        bucket = {'CONFIRMED': 'confirmed', 'PROBABLE': 'probable', 'INSUFFICIENT_EVIDENCE': 'insufficient'}[row['status']]
        kind = row['evidence_type'] or row['diagnostic_type']
        session[bucket][kind] += 1
        if kind not in ('ANALYZER_DATA_GAP', 'RAW_UNCHANGED') and len(session['examples']) < 3:
            session['examples'].append(dict(record_id=row['record_id'], kind=kind, status=row['status'],
                                            source_event_sequences=[r['event_seq'] for r in row['source_refs']][:6]))

    def markdown(self):
        lines = ['# Behavior Analyzer A1', '', 'HOST / OFFLINE; hypotheses are not established IME defects.', '',
                 'Counts cover observed eligible events. Page identity/global ordinal remain unknown.', '']
        if not self.data['sessions']:
            lines += ['No sessions: INSUFFICIENT_EVIDENCE (OFF/empty/absent capture cannot be distinguished).', '']
        for sid, session in self.data['sessions'].items():
            lines += [f'## Session {sid}', '',
                      f"{session['compositions']} observed compositions; history_complete={session['history_complete']}; {session['gap_count']} gaps.", '']
            for bucket in ('confirmed', 'probable', 'insufficient'):
                counts = session[bucket]
                lines.append(f"- {bucket}: "+(', '.join(f'{n} {kind}' for kind, n in sorted(counts.items())) or '0'))
            lines += ['', 'Representative cases (event sequences refer to the input reconstruction):', '']
            for sample in session['examples']:
                lines.append(f"- {sample['record_id']}: {sample['kind']} / {sample['status']}; events {sample['source_event_sequences']}")
            lines.append('')
        lines += ['No diagnosis is CONFIRMED. No rendering CPU/layout/framebuffer/SPI/LCD decomposition.', '']
        return '\n'.join(lines)
