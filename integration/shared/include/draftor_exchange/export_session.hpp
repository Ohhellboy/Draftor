#pragma once
#include "txt.hpp"
#include <array>
#include <cstdint>

namespace draftor::exchange {
struct Identity {
  std::array<std::uint8_t, 16> collection{}, document{};
  std::uint64_t mount = 0, document_session = 0, revision_epoch = 0;
  std::uint32_t body_revision = 0;
  bool operator==(const Identity& b) const noexcept;
  bool operator!=(const Identity& b) const noexcept { return !(*this == b); }
};
struct OwnerState {
  Identity identity{};
  std::size_t body_bytes = 0;
  bool has_current = false, composition = false, semantic_pending = false;
  bool file_operation = false, pending_body_edits = false;
  bool revision_reliable = true, save_busy = false;
  bool body_dirty = false, position_dirty = false;
};
enum class SaveState { Pending, Complete, Failed, Partial };
struct SaveReply {
  SaveState state = SaveState::Complete;
  int native_status = 0;
  bool body_committed = false, position_committed = false;
};
using BorrowConsumer = void (*)(void*, std::string_view);
struct BorrowReply { bool ok = false; std::size_t bytes = 0; };

// Future product-owner seam, NOT an existing R2 API. Calls are serialized,
// non-reentrant; the owner, allocator and module outlive every reader callback.
class OwnerPort {
 public:
  virtual ~OwnerPort() = default;
  virtual OwnerState probe() noexcept = 0;
  virtual bool freeze() noexcept = 0;
  virtual void unfreeze() noexcept = 0;
  virtual SaveReply beginAutoSave() noexcept = 0;
  virtual SaveReply serviceSave() noexcept = 0;
  // Calls consume exactly once with 1..max bytes, entirely within this call.
  // No borrowed pointer is retained by ExportSession.
  virtual BorrowReply borrow(std::size_t offset, std::size_t max,
                             void* context, BorrowConsumer consume) noexcept = 0;
};
class Allocator {
 public:
  virtual ~Allocator() = default;
  virtual char* allocate(std::size_t bytes) noexcept = 0;
  virtual void deallocate(char* data, std::size_t bytes) noexcept = 0;
};
enum class Phase { Idle, Saving, Copying, Ready, Closing, Closed };
enum class SessionError { None, Busy, NoCurrent, Composition, FileOperation,
                          PendingEdits, IdentityChanged, RevisionUncertain,
                          InvalidConfig, SourceTooLarge, SaveFailed, SavePartial,
                          AllocationFailed, ReadFailed, PortContract, Text,
                          GenerationExhausted };
struct SessionConfig {
  std::size_t body_limit, work_bytes = 256;
  // Checkpoint seeds make exhaustion explicit; never reset counters on begin().
  std::uint64_t session_seed = 0, reader_seed = 0;
};
struct ReaderToken {
  std::uint64_t session = 0, generation = 0;
  bool operator==(const ReaderToken& b) const noexcept {
    return session == b.session && generation == b.generation;
  }
};
struct Lease {
  const char* data = nullptr;
  std::size_t size = 0;
  ReaderToken token{};
  Identity identity{};
  std::size_t source_bytes = 0;
};
enum class ReaderEnd { Released, SimulatedSendComplete, SendFailed };
enum class ReaderStatus { Ok, NotReady, Busy, Stale, Exhausted };
struct SessionStats {
  std::size_t source_bytes = 0, output_bytes = 0, allocation_bytes = 0;
  std::size_t borrowed_bytes = 0, max_borrow = 0, borrow_calls = 0;
  std::size_t copy_ticks = 0, save_calls = 0, readers = 0;
};
class ExportSession final {
 public:
  ExportSession(OwnerPort&, Allocator&, SessionConfig) noexcept;
  ~ExportSession(); // Terminates on violation: a live reader MUST first return.
  ExportSession(const ExportSession&) = delete;
  ExportSession& operator=(const ExportSession&) = delete;
  SessionError begin() noexcept;
  Phase tick() noexcept;
  void cancel() noexcept;
  void stop() noexcept;
  // Only Ok writes out. Every non-Ok result preserves ALL caller fields,
  // including a currently active lease's return token. Session state can
  // still retire on identity failure/exhaustion; existing readers must return.
  ReaderStatus acquire(Lease& out) noexcept;
  ReaderStatus release(ReaderToken token, ReaderEnd end = ReaderEnd::Released) noexcept;
  Phase phase() const noexcept { return phase_; }
  SessionError error() const noexcept { return error_; }
  TextFailure textFailure() const noexcept { return text_failure_; }
  SaveReply lastSave() const noexcept { return last_save_; }
  SessionStats stats() const noexcept { return stats_; }
  Identity identity() const noexcept { return identity_; }
  std::uint64_t session() const noexcept { return session_counter_; }
  bool cancelled() const noexcept { return cancelled_; }
  bool readerActive() const noexcept { return reader_active_; }
  bool canDestroy() const noexcept { return !reader_active_; }
  ReaderEnd lastReaderEnd() const noexcept { return reader_end_; }
 private:
  SessionError checkIdentity(const OwnerState&) const noexcept;
  void fail(SessionError) noexcept;
  void closeIfDrained() noexcept;
  bool acceptSave(SaveReply) noexcept;
  static void consume(void*, std::string_view) noexcept;
  OwnerPort& owner_;
  Allocator& allocator_;
  SessionConfig config_;
  ExportBody serializer_;
  Identity identity_{};
  SessionStats stats_{};
  SaveReply last_save_{};
  TextFailure text_failure_{};
  Step step_{};
  char* data_ = nullptr;
  std::size_t source_at_ = 0, output_at_ = 0, offered_ = 0;
  std::size_t callback_count_ = 0, callback_bytes_ = 0;
  std::uint64_t session_counter_, reader_counter_;
  ReaderToken reader_token_{};
  Phase phase_ = Phase::Idle;
  SessionError error_ = SessionError::None;
  ReaderEnd reader_end_ = ReaderEnd::Released;
  bool frozen_ = false, reader_active_ = false, cancelled_ = false;
};
static_assert(sizeof(ExportSession) <= 2048, "shared control-state target exceeded");
} // namespace draftor::exchange
