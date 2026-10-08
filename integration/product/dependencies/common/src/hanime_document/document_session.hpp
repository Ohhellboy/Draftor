#pragma once

#include "persistence_service.hpp"

#include <cstdint>
#include <string_view>

namespace hanime::documents {

enum class RestoreCommand : std::uint8_t { Latest, Manual, BeforeRollback };

struct RestoreToken {
  bool valid = false;
  std::uint64_t token_serial = 0;
  DocumentId document_id{};
  std::uint64_t editor_revision = 0;
  std::uint64_t session_serial = 0;
  std::uint64_t mount_epoch = 0;
  std::uint64_t latest_body_generation = 0;
  RestoreCommand command = RestoreCommand::Latest;
  Snapshot target{};
  RestoreSource target_source = RestoreSource::None;
};

// Retires an invalidated restore target and returns its text allocation to the
// allocator. Assigning an empty token is insufficient on the target libstdc++
// when the source string uses SSO.
void retireRestoreToken(RestoreToken* token) noexcept;

struct SessionState {
  bool has_current = false;
  DocumentId current_id{};
  bool body_dirty = false;
  bool position_recorded = true;
  bool medium_available = false;
  bool busy = false;
  bool background_save_active = false;
  std::uint64_t session_serial = 0;
};

class DocumentSession {
 public:
  DocumentSession(DocumentCatalog& catalog, PersistenceService& persistence,
                  EditorPort& editor) noexcept
      : catalog_(catalog), persistence_(persistence), editor_(editor) {}

  OperationResult startupRestore();
  OperationResult open(const DocumentId& id, std::uint32_t now_ms);
  OperationResult create(std::string_view name, std::uint32_t now_ms,
                         DocumentId* created_id = nullptr);
  OperationResult startCreate(std::string_view name, std::uint32_t now_ms);
  OperationResult advanceCreate();
  bool creating() const noexcept { return create_phase_!=CreatePhase::Idle; }
  void abortCreate() noexcept;
  const char* createStage() const noexcept;
  OperationResult moveToTrash(const DocumentId& id, std::uint32_t now_ms);
  OperationResult restoreFromTrash(const DocumentId& id);
  Status rename(const DocumentId& id, std::string_view name) { return catalog_.rename(id, name); }

  Status prepareRestore(RestoreCommand command, RestoreToken* out);
  OperationResult confirmRestore(RestoreToken* token, std::uint32_t now_ms);
  void cancelRestore(RestoreToken* token) noexcept {
    retireRestoreToken(token);
  }

  void onBodyMutation(std::uint32_t now_ms) noexcept;
  void onUserActivity(std::uint32_t now_ms) noexcept { persistence_.onUserActivity(now_ms); }
  void onPositionChanged(std::uint32_t now_ms) noexcept { persistence_.onPositionChanged(now_ms); }
  OperationResult tick(std::uint32_t now_ms, const SaveGates& gates) {
    if(external_create_)return {Status::Busy,"external_create"};
    return persistence_.tick(now_ms, gates, editor_);
  }
  OperationResult manualSave(std::uint32_t now_ms) {
    return external_create_ || persistence_.backgroundSaveActive()
        ? OperationResult{Status::Busy, "background_save"}
        : persistence_.saveCurrentManual(now_ms, editor_);
  }

  bool hasCurrent() const noexcept { return !current_id_.empty(); }
  const DocumentId& currentId() const noexcept { return current_id_; }
  std::uint64_t sessionSerial() const noexcept { return session_serial_; }
  bool busy() const noexcept { return busy_; }
  // One external initial-body writer shares the ordinary session owner. It
  // deliberately does not install a document or alter CURRENT.
  Status acquireExternalCreate() noexcept;
  void releaseExternalCreate() noexcept;
  SessionState state() const noexcept;

 private:
  OperationResult saveOutgoingIfDirty(std::uint32_t now_ms);
  OperationResult installAndCommitCurrent(const DocumentId& id, Snapshot prepared,
                                          bool touch_order,
                                          std::uint64_t latest_body_generation = 0U,
                                          RestoreSource body_source = RestoreSource::None);
  LoadResult loadRestoreTarget(RestoreCommand command);

  DocumentCatalog& catalog_;
  PersistenceService& persistence_;
  EditorPort& editor_;
  DocumentId current_id_{};
  std::uint64_t session_serial_ = 1U;
  std::uint64_t token_serial_ = 0U;
  bool busy_ = false;
  bool external_create_ = false;
  enum class CreatePhase : std::uint8_t { Idle,Plan,Collection,Intent,Initial,Publish,Clear,Install,Current,Finish };
  CreatePhase create_phase_=CreatePhase::Idle;
  CreateIntent create_intent_{};
  draftor::memory::String create_name_,create_payload_; // <=96 B name / <=320 B record payload; never body
  CooperativeRecordWriter create_writer_{};
  std::uint64_t create_generation_=0,create_revision_=0;
  bool create_resumed_=false,create_writing_=false,create_published_=false,create_restart_=false;

};

}  // namespace hanime::documents
