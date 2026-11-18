#pragma once

#include "hanime/paged_shared_resource.hpp"

namespace hanime {

// State adapters own these tokens; old views keep an old token alive. They are
// runtime lifetimes, never persistent identities. Serial host calls only.
struct CountOwnerIdentity {};
struct StateView {
  std::shared_ptr<const CountOwnerIdentity> owner;
  RegistryNamespaceId registry_namespace{0};
  std::uint64_t count_revision{0}, availability_revision{0};
};
struct CountTicket {
  StateView view;
  std::uint64_t revision{0};
};
enum class CountStatus { Found, NotFound, Pending, Unavailable, Stale };
struct CountResult {
  CountStatus status{CountStatus::Stale};
  CountTicket ticket;
  std::uint32_t count{0};
  WorkCost cost;
  bool progressed{false};
};
class CountSource {
 public:
  virtual ~CountSource() = default;
  virtual StateView view() const = 0;
  virtual bool view_is_current(const StateView&) const = 0;
  // begin is fixed-cost, no I/O. Only one current ticket per source. Actual
  // missing data is NotFound, not a cache miss. Invalid/foreign work is no-op.
  virtual CountTicket begin_count(const StateView&, const StableLexemeKey&) = 0;
  virtual CountResult advance_count(const CountTicket&, const WorkBudget&) = 0;
  // Fixed-cost, no I/O/throw. Check operation ownership, not current StateView:
  // a stale scoring view can still own an operation. Foreign/replaced is no-op.
  virtual void cancel_count(const CountTicket&) = 0;
};

struct RankRule {
  std::uint64_t revision{1};
  std::uint16_t lambda{100};
  static bool make(std::uint64_t revision, std::uint64_t lambda, RankRule* out);
};
enum class ScoringMode { Exact, BaseOnly };
enum class BaseOnlyReason { None, StateUnavailable, HostSelected };
enum class RangeDirection { First, After, Before };
enum class RankStatus {
  InProgress, WaitingState, Ready, NoMatches, RangeExhausted,
  StateUnavailable, Stale, Cancelled, InvalidRequest, InvalidBudget,
  CapacityExceeded, Busy, GroupOrderError, ResourceError
};
struct RankItem {
  StableLexemeKey key;
  std::int64_t score{0};
  RuntimeLexemeRef ref{0};
  std::int32_t scheme_weight{0};
  std::uint32_t effective_count{0}, path_count{0}, representative_ordinal{0};
};
struct RankLifetime;
struct InputQueryIdentity;
class InputHandle {
 public:
  InputHandle() = default;
 private:
  friend class ExactRanker;
  std::shared_ptr<const InputQueryIdentity> identity_;
};
class RankBoundary {
 public:
  RankBoundary() = default;
 private:
  friend class ExactRanker;
  std::shared_ptr<const RankLifetime> lifetime_;
  StableLexemeKey key_;
  std::int64_t score_{0};
  std::uint32_t position_{0}; // Private ordinal, valid only in this ranking life.
};
class RankTicket {
 public:
  RankTicket() = default;
 private:
  friend class ExactRanker;
  std::shared_ptr<const RankLifetime> lifetime_;
  std::uint64_t revision_{0};
};
struct RankStep { RankStatus status{RankStatus::Stale}; WorkCost cost; };
// Cumulative logical work for this ranker. Nested costs are included exactly
// once; fewer public advances must not be mistaken for fewer work items.
struct RankStatistics {
  std::uint64_t input_starts{0}, range_requests{0}, full_scans{0}, candidate_groups{0};
  std::uint64_t count_lookups{0}, count_pending{0}, empty_pending{0};
  std::uint64_t replay_ranges{0}, replay_overflows{0}, sort_work{0}, cached_ranges{0};
  std::uint64_t restarts{0}, state_fallback_restarts{0}, advance_calls{0}, micro_steps{0};
  std::uint64_t work_items{0}, read_calls{0}, read_bytes{0};
  // Composition-owned recovery reason; standalone rankers leave this zero.
  std::uint64_t count_revision_restarts{0};
  void add(const RankStatistics& x) {
#define HANIME_RANK_ADD(field) field += x.field
    HANIME_RANK_ADD(input_starts); HANIME_RANK_ADD(range_requests);
    HANIME_RANK_ADD(full_scans); HANIME_RANK_ADD(candidate_groups);
    HANIME_RANK_ADD(count_lookups); HANIME_RANK_ADD(count_pending); HANIME_RANK_ADD(empty_pending);
    HANIME_RANK_ADD(replay_ranges); HANIME_RANK_ADD(replay_overflows); HANIME_RANK_ADD(sort_work);
    HANIME_RANK_ADD(cached_ranges); HANIME_RANK_ADD(restarts); HANIME_RANK_ADD(state_fallback_restarts);
    HANIME_RANK_ADD(advance_calls); HANIME_RANK_ADD(micro_steps); HANIME_RANK_ADD(work_items); HANIME_RANK_ADD(read_calls); HANIME_RANK_ADD(read_bytes);
    HANIME_RANK_ADD(count_revision_restarts);
#undef HANIME_RANK_ADD
  }
};
struct RankResultView {
  const RankItem* items{nullptr};
  std::size_t size{0};
  bool has_more{false};
  ScoringMode mode{ScoringMode::Exact};
  BaseOnlyReason reason{BaseOnlyReason::None};
};
struct RankMemoryUsage {
  std::size_t owner_bytes{0}, impl_bytes{0}, lifetime_bytes{0};
  std::size_t workspace_bytes{0}, result_bytes{0}, batch_capacity_bytes{0};
  std::size_t group_bytes{0}, pending_item_bytes{0}, shared_cursor_bytes{0};
  std::size_t shared_record_bytes{0}, count_ticket_bytes{0}, boundary_bytes{0};
  ExactQueryMemoryUsage raw;
  std::size_t ordered_cursor_bytes{0}, captured_code_capacity{0};
  std::size_t replay_allocated_bytes{0}, replay_capacity{0}, replay_items{0};
  bool replay_complete{false};
  // Subfields above may be INCLUDED in impl, not additive. Code/page buffers
  // ordered_cursor_bytes includes its pointer already in impl; subtract that
  // pointer when adding. Code and allocator/control-block overhead are separate.
};

class BaseOrderPrefixState;
class SparseCorrections;
class ExactRanker {
 public:
  // Bound synchronous batching independently of a caller's large work budget.
  // Nested operations retain their existing budgets; I/O exhaustion or an
  // empty Pending stops earlier. This is not a wall-clock preemption promise.
  static constexpr unsigned LocalBatchSteps=8;
  // Buffers must remain alive, disjoint and exclusive for the ranker's life.
  // Construction does not initialize caller workspace. Owners outlive ranker.
  ExactRanker(PagedSchemeIndex&, PagedSharedResource&, CountSource&,
              RankItem* workspace, std::size_t capacity,
              std::size_t batch_capacity, std::size_t max_code_bytes = 128,
              std::size_t replay_capacity = 0, bool exact_from_base_order = false, SparseCorrections* sparse = nullptr);
  // Optional exact_from_base_order consumes trusted rule-1 HBO rows as raw S.
  // Only rule 1 and the explicitly compatible A_REV1 runtime policy use these
  // facts; other revisions retain the legacy raw linear-scoring fallback.
  // Runtime policy scores are never serialized back into immutable HBO rows.
  // A known invalid attached HBO fails conservatively, never supplies facts.
  // Optional 0..512 full-group replay slots, allocated only for Exact.
  // Replay is valid only after a COMPLETE scan fitting the capacity and only
  // within this resource/count/rule lifetime. Overflow retains the full scan.
  // The first range pays no complete-sort cost. A later range lazily orders
  // complete replay in-place, then uses bounded anchor lookup/direct paging.
  ~ExactRanker(); // Releases own operations only; borrowed ports must still live.
  ExactRanker(const ExactRanker&) = delete;
  ExactRanker& operator=(const ExactRanker&) = delete;

