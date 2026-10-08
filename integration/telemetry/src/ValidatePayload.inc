// Generated enum validity rules; no separate enum registry.
switch (source.type) {
case EventType::SessionStart:
    if (!valid_WritingMode(source.payload.session_start.writing_mode)) return FilterResult::Invalid;
    if (source.payload.session_start.writing_mode == static_cast<std::uint8_t>(WritingMode::Unknown)) return FilterResult::Invalid;
    if (!valid_BaseInputMode(source.payload.session_start.base_input_mode)) return FilterResult::Invalid;
    if (!valid_InputScheme(source.payload.session_start.input_scheme)) return FilterResult::Invalid;
    if (!valid_EditContext(source.payload.session_start.edit_context)) return FilterResult::Invalid;
    if (!valid_Boolean(source.payload.session_start.candidate_valid)) return FilterResult::Invalid;
    break;
case EventType::StateSnapshot:
    if (!valid_WritingMode(source.payload.state_snapshot.writing_mode)) return FilterResult::Invalid;
    if (source.payload.state_snapshot.writing_mode == static_cast<std::uint8_t>(WritingMode::Unknown)) return FilterResult::Invalid;
    if (!valid_BaseInputMode(source.payload.state_snapshot.base_input_mode)) return FilterResult::Invalid;
    if (!valid_InputScheme(source.payload.state_snapshot.input_scheme)) return FilterResult::Invalid;
    if (!valid_EditContext(source.payload.state_snapshot.edit_context)) return FilterResult::Invalid;
    if (!valid_Boolean(source.payload.state_snapshot.candidate_valid)) return FilterResult::Invalid;
    break;
case EventType::StateChange:
    if (!valid_WritingMode(source.payload.state_change.writing_mode)) return FilterResult::Invalid;
    if (source.payload.state_change.writing_mode == static_cast<std::uint8_t>(WritingMode::Unknown)) return FilterResult::Invalid;
    if (!valid_BaseInputMode(source.payload.state_change.base_input_mode)) return FilterResult::Invalid;
    if (!valid_InputScheme(source.payload.state_change.input_scheme)) return FilterResult::Invalid;
    if (!valid_EditContext(source.payload.state_change.edit_context)) return FilterResult::Invalid;
    if (!valid_Boolean(source.payload.state_change.candidate_valid)) return FilterResult::Invalid;
    break;
case EventType::SyncSnapshot:
    if (!valid_WritingMode(source.payload.sync_snapshot.writing_mode)) return FilterResult::Invalid;
    if (source.payload.sync_snapshot.writing_mode == static_cast<std::uint8_t>(WritingMode::Unknown)) return FilterResult::Invalid;
    if (!valid_BaseInputMode(source.payload.sync_snapshot.base_input_mode)) return FilterResult::Invalid;
    if (!valid_InputScheme(source.payload.sync_snapshot.input_scheme)) return FilterResult::Invalid;
    if (!valid_EditContext(source.payload.sync_snapshot.edit_context)) return FilterResult::Invalid;
    if (!valid_Boolean(source.payload.sync_snapshot.candidate_valid)) return FilterResult::Invalid;
    break;
case EventType::TelemetryHealth:
    break;
case EventType::InputReceived:
    if (!valid_PhysicalKey(source.payload.input_received.physical_key)) return FilterResult::Invalid;
    break;
case EventType::RouteDecision:
    if (!valid_RouteTarget(source.payload.route_decision.route_target)) return FilterResult::Invalid;
    if (!valid_SemanticAction(source.payload.route_decision.semantic_action)) return FilterResult::Invalid;
    break;
case EventType::CompositionBegin:
    if (!valid_RevisionCause(source.payload.composition_begin.cause)) return FilterResult::Invalid;
    break;
case EventType::CompositionRevision:
    if (!valid_RevisionCause(source.payload.composition_revision.cause)) return FilterResult::Invalid;
    break;
case EventType::CompositionEnd:
    if (!valid_CompositionOutcome(source.payload.composition_end.outcome)) return FilterResult::Invalid;
    break;
case EventType::ImePlanBegin:
    break;
case EventType::ImeRangeReady:
    break;
case EventType::CandidateDataReady:
    if (!valid_CompletionState(source.payload.candidate_data_ready.completion_state)) return FilterResult::Invalid;
    break;
case EventType::CandidateNavigation:
    if (!valid_SpatialDirection(source.payload.candidate_navigation.spatial_direction)) return FilterResult::Invalid;
    if (!valid_NavigationOutcome(source.payload.candidate_navigation.outcome)) return FilterResult::Invalid;
    break;
case EventType::SelectionRequest:
    break;
case EventType::SelectionResult:
    if (!valid_SelectionOutcome(source.payload.selection_result.outcome)) return FilterResult::Invalid;
    if (!valid_OutputCountUnit(source.payload.selection_result.output_count_unit)) return FilterResult::Invalid;
    break;
case EventType::CandidateItemDetail:
    break;
case EventType::EditorAction:
    if (!valid_SemanticAction(source.payload.editor_action.semantic_action)) return FilterResult::Invalid;
    if (!valid_SpatialDirection(source.payload.editor_action.spatial_direction)) return FilterResult::Invalid;
    break;
case EventType::DocumentMutation:
    if (!valid_MutationType(source.payload.document_mutation.mutation_type)) return FilterResult::Invalid;
    break;
case EventType::ViewReady:
    if (!valid_WritingMode(source.payload.view_ready.writing_mode)) return FilterResult::Invalid;
    if (source.payload.view_ready.writing_mode == static_cast<std::uint8_t>(WritingMode::Unknown)) return FilterResult::Invalid;
    if (!valid_ViewReason(source.payload.view_ready.reason)) return FilterResult::Invalid;
    break;
case EventType::DisplaySubmit:
    break;
case EventType::DisplayAck:
    if (!valid_OperationOutcome(source.payload.display_ack.outcome)) return FilterResult::Invalid;
    break;
case EventType::FileOpBegin:
    if (!valid_FileOperation(source.payload.file_op_begin.operation)) return FilterResult::Invalid;
    break;
case EventType::FileOpEnd:
    if (!valid_FileOperation(source.payload.file_op_end.operation)) return FilterResult::Invalid;
    if (!valid_OperationOutcome(source.payload.file_op_end.outcome)) return FilterResult::Invalid;
    break;
case EventType::RuntimeIdentity:
    if (!valid_IdentityScope(source.payload.runtime_identity.identity_scope)) return FilterResult::Invalid;
    if (!valid_ToggleState(source.payload.runtime_identity.p2)) return FilterResult::Invalid;
    if (!valid_ToggleState(source.payload.runtime_identity.p3)) return FilterResult::Invalid;
    if (!valid_ToggleState(source.payload.runtime_identity.prefetch)) return FilterResult::Invalid;
    if (!valid_LearningMode(source.payload.runtime_identity.learning_mode)) return FilterResult::Invalid;
    break;
default: return FilterResult::Invalid;
}
