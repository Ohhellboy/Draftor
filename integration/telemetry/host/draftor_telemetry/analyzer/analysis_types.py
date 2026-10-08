"""A1 output contract. Reason codes and data-gap policy live here, not in rules."""
from dataclasses import dataclass
from enum import Enum


class Reason(str, Enum):
    EXPLICIT_ENDPOINTS = 'EXPLICIT_ENDPOINTS'
    ACCEPTED_SELECTION = 'ACCEPTED_SELECTION'
    PRESENTED_FIRST_WINDOW = 'PRESENTED_FIRST_WINDOW'
    STABLE_REVISION = 'STABLE_REVISION'
    OBSERVED_NAVIGATION = 'OBSERVED_NAVIGATION'
    OBSERVED_REVISION = 'OBSERVED_REVISION'
    NO_NORMAL_COMMIT = 'NO_NORMAL_COMMIT'
    DOCUMENT_OVERLAP = 'DOCUMENT_OVERLAP'
    SAME_POSITION_REPLACEMENT = 'SAME_POSITION_REPLACEMENT'
    ROUTE_ENDPOINTS = 'ROUTE_ENDPOINTS'
    BROWSED_STABLE_NONFIRST = 'BROWSED_STABLE_NONFIRST'
    ABANDON_THEN_DIRECT_INPUT = 'ABANDON_THEN_DIRECT_INPUT'
    USER_INTENT_UNKNOWN = 'USER_INTENT_UNKNOWN'
    INCOMPLETE_CHAIN = 'INCOMPLETE_CHAIN'
    UNRELIABLE_INTERVAL = 'UNRELIABLE_INTERVAL'
    COMPOSITION_REVISED = 'COMPOSITION_REVISED'
    NO_PRESENTATION = 'NO_PRESENTATION'
    RANK_UNAVAILABLE = 'RANK_UNAVAILABLE'
    GLOBAL_ORDINAL_UNAVAILABLE = 'GLOBAL_ORDINAL_UNAVAILABLE'
    PAGE_IDENTITY_UNAVAILABLE = 'PAGE_IDENTITY_UNAVAILABLE'
    QUERY_GENERATION_UNAVAILABLE = 'QUERY_GENERATION_UNAVAILABLE'
    SEGMENTATION_UNAVAILABLE = 'SEGMENTATION_UNAVAILABLE'
    QUERY_EQUIVALENCE_UNAVAILABLE = 'QUERY_EQUIVALENCE_UNAVAILABLE'
    CANDIDATE_CONTENT_UNAVAILABLE = 'CANDIDATE_CONTENT_UNAVAILABLE'
    TARGET_TEXT_UNAVAILABLE = 'TARGET_TEXT_UNAVAILABLE'
    SELECTION_COVERAGE_UNAVAILABLE = 'SELECTION_COVERAGE_UNAVAILABLE'
    INSUFFICIENT_EVIDENCE_DUE_TO_PRIVACY_PROFILE = 'INSUFFICIENT_EVIDENCE_DUE_TO_PRIVACY_PROFILE'
    NO_EVENTS = 'NO_EVENTS'


# No schema extension is implied by these future instrumentation suggestions.
DATA_GAPS = {
    Reason.GLOBAL_ORDINAL_UNAVAILABLE: (
        'Reliable ordinal coverage for hidden/default and deep-page selections',
        'Cannot report global rank, global top-1 rate or whole-population preferences',
        'Validate naturally available ordinal and eligibility at the candidate owner'),
    Reason.PAGE_IDENTITY_UNAVAILABLE: (
        'Explicit page identity and forward/backward page transitions',
        'Window movement cannot establish page count or page navigation cost',
        'Observe actual page/anchor transitions with stable query identity'),
    Reason.QUERY_GENERATION_UNAVAILABLE: (
        'Query generation distinct from work_id and candidate_snapshot_id',
        'Cannot equate a work operation or snapshot with a logical query generation',
        'Publish an existing logical query generation when available'),
    Reason.SEGMENTATION_UNAVAILABLE: (
        'Full syllable/span boundaries and explicit boundary edit/replan semantics',
        'Plan/range endpoints alone do not prove segmentation changed',
        'Observe existing segmentation identity/boundaries without rerunning IME'),
    Reason.QUERY_EQUIVALENCE_UNAVAILABLE: (
        'Complete comparison state including context and learning update epochs',
        'Candidate changes cannot establish equivalent-query instability',
        'Expose existing state epochs and bounded result identity/coverage'),
    Reason.SELECTION_COVERAGE_UNAVAILABLE: (
        'Complete normal/hidden-default selection and commit correlation coverage',
        'Absent selection event does not prove absence of a normal commit',
        'Correlate existing commit acceptance and record coverage limits explicitly'),
    Reason.TARGET_TEXT_UNAVAILABLE: (
        'Verified intended text, exhaustive candidate coverage and retry linkage',
        'Abandonment or later direct input does not prove a lexicon miss',
        'Consider an explicit user-confirmed retry relation under the privacy contract'),
}


@dataclass(frozen=True)
class Options:
    post_commit_us: int = 2_000_000
    reroute_us: int = 2_000_000
    max_session_events: int = 200_000

    def __post_init__(self):
        if min(self.post_commit_us, self.reroute_us, self.max_session_events) <= 0:
            raise ValueError('Analyzer limits must be positive')


class Records:
    def __init__(self, adapter):
        self.adapter = adapter
        self.rows = []

    def add(self, kind, events=(), *, unit=None, diagnostic=False, status='CONFIRMED',
            confidence='HIGH', metrics=None, reasons=(), missing=(), supporting=(), contradicting=()):
        events = sorted({e['event_seq']: e for e in events}.values(), key=lambda e: e['event_seq'])
        row = dict(
            analysis_version='A1', record_id=f'{self.adapter.session_id}:a{len(self.rows)+1}',
            session_id=self.adapter.session_id, analysis_unit_id=unit or f'{self.adapter.session_id}:session',
            start_ts=events[0]['device_monotonic_us'] if events else None,
            end_ts=events[-1]['device_monotonic_us'] if events else None, timestamp_unit='device_us',
            evidence_type=None if diagnostic else kind, diagnostic_type=kind if diagnostic else None,
            layer='DIAGNOSTIC_HYPOTHESIS' if diagnostic else 'OBSERVED_EVIDENCE',
            status=status, confidence=confidence,
            source_refs=[self.adapter.ref(e) for e in events], metrics=metrics or {},
            reason_codes=list(dict.fromkeys(reasons)), missing_evidence=list(dict.fromkeys(missing)),
            supporting_evidence=[r['record_id'] for r in supporting],
            contradicting_evidence=[r['record_id'] for r in contradicting])
        self.rows.append(row)
        return row

    def insufficient(self, kind, events=(), **kwargs):
        return self.add(kind, events, status='INSUFFICIENT_EVIDENCE', confidence='NONE', **kwargs)
