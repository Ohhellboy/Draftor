#pragma once
#include "hanime/bounded_output.hpp"
#include "hanime/exact_ranker.hpp"
#include <functional>

namespace hanime {
struct SessionInputIdentity;
struct CandidateSnapshotIdentity;
struct CommitIdentity;
class BoundedUserState;
class LearningTicket {
 public: LearningTicket() = default;
 private:
  friend class CommitHandle; friend class BoundedUserState;
  std::shared_ptr<const CommitIdentity> identity_;
};
class SessionInputHandle {
 public: SessionInputHandle() = default;
 private: friend class CandidateSession; std::shared_ptr<const SessionInputIdentity> identity_;
};
class SnapshotHandle {
 public: SnapshotHandle() = default;
 private: friend class CandidateSession; std::shared_ptr<const CandidateSnapshotIdentity> identity_;
};
struct SelectionHandle { SnapshotHandle snapshot; std::size_t slot{0}; };
class CommitHandle {
 public: CommitHandle() = default;
  LearningTicket learning_ticket() const { LearningTicket t; t.identity_=identity_; return t; }
 private: friend class CandidateSession; friend class BoundedUserState;
  std::shared_ptr<const CommitIdentity> identity_;
};
class RetryPermit {
 public: RetryPermit() = default;
 private: friend class CandidateSession; CommitHandle transaction_; std::uint64_t attempt_{0};
};
enum class LearningStatus { NotNotified, RecordedDirty, Persisted, PausedUnavailable, PausedCapacity, PausedUnknown,
                            Pending, NoChangeSaturated, PausedClosed, Stale, PausedExhausted };
struct LearningReceipt {
  LearningStatus status{LearningStatus::NotNotified};
  std::size_t local_work{0}; // Synchronous RAM admission; NOT rank WorkCost.
};
// Owned by a transaction handle and at most one pending learning intent.
// No owner-side historical receipt collection. Only serial access is supported.
struct CommitIdentity {
  mutable bool accepted{false};
  mutable std::shared_ptr<const CountOwnerIdentity> claimed_owner;
  mutable RegistryNamespaceId registry_namespace{0};
  mutable StableLexemeKey key;
  mutable LearningReceipt receipt;
};
// Implemented by the SAME object as CountSource. Pending learning is observed
// through the transaction's LearningTicket; it never retries text submission.
class CommitCountSource : public CountSource {
 public:
  virtual LearningStatus notify_committed(const CommitHandle&, RegistryNamespaceId,
                                          const StableLexemeKey&) noexcept = 0;
};
enum class SessionStatus {
  Idle, Pending, WaitingState, StagingReady, Ready, NeedsRankRestart, NoMatches,
  RangeExhausted, Busy, InvalidRequest, InvalidBudget, NotReady, CapacityExceeded,
  ReadError, Stale, Cancelled, AllocationFailure
};
enum class CommitStatus { None, Preparing, CallingSink, Accepted, RetryableNoEffect,
                          Stale, Cancelled, OutputRestricted, UnknownOutcome, Busy };
struct CommitResult {
  CommitStatus status{CommitStatus::None};
  CommitHandle transaction;
  std::uint64_t attempt{0};
  OutputStatus output{OutputStatus::Pending};
  LearningStatus learning{LearningStatus::NotNotified};
  RetryPermit retry;
};
struct CandidateSlot {
  RankItem ranked;
  OutputStatus output{OutputStatus::Pending};
  std::uint32_t payload{kNoSharedString};
  std::size_t text_bytes{0};
};
struct CandidateViewPage {
  SnapshotHandle snapshot;
  const CandidateSlot* items{nullptr};
  std::size_t size{0};
  const char* text_pool{nullptr};
  std::size_t stride{0};
  bool order_stale{false}, has_more{false};
  ScoringMode mode{ScoringMode::Exact};
  std::string_view text(std::size_t slot) const {
    return slot<size && output_has_text(items[slot].output) ?
        std::string_view(text_pool+slot*stride,items[slot].text_bytes) : std::string_view{};
  }
};
struct SessionConfig {
  std::size_t ranking_slots{16}, snapshot_slots{10}, raw_batch{3};
  std::size_t output_bytes{128}, form_bytes{64}, code_bytes{128};
};
struct SessionStep { SessionStatus status{SessionStatus::Idle}; WorkCost cost; };
struct SessionCosts {
  WorkCost admission, ranking, display, commit, local;
  std::size_t copied_items{0}, abandoned_ranges{0}, resumed_ranges{0};
  std::size_t sink_calls{0}, present_calls{0}; // Host measures callback duration.
};
struct SessionMemoryUsage {
  std::size_t owner_bytes{0}, impl_bytes{0}, rank_work_bytes{0}, rank_result_bytes{0};
  std::size_t snapshot_bytes{0}, text_pool_bytes{0}, commit_pool_bytes{0}, name_bytes{0};
  std::size_t form_capacity{0}, code_capacity{0}, output_reader_bytes{0}, access_check_bytes{0};
  std::size_t text_cursor_bytes{0}, transaction_bytes{0}, snapshot_common_bytes{0};
  std::size_t input_token_bytes{0}, snapshot_token_bytes{0}, commit_token_bytes{0};
  RankMemoryUsage ranking;
};

class CandidateSession {
 public:
  // Owners/source/sink outlive this session. Serial calls; callbacks may enqueue
  // input but cannot recursively tick or reuse a present/sink borrowed buffer.
  CandidateSession(PagedSchemeIndex&, PagedSharedResource&, CommitCountSource&,
                   TextCommitSink&, const SessionConfig& = {});
  ~CandidateSession();
  CandidateSession(const CandidateSession&)=delete;
  CandidateSession& operator=(const CandidateSession&)=delete;
  bool valid() const;
  TextAccessStatus begin_text_access();
  TextAccessStep advance_text_access(const WorkBudget&);
  bool text_access_ready() const;
  SessionStatus set_output(OutputRequest); // Idle only; form copied, bounded F.
  SessionStatus bind_scheme(PagedSchemeIndex&); // Idle only, exact shared pairing.
  SessionStatus begin_input(std::string_view code, MountEpoch, std::size_t q,
                            std::size_t text_count, ScoringMode=ScoringMode::Exact);
  SessionInputHandle input_handle() const;
  void cancel_input(const SessionInputHandle&);
  SessionStatus request_range(RangeDirection, std::size_t q, std::size_t text_count);
  SessionStatus restart_first(std::size_t q, std::size_t text_count, ScoringMode=ScoringMode::Exact);
  // Resume the one interrupted range (rescan), never begin a new input.
  SessionStatus resume_range();
  SnapshotHandle staging() const;
  SnapshotHandle published() const;
  SessionStatus request_text(const SnapshotHandle&, std::size_t start, std::size_t count);
  SessionStatus publish(const SnapshotHandle&);
  // View valid ONLY in this synchronous callback. No implicit string copies.
  SessionStatus present(const std::function<void(const CandidateViewPage&)>&);
  SelectionHandle selection(std::size_t slot) const;
  CommitResult confirm(const SelectionHandle&);
  CommitResult retry(const RetryPermit&);
  CommitResult cancel_commit(const CommitHandle&);
  CommitResult commit_result() const;
  SessionStep tick(const WorkBudget&);
  const SessionCosts& costs() const;
  OutputCosts output_costs() const;
  SessionMemoryUsage memory_usage() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace hanime
