#pragma once
#include "hanime/candidate_session.hpp"
#include "hanime/composition_commit.hpp"
#include "hanime/state_v3.hpp"

namespace hanime {
namespace detail { struct KeyFenceAccess; }
enum class StateSlotKind { Empty, ReservedLoad, Clean, Dirty };
struct StateEntry {
  StableLexemeKey key; std::uint32_t count{0}; StateSlotKind kind{StateSlotKind::Empty};
  std::uint64_t generation{0}, update{0};
};
struct UserStateConfig { std::size_t slots{4}, notification_slot_limit{64}, foreground_quota{8};
  std::size_t batch_slots{4}, batch_members{16};
  // Opt-in full-key grouping and deferred, versioned receipt confirmation.
  // False retains the v1 per-member durable progression for comparison.
  bool coalesce_batches{false};
};
struct StateEvent {
  bool confirmed{false}; StableLexemeKey key; std::uint32_t count{0}; std::uint64_t sequence{0}, update{0};
};
struct UserStateHealth {
  std::size_t dirty{0}, reserved{0}; bool intent_pending{false}, frozen_write{false}, closing{false};
  StateStatus write_health{StateStatus::Ready};
  std::uint64_t notifications{0}, pending{0}, paused{0}, recorded{0}, retries{0};
  bool rank_pending{false};
  std::size_t batches_pending{0}, batch_members_pending{0};
};
struct UserStateStep { StateStep io; StateEvent event; };
struct UserStateMemory {
  std::size_t owner{0}, impl{0}, slots{0}, entry{0}, rank_read{0}, intent{0}, frozen_write{0}, receipt{0};
  std::size_t batch_slots{0}, batch_receipt_limit{0};
  std::size_t batch_progress_bytes{0}, batch_group_bytes{0}; // Group is included in impl.
};
// One authority, not a facade over the old synchronous map. Backend outlives
// owner, owner outlives sessions. All calls serial; no recursive scheduling.
enum class CountResetStatus { Idle, Pending, Changed, AlreadyZero, Busy, Stale, Unavailable, Capacity, Exhausted, Closed };
struct CountChange { StableLexemeKey key; std::uint32_t count{0}; std::uint64_t revision{0}; };
class BoundedUserState final : public CommitCountSource, public CompositionCountSource {
 public:
  BoundedUserState(StateV3&, RegistryNamespaceId, UserStateConfig={});
  ~BoundedUserState() override; // Conditional read release, never flush.
  BoundedUserState(const BoundedUserState&)=delete;
  BoundedUserState& operator=(const BoundedUserState&)=delete;
  bool valid() const;
  StateView view() const override;
  bool view_is_current(const StateView&) const override;
  CountTicket begin_count(const StateView&, const StableLexemeKey&) override;
  CountResult advance_count(const CountTicket&, const WorkBudget&) override;
  void cancel_count(const CountTicket&) override;
  // Requires this owner's completed scalar read and unchanged logical view.
  // Zero is a dirty absolute value, never an empty slot; no I/O here.
  CountResetStatus reset_count(const CountTicket&);
  CountTicket begin_counts(const StateView&,const void* first_key,std::size_t stride,std::size_t count,CountBatchAllocator={});
  CountResult advance_counts(const CountTicket&,const WorkBudget&);
  CountResult counts_result(const CountTicket&,std::size_t) const;
  std::size_t counts_memory() const;
  CountBatchProgress counts_progress() const;
  LearningStatus notify_committed(const CommitHandle&,RegistryNamespaceId,const StableLexemeKey&) noexcept override;
  CompositionLearningReceipt notify_composition(const CompositionLearningTicket&) noexcept override;
  LearningReceipt observe_learning(const LearningTicket&) const;
  UserStateStep advance_state(const StateBudget&);
  // Host calls this between session ticks. It requests a background opportunity
  // after at most G foreground steps, not a wall-clock latency guarantee.
  bool background_due() const;
  void foreground_advanced();
  void begin_close();
  UserStateStep advance_close(const StateBudget&);
  UserStateHealth health() const; // O(1), counters maintained on transitions.
  StateCost costs() const; // All state advances, including those called by rank.
  UserStateMemory memory_usage() const;
  // Optional derived-index observer. No second authority and no disk writes.
  // Activated only by a learning-enabled sparse consumer; fixed 64 event ring.
  void enable_change_journal();
  bool change(std::uint64_t revision,CountChange&) const;
  std::size_t snapshot_ram(StateEntry*,std::size_t) const;
  std::size_t journal_bytes() const;
  StateV3& state_backend();
 private:
  friend struct detail::KeyFenceAccess;
  CountResult advance_count_one(const CountTicket&, const WorkBudget&);
  struct Impl; std::unique_ptr<Impl> impl_;
};
} // namespace hanime
