#include "../../core_memory.hpp"
#pragma once

#include "autosave_scheduler.hpp"
#include "cooperative_record_writer.hpp"
#include "document_catalog.hpp"
#include "editor_port.hpp"

#include <array>
#include <algorithm>
#include <cstdint>

namespace hanime::documents {

struct SaveBufferConfig {
  std::size_t body_bytes = kMaxDocumentBytes;
  std::size_t snapshot_bytes = kMaxDocumentBytes;
  char* borrowed_snapshot = nullptr; // Caller retains ownership/lifetime.
  bool retain_snapshot = true;
};

class PersistenceService {
 public:
  PersistenceService(StoragePort& storage, DocumentCatalog& catalog,
                     AutosaveConfig config = {}, SaveBufferConfig buffers = {}) noexcept
      : storage_(storage), catalog_(catalog), records_(storage), scheduler_(config), buffers_(buffers) {}

  Status save(const DocumentId& id, SaveLane lane, const Snapshot& snapshot,
              std::uint64_t* committed_generation = nullptr);
  Status saveView(const DocumentId& id, SaveLane lane, const EditorView& snapshot,
                  std::uint64_t* committed_generation = nullptr);
  Status startBlankInitial(const DocumentId& id, CooperativeRecordWriter& writer, draftor::memory::String& prefix);
  Status saveInitial(const DocumentId& id, const Snapshot& snapshot);
  LoadResult loadLatest(const DocumentId& id);
  LoadResult loadLane(const DocumentId& id, SaveLane lane);

  Status savePosition(const DocumentId& id, const Snapshot& body);
  Status savePosition(const DocumentId& id, std::uint64_t body_generation,
                      std::uint32_t text_crc32, std::size_t cursor_byte,
                      VisualAnchor viewport);
  Status applyPosition(const DocumentId& id, Snapshot* body);

  void bindCurrent(const DocumentId& id, std::uint64_t session_serial,
                   std::uint64_t body_generation = 0U,
                   std::uint32_t body_crc32 = 0U,
                   std::uint64_t body_revision = 0U,
                   std::uint64_t latest_body_generation = 0U,
                   RestoreSource body_source = RestoreSource::None) noexcept;
  void unbindCurrent() noexcept;
  void onBodyMutation(std::uint32_t now_ms, std::uint64_t body_revision) noexcept;
  void onUserActivity(std::uint32_t now_ms) noexcept {
    scheduler_.onUserActivity(now_ms);
    last_input_activity_ms_ = now_ms;
  }
  void onPositionChanged(std::uint32_t now_ms) noexcept;
  bool bodySaveDue(std::uint32_t now_ms, const SaveGates& gates,
                   const EditorPort& editor) const noexcept;
  OperationResult tick(std::uint32_t now_ms, const SaveGates& gates, EditorPort& editor);
  OperationResult saveCurrentManual(std::uint32_t now_ms, EditorPort& editor);
  OperationResult saveCurrentAuto(std::uint32_t now_ms, EditorPort& editor);
  OperationResult flushCurrentPosition(EditorPort& editor);

  // S3: manual save reuses the verified cooperative writer and immutable snapshot.
  OperationResult startManualSave(std::uint32_t now_ms, EditorPort& editor) {
    return startBodyJob(now_ms, AutosaveTrigger::Idle, editor, SaveLane::Manual);
  }
  // Use the existing cooperative AUTO writer before leaving a dirty document.
  OperationResult startAutoSave(std::uint32_t now_ms, EditorPort& editor) {
    return startBodyJob(now_ms, AutosaveTrigger::Idle, editor, SaveLane::Auto);
  }
  // S3 host catches allocation exceptions outside the core. Retire in-flight I/O,
  // keep dirty state and never acknowledge a failed operation.
  void abortForMemoryFailure() {
    storage_.cooperativeClose();
    job_.writer = CooperativeRecordWriter{};
    resetJob();
  }
  const void* snapshotAllocation() const noexcept { return snapshotData(); }
  std::size_t snapshotCapacity() const noexcept {
    return snapshotData() ? buffers_.snapshot_bytes : 0U;
  }
  std::size_t bodyCapacity() const noexcept { return std::min(buffers_.body_bytes, kMaxDocumentBytes); }
  bool bodyDirty() const noexcept { return scheduler_.pending(); }
  bool positionDirty() const noexcept {
    return position_revision_ != committed_position_revision_;
  }
  bool backgroundSaveActive() const noexcept { return job_.kind != JobKind::None; }
  bool backgroundBodySaveActive() const noexcept {
    return job_.kind == JobKind::BodyAuto;
  }
  std::uint32_t firstDirtyMs() const noexcept { return scheduler_.firstDirtyMs(); }

