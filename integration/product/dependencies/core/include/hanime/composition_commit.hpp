#pragma once
#include "hanime/candidate_session.hpp"
#include "hanime/selection_learning.hpp"
#include <functional>

namespace hanime {
// Source capability for this isolated software candidate; rebuild all consumers.
inline constexpr unsigned kCompositionLearningApiVersion=2;
enum class CompositionLearningPolicy { Disabled, Record };
enum class CompositionLearning { DisabledByConfiguration, NotAccepted, Pending, Recorded, Persisted, PartialPaused };
struct CompositionLearningIdentity;
struct CompositionLearningMemberReceipt {
  StableLexemeKey key;
  LearningStatus status{LearningStatus::NotNotified};
  std::uint32_t count{0};
  bool persisted{false};
  RuntimeLexemeRef runtime_ref{0}; // Captured with full key by the existing commit preparation.
};
struct CompositionLearningReceipt {
  std::size_t members{0}, pending{0}, recorded{0}, saturated{0}, paused{0}, persisted{0};
  std::size_t local_work{0};
};
class CompositionLearningTicket {
 public:
  CompositionLearningTicket() = default;
  // Bounded O(member count) observation after Accepted, no I/O or mutation.
  CompositionLearningReceipt receipt() const;
  CompositionLearningMemberReceipt member(std::size_t) const;
 private:
  friend class CompositionCommitter; friend class BoundedUserState;
  std::shared_ptr<CompositionLearningIdentity> identity_;
};
// A ticket can only be issued by the Core after identity/output capture.
// Implemented by the same authority as CountSource; no text replay on failure.
class CompositionCountSource {
 public:
  virtual ~CompositionCountSource() = default;
  virtual CompositionLearningReceipt notify_composition(const CompositionLearningTicket&) noexcept = 0;
};
struct SourceSpan { std::size_t begin{0}, end{0}; };
struct CompositionOrigin {
  std::uint64_t input_revision{0}, selection_revision{0};
  SourceSpan source;
};
struct CompositionMember {
  RuntimeLexemeRef ref{0};
  StableLexemeKey expected_key;
  SourceSpan source;
  OutputRequest output;
  SelectionLearningProof learning_source;
};
struct PreparedCompositionMember {
  CompositionMember selected;
  SourceSpan text; // Byte range in the complete prepared UTF-8 string.
  OutputStatus resolved{OutputStatus::Pending};
};
struct CompositionConfig {
  std::size_t max_members{8}, form_bytes{128}, member_text_bytes{128}, total_text_bytes{1024};
  CompositionLearningPolicy learning{CompositionLearningPolicy::Disabled};
};
enum class CompositionStatus {
  None, Preparing, Ready, CallingSink, Accepted, RetryableNoEffect, UnknownOutcome,
  Cancelled, Stale, IdentityMismatch, OutputRestricted, ReadError, CapacityExceeded,
  InvalidRequest, InvalidBudget, AllocationFailure, Busy, Closed
};
enum class CompositionCapacity { None, Members, RequestName, ResolverName, MemberText, TotalText };
struct CompositionIdentity;
struct CompositionResult;
class CompositionHandle {
 public:
  CompositionHandle() = default;
  // Historical outcome, not a new resource/selection authorization check.
  // Survives owner destruction; contains no borrowed member/text pointers.
  CompositionResult result() const;
  CompositionLearningTicket learning_ticket() const;
 private:
  friend class CompositionCommitter;
  std::shared_ptr<CompositionIdentity> identity_;
};
class CompositionRetryPermit {
 public: CompositionRetryPermit() = default;
 private:
  friend class CompositionHandle; friend class CompositionCommitter;
  std::shared_ptr<CompositionIdentity> identity_;
  std::uint64_t attempt_{0};
};
struct CompositionResult {
  CompositionStatus status{CompositionStatus::None};
  CompositionHandle transaction;
  CompositionOrigin origin;
  std::uint64_t attempt{0};
  std::size_t member_index{0}, member_count{0}, text_bytes{0};
  CompositionCapacity capacity{CompositionCapacity::None};
  std::size_t required_capacity{0}; // TotalText is a prefix lower bound.
  OutputStatus output{OutputStatus::Pending};
  CompositionLearning learning{CompositionLearning::DisabledByConfiguration};
  CompositionRetryPermit retry;
  // The transaction already owns this historical ticket. Return a shared
  // handle on demand instead of enlarging every by-value product result.
  CompositionLearningTicket learning_ticket() const { return transaction.learning_ticket(); }
};
struct CompositionStep { CompositionResult result; WorkCost cost; };
struct PreparedCompositionView {
  SharedActivationHandle binding;
  CompositionOrigin origin;
  const PreparedCompositionMember* members{nullptr};
  std::size_t size{0};
  std::string_view text;
};
struct CompositionCosts {
  WorkCost preparation;
  std::size_t captured_bytes{0}, copied_text_bytes{0}, sink_calls{0};
};
struct CompositionMemoryUsage {
  std::size_t owner_bytes{0}, impl_bytes{0}, members_bytes{0}, request_names_bytes{0};
  std::size_t resolver_name_bytes{0}, member_text_bytes{0}, total_text_bytes{0};
  // Token payload only, excludes implementation-specific shared_ptr controls.
  std::size_t owner_token_bytes{0}, receipt_bytes{0};
  // Included in Impl, not additional allocations.
  std::size_t output_reader_bytes{0}, read_cursor_bytes{0};
  std::size_t learning_receipt_bytes{0}, learning_members_bytes{0};
};
class CompositionCommitter {
 public:
  // shared, sink and an injected learning authority outlive this owner. Single-threaded calls; no destruction
  // or recursive calls from callbacks. Mutating re-entry returns Busy.
  CompositionCommitter(PagedSharedResource&, TextCommitSink&, const CompositionConfig& = {}, CompositionCountSource* = nullptr);
  ~CompositionCommitter();
  CompositionCommitter(const CompositionCommitter&) = delete;
  CompositionCommitter& operator=(const CompositionCommitter&) = delete;
  bool valid() const;
  // Bounded synchronous capture, NO I/O. Replaces unfinished prior work, but
  // never silently replaces UnknownOutcome. Input buffers may die on return.
  CompositionResult prepare(const SharedActivationHandle&, CompositionOrigin,
                            const CompositionMember*, std::size_t count);
  // Capture a per-transaction Record/target decision. Retrying cannot change it.
  CompositionResult prepare(const SharedActivationHandle&, CompositionOrigin,
                            const CompositionMember*, std::size_t count, CompositionLearningPolicy);
  CompositionStep advance(const CompositionHandle&, const WorkBudget&);
  // Complete text only; pointers valid solely in this synchronous callback.
  CompositionStatus prepared(const CompositionHandle&,
                            const std::function<void(const PreparedCompositionView&)>&);
  CompositionResult confirm(const CompositionHandle&);
  CompositionResult retry(const CompositionRetryPermit&);
  CompositionResult cancel(const CompositionHandle&);
  CompositionResult close(); // No sink call; terminal Accepted/Unknown survive.
  const CompositionCosts& costs() const;
  CompositionMemoryUsage memory_usage() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace hanime
