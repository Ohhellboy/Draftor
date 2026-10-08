// Generated required-envelope rules; zero-valued ordinary revisions remain legal.
// State activity uses the caller-supplied composition_id, never business lookup.
switch (source.type) {
case EventType::SessionStart:
    if (source.envelope.composition_id != 0 && source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.payload.session_start.candidate_valid == 1 && source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::StateSnapshot:
    if (source.envelope.composition_id != 0 && source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.payload.state_snapshot.candidate_valid == 1 && source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::StateChange:
    if (source.envelope.composition_id != 0 && source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.payload.state_change.candidate_valid == 1 && source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::SyncSnapshot:
    if (source.envelope.composition_id != 0 && source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.payload.sync_snapshot.candidate_valid == 1 && source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::TelemetryHealth:
    break;
case EventType::InputReceived:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    break;
case EventType::RouteDecision:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    break;
case EventType::CompositionBegin:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    break;
case EventType::CompositionRevision:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    break;
case EventType::CompositionEnd:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    break;
case EventType::ImePlanBegin:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    break;
case EventType::ImeRangeReady:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    break;
case EventType::CandidateDataReady:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::CandidateNavigation:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::SelectionRequest:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::SelectionResult:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::CandidateItemDetail:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_id == 0) return FilterResult::Invalid;
    if (source.envelope.composition_revision == 0) return FilterResult::Invalid;
    if (source.envelope.candidate_snapshot_id == 0) return FilterResult::Invalid;
    break;
case EventType::EditorAction:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    break;
case EventType::DocumentMutation:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    break;
case EventType::ViewReady:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    break;
case EventType::DisplaySubmit:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    break;
case EventType::DisplayAck:
    if (source.envelope.action_id == 0) return FilterResult::Invalid;
    break;
case EventType::FileOpBegin:
    break;
case EventType::FileOpEnd:
    break;
case EventType::RuntimeIdentity:
    break;
default: return FilterResult::Invalid;
}
