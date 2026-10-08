// Generated explicit field serialization; no struct packing.
switch (event.type) {
case EventType::SessionStart:
    writer.u8(event.payload.session_start.writing_mode);
    writer.u8(event.payload.session_start.base_input_mode);
    writer.u8(event.payload.session_start.input_scheme);
    writer.u8(event.payload.session_start.edit_context);
    writer.u32(event.payload.session_start.raw_length);
    writer.u32(event.payload.session_start.focus);
    writer.u32(event.payload.session_start.selected_member_count);
    writer.u8(event.payload.session_start.candidate_valid);
    break;
case EventType::StateSnapshot:
    writer.u8(event.payload.state_snapshot.writing_mode);
    writer.u8(event.payload.state_snapshot.base_input_mode);
    writer.u8(event.payload.state_snapshot.input_scheme);
    writer.u8(event.payload.state_snapshot.edit_context);
    writer.u32(event.payload.state_snapshot.raw_length);
    writer.u32(event.payload.state_snapshot.focus);
    writer.u32(event.payload.state_snapshot.selected_member_count);
    writer.u8(event.payload.state_snapshot.candidate_valid);
    break;
case EventType::StateChange:
    writer.u8(event.payload.state_change.writing_mode);
    writer.u8(event.payload.state_change.base_input_mode);
    writer.u8(event.payload.state_change.input_scheme);
    writer.u8(event.payload.state_change.edit_context);
    writer.u32(event.payload.state_change.raw_length);
    writer.u32(event.payload.state_change.focus);
    writer.u32(event.payload.state_change.selected_member_count);
    writer.u8(event.payload.state_change.candidate_valid);
    break;
case EventType::SyncSnapshot:
    writer.u8(event.payload.sync_snapshot.writing_mode);
    writer.u8(event.payload.sync_snapshot.base_input_mode);
    writer.u8(event.payload.sync_snapshot.input_scheme);
    writer.u8(event.payload.sync_snapshot.edit_context);
    writer.u32(event.payload.sync_snapshot.raw_length);
    writer.u32(event.payload.sync_snapshot.focus);
    writer.u32(event.payload.sync_snapshot.selected_member_count);
    writer.u8(event.payload.sync_snapshot.candidate_valid);
    break;
case EventType::TelemetryHealth:
    writer.u64(event.payload.telemetry_health.attempted);
    writer.u64(event.payload.telemetry_health.enqueued);
    writer.u64(event.payload.telemetry_health.dropped_queue);
    writer.u64(event.payload.telemetry_health.dropped_invalid);
    writer.u64(event.payload.telemetry_health.sink_busy);
    writer.u64(event.payload.telemetry_health.dropped_sink_failed);
    writer.u64(event.payload.telemetry_health.dropped_encode);
    writer.u64(event.payload.telemetry_health.transport_failures);
    writer.u32(event.payload.telemetry_health.queue_high_water);
    writer.u64(event.payload.telemetry_health.frames_accepted);
    writer.u64(event.payload.telemetry_health.bytes_accepted);
    break;
case EventType::InputReceived:
    writer.u8(event.payload.input_received.physical_key);
    writer.u8(event.payload.input_received.modifiers);
    break;
case EventType::RouteDecision:
    writer.u8(event.payload.route_decision.route_target);
    writer.u8(event.payload.route_decision.semantic_action);
    break;
case EventType::CompositionBegin:
    writer.u32(event.payload.composition_begin.raw_length);
    writer.u32(event.payload.composition_begin.focus);
    writer.u32(event.payload.composition_begin.selected_member_count);
    writer.u8(event.payload.composition_begin.cause);
    break;
case EventType::CompositionRevision:
    writer.u32(event.payload.composition_revision.raw_length);
    writer.u32(event.payload.composition_revision.focus);
    writer.u32(event.payload.composition_revision.selected_member_count);
    writer.u8(event.payload.composition_revision.cause);
    break;
case EventType::CompositionEnd:
    writer.u8(event.payload.composition_end.outcome);
    break;
case EventType::ImePlanBegin:
    writer.u32(event.payload.ime_plan_begin.work_id);
    writer.u32(event.payload.ime_plan_begin.range_start);
    writer.u32(event.payload.ime_plan_begin.range_end);
    break;
case EventType::ImeRangeReady:
    writer.u32(event.payload.ime_range_ready.work_id);
    writer.u32(event.payload.ime_range_ready.range_start);
    writer.u32(event.payload.ime_range_ready.range_end);
    break;
case EventType::CandidateDataReady:
    writer.u32(event.payload.candidate_data_ready.work_id);
    writer.u32(event.payload.candidate_data_ready.range_start);
    writer.u32(event.payload.candidate_data_ready.range_end);
    writer.u32(event.payload.candidate_data_ready.candidate_count_known);
    writer.u32(event.payload.candidate_data_ready.visible_start);
    writer.u32(event.payload.candidate_data_ready.visible_count);
    writer.u8(event.payload.candidate_data_ready.completion_state);
    break;
case EventType::CandidateNavigation:
    writer.u32(event.payload.candidate_navigation.previous_index);
    writer.u32(event.payload.candidate_navigation.global_index);
    writer.u32(event.payload.candidate_navigation.window_start);
    writer.u32(event.payload.candidate_navigation.window_count);
    writer.u8(event.payload.candidate_navigation.spatial_direction);
    writer.u8(event.payload.candidate_navigation.outcome);
    break;
case EventType::SelectionRequest:
    writer.u32(event.payload.selection_request.selection_id);
    writer.u32(event.payload.selection_request.global_index);
    writer.u32(event.payload.selection_request.rank);
    break;
case EventType::SelectionResult:
    writer.u32(event.payload.selection_result.selection_id);
    writer.u8(event.payload.selection_result.outcome);
    writer.u32(event.payload.selection_result.consumed_start);
    writer.u32(event.payload.selection_result.consumed_end);
    writer.u32(event.payload.selection_result.new_focus);
    writer.u32(event.payload.selection_result.new_composition_revision);
    writer.u32(event.payload.selection_result.confirmed_output_count);
    writer.u8(event.payload.selection_result.output_count_unit);
    break;
case EventType::CandidateItemDetail:
    writer.u32(event.payload.candidate_item_detail.global_index);
    writer.u32(event.payload.candidate_item_detail.rank);
    writer.u8(event.payload.candidate_item_detail.provenance);
    break;
case EventType::EditorAction:
    writer.u8(event.payload.editor_action.semantic_action);
    writer.u8(event.payload.editor_action.spatial_direction);
    writer.u32(event.payload.editor_action.cursor_before);
    writer.u32(event.payload.editor_action.cursor_after);
    break;
case EventType::DocumentMutation:
    writer.u32(event.payload.document_mutation.revision_before);
    writer.u32(event.payload.document_mutation.revision_after);
    writer.u8(event.payload.document_mutation.mutation_type);
    writer.u32(event.payload.document_mutation.byte_start);
    writer.u32(event.payload.document_mutation.byte_end);
    writer.u32(event.payload.document_mutation.inserted_bytes);
    writer.u32(event.payload.document_mutation.removed_bytes);
    writer.u32(event.payload.document_mutation.cursor_before);
    writer.u32(event.payload.document_mutation.cursor_after);
    break;
case EventType::ViewReady:
    writer.u8(event.payload.view_ready.writing_mode);
    writer.u8(event.payload.view_ready.reason);
    break;
case EventType::DisplaySubmit:
    writer.u32(event.payload.display_submit.display_id);
    break;
case EventType::DisplayAck:
    writer.u32(event.payload.display_ack.display_id);
    writer.u8(event.payload.display_ack.outcome);
    break;
case EventType::FileOpBegin:
    writer.u32(event.payload.file_op_begin.file_operation_id);
    writer.u8(event.payload.file_op_begin.operation);
    break;
case EventType::FileOpEnd:
    writer.u32(event.payload.file_op_end.file_operation_id);
    writer.u8(event.payload.file_op_end.operation);
    writer.u8(event.payload.file_op_end.outcome);
    writer.u32(event.payload.file_op_end.byte_count);
    break;
case EventType::RuntimeIdentity:
    writer.u32(event.payload.runtime_identity.identity_namespace);
    writer.u32(event.payload.runtime_identity.firmware_build_id);
    writer.u32(event.payload.runtime_identity.core_api_family_id);
    writer.u32(event.payload.runtime_identity.input_host_family_id);
    writer.u32(event.payload.runtime_identity.resource_generation);
    writer.u32(event.payload.runtime_identity.rule_policy_generation);
    writer.u32(event.payload.runtime_identity.runtime_config_id);
    writer.u8(event.payload.runtime_identity.identity_scope);
    writer.u8(event.payload.runtime_identity.p2);
    writer.u8(event.payload.runtime_identity.p3);
    writer.u32(event.payload.runtime_identity.prefix_slots);
    writer.u32(event.payload.runtime_identity.tf_frequency_hz);
    writer.u32(event.payload.runtime_identity.profile_id);
    writer.u32(event.payload.runtime_identity.locate);
    writer.u8(event.payload.runtime_identity.prefetch);
    writer.u8(event.payload.runtime_identity.learning_mode);
    break;
default: return {EncodeStatus::InvalidEvent, 0};
}
