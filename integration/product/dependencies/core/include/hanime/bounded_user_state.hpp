#pragma once
#include "hanime/candidate_session.hpp"
#include "hanime/state_v3.hpp"

namespace hanime {
enum class StateSlotKind { Empty, ReservedLoad, Clean, Dirty };
struct StateEntry {
  StableLexemeKey key; std::uint32_t count{0}; StateSlotKind kind{StateSlotKind::Empty};
  std::uint64_t generation{0}, update{0};
};
struct UserStateConfig { std::size_t slots{4}, notification_slot_limit{64}, foreground_quota{8}; };
struct StateEvent {
  bool confirmed{false}; StableLexemeKey key; std::uint32_t count{0}; std::uint64_t sequence{0}, update{0};
};
struct UserStateHealth {
  std::size_t dirty{0}, reserved{0}; bool intent_pending{false}, frozen_write{false}, closing{false};
  StateStatus write_health{StateStatus::Ready};
  std::uint64_t notifications{0}, pending{0}, paused{0}, recorded{0}, retries{0};
  bool rank_pending{false};
};
struct UserStateStep { StateStep io; StateEvent event; };
struct UserStateMemory {
  std::size_t owner{0}, impl{0}, slots{0}, entry{0}, rank_read{0}, intent{0}, frozen_write{0}, receipt{0};
};
// One authority, not a facade over the old synchronous map. Backend outlives
// owner, owner outlives sessions. All calls serial; no recursive scheduling.
class BoundedUserState final : public CommitCountSource {
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
  LearningStatus notify_committed(const CommitHandle&,RegistryNamespaceId,const StableLexemeKey&) noexcept override;
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
 private: struct Impl; std::unique_ptr<Impl> impl_;
};
} // namespace hanime
