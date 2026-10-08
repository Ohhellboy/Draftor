#include "../../core_memory.hpp"
#include "persistence_service.hpp"

#include "paths.hpp"
#include "save_diagnostic.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace hanime::documents {
namespace {

LoadResult decodeLoaded(PairLoad loaded, const RestoreSource source) {
  LoadResult result{}; result.status = loaded.status; result.source = source;
  result.used_older_slot = loaded.used_older_slot;
  if (loaded.status != Status::Ok && loaded.status != Status::OkDegraded) return result;
  result.status = decodeSnapshotOwned(std::move(loaded.record.payload), &result.snapshot);
  if (result.status == Status::Ok) {
    result.snapshot.generation = loaded.record.generation;
    result.latest_body_generation = loaded.record.generation;
    // decodeSnapshotOwned retains the verified body CRC; do not scan it again.
    if (loaded.status == Status::OkDegraded) result.status = Status::OkDegraded;
  }
  return result;
}

RestoreSource sourceForLane(const SaveLane lane) {
  switch (lane) {
    case SaveLane::Auto: return RestoreSource::Auto;
    case SaveLane::Manual: return RestoreSource::Manual;
    case SaveLane::Protection: return RestoreSource::Protection;
  }
  return RestoreSource::None;
}

void updateCrc32(std::uint32_t* crc, const char* bytes,
                 const std::size_t count) noexcept {
  if (crc == nullptr || (bytes == nullptr && count != 0U)) return;
  for (std::size_t i = 0U; i < count; ++i) {
    *crc ^= static_cast<std::uint8_t>(bytes[i]);
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0U - (*crc & 1U);
      *crc = (*crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
}

}  // namespace

std::uint64_t PersistenceService::nextBodyGeneration(const DocumentId& id, Status* status) {
  save_diag::Scope diagnostic_span(save_diag::Op::GenerationPeek);
  if (!status) return 0U;
  *status = Status::Ok;
  std::uint64_t maximum = 0U;
  for (const SaveLane lane : {SaveLane::Auto, SaveLane::Manual}) {
    const PairPeek loaded = records_.peekPair(paths::lane(id, lane), RecordKind::Snapshot, kMaxDocumentBytes + 128U);
    if (loaded.status == Status::Ok || loaded.status == Status::OkDegraded) maximum = std::max(maximum, loaded.generation);
    else if (loaded.status != Status::NotFound) { *status = loaded.status; return 0U; }
  }
  if (maximum == std::numeric_limits<std::uint64_t>::max()) { *status = Status::Conflict; return 0U; }
  return maximum + 1U;
}

Status PersistenceService::save(const DocumentId& id, const SaveLane lane,
                                const Snapshot& snapshot, std::uint64_t* committed_generation) {
  const EditorView view{snapshot.text, snapshot.cursor_byte, snapshot.viewport,
                        0U, 0U};
  return saveView(id, lane, view, committed_generation);
}

Status PersistenceService::saveView(const DocumentId& id, const SaveLane lane,
                                    const EditorView& snapshot,
                                    std::uint64_t* committed_generation) {
  const Status session = catalog_.validateSession(); if (session != Status::Ok) return session;
  DocumentMetadata meta{}; const Status got = save_diag::measuredStatus(save_diag::Op::MetaLoad, [&] { return catalog_.get(id, &meta); });
  if (got != Status::Ok && got != Status::OkDegraded && lane != SaveLane::Protection) return got;
  if (snapshot.text.size() > bodyCapacity()) return Status::TooLarge;
  if (!save_diag::measured(save_diag::Op::Utf8, snapshot.text.size(), [&] { return isValidUtf8(snapshot.text); }, [](bool ok) { return ok ? 1 : 0; })) return Status::InvalidUtf8;
  if (!isUtf8Boundary(snapshot.text, snapshot.cursor_byte) || !isUtf8Boundary(snapshot.text, snapshot.viewport.byte_offset)) return Status::InvalidCursor;

  std::uint64_t generation = 0U;
  if (lane == SaveLane::Protection) {
    const PairPeek old = records_.peekPair(paths::lane(id, lane), RecordKind::Snapshot, kMaxDocumentBytes + 128U);
    if (old.status == Status::Ok || old.status == Status::OkDegraded) generation = old.generation;
    else if (old.status != Status::NotFound) return old.status;
    if (generation == std::numeric_limits<std::uint64_t>::max()) return Status::Conflict;
    ++generation;
  } else {
    Status next_status = Status::Ok; generation = nextBodyGeneration(id, &next_status);
    if (next_status != Status::Ok) return next_status;
  }
  const draftor::memory::String prefix = encodeSnapshotPrefix(snapshot.text, snapshot.cursor_byte,
                                                  snapshot.viewport);
  const std::string_view parts[2] = {prefix, snapshot.text};
  const Status saved = records_.commitPairParts(paths::lane(id, lane), RecordKind::Snapshot,
                                                generation, parts, 2U);
  if (saved == Status::Ok && committed_generation) *committed_generation = generation;
  return saved;
}

Status PersistenceService::startBlankInitial(const DocumentId& id, CooperativeRecordWriter& writer, draftor::memory::String& prefix) {
  const auto valid=catalog_.validateSession();if(valid!=Status::Ok)return valid;
  // Only a fresh, collision-checked unpublished ID may use this empty snapshot.
  Snapshot blank;prefix=encodeSnapshotPrefix(blank);const std::string_view part=prefix;
  return writer.start(paths::lane(id,SaveLane::Auto),RecordKind::Snapshot,1,&part,1,storage_.mediumSession());
}

Status PersistenceService::saveInitial(const DocumentId& id, const Snapshot& snapshot) {
  if (id.empty()) return Status::Conflict;
  const Status session = catalog_.validateSession(); if (session != Status::Ok) return session;
  if (snapshot.text.size() > bodyCapacity() || !isValidUtf8(snapshot.text) ||
      !isUtf8Boundary(snapshot.text, snapshot.cursor_byte) ||
      !isUtf8Boundary(snapshot.text, snapshot.viewport.byte_offset)) return Status::Corrupt;
  const PairLoad existing = records_.loadPair(paths::lane(id, SaveLane::Auto),
                                              RecordKind::Snapshot, kMaxDocumentBytes + 128U, bodyCapacity() + 30U);
  if (existing.status == Status::Ok || existing.status == Status::OkDegraded) return Status::Ok;
  if (existing.status != Status::NotFound) {
    const Status reset = records_.resetUnrecoverablePair(paths::lane(id, SaveLane::Auto),
                                                         RecordKind::Snapshot,
                                                         kMaxDocumentBytes + 128U);
    if (reset != Status::Ok) return reset;
  }
  const draftor::memory::String prefix = encodeSnapshotPrefix(snapshot);
  const std::string_view parts[2] = {prefix, snapshot.text};
  return records_.commitPairParts(paths::lane(id, SaveLane::Auto), RecordKind::Snapshot,
                                  1U, parts, 2U);
}

LoadResult PersistenceService::loadLane(const DocumentId& id, const SaveLane lane) {
  const Status session = catalog_.validateSession(); if (session != Status::Ok) return {session};
  DocumentMetadata meta{}; const Status got = catalog_.get(id, &meta);
  if (got != Status::Ok && got != Status::OkDegraded) return {got};
  LoadResult result = decodeLoaded(
      records_.loadPair(paths::lane(id, lane), RecordKind::Snapshot,
                        kMaxDocumentBytes + 128U, bodyCapacity() + 30U), sourceForLane(lane));
  if (result.status == Status::Ok || result.status == Status::OkDegraded) {
    const PairPeek auto_peek = records_.peekPair(
        paths::lane(id, SaveLane::Auto), RecordKind::Snapshot,
        kMaxDocumentBytes + 128U);
    const PairPeek manual_peek = records_.peekPair(
        paths::lane(id, SaveLane::Manual), RecordKind::Snapshot,
        kMaxDocumentBytes + 128U);
    result.latest_body_generation = std::max(
        (auto_peek.status == Status::Ok ||
         auto_peek.status == Status::OkDegraded)
            ? auto_peek.generation : 0U,
        (manual_peek.status == Status::Ok ||
         manual_peek.status == Status::OkDegraded)
            ? manual_peek.generation : 0U);
  }
  return result;
}

LoadResult PersistenceService::loadLatest(const DocumentId& id) {
  const Status session = catalog_.validateSession(); if (session != Status::Ok) return {session};
  DocumentMetadata meta{}; const Status got = catalog_.get(id, &meta);
  if (got != Status::Ok && got != Status::OkDegraded) return {got};
  const PairPeek auto_peek = records_.peekPair(paths::lane(id, SaveLane::Auto), RecordKind::Snapshot,
                                               kMaxDocumentBytes + 128U);
  const PairPeek manual_peek = records_.peekPair(paths::lane(id, SaveLane::Manual), RecordKind::Snapshot,
                                                 kMaxDocumentBytes + 128U);
  const bool auto_ok = auto_peek.status == Status::Ok || auto_peek.status == Status::OkDegraded;
  const bool manual_ok = manual_peek.status == Status::Ok || manual_peek.status == Status::OkDegraded;
  const std::uint64_t latest_generation = std::max(
      auto_ok ? auto_peek.generation : 0U,
      manual_ok ? manual_peek.generation : 0U);
  if (auto_ok || manual_ok) {
    const SaveLane first = auto_ok && (!manual_ok || auto_peek.generation >= manual_peek.generation)
        ? SaveLane::Auto : SaveLane::Manual;
    LoadResult selected = decodeLoaded(
        records_.loadPair(paths::lane(id, first), RecordKind::Snapshot,
                          kMaxDocumentBytes + 128U, bodyCapacity() + 30U),
        sourceForLane(first));
    if (selected.status == Status::Ok || selected.status == Status::OkDegraded) {
      selected.latest_body_generation = latest_generation;
      return selected;
    }
    if (selected.status == Status::TooLarge) return selected;
    const bool other_ok = first == SaveLane::Auto ? manual_ok : auto_ok;
    if (other_ok) {
      const SaveLane fallback_lane = first == SaveLane::Auto
          ? SaveLane::Manual : SaveLane::Auto;
      LoadResult fallback = decodeLoaded(
          records_.loadPair(paths::lane(id, fallback_lane),
                            RecordKind::Snapshot,
                            kMaxDocumentBytes + 128U, bodyCapacity() + 30U),
          sourceForLane(fallback_lane));
      if (fallback.status == Status::Ok) fallback.status = Status::OkDegraded;
      if (fallback.status == Status::OkDegraded) fallback.used_older_slot = true;
      fallback.latest_body_generation = latest_generation;
      return fallback;
    }
    return selected;
  }
  if (auto_peek.status == Status::NotFound && manual_peek.status == Status::NotFound) return {Status::NotFound};
  if (auto_peek.status == Status::Corrupt || manual_peek.status == Status::Corrupt) return {Status::Corrupt};
  return {auto_peek.status != Status::NotFound ? auto_peek.status : manual_peek.status};
}

Status PersistenceService::savePosition(const DocumentId& id, const Snapshot& body) {
  if (body.generation == 0U) return Status::Conflict;
  return savePosition(id, body.generation,
                      body.text_crc32 ? body.text_crc32 : crc32(body.text),
                      body.cursor_byte, body.viewport);
}

Status PersistenceService::savePosition(const DocumentId& id,
                                        const std::uint64_t body_generation,
                                        const std::uint32_t text_crc32,
                                        const std::size_t cursor_byte,
                                        const VisualAnchor viewport) {
  PositionRecord value{}; value.document_id = id;
  value.body_generation = body_generation; value.text_crc32 = text_crc32;
  value.cursor_byte = cursor_byte; value.viewport = viewport;
  save_diag::Scope diagnostic_span(save_diag::Op::PositionSave);
  const PairLoad old = save_diag::measuredResult(save_diag::Op::PositionLoad, [&] { return records_.loadPair(paths::position(id), RecordKind::Position, 128U); });
  std::uint64_t generation = 1U;
  if (old.status == Status::Ok || old.status == Status::OkDegraded) generation = old.record.generation + 1U;
  else if (old.status != Status::NotFound && old.status != Status::Corrupt) return old.status;
  return records_.commitPair(paths::position(id), RecordKind::Position, generation, encodePosition(value));
}

Status PersistenceService::applyPosition(const DocumentId& id, Snapshot* body) {
  if (!body) return Status::Conflict;
  const PairLoad loaded = records_.loadPair(paths::position(id), RecordKind::Position, 128U);
  if (loaded.status == Status::NotFound) return Status::NotFound;
  if (loaded.status != Status::Ok && loaded.status != Status::OkDegraded) return loaded.status;
  PositionRecord value{}; const Status decoded = decodePosition(loaded.record.payload, &value); if (decoded != Status::Ok) return decoded;
  const std::uint32_t body_crc = body->text_crc32 ? body->text_crc32 : crc32(body->text);
  if (value.document_id != id || value.body_generation != body->generation || value.text_crc32 != body_crc ||
      !isUtf8Boundary(body->text, value.cursor_byte) || !isUtf8Boundary(body->text, value.viewport.byte_offset)) return Status::Conflict;
  body->cursor_byte = value.cursor_byte; body->viewport = value.viewport;
  return loaded.status;
}

void PersistenceService::bindCurrent(const DocumentId& id,
                                     const std::uint64_t session_serial,
                                     const std::uint64_t body_generation,
                                     const std::uint32_t body_crc32,
                                     const std::uint64_t body_revision,
                                     const std::uint64_t latest_body_generation,
                                     const RestoreSource body_source) noexcept {
  resetJob();
  current_id_ = id;
  session_serial_ = session_serial;
  latest_dirty_body_revision_ = 0U;
  position_revision_ = committed_position_revision_ = 0U;
  scheduler_.clearPending();
  credential_ = {};
  if (!id.empty() &&
      (body_generation != 0U || latest_body_generation != 0U)) {
    credential_.valid = body_generation != 0U;
    credential_.id = id;
    credential_.session_serial = session_serial;
    credential_.mount_epoch = catalog_.boundSession().mount_epoch;
    credential_.generation = body_generation;
    credential_.latest_generation = std::max(body_generation,
        latest_body_generation);
    credential_.text_crc32 = body_crc32;
    credential_.body_revision = body_revision;
    credential_.source = body_source;
  }
}
void PersistenceService::unbindCurrent() noexcept {
  resetJob();
  current_id_ = {}; ++session_serial_; latest_dirty_body_revision_ = 0U;
  position_revision_ = committed_position_revision_ = 0U;
  credential_ = {};
  scheduler_.clearPending();
}
void PersistenceService::onBodyMutation(
    const std::uint32_t now_ms,
    const std::uint64_t body_revision) noexcept {
  if (current_id_.empty()) return;
  if (backgroundBodySaveActive() && body_revision != job_.body_revision &&
      post_capture_first_dirty_ms_ == 0U) {
    post_capture_first_dirty_ms_ = now_ms;
  }
  latest_dirty_body_revision_ = body_revision;
  scheduler_.onDocumentMutation(now_ms);
}
void PersistenceService::onPositionChanged(const std::uint32_t now_ms) noexcept {
  if (current_id_.empty()) return;
  ++position_revision_;
  if (position_revision_ == 0U) ++position_revision_;
  position_changed_ms_ = now_ms;
}

OperationResult PersistenceService::saveCaptured(const std::uint32_t now_ms, const SaveLane lane, EditorPort& editor) {
  if (current_id_.empty()) return {Status::NoCurrentDocument, "identity"};
  if (editor.compositionActive()) return {Status::CompositionActive, "gate"};
  const bool was_pending = scheduler_.pending();
  EditorView captured{}; const Status captured_status = save_diag::measuredStatus(save_diag::Op::Capture, [&] { return editor.capture(&captured); });
  save_diag::recorder.capture(captured, captured_status);
  if (captured_status != Status::Ok) return {captured_status, "capture"};
  const DocumentId target = current_id_; const std::uint64_t serial = session_serial_;
  std::uint64_t generation = 0U; const Status saved = save_diag::measuredStatus(save_diag::Op::BodySave, [&] { return saveView(target, lane, captured, &generation); });
  if (target != current_id_ || serial != session_serial_) return {Status::SessionChanged, "completion_identity", saved == Status::Ok};
  const bool body_revision_advanced =
      editor.bodyRevision() != captured.body_revision;
  if (saved != Status::Ok) {
    if (lane == SaveLane::Auto) {
      scheduler_.onSaveResult(now_ms, false);
    }
    // Manual failure leaves exactly the pre-attempt dirty state intact. A
    // failed F6 on an already-clean document must not invent unsaved edits.
    return {saved, "body_write"};
  }
  save_diag::measuredVoid(save_diag::Op::BodyAck, [&] {
    editor.onBodyCommitted(captured.body_revision, lane);
  });
  credential_.valid = true;
  credential_.id = target;
  credential_.session_serial = serial;
  credential_.mount_epoch = catalog_.boundSession().mount_epoch;
  credential_.generation = generation;
  credential_.latest_generation = generation;
  credential_.text_crc32 = save_diag::measured(
      save_diag::Op::BodyCrc, captured.text.size(),
      [&] { return crc32(captured.text); }, [](auto) { return 1; });
  credential_.body_revision = captured.body_revision;
  credential_.source = lane == SaveLane::Auto
      ? RestoreSource::Auto : RestoreSource::Manual;
  if (lane == SaveLane::Auto) {
    scheduler_.onSaveResult(now_ms, true);
  } else {
    // Manual save acknowledges precisely the captured revision.  Clear the
    // old pending interval, then start a new one when the editor advanced
    // while the synchronous write was completing.
    if (was_pending || body_revision_advanced) scheduler_.clearPending();
  }
  if (body_revision_advanced) scheduler_.onDocumentMutation(now_ms);
  save_diag::Phase diagnostic_position_phase(1);
  const std::uint64_t captured_position_revision = position_revision_;
  const Status position = savePosition(target, generation, credential_.text_crc32,
                                       captured.cursor_byte, captured.viewport);
  save_diag::recorder.position(position);
  if (position == Status::Ok && position_revision_ == captured_position_revision) {
    committed_position_revision_ = captured_position_revision;
  }
  if (position != Status::Ok) position_changed_ms_ = now_ms;
  return {position == Status::Ok ? Status::Ok : Status::Partial,
          position == Status::Ok ? "complete" : "position_write", true, false};
}

void PersistenceService::clearPendingForDiagnosticInstall() noexcept {
  resetJob();
  scheduler_.clearPending();
  committed_position_revision_ = position_revision_;
}

OperationResult PersistenceService::saveCurrentManual(const std::uint32_t now_ms, EditorPort& editor) { return saveCaptured(now_ms, SaveLane::Manual, editor); }
OperationResult PersistenceService::saveCurrentAuto(const std::uint32_t now_ms, EditorPort& editor) { return saveCaptured(now_ms, SaveLane::Auto, editor); }

OperationResult PersistenceService::flushCurrentPosition(EditorPort& editor) {
  if (current_id_.empty()) return {Status::NoCurrentDocument, "identity"};
  if (!positionDirty()) return {Status::Ok, "clean"};
  if (editor.compositionActive()) return {Status::CompositionActive, "gate"};
  EditorView captured{};
  const Status captured_status = save_diag::measuredStatus(save_diag::Op::Capture, [&] { return editor.capture(&captured); });
  save_diag::recorder.capture(captured, captured_status);
  if (captured_status != Status::Ok) {
    return {captured_status, "position_capture"};
  }
  const DocumentId target = current_id_;
  const std::uint64_t serial = session_serial_;
  if (!credential_.valid || credential_.id != target ||
      credential_.session_serial != serial ||
      credential_.mount_epoch != catalog_.boundSession().mount_epoch ||
      credential_.body_revision != captured.body_revision) {
    return {Status::Conflict, "position_body_credential"};
  }
  const std::uint64_t captured_position_revision = position_revision_;
  const Status saved = savePosition(
      target, credential_.generation, credential_.text_crc32,
      captured.cursor_byte, captured.viewport);
  if (target != current_id_ || serial != session_serial_) {
    return {Status::SessionChanged, "completion_identity"};
  }
  save_diag::recorder.position(saved);
  if (saved == Status::Ok && position_revision_ == captured_position_revision) {
    committed_position_revision_ = captured_position_revision;
  }
  return {saved, saved == Status::Ok ? "complete" : "position_write"};
}

bool PersistenceService::bodySaveDue(
    const std::uint32_t now_ms,
    const SaveGates& gates,
    const EditorPort& editor) const noexcept {
  const bool can_save = canService(gates, editor);
  if (backgroundBodySaveActive()) return can_save;
  if (backgroundSaveActive()) return false;
  return scheduler_.due(now_ms, can_save) != AutosaveTrigger::None;
}

bool PersistenceService::canService(const SaveGates& gates,
                                    const EditorPort& editor) const noexcept {
  return gates.storage_granted && !gates.composition_active &&
         !gates.input_queue_pending && !editor.compositionActive() &&
         !editor.inputQueuePending();
}

bool PersistenceService::jobIdentityValid() const noexcept {
  return job_.id == current_id_ && job_.session_serial == session_serial_ &&
         job_.mount_epoch == catalog_.boundSession().mount_epoch &&
         catalog_.validateSession() == Status::Ok;
}

void PersistenceService::resetJob() noexcept {
  if (!buffers_.retain_snapshot) snapshot_.reset();
  job_.kind = JobKind::None;
  job_.phase = JobPhase::None;
  job_.snapshot_prefix.clear();
  job_.position_payload.clear();
  job_.diagnostic_recording = false;
  job_.body_committed = false;
  job_.position_committed = false;
  job_.terminal_status = Status::Ok;
  job_.terminal_stage = "complete";
  job_.pause_count = 0U;
  job_.pause_input = job_.pause_composition = job_.pause_storage_gate = 0U;
  job_.service_steps = 1U;  // snapshot capture/start is the first CPU service
  post_capture_first_dirty_ms_ = 0U;
}

OperationResult PersistenceService::startBodyJob(
    const std::uint32_t now_ms, const AutosaveTrigger trigger,
    EditorPort& editor, const SaveLane lane) {
  if (backgroundSaveActive()) return {Status::Busy, "background_active"};
  if (current_id_.empty()) return {Status::NoCurrentDocument, "identity"};
  const DocumentId target = current_id_;
  const std::uint64_t serial = session_serial_;
  const std::uint64_t mount = catalog_.boundSession().mount_epoch;
  EditorView captured{};
  const Status captured_status = save_diag::measuredStatus(
      save_diag::Op::Capture, [&] { return editor.capture(&captured); });
  if (captured_status != Status::Ok) return {captured_status, "capture"};
  if (captured.text.size() > bodyCapacity()) return {Status::TooLarge, "capture"};
  if (!isValidUtf8(captured.text)) return {Status::InvalidUtf8, "capture"};
  if (!isUtf8Boundary(captured.text, captured.cursor_byte) ||
      !isUtf8Boundary(captured.text, captured.viewport.byte_offset)) {
    return {Status::InvalidCursor, "capture"};
  }
  if (captured.text.size() > buffers_.snapshot_bytes) return {Status::TooLarge, "snapshot_budget"};
  if (!snapshotData()) {
    snapshot_.reset(draftor::memory::array<char>(buffers_.snapshot_bytes));
    if (!snapshotData()) return {Status::ResourceExhausted, "snapshot_memory"};
  }
  const bool credential_matches = credential_.id == target &&
      credential_.session_serial == serial &&
      credential_.mount_epoch == mount &&
      credential_.latest_generation != 0U;
  if (credential_matches &&
      credential_.latest_generation ==
          std::numeric_limits<std::uint64_t>::max()) {
    return {Status::Conflict, "generation"};
  }
  const bool diagnostic_recording = save_diag::recorder.begin(
      save_diag::Kind::BodyAuto,
      trigger == AutosaveTrigger::HardDeadline
          ? save_diag::Trigger::HardDeadline : save_diag::Trigger::Idle,
      target, serial, mount);
  save_diag::recorder.capture(captured, captured_status);
  job_.kind = JobKind::BodyAuto;
  job_.lane = lane;
  job_.phase = JobPhase::BodyCrc;
  job_.trigger = trigger;
  job_.id = target;
  job_.session_serial = serial;
  job_.mount_epoch = mount;
  job_.editor_revision = captured.revision;
  job_.body_revision = captured.body_revision;
  job_.position_revision = position_revision_;
  job_.cursor_byte = captured.cursor_byte;
  job_.viewport = captured.viewport;
  job_.text_size = captured.text.size();
  if (!captured.text.empty()) {
    std::memcpy(snapshotData(), captured.text.data(), captured.text.size());
  }
  job_.running_crc = 0xFFFFFFFFU;
  job_.crc_offset = 0U;
  job_.body_crc32 = 0U;
  job_.started_ms = now_ms;
  job_.last_step_ms = now_ms;
  job_.terminal_status = Status::Ok;
  job_.terminal_stage = "complete";
  job_.body_committed = false;
  job_.position_committed = false;
  job_.pause_count = 0U;
  job_.pause_input = job_.pause_composition = job_.pause_storage_gate = 0U;
  job_.service_steps = 1U;  // position capture/start is the first CPU service
  job_.storage_steps = job_.cpu_steps = job_.write_chunks = 0U;
  job_.max_io_request = 0U;
  post_capture_first_dirty_ms_ = 0U;
  job_.body_generation = credential_matches
      ? credential_.latest_generation + 1U : 0U;
  job_.trusted_body_generation = credential_.valid &&
      credential_.source == (lane == SaveLane::Manual ? RestoreSource::Manual : RestoreSource::Auto)
      ? credential_.generation : 0U;
  job_.diagnostic_recording = diagnostic_recording;
  return {Status::Busy, "background_started"};
}

OperationResult PersistenceService::startPositionJob(
    const std::uint32_t now_ms, EditorPort& editor) {
  if (backgroundSaveActive()) return {Status::Busy, "background_active"};
  if (current_id_.empty()) return {Status::NoCurrentDocument, "identity"};
  const DocumentId target = current_id_;
  const std::uint64_t serial = session_serial_;
  const std::uint64_t mount = catalog_.boundSession().mount_epoch;
  EditorView captured{};
  const Status captured_status = save_diag::measuredStatus(
      save_diag::Op::Capture, [&] { return editor.capture(&captured); });
  if (captured_status != Status::Ok) return {captured_status, "position_capture"};
  if (!credential_.valid || credential_.id != target ||
      credential_.session_serial != serial ||
      credential_.mount_epoch != mount ||
      credential_.body_revision != captured.body_revision) {
    return {Status::Conflict, "position_body_credential"};
  }
  const bool diagnostic_recording = save_diag::recorder.begin(
      save_diag::Kind::PositionOnly, save_diag::Trigger::PositionIdle,
      target, serial, mount);
  save_diag::recorder.capture(captured, captured_status);
  job_.kind = JobKind::PositionOnly;
  job_.phase = JobPhase::BuildPosition;
  job_.trigger = AutosaveTrigger::None;
  job_.id = target;
  job_.session_serial = serial;
  job_.mount_epoch = mount;
  job_.editor_revision = captured.revision;
  job_.body_revision = captured.body_revision;
  job_.position_revision = position_revision_;
  job_.cursor_byte = captured.cursor_byte;
  job_.viewport = captured.viewport;
  job_.body_generation = credential_.generation;
  job_.body_crc32 = credential_.text_crc32;
  job_.started_ms = now_ms;
  job_.last_step_ms = now_ms;
  job_.terminal_status = Status::Ok;
  job_.terminal_stage = "complete";
  job_.body_committed = false;
  job_.position_committed = false;
  job_.pause_count = 0U;
  job_.pause_input = job_.pause_composition = job_.pause_storage_gate = 0U;
  job_.service_steps = 0U;
  job_.storage_steps = job_.cpu_steps = job_.write_chunks = 0U;
  job_.max_io_request = 0U;
  job_.diagnostic_recording = diagnostic_recording;
  return {Status::Busy, "position_background_started"};
}

OperationResult PersistenceService::serviceBackgroundJob(
    const std::uint32_t now_ms, EditorPort& editor) {
  if (!backgroundSaveActive()) return {Status::Ok, "idle"};
  if (!jobIdentityValid()) {
    job_.terminal_status = Status::SessionChanged;
    job_.terminal_stage = "completion_identity";
    job_.phase = JobPhase::Finish;
  }
  ++job_.service_steps;
  job_.last_step_ms = now_ms;

  switch (job_.phase) {
    case JobPhase::BodyCrc: {
      const std::size_t count = std::min<std::size_t>(
          CooperativeRecordWriter::kChunkBytes,
          job_.text_size - job_.crc_offset);
      if (count != 0U) {
        updateCrc32(&job_.running_crc,
                    snapshotData() + job_.crc_offset, count);
        job_.crc_offset += count;
      }
      if (job_.crc_offset == job_.text_size) {
        job_.body_crc32 = ~job_.running_crc;
        job_.phase = JobPhase::BuildBodyPrefix;
      }
      return {Status::Busy, "body_crc"};
    }
    case JobPhase::BuildBodyPrefix:
      job_.snapshot_prefix = encodeSnapshotPrefix(
          job_.text_size, job_.body_crc32, job_.cursor_byte, job_.viewport);
      job_.phase = JobPhase::StartBodyWriter;
      return {Status::Busy, "snapshot_prefix"};
    case JobPhase::StartBodyWriter: {
      const std::string_view parts[2] = {
          job_.snapshot_prefix,
          std::string_view(snapshotData(), job_.text_size)};
      const Status started = job_.writer.start(
          paths::lane(job_.id, job_.lane), RecordKind::Snapshot,
          job_.body_generation, parts, 2U,
          storage_.mediumSession(), job_.trusted_body_generation);
      if (started != Status::Ok) {
        scheduler_.onSaveResult(now_ms, false);
        job_.terminal_status = started;
        job_.terminal_stage = "body_writer_start";
        job_.phase = JobPhase::Finish;
      } else {
        job_.phase = JobPhase::ServiceBodyWriter;
      }
      return {Status::Busy, "body_writer_start"};
    }
    case JobPhase::ServiceBodyWriter: {
      const OperationResult step = job_.writer.service(storage_);
      if (job_.writer.active()) return step;
      job_.storage_steps += job_.writer.storageSteps();
      job_.cpu_steps += job_.writer.cpuSteps();
      job_.write_chunks += job_.writer.writeChunks();
      job_.max_io_request = std::max(job_.max_io_request,
                                    job_.writer.maxIoRequest());
      if (job_.writer.status() != Status::Ok) {
        scheduler_.onSaveResult(now_ms, false);
        job_.terminal_status = job_.writer.status();
        job_.terminal_stage = "body_write";
        job_.phase = JobPhase::Finish;
      } else {
        job_.body_generation = job_.writer.generation();
        job_.body_committed = true;
        job_.phase = JobPhase::AcknowledgeBody;
      }
      return {Status::Busy, "body_writer_complete", job_.body_committed};
    }
    case JobPhase::AcknowledgeBody: {
      editor.onBodyCommitted(job_.body_revision, job_.lane);
      credential_.valid = true;
      credential_.id = job_.id;
      credential_.session_serial = job_.session_serial;
      credential_.mount_epoch = job_.mount_epoch;
      credential_.generation = job_.body_generation;
      credential_.latest_generation = job_.body_generation;
      credential_.text_crc32 = job_.body_crc32;
      credential_.body_revision = job_.body_revision;
      credential_.source = job_.lane == SaveLane::Manual ? RestoreSource::Manual : RestoreSource::Auto;
      const bool newer_dirty = editor.bodyRevision() != job_.body_revision;
      scheduler_.onSaveResult(
          now_ms, true, newer_dirty,
          post_capture_first_dirty_ms_ != 0U
              ? post_capture_first_dirty_ms_ : now_ms);
      job_.phase = JobPhase::BuildPosition;
      return {Status::Busy, "body_ack", true};
    }
    case JobPhase::BuildPosition: {
      PositionRecord value{};
      value.document_id = job_.id;
      value.body_generation = job_.body_generation;
      value.text_crc32 = job_.body_crc32;
      value.cursor_byte = job_.cursor_byte;
      value.viewport = job_.viewport;
      job_.position_payload = encodePosition(value);
      job_.phase = JobPhase::StartPositionWriter;
      return {Status::Busy, "position_encode", job_.body_committed};
    }
    case JobPhase::StartPositionWriter: {
      const std::string_view part = job_.position_payload;
      const Status started = job_.writer.start(
          paths::position(job_.id), RecordKind::Position, 0U,
          &part, 1U, storage_.mediumSession());
      if (started != Status::Ok) {
        save_diag::recorder.position(started);
        job_.terminal_status = job_.body_committed ? Status::Partial : started;
        job_.terminal_stage = "position_writer_start";
        job_.phase = JobPhase::Finish;
      } else {
        job_.phase = JobPhase::ServicePositionWriter;
      }
      return {Status::Busy, "position_writer_start", job_.body_committed};
    }
    case JobPhase::ServicePositionWriter: {
      save_diag::Phase diagnostic_position_phase(1);
      const OperationResult step = job_.writer.service(storage_);
      if (job_.writer.active()) return step;
      job_.storage_steps += job_.writer.storageSteps();
      job_.cpu_steps += job_.writer.cpuSteps();
      job_.write_chunks += job_.writer.writeChunks();
      job_.max_io_request = std::max(job_.max_io_request,
                                    job_.writer.maxIoRequest());
      if (job_.writer.status() != Status::Ok) {
        save_diag::recorder.position(job_.writer.status());
        job_.terminal_status = job_.body_committed
            ? Status::Partial : job_.writer.status();
        job_.terminal_stage = "position_write";
        job_.phase = JobPhase::Finish;
      } else {
        save_diag::recorder.position(Status::Ok);
        job_.position_committed = true;
        job_.phase = JobPhase::AcknowledgePosition;
      }
      return {Status::Busy, "position_writer_complete", job_.body_committed};
    }
    case JobPhase::AcknowledgePosition:
      if (position_revision_ == job_.position_revision) {
        committed_position_revision_ = job_.position_revision;
      }
      job_.phase = JobPhase::Finish;
      return {Status::Busy, "position_ack", job_.body_committed};
    case JobPhase::Finish:
      return finishBackgroundJob(now_ms, editor);
    case JobPhase::None:
      break;
  }
  return {Status::Conflict, "job_phase"};
}

OperationResult PersistenceService::finishBackgroundJob(
    const std::uint32_t now_ms, EditorPort& editor) {
  const bool identity_same = jobIdentityValid();
  OperationResult result{job_.terminal_status, job_.terminal_stage,
                         job_.body_committed, false};
  if (job_.terminal_status == Status::Ok) {
    result = {Status::Ok, "complete", job_.body_committed, false};
  }
  if (!job_.position_committed && positionDirty()) {
    position_changed_ms_ = now_ms;
  }
  if (job_.diagnostic_recording) {
    save_diag::recorder.cooperative(
        job_.service_steps, job_.pause_count, job_.pause_input,
        job_.pause_composition, job_.pause_storage_gate, job_.storage_steps,
        job_.service_steps >= job_.storage_steps
            ? job_.service_steps - job_.storage_steps : job_.cpu_steps,
        job_.write_chunks, job_.max_io_request);
    save_diag::recorder.finish(result, editor.revision(),
                               editor.bodyRevision(), identity_same,
                               scheduler_.pending(), positionDirty(),
                               job_.position_revision, position_revision_);
  }
  resetJob();
  return result;
}

OperationResult PersistenceService::tick(const std::uint32_t now_ms,
                                         const SaveGates& gates,
                                         EditorPort& editor) {
  const bool can_save = canService(gates, editor);
  if (backgroundSaveActive()) {
    if (!can_save) {
      ++job_.pause_count;
      if (gates.input_queue_pending || editor.inputQueuePending()) {
        ++job_.pause_input;
      } else if (gates.composition_active || editor.compositionActive()) {
        ++job_.pause_composition;
      } else {
        ++job_.pause_storage_gate;
      }
      return {Status::Busy, "background_paused", job_.body_committed};
    }
    save_diag::Scope service_step(save_diag::Op::ServiceStep);
    return serviceBackgroundJob(now_ms, editor);
  }
  const AutosaveTrigger trigger = scheduler_.due(now_ms, can_save);
  if (trigger != AutosaveTrigger::None) {
    return startBodyJob(now_ms, trigger, editor);
  }
  if (positionDirty() && !scheduler_.pending() && can_save &&
      static_cast<std::uint32_t>(now_ms - position_changed_ms_) >= 5000U) {
    const OperationResult started = startPositionJob(now_ms, editor);
    if (!started) position_changed_ms_ = now_ms;
    return started;
  }
  return {Status::Ok, "idle"};
}

}  // namespace hanime::documents