  RankStatus begin_input(std::string code, MountEpoch, RankRule = {},
                         ScoringMode = ScoringMode::Exact);
  // Same input query, NEW ranking lifetime, First required. BaseOnly latches
  // until begin_input; an Exact request while latched stays BaseOnly.
  RankStatus restart_rank(ScoringMode);
  RankStatus request_range(RangeDirection, const RankBoundary*, std::size_t q,
                           RankItem* result, std::size_t capacity, RankTicket*);
  // EXPERIMENTAL P3. First range of a new input lifetime only. The hint is
  // consumed synchronously; it never supplies a range result or qualification.
  RankStatus request_first_prefix(BaseOrderPrefixState&, std::size_t q,
                                  RankItem*, std::size_t capacity, RankTicket*);
  // Call under a Ready/NoMatches result lease, before release/abandon. Charge
  // returned work against the remaining slice/total budget. With no remaining
  // unit skip the optional checkpoint; never exceed the query budget to save it.
  RankStep save_prefix(const RankTicket&, BaseOrderPrefixState&, const WorkBudget&);
  RankStep advance(const RankTicket&, const WorkBudget&);
  bool range_is_current(const RankTicket&) const;
  // Read-only proof for an owner to restart an unpublished first range.
  // Rejects resource/owner/namespace/availability changes and cancellation.
  bool count_revision_changed(const RankTicket&) const;
  bool boundary_is_current(const RankBoundary&) const;
  RankResultView result(const RankTicket&) const;
  RankBoundary boundary(const RankTicket&, std::size_t slot) const;
  void release(const RankTicket&); // Releases borrow even if resource is stale.
  void cancel(const RankTicket&);  // Foreign/replaced tickets are complete no-ops.
  InputHandle input_handle() const;
  bool input_is_current(const InputHandle&) const; // No CountSource access.
  bool abandon_range(const RankTicket&); // Preserve input/latch; conditional cleanup.
  ScoringMode mode() const;
  bool uses_base_order() const; // True only for BaseOnly ordered paging.
  bool uses_base_order_facts() const; // Opt-in Exact full-row input; no static stop.
  RankMemoryUsage memory_usage() const;
  RankStatistics statistics() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  RankStep advance_one(const RankTicket&, const WorkBudget&);
};

} // namespace hanime
