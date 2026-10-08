#include "hanime/bounded_user_state.hpp"
#include <limits>

namespace hanime {
namespace {
constexpr auto max_version=std::numeric_limits<std::uint64_t>::max();
bool known(StateSlotKind k) {return k==StateSlotKind::Clean || k==StateSlotKind::Dirty;}
StateBudget read_budget(const WorkBudget& b) {return {b.max_read_calls,0,b.max_read_bytes,0,0,b.max_work_items};}
bool success(StateStatus s) {return s==StateStatus::Found || s==StateStatus::NotFound;}
}
struct BoundedUserState::Impl {
  StateV3& file; RegistryNamespaceId ns; UserStateConfig cfg;
  std::shared_ptr<const CountOwnerIdentity> identity{std::make_shared<CountOwnerIdentity>()};
  std::unique_ptr<StateEntry[]> slots;
  std::uint64_t count_revision{1};
  mutable std::uint64_t availability{1};
  mutable std::shared_ptr<const StateIoIdentity> io_activation;
  mutable bool unavailable{false}, exhausted{false}, activation_ready{false};
  UserStateHealth health;
  StateCost costs;
  std::size_t scan_write{0},foreground{0}; bool intent_turn{true};
  struct RankRead {
    CountTicket ticket; StableLexemeKey key; std::size_t scan{0}; bool active{false}, loading{false};
    StateIoTicket io;
  } rank;
  struct Intent {
    std::shared_ptr<const CommitIdentity> receipt;
    std::size_t slot{0},scan{0}; std::uint64_t generation{0};
    StateIoTicket io; int phase{0}; std::uint32_t old{0}; bool found_ram{false};
  } intent;
  struct Frozen {
    bool active{false}; std::size_t slot{0}; StableLexemeKey key; std::uint32_t count{0};
    std::uint64_t generation{0},update{0};
  } frozen;
  Impl(StateV3& f,RegistryNamespaceId n,UserStateConfig c):file(f),ns(n),cfg(c) {
    if (ns && ns==file.registry_namespace() && cfg.slots && cfg.slots<=cfg.notification_slot_limit && cfg.notification_slot_limit<=65536 && cfg.foreground_quota)
      slots.reset(new StateEntry[cfg.slots]);
    io_activation=file.activation();
    activation_ready=file.ready();
  }
  void refresh() const {
    const auto a=file.activation();
    bool changed=false;
    if (a!=io_activation) {
      io_activation=a;
      activation_ready=false;
      changed=true;
    }
    // Only the first validated Ready of this activation clears a read fault.
    // Observing Pending must not consume the later recovery transition, and
    // repeated view/notification calls must not hide a real lookup failure.
    if (!activation_ready && file.ready()) {
      activation_ready=true;
      unavailable=false;
      changed=true;
    }
    if (changed) {if (availability==max_version) exhausted=true; else ++availability;}
  }
  void read_failed() {
    if (!unavailable) {unavailable=true; if (availability==max_version) exhausted=true; else ++availability;}
  }
  bool owns(const CountTicket& t) const {
    return rank.active && t.view.owner==identity && t.revision && t.revision==rank.ticket.revision && t.view.registry_namespace==ns;
  }
  void kind(StateEntry& e,StateSlotKind k) {
    if (e.kind==StateSlotKind::Dirty) --health.dirty;
    if (e.kind==StateSlotKind::ReservedLoad) --health.reserved;
    e.kind=k;
    if (k==StateSlotKind::Dirty) ++health.dirty;
    if (k==StateSlotKind::ReservedLoad) ++health.reserved;
  }
  LearningStatus increment(StateEntry& e) {
    if (e.count==std::numeric_limits<std::uint32_t>::max()) return LearningStatus::NoChangeSaturated;
    if (exhausted || count_revision==max_version || e.update==max_version) return LearningStatus::PausedExhausted;
    ++e.count; ++e.update; ++count_revision; kind(e,StateSlotKind::Dirty); ++health.recorded;
    return LearningStatus::RecordedDirty;
  }
  void end_intent(LearningStatus s) {
    file.cancel_read(intent.io);
    auto& e=slots[intent.slot];
    if (e.generation==intent.generation && e.kind==StateSlotKind::ReservedLoad) kind(e,StateSlotKind::Empty);
    intent.receipt->receipt.status=s;
    if (s!=LearningStatus::RecordedDirty && s!=LearningStatus::NoChangeSaturated) ++health.paused;
    intent={}; health.intent_pending=false;
  }
  UserStateStep advance_intent(const StateBudget& b) {
    UserStateStep r;
    if (!b.work_items) {r.io.status=StateStatus::InvalidBudget; return r;}
    auto& e=slots[intent.slot];
    if (e.generation!=intent.generation) {end_intent(LearningStatus::PausedUnknown); r.io.status=StateStatus::Stale; return r;}
    // All unfinished phases, including a returned-but-not-applied value,
    // wait for same-source open validation. Terminal failures still settle.
    if (!file.ready()) {
      r.io.cost.work_items=1;
      if (file.status()!=StateStatus::Pending) {end_intent(LearningStatus::PausedUnavailable); r.io.status=StateStatus::Unavailable;}
      return r;
    }
    if (intent.phase && intent.io.activation!=file.activation()) {
      file.cancel_read(intent.io); // Conditional ownership, never a new read.
      intent.io={}; intent.phase=0; intent.scan=0; intent.old=0;
    }
    if (intent.phase==0) {
      r.io.cost.work_items=1;
      intent.io=file.begin_lookup(StateReadRole::IntentLoad,e.key); intent.phase=1;
    } else if (intent.phase==1) {
      r.io=file.advance_lookup(intent.io,b);
      if (success(r.io.status)) {intent.old=r.io.count; intent.phase=2; intent.scan=0; file.cancel_read(intent.io);}
      else if (r.io.status==StateStatus::Stale) end_intent(LearningStatus::PausedUnknown);
      else if (r.io.status!=StateStatus::Pending && r.io.status!=StateStatus::InvalidBudget) {
        end_intent(LearningStatus::PausedUnavailable); read_failed();
      }
    } else if (intent.phase==2) {
      r.io.cost.work_items=1;
      // Rescan RAM before applying the one increment. Never install a late disk
      // value over a newer authority. One slot/key comparison per work item.
      if (intent.scan<cfg.slots) {
        auto& other=slots[intent.scan++];
        if (&other!=&e && known(other.kind) && other.key==e.key) {
          const auto s=increment(other); end_intent(s); r.io.status=StateStatus::Ready;
        }
      } else {
        e.count=intent.old; kind(e,StateSlotKind::Clean);
        const auto s=increment(e); end_intent(s); r.io.status=StateStatus::Ready;
      }
    }
    return r;
  }
  UserStateStep advance_writer(const StateBudget& b) {
    UserStateStep r;
    const auto info=file.info();
    if (info.maintaining) r.io=file.advance_maintenance(b);
    else if (frozen.active) {
      r.io=file.advance_write(b);
      if (r.io.status==StateStatus::HostStorageConfirmed) {
        auto& e=slots[frozen.slot];
        r.event={true,frozen.key,frozen.count,file.info().sequence,frozen.update};
        if (e.generation==frozen.generation && e.update==frozen.update && e.count==frozen.count) kind(e,StateSlotKind::Clean);
        frozen={}; health.frozen_write=false;
      }
    } else {
      if (!b.work_items) {r.io.status=StateStatus::InvalidBudget; return r;}
      r.io.cost.work_items=1;
      auto& e=slots[scan_write]; const auto i=scan_write; scan_write=(scan_write+1)%cfg.slots;
      if (e.kind==StateSlotKind::Dirty) {
        const auto s=file.begin_write(e.key,e.count);
        if (s==StateStatus::MaintenanceBlocked) r.io.status=file.begin_maintenance();
        else if (s==StateStatus::Pending) {
          frozen={true,i,e.key,e.count,e.generation,e.update}; health.frozen_write=true;
        } else r.io.status=file.ready() ? s : StateStatus::Unavailable;
      } else if (!health.dirty) r.io.status=StateStatus::Ready;
    }
    if (r.io.status==StateStatus::Unavailable || r.io.status==StateStatus::MaintenanceBlocked ||
        r.io.status==StateStatus::Corrupt || r.io.status==StateStatus::TailUncertain) {
      health.write_health=r.io.status; ++health.retries;
    } else if (r.io.status==StateStatus::HostStorageConfirmed || r.io.status==StateStatus::Ready) health.write_health=StateStatus::Ready;
    return r;
  }
};
BoundedUserState::BoundedUserState(StateV3& f,RegistryNamespaceId n,UserStateConfig c):impl_(new Impl(f,n,c)) {}
BoundedUserState::~BoundedUserState() {
  auto& p=*impl_; p.file.cancel_read(p.rank.io); p.file.cancel_read(p.intent.io);
  if (p.intent.receipt) p.end_intent(LearningStatus::PausedClosed);
}
bool BoundedUserState::valid() const {return bool(impl_->slots);}
StateView BoundedUserState::view() const {
  const auto& p=*impl_; p.refresh(); return {p.identity,p.ns,p.count_revision,p.availability};
}
bool BoundedUserState::view_is_current(const StateView& v) const {
  const auto current=view(); return !impl_->exhausted && v.owner==current.owner && v.registry_namespace==current.registry_namespace &&
      v.count_revision==current.count_revision && v.availability_revision==current.availability_revision;
}
CountTicket BoundedUserState::begin_count(const StateView& v,const StableLexemeKey& k) {
  auto& p=*impl_;
  if (!valid() || !view_is_current(v) || k.empty() || p.rank.ticket.revision==max_version) return {};
  p.file.cancel_read(p.rank.io);
  const auto rev=p.rank.ticket.revision+1; p.rank={}; p.rank.ticket={v,rev}; p.rank.key=k; p.rank.active=true;
  return p.rank.ticket;
}
CountResult BoundedUserState::advance_count(const CountTicket& t,const WorkBudget& b) {
  auto& p=*impl_; CountResult r; r.ticket=t;
  struct Account {StateCost& total; CountResult& r; ~Account() {total.add({r.cost.read_calls,0,r.cost.read_bytes,0,0,r.cost.work_items});}} account{p.costs,r};
  if (!p.owns(t)) return r;
  if (!view_is_current(t.view)) {cancel_count(t); return r;}
  if (!b.max_work_items) {r.status=CountStatus::Pending; return r;}
  r.status=CountStatus::Pending;
  if (p.rank.scan<p.cfg.slots) {
    r.cost.work_items=1; auto& e=p.slots[p.rank.scan++];
    if (known(e.kind) && e.key==p.rank.key) {r.status=CountStatus::Found; r.count=e.count;}
    return r;
  }
  if (!p.rank.loading) {
    r.cost.work_items=1;
    if (p.file.status()==StateStatus::Pending) return r;
    if (!p.file.ready() || p.unavailable) {p.read_failed(); r.status=CountStatus::Unavailable;}
    else {p.rank.io=p.file.begin_lookup(StateReadRole::RankLookup,p.rank.key); p.rank.loading=true;}
    return r;
  }
  const auto s=p.file.advance_lookup(p.rank.io,read_budget(b));
  r.cost={s.cost.read_calls,s.cost.read_bytes,s.cost.work_items};
  if (!view_is_current(t.view)) {cancel_count(t); r.status=CountStatus::Stale; return r;}
  if (success(s.status)) {r.status=s.status==StateStatus::Found ? CountStatus::Found : CountStatus::NotFound; r.count=s.count;}
  else if (s.status==StateStatus::Stale) r.status=CountStatus::Stale;
  else if (s.status!=StateStatus::Pending && s.status!=StateStatus::InvalidBudget) {p.read_failed(); r.status=CountStatus::Unavailable;}
  // Cold rank values are returned through scratch, not installed as cache
  // authority. This also makes late cache fill impossible; learning reserves H.
  return r;
}
void BoundedUserState::cancel_count(const CountTicket& t) {
  auto& p=*impl_; if (!p.owns(t)) return;
  p.file.cancel_read(p.rank.io); p.rank.io={}; p.rank.active=false;
}
LearningStatus BoundedUserState::notify_committed(const CommitHandle& h,RegistryNamespaceId ns,const StableLexemeKey& key) noexcept {
  auto& p=*impl_; const auto id=h.identity_;
  if (!id || !id->accepted || !valid() || ns!=p.ns || key.empty()) return LearningStatus::Stale;
  if (id->claimed_owner) {
    return id->claimed_owner==p.identity && id->registry_namespace==ns && id->key==key ? id->receipt.status : LearningStatus::Stale;
  }
  id->claimed_owner=p.identity; id->registry_namespace=ns; id->key=key; ++p.health.notifications;
  p.refresh(); // No I/O; BaseOnly callers need not touch the ranking view.
  id->receipt.local_work=4; // Claim/result/transition + activation check, plus H.
  auto finish=[&](LearningStatus s) {
    id->receipt.status=s;
    if (s!=LearningStatus::Pending && s!=LearningStatus::RecordedDirty && s!=LearningStatus::NoChangeSaturated) ++p.health.paused;
    return s;
  };
  if (p.health.closing) return finish(LearningStatus::PausedClosed);
  std::size_t slot=p.cfg.slots;
  for (std::size_t i=0;i<p.cfg.slots;++i) {
    ++id->receipt.local_work; auto& e=p.slots[i];
    if (known(e.kind) && e.key==key) return finish(p.increment(e));
    if (e.generation!=max_version && (e.kind==StateSlotKind::Empty || e.kind==StateSlotKind::Clean) &&
        (slot==p.cfg.slots || e.kind==StateSlotKind::Empty)) slot=i;
  }
  if (p.intent.receipt || slot==p.cfg.slots) return finish(LearningStatus::PausedCapacity);
  if (!p.file.ready() || p.unavailable) return finish(LearningStatus::PausedUnavailable);
  auto& e=p.slots[slot]; ++e.generation; e.key=key; e.count=0; e.update=0; p.kind(e,StateSlotKind::ReservedLoad);
  p.intent={}; p.intent.receipt=id; p.intent.slot=slot; p.intent.generation=e.generation;
  p.health.intent_pending=true; ++p.health.pending; return finish(LearningStatus::Pending);
}
LearningReceipt BoundedUserState::observe_learning(const LearningTicket& t) const {
  if (!t.identity_ || t.identity_->claimed_owner!=impl_->identity) return {LearningStatus::Stale,0};
  return t.identity_->receipt;
}
UserStateStep BoundedUserState::advance_state(const StateBudget& b) {
  auto& p=*impl_; UserStateStep r;
  if (!valid()) {r.io.status=StateStatus::InvalidRequest; return r;}
  if (!b.work_items) {r.io.status=StateStatus::InvalidBudget; return r;}
  p.refresh(); // Fixed lifecycle check; no file advance or recursive scheduling.
  p.foreground=0;
  const bool use_intent=bool(p.intent.receipt) && (p.intent_turn || (!p.health.dirty && !p.frozen.active && !p.file.info().maintaining));
  p.intent_turn=!use_intent;
  r=use_intent ? p.advance_intent(b) : p.advance_writer(b); p.costs.add(r.io.cost); return r;
}
bool BoundedUserState::background_due() const {return impl_->foreground>=impl_->cfg.foreground_quota;}
void BoundedUserState::foreground_advanced() {if (impl_->foreground<impl_->cfg.foreground_quota) ++impl_->foreground;}
void BoundedUserState::begin_close() {impl_->health.closing=true;}
UserStateStep BoundedUserState::advance_close(const StateBudget& b) {
  auto& p=*impl_; UserStateStep r;
  if (!p.health.closing) {r.io.status=StateStatus::InvalidRequest; return r;}
  if (p.health.dirty || p.intent.receipt || p.frozen.active || p.file.info().maintaining) {
    r=advance_state(b);
    if (r.io.status==StateStatus::Unavailable || r.io.status==StateStatus::MaintenanceBlocked || r.io.status==StateStatus::Corrupt || r.io.status==StateStatus::TailUncertain)
      r.io.status=StateStatus::CloseUnsaved;
    return r;
  }
  p.file.cancel_read(p.rank.io); p.rank.active=false; r.io=p.file.advance_close(b); p.costs.add(r.io.cost); return r;
}
UserStateHealth BoundedUserState::health() const {auto h=impl_->health; h.rank_pending=impl_->rank.active; return h;}
StateCost BoundedUserState::costs() const {return impl_->costs;}
UserStateMemory BoundedUserState::memory_usage() const {
  const auto& p=*impl_;
  return {sizeof(*this),sizeof(Impl),p.cfg.slots*sizeof(StateEntry),sizeof(StateEntry),sizeof(Impl::RankRead),sizeof(Impl::Intent),sizeof(Impl::Frozen),sizeof(CommitIdentity)};
}
} // namespace hanime