  // Test-fixture compatibility hook. Product document restore/switch paths
  // must use their normal commit/install state machine instead.
  void clearPendingForDiagnosticInstall() noexcept;

 private:
  enum class JobKind : std::uint8_t { None, BodyAuto, PositionOnly };
  enum class JobPhase : std::uint8_t {
    None,
    BodyCrc,
    BuildBodyPrefix,
    StartBodyWriter,
    ServiceBodyWriter,
    AcknowledgeBody,
    BuildPosition,
    StartPositionWriter,
    ServicePositionWriter,
    AcknowledgePosition,
    Finish,
  };
  struct BodyCredential {
    bool valid = false;
    DocumentId id{};
    std::uint64_t session_serial = 0U;
    std::uint64_t mount_epoch = 0U;
    std::uint64_t generation = 0U;
    std::uint64_t latest_generation = 0U;
    std::uint32_t text_crc32 = 0U;
    std::uint64_t body_revision = 0U;
    RestoreSource source = RestoreSource::None;
  };
  struct BackgroundJob {
    JobKind kind = JobKind::None;
    SaveLane lane = SaveLane::Auto;
    JobPhase phase = JobPhase::None;
    AutosaveTrigger trigger = AutosaveTrigger::None;
    DocumentId id{};
    std::uint64_t session_serial = 0U;
    std::uint64_t mount_epoch = 0U;
    std::uint64_t editor_revision = 0U;
    std::uint64_t body_revision = 0U;
    std::uint64_t position_revision = 0U;
    std::uint64_t body_generation = 0U;
    std::uint64_t trusted_body_generation = 0U;
    std::uint32_t body_crc32 = 0U;
    std::uint32_t running_crc = 0xFFFFFFFFU;
    std::size_t crc_offset = 0U;
    std::size_t text_size = 0U;
    std::size_t cursor_byte = 0U;
    VisualAnchor viewport{};

    draftor::memory::String snapshot_prefix;
    draftor::memory::String position_payload;
    CooperativeRecordWriter writer{};
    bool body_committed = false;
    bool position_committed = false;
    bool diagnostic_recording = false;
    Status terminal_status = Status::Ok;
    const char* terminal_stage = "complete";
    std::uint32_t started_ms = 0U;
    std::uint32_t last_step_ms = 0U;
    std::uint32_t pause_count = 0U;
    std::uint32_t pause_input = 0U;
    std::uint32_t pause_composition = 0U;
    std::uint32_t pause_storage_gate = 0U;
    std::uint32_t service_steps = 0U;
    std::size_t storage_steps = 0U;
    std::size_t cpu_steps = 0U;
    std::size_t write_chunks = 0U;
    std::size_t max_io_request = 0U;
  };

  OperationResult saveCaptured(std::uint32_t now_ms, SaveLane lane, EditorPort& editor);
  std::uint64_t nextBodyGeneration(const DocumentId& id, Status* status);
  OperationResult startBodyJob(std::uint32_t now_ms, AutosaveTrigger trigger,
                               EditorPort& editor, SaveLane lane = SaveLane::Auto);
  OperationResult startPositionJob(std::uint32_t now_ms, EditorPort& editor);
  OperationResult serviceBackgroundJob(std::uint32_t now_ms, EditorPort& editor);
  OperationResult finishBackgroundJob(std::uint32_t now_ms, EditorPort& editor);
  bool jobIdentityValid() const noexcept;
  bool canService(const SaveGates& gates, const EditorPort& editor) const noexcept;
  void resetJob() noexcept;

  char* snapshotData() const noexcept {
    return buffers_.borrowed_snapshot ? buffers_.borrowed_snapshot : snapshot_.get();
  }
  StoragePort& storage_;
  DocumentCatalog& catalog_;
  RecordStore records_;
  AutosaveScheduler scheduler_;
  DocumentId current_id_{};
  std::uint64_t session_serial_ = 0;
  std::uint64_t latest_dirty_body_revision_ = 0;
  std::uint64_t position_revision_ = 0U;
  std::uint64_t committed_position_revision_ = 0U;
  std::uint32_t position_changed_ms_ = 0;
  std::uint32_t last_input_activity_ms_ = 0U;
  std::uint32_t post_capture_first_dirty_ms_ = 0U;
  BodyCredential credential_{};
  SaveBufferConfig buffers_;
  draftor::memory::ArrayPtr<char> snapshot_;
  BackgroundJob job_{};
};

}  // namespace hanime::documents
