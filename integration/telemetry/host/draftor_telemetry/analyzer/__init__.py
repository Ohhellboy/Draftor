"""Host-only Behavior Analyzer A1, consuming existing reconstruction summaries."""
from .adapter import SessionAdapter, validate_summary
from .analysis_types import DATA_GAPS, Options, Records, Reason as R
from .evidence import extract_composition
from .document import extract_document, extract_routes
from .diagnostics import diagnose, extract_candidate_changes


def iter_analysis(summary, options=None):
    """Yield deterministic JSON-compatible rows, retaining only one session's rules/output.

    The input is borrowed and never mutated. Reconstruction itself still owns a full
    trace graph; CLI file and per-session event caps make that inherited limit explicit.
    """
    validate_summary(summary)
    options = options or Options()
    if not summary['sessions']:
        adapter = SessionAdapter('none', {'events': []}, options)
        rows = Records(adapter)
        row = rows.insufficient('ANALYZER_DATA_GAP', reasons=[R.NO_EVENTS],
                                missing=['behavior_events'], metrics={
                                    'what_is_missing': 'No reconstructed sessions',
                                    'why_it_matters': 'OFF, empty capture and unobserved behavior cannot be distinguished',
                                    'future_instrumentation_candidate': 'No change required; retain capture/profile metadata',
                                    'unassigned_error_count': len(summary.get('unassigned_errors', []))})
        row['session_id'] = None
        yield row
        return
    for sid in sorted(summary['sessions'], key=lambda key: (len(key), key)):
        adapter = SessionAdapter(sid, summary['sessions'][sid], options)
        records = Records(adapter)
        for reason, (what, why, future) in DATA_GAPS.items():
            records.insufficient('ANALYZER_DATA_GAP', adapter.events[:1], reasons=[reason], missing=[reason],
                                 metrics=dict(what_is_missing=what, why_it_matters=why, future_instrumentation_candidate=future))
        for cid, events in adapter.by_comp.items():
            if cid:
                extract_composition(adapter, records, cid, events)
                extract_candidate_changes(adapter, records, events, adapter.unit(cid))
        extract_document(adapter, records, options)
        extract_routes(adapter, records, options)
        diagnose(adapter, records, options)
        yield from records.rows


__all__ = ['Options', 'iter_analysis']
