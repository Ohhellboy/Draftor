#include "hanime/bounded_user_state.hpp"
#include <stdexcept>
#include "hanime/key_fence_access.hpp"
#include <cstdlib>
#include <new>
#include "composition_learning_internal.hpp"
#include "hanime/execution_policy.hpp"
#include <algorithm>
#include <limits>

namespace hanime {
namespace {
constexpr auto max_version=std::numeric_limits<std::uint64_t>::max();
bool known(StateSlotKind k) {return k==StateSlotKind::Clean || k==StateSlotKind::Dirty;}
StateBudget read_budget(const WorkBudget& b) {return {b.max_read_calls,0,b.max_read_bytes,0,0,b.max_work_items,b.stop_requested,b.stop_context};}
bool success(StateStatus s) {return s==StateStatus::Found || s==StateStatus::NotFound;}
}
struct BoundedUserState::Impl {
  StateV3& file; RegistryNamespaceId ns; UserStateConfig cfg;
  std::shared_ptr<const CountOwnerIdentity> identity{std::make_shared<CountOwnerIdentity>()};
  std::unique_ptr<StateEntry[]> slots;
  struct BatchSlot {
    std::shared_ptr<CompositionLearningIdentity> receipt;
    std::size_t next{0}, scan{0}, candidate{0};
    std::size_t unsettled{0};
  };
  std::unique_ptr<BatchSlot[]> batches;
  struct MemberProgress {
    std::uint64_t generation{0}, update{0};
    std::uint32_t slot{0};
  };
  // One proof per admitted occurrence. Historical receipts do not retain this
  // owner-side array after all their obligations have been settled.
  std::unique_ptr<MemberProgress[]> progress;
  struct BatchGroup {
    bool active{false};
    std::size_t batch{0}, first{0}, scan{0}, slot{0};
    std::uint64_t generation{0};
  } group;
  std::size_t batch_apply{0};
  std::size_t batch_head{0}, batch_tail{0};
  bool batch_turn{true};
  std::uint64_t count_revision{1};
  struct ChangeFree {void operator()(CountChange* p)const{std::free(p);}};
  std::unique_ptr<CountChange[],ChangeFree> changes,staging_changes;
  std::size_t staging_change_count{0};
  mutable std::uint64_t availability{1};
  mutable std::shared_ptr<const StateIoIdentity> io_activation;
  mutable bool unavailable{false}, exhausted{false}, activation_ready{false};
  UserStateHealth health;
  StateCost costs;
  std::size_t scan_write{0},foreground{0}; bool intent_turn{true};
  struct RankRead {
    CountTicket ticket; StableLexemeKey key; std::size_t scan{0}; bool active{false}, loading{false};
    StateIoTicket io; bool completed{false}; std::uint32_t value{0};
    std::shared_ptr<StateBatch> batch;std::size_t batch_member{0};bool batch_done{false};const char* batch_stop="idle";
  } rank;
  struct Intent {
    std::shared_ptr<const CommitIdentity> receipt;
    std::shared_ptr<CompositionLearningIdentity> batch;
    std::size_t member{0}, batch_slot{0};
    std::size_t slot{0},scan{0}; std::uint64_t generation{0};
    StateIoTicket io; int phase{0}; std::uint32_t old{0}; bool found_ram{false};
  } intent;
  struct Frozen {
    bool active{false}; std::size_t slot{0}; StableLexemeKey key; std::uint32_t count{0};
    std::uint64_t generation{0},update{0};
    bool confirmed{false}; std::size_t ack_batch{0}, ack_member{0};
  } frozen;
  Impl(StateV3& f,RegistryNamespaceId n,UserStateConfig c):file(f),ns(n),cfg(c) {
    if (ns && ns==file.registry_namespace() && cfg.slots && cfg.slots<=cfg.notification_slot_limit && cfg.notification_slot_limit<=65536 && cfg.foreground_quota &&
        cfg.batch_slots && cfg.batch_slots<=64 && cfg.batch_members && cfg.batch_members<=64) {
      slots.reset(new StateEntry[cfg.slots]); batches.reset(new BatchSlot[cfg.batch_slots]);
      if (cfg.coalesce_batches) progress.reset(new MemberProgress[cfg.batch_slots*cfg.batch_members]);
    }
    io_activation=file.activation();
    activation_ready=file.ready();
  }
  void refresh() const {
    const auto a=file.activation();
    bool changed=false;
    if (!file.storage_current() && !unavailable) {unavailable=true; changed=true;}
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
    if(changes)changes[count_revision%64]={e.key,e.count,count_revision};
    return LearningStatus::RecordedDirty;
  }
  bool has_intent() const {return bool(intent.receipt) || bool(intent.batch);}
  bool group_pins(std::size_t slot) const {
    return cfg.coalesce_batches && group.active && group.slot==slot && slots[slot].generation==group.generation;
  }
  MemberProgress& member_progress(std::size_t batch,std::size_t member) {
    return progress[batch*cfg.batch_members+member];
  }
  bool confirmed_member(std::size_t batch,std::size_t member) {
    if (!frozen.active || !frozen.confirmed) return false;
    const auto& m=batches[batch].receipt->members[member];
    const auto& proof=member_progress(batch,member);
    return (m.status==LearningStatus::RecordedDirty || m.status==LearningStatus::NoChangeSaturated) &&
        proof.slot==frozen.slot && proof.generation==frozen.generation && proof.update<=frozen.update &&
        m.key==frozen.key; // A later absolute zero may durably supersede this increment.
  }
  void mark_persisted(std::size_t batch,std::size_t member) {
    auto& q=batches[batch]; auto& m=q.receipt->members[member];
    if (!m.persisted) {m.persisted=true; --q.unsettled;}
  }
  void batch_result(CompositionLearningMemberReceipt& m,LearningStatus status,const StateEntry* e=nullptr) {
    if (m.status==LearningStatus::Pending) --health.batch_members_pending;
    m.status=status;
    if (e) {m.count=e->count; m.persisted=e->kind==StateSlotKind::Clean;}
    if (status!=LearningStatus::RecordedDirty && status!=LearningStatus::NoChangeSaturated) ++health.paused;
  }
  void grouped_result(std::size_t batch,std::size_t member,LearningStatus status,const StateEntry* e=nullptr) {
    auto& q=batches[batch]; auto& m=q.receipt->members[member];
    batch_result(m,status,e);
    const bool recorded=status==LearningStatus::RecordedDirty || status==LearningStatus::NoChangeSaturated;
    if (!recorded) m.persisted=false;
    if (recorded && e) {
      auto& proof=member_progress(batch,member);
      proof={e->generation,e->update,static_cast<std::uint32_t>(e-slots.get())};
      if (group.active && group.batch==batch && group.first==member) {
        group.slot=proof.slot; group.generation=proof.generation;
      }
      // A saturated occurrence may arrive after its position was visited by
      // the confirmation scan. The confirmed watermark already proves it;
      // waiting for a new write would otherwise wait forever.
      if (!m.persisted && confirmed_member(batch,member)) m.persisted=true;
    }
    if (!recorded || m.persisted) --q.unsettled;
  }
  void end_intent(LearningStatus s,const StateEntry* recorded=nullptr) {
    file.cancel_read(intent.io);
    auto& e=slots[intent.slot];
    if (e.generation==intent.generation && e.kind==StateSlotKind::ReservedLoad) kind(e,StateSlotKind::Empty);
    if (intent.batch) {
      if (cfg.coalesce_batches) grouped_result(intent.batch_slot,intent.member,s,recorded);
      else batch_result(intent.batch->members[intent.member],s,recorded);
    }
    else {
      intent.receipt->receipt.status=s;
      if (s!=LearningStatus::RecordedDirty && s!=LearningStatus::NoChangeSaturated) ++health.paused;
    }
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
      else if (r.io.status==StateStatus::Stale) {
        if (r.io.restart_lookup && intent.io.activation==file.activation() && file.ready()) {
          // Keep the original accepted receipt and reserved slot. Only its
          // physical read restarts; the one increment is still phase 2.
          file.cancel_read(intent.io); intent.io={}; intent.phase=0; intent.scan=0; intent.old=0;
          ++health.retries; r.io.status=StateStatus::Pending;
        } else end_intent(LearningStatus::PausedUnknown);
      }
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
          const auto s=increment(other); end_intent(s,&other); r.io.status=StateStatus::Ready;
        }
      } else {
        e.count=intent.old; kind(e,StateSlotKind::Clean);
        const auto s=increment(e); end_intent(s,&e); r.io.status=StateStatus::Ready;
      }
    }
    return r;
  }
  UserStateStep advance_batch() {
    UserStateStep r; r.io.cost.work_items=1;
    auto& q=batches[batch_head];
    if (!q.receipt) {r.io.status=StateStatus::InvalidRequest; return r;}
    if (q.next==q.receipt->count) {
      q={}; batch_head=(batch_head+1)%cfg.batch_slots; --health.batches_pending;
      r.io.status=StateStatus::Ready; return r;
    }
    auto& m=q.receipt->members[q.next];
    if (m.status!=LearningStatus::Pending) {
      if (m.persisted || (m.status!=LearningStatus::RecordedDirty && m.status!=LearningStatus::NoChangeSaturated)) {
        ++q.next; q.scan=0; q.candidate=cfg.slots;
      }
      return r;
    }
    if (has_intent()) return r;
    // One RAM identity comparison per bounded service step. Dirty entries are
    // never evicted to make room; the writer creates the next clean opportunity.
    if (q.scan<cfg.slots) {
      const auto i=q.scan++; auto& e=slots[i];
      if (known(e.kind) && e.key==m.key) {
        const auto status=increment(e); batch_result(m,status,&e);
      } else if (e.generation!=max_version && (e.kind==StateSlotKind::Empty || e.kind==StateSlotKind::Clean) &&
          (q.candidate==cfg.slots || e.kind==StateSlotKind::Empty)) q.candidate=i;
      return r;
    }
    if (!file.ready()) {
      if (file.status()!=StateStatus::Pending) batch_result(m,LearningStatus::PausedUnavailable);
      return r;
    }
    if (unavailable) {batch_result(m,LearningStatus::PausedUnavailable); return r;}
    if (q.candidate==cfg.slots) {q.scan=0; return r;}
    auto& e=slots[q.candidate];
    // An intervening single-word notification may have occupied the candidate.
    if ((e.kind!=StateSlotKind::Empty && e.kind!=StateSlotKind::Clean) || e.generation==max_version) {
      q.scan=0; q.candidate=cfg.slots; return r;
    }
    ++e.generation; e.key=m.key; e.count=0; e.update=0; kind(e,StateSlotKind::ReservedLoad);
    intent={}; intent.batch=q.receipt; intent.member=q.next; intent.slot=q.candidate; intent.generation=e.generation;
    health.intent_pending=true; ++health.pending; return r;
  }
  UserStateStep advance_grouped_batch() {
    UserStateStep r; r.io.cost.work_items=1;
    if (!group.active) {
      auto& head=batches[batch_head];
      if (head.receipt && head.next==head.receipt->count && !head.unsettled) {
        head={}; batch_head=(batch_head+1)%cfg.batch_slots; --health.batches_pending;
        r.io.status=StateStatus::Ready; return r;
      }
      auto& q=batches[batch_apply];
      if (!q.receipt || q.next==q.receipt->count) {batch_apply=(batch_apply+1)%cfg.batch_slots; return r;}
      if (q.receipt->members[q.next].status!=LearningStatus::Pending) {++q.next; return r;}
      group={true,batch_apply,q.next,q.next+1,cfg.slots,0};
      q.scan=0; q.candidate=cfg.slots; return r;
    }
    auto& q=batches[group.batch]; auto& first=q.receipt->members[group.first];
    if (first.status==LearningStatus::Pending) {
      if (has_intent()) return r;
      if (q.scan<cfg.slots) {
        const auto i=q.scan++; auto& e=slots[i];
        if (known(e.kind) && e.key==first.key) grouped_result(group.batch,group.first,increment(e),&e);
        else if (e.generation!=max_version && (e.kind==StateSlotKind::Empty || e.kind==StateSlotKind::Clean) &&
            !group_pins(i) && (q.candidate==cfg.slots || e.kind==StateSlotKind::Empty)) q.candidate=i;
        return r;
      }
      if (!file.ready()) {
        if (file.status()!=StateStatus::Pending) grouped_result(group.batch,group.first,LearningStatus::PausedUnavailable);
        return r;
      }
      if (unavailable) {grouped_result(group.batch,group.first,LearningStatus::PausedUnavailable); return r;}
      if (q.candidate==cfg.slots) {q.scan=0; return r;}
      auto& e=slots[q.candidate];
      if ((e.kind!=StateSlotKind::Empty && e.kind!=StateSlotKind::Clean) || e.generation==max_version || group_pins(q.candidate)) {
        q.scan=0; q.candidate=cfg.slots; return r;
      }
      ++e.generation; e.key=first.key; e.count=0; e.update=0; kind(e,StateSlotKind::ReservedLoad);
      intent={}; intent.batch=q.receipt; intent.batch_slot=group.batch; intent.member=group.first;
      intent.slot=q.candidate; intent.generation=e.generation;
      health.intent_pending=true; ++health.pending; return r;
    }
    if (group.slot==cfg.slots || group.scan==q.receipt->count) {
      q.next=group.first+1; q.scan=0; q.candidate=cfg.slots; group={}; return r;
    }
    const auto member=group.scan++; auto& m=q.receipt->members[member];
    if (m.status==LearningStatus::Pending && m.key==first.key) {
      auto& e=slots[group.slot];
      if (e.generation!=group.generation || !known(e.kind) || e.key!=first.key)
        grouped_result(group.batch,member,LearningStatus::PausedUnknown);
      else grouped_result(group.batch,member,increment(e),&e);
    }
    return r;
  }
  void confirm_batch(const StateEvent& event) {
    // Serial batch progression leaves only the head/current member awaiting a
    // write. No history scan, seen set, or allocation is needed on confirmation.
    if (!health.batches_pending) return;
    auto& q=batches[batch_head];
    if (!q.receipt || q.next==q.receipt->count) return;
    auto& m=q.receipt->members[q.next];
    if ((m.status==LearningStatus::RecordedDirty || m.status==LearningStatus::NoChangeSaturated) &&
        m.key==event.key && event.count>=m.count) m.persisted=true;
  }
  UserStateStep advance_writer(const StateBudget& b) {
    UserStateStep r;
    const auto info=file.info();
    if (info.maintaining) r.io=file.advance_maintenance(b);
    else if (frozen.active && frozen.confirmed) {
      r.io.cost.work_items=1;
      if (frozen.ack_batch==cfg.batch_slots) {
        auto& e=slots[frozen.slot];
        if (e.generation==frozen.generation && e.update==frozen.update && e.count==frozen.count) kind(e,StateSlotKind::Clean);
        frozen={}; health.frozen_write=false; r.io.status=StateStatus::Ready;
      } else {
        auto& q=batches[frozen.ack_batch];
        if (!q.receipt || frozen.ack_member==q.receipt->count) {++frozen.ack_batch; frozen.ack_member=0;}
        else {const auto m=frozen.ack_member++; if (confirmed_member(frozen.ack_batch,m)) mark_persisted(frozen.ack_batch,m);}
      }
    } else if (frozen.active) {
      r.io=file.advance_write(b);
      if (r.io.status==StateStatus::HostStorageConfirmed) {
        auto& e=slots[frozen.slot];
        r.event={true,frozen.key,frozen.count,file.info().sequence,frozen.update};
        if (cfg.coalesce_batches) {frozen.confirmed=true; frozen.ack_batch=frozen.ack_member=0;}
        else {
          confirm_batch(r.event);
          if (e.generation==frozen.generation && e.update==frozen.update && e.count==frozen.count) kind(e,StateSlotKind::Clean);
          frozen={}; health.frozen_write=false;
        }
      }
    } else {
      if (!b.work_items) {r.io.status=StateStatus::InvalidBudget; return r;}
      r.io.cost.work_items=1;
      auto& e=slots[scan_write]; const auto i=scan_write; scan_write=(scan_write+1)%cfg.slots;
      if (e.kind==StateSlotKind::Dirty && !group_pins(i)) {
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
  if (p.has_intent()) p.end_intent(LearningStatus::PausedClosed);
  if (p.batches) for (std::size_t i=0;i<p.cfg.batch_slots;++i) if (p.batches[i].receipt) {
    auto& batch=*p.batches[i].receipt;
    for (std::size_t m=0;m<batch.count;++m) {
      // Settle only an already confirmed watermark; destruction never writes.
      if (p.cfg.coalesce_batches && p.confirmed_member(i,m)) p.mark_persisted(i,m);
      if (batch.members[m].status==LearningStatus::Pending) {
        if (p.cfg.coalesce_batches) p.grouped_result(i,m,LearningStatus::PausedClosed);
        else p.batch_result(batch.members[m],LearningStatus::PausedClosed);
      }
    }
  }
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
#if BGIME_RANKING_ARM == 0
  auto out=advance_count_one(t,b);
  if(impl_->owns(t) && (out.status==CountStatus::Found || out.status==CountStatus::NotFound)){impl_->rank.completed=true;impl_->rank.value=out.count;}
  return out;
#else
  CountResult out;out.ticket=t;out.status=CountStatus::Pending;auto& p=*impl_;
  while(out.cost.work_items<b.max_work_items && !b.should_stop()) {
    const auto scan=p.rank.scan;const bool loading=p.rank.loading;
    const WorkBudget left{b.max_read_calls-out.cost.read_calls,b.max_read_bytes-out.cost.read_bytes,
      b.max_work_items-out.cost.work_items,b.stop_requested,b.stop_context};
    auto step=advance_count_one(t,left);
    const bool progress=step.progressed || scan!=p.rank.scan || loading!=p.rank.loading;
    out.status=step.status;out.count=step.count;out.progressed|=progress;
    out.cost.read_calls+=step.cost.read_calls;out.cost.read_bytes+=step.cost.read_bytes;
    out.cost.work_items+=step.cost.work_items;
    if(step.status!=CountStatus::Pending || !progress)break;
  }
  if(p.owns(t) && (out.status==CountStatus::Found || out.status==CountStatus::NotFound)){p.rank.completed=true;p.rank.value=out.count;}
  return out;
#endif
}
CountResult BoundedUserState::advance_count_one(const CountTicket& t,const WorkBudget& b) {
  auto& p=*impl_; CountResult r; r.ticket=t;
  struct Account {StateCost& total; CountResult& r; ~Account() {total.add({r.cost.read_calls,0,r.cost.read_bytes,0,0,r.cost.work_items});}} account{p.costs,r};
  if (!p.owns(t) || p.rank.batch) return r;
  if (!view_is_current(t.view)) {cancel_count(t); return r;}
  if (!p.file.storage_current()) {p.read_failed(); r.status=CountStatus::Unavailable; return r;}
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
  r.cost={s.cost.read_calls,s.cost.read_bytes,s.cost.work_items};r.progressed=s.progressed;
  if (!view_is_current(t.view)) {cancel_count(t); r.status=CountStatus::Stale; return r;}
  if (success(s.status)) {r.status=s.status==StateStatus::Found ? CountStatus::Found : CountStatus::NotFound; r.count=s.count;}
  else if (s.status==StateStatus::Stale) r.status=CountStatus::Stale;
  else if (s.status!=StateStatus::Pending && s.status!=StateStatus::InvalidBudget) {p.read_failed(); r.status=CountStatus::Unavailable;}
  // Cold rank values are returned through scratch, not installed as cache
  // authority. This also makes late cache fill impossible; learning reserves H.
  return r;
}
CountResetStatus BoundedUserState::reset_count(const CountTicket& t) {
  auto& p=*impl_; p.refresh();
  if (!valid() || !p.owns(t) || p.rank.batch || !view_is_current(t.view)) return CountResetStatus::Stale;
  if (!p.file.storage_current() || !p.file.ready() || p.unavailable) return CountResetStatus::Unavailable;
  if (p.health.closing) return CountResetStatus::Closed;
  if (!p.rank.completed) return CountResetStatus::Pending;
  // Never move a reset ahead of an already Accepted but unapplied increment.
  // The caller may retry a new action once these bounded obligations settle.
  if (p.has_intent() || p.health.batch_members_pending || p.group.active ||
      (!p.cfg.coalesce_batches && p.health.batches_pending)) return CountResetStatus::Busy;
  if (!p.rank.value) return CountResetStatus::AlreadyZero;
  if (p.exhausted || p.count_revision==max_version) return CountResetStatus::Exhausted;
  std::size_t slot=p.cfg.slots;
  for (std::size_t i=0;i<p.cfg.slots;++i) {
    const auto& e=p.slots[i];
    if (known(e.kind) && e.key==p.rank.key) {slot=i;break;}
    if (e.generation!=max_version && (e.kind==StateSlotKind::Empty || e.kind==StateSlotKind::Clean) &&
        !p.group_pins(i) && !(p.frozen.active && p.frozen.slot==i) &&
        (slot==p.cfg.slots || e.kind==StateSlotKind::Empty)) slot=i;
  }
  if (slot==p.cfg.slots) return CountResetStatus::Capacity;
  auto& e=p.slots[slot];
  if (known(e.kind) && e.key==p.rank.key) {
    if (e.update==max_version) return CountResetStatus::Exhausted;
  } else {
    ++e.generation;e.key=p.rank.key;e.update=0;
  }
  e.count=0;++e.update;++p.count_revision;p.kind(e,StateSlotKind::Dirty);
  if(p.changes)p.changes[p.count_revision%64]={e.key,0,p.count_revision};
  return CountResetStatus::Changed;
}
void BoundedUserState::cancel_count(const CountTicket& t) {
  auto& p=*impl_; if (!p.owns(t)) return;
  p.file.cancel_read(p.rank.io); p.rank.io={}; p.rank.active=false; p.rank.batch.reset();
}
#include "count_batch_owner.hpp"
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
    if (e.generation!=max_version && (e.kind==StateSlotKind::Empty || e.kind==StateSlotKind::Clean) && !p.group_pins(i) &&
        (slot==p.cfg.slots || e.kind==StateSlotKind::Empty)) slot=i;
  }
  if (p.has_intent() || slot==p.cfg.slots) return finish(LearningStatus::PausedCapacity);
  if (!p.file.ready() || p.unavailable) return finish(LearningStatus::PausedUnavailable);
  auto& e=p.slots[slot]; ++e.generation; e.key=key; e.count=0; e.update=0; p.kind(e,StateSlotKind::ReservedLoad);
  p.intent={}; p.intent.receipt=id; p.intent.slot=slot; p.intent.generation=e.generation;
  p.health.intent_pending=true; ++p.health.pending; return finish(LearningStatus::Pending);
}
CompositionLearningReceipt BoundedUserState::notify_composition(const CompositionLearningTicket& ticket) noexcept {
  auto& p=*impl_; const auto id=ticket.identity_;
  if (!id || !id->accepted) return {};
  if (id->claimed_owner) return id->claimed_owner==p.identity ? ticket.receipt() : CompositionLearningReceipt{};
  id->claimed_owner=p.identity; ++p.health.notifications; id->local_work=4;
  auto pause=[&](LearningStatus status) {
    for (std::size_t i=0;i<id->count;++i) {id->members[i].status=status; ++id->local_work; ++p.health.paused;}
    return ticket.receipt();
  };
  if (!valid() || id->registry_namespace!=p.ns) return pause(LearningStatus::Stale);
  if (p.health.closing) return pause(LearningStatus::PausedClosed);
  if (!id->count || id->count>p.cfg.batch_members || p.health.batches_pending==p.cfg.batch_slots)
    return pause(LearningStatus::PausedCapacity);
  const auto batch_slot=p.batch_tail;
  auto& q=p.batches[batch_slot]; q={}; q.receipt=id; q.candidate=p.cfg.slots; q.unsettled=id->count;
  p.batch_tail=(p.batch_tail+1)%p.cfg.batch_slots; ++p.health.batches_pending;
  for (std::size_t i=0;i<id->count;++i) {
    id->members[i].status=LearningStatus::Pending;
    if (p.cfg.coalesce_batches) p.member_progress(batch_slot,i)={};
    ++id->local_work;
  }
  p.health.batch_members_pending+=id->count;
  return ticket.receipt();
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
  const bool use_intent=p.has_intent() && (p.intent_turn || (!p.health.dirty && !p.frozen.active && !p.file.info().maintaining));
  p.intent_turn=!use_intent;
  if (use_intent) r=p.advance_intent(b);
  else if (!p.has_intent() && p.health.batches_pending && p.batch_turn) {
    r=p.cfg.coalesce_batches ? p.advance_grouped_batch() : p.advance_batch(); p.batch_turn=false;
  }
  else {r=p.advance_writer(b); p.batch_turn=true;}
  p.costs.add(r.io.cost); return r;
}
bool BoundedUserState::background_due() const {return impl_->foreground>=impl_->cfg.foreground_quota;}
void BoundedUserState::foreground_advanced() {if (impl_->foreground<impl_->cfg.foreground_quota) ++impl_->foreground;}
void BoundedUserState::begin_close() {impl_->health.closing=true;}
UserStateStep BoundedUserState::advance_close(const StateBudget& b) {
  auto& p=*impl_; UserStateStep r;
  if (!p.health.closing) {r.io.status=StateStatus::InvalidRequest; return r;}
  if (p.health.dirty || p.has_intent() || p.health.batches_pending || p.frozen.active || p.file.info().maintaining) {
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
  return {sizeof(*this),sizeof(Impl),p.cfg.slots*sizeof(StateEntry),sizeof(StateEntry),sizeof(Impl::RankRead),sizeof(Impl::Intent),sizeof(Impl::Frozen),sizeof(CommitIdentity),
    p.cfg.batch_slots*sizeof(Impl::BatchSlot),
    p.cfg.batch_slots*(sizeof(CompositionLearningIdentity)+p.cfg.batch_members*sizeof(CompositionLearningMemberReceipt)),
    p.cfg.coalesce_batches ? p.cfg.batch_slots*p.cfg.batch_members*sizeof(Impl::MemberProgress) : 0,
    sizeof(Impl::BatchGroup)};
}
void BoundedUserState::enable_change_journal(){
  while(!impl_->changes){auto s=detail::KeyFenceAccess::journal(*this,{0,0,1});if(s.status==StateStatus::Unavailable)throw std::bad_alloc();}
}
StateStep detail::KeyFenceAccess::journal(BoundedUserState& owner,const WorkBudget& b) {
  auto& p=*owner.impl_;StateStep out;
  if(p.changes){out.status=StateStatus::Ready;return out;}
  if(!b.max_work_items||b.should_stop())return out;
  out.cost.work_items=1;
  if(!p.staging_changes){p.staging_changes.reset(static_cast<CountChange*>(std::malloc(64*sizeof(CountChange))));if(!p.staging_changes)out.status=StateStatus::Unavailable;return out;}
  new(p.staging_changes.get()+p.staging_change_count) CountChange{};
  if(++p.staging_change_count==64){p.changes=std::move(p.staging_changes);out.status=StateStatus::Ready;}
  return out;
}
bool BoundedUserState::change(std::uint64_t revision,CountChange& out) const {
  const auto& p=*impl_;if(!p.changes||p.changes[revision%64].revision!=revision)return false;
  out=p.changes[revision%64];return true;
}
std::size_t BoundedUserState::snapshot_ram(StateEntry* out,std::size_t capacity) const {
  const auto& p=*impl_;if(capacity<p.cfg.slots)return p.cfg.slots;
  for(std::size_t i=0;i<p.cfg.slots;++i)out[i]=p.slots[i];return p.cfg.slots;
}
std::size_t BoundedUserState::journal_bytes()const{return impl_->changes?64*sizeof(CountChange):0;}
StateV3& BoundedUserState::state_backend(){return impl_->file;}
bool detail::KeyFenceAccess::capture(BoundedUserState& owner,KeyFenceCapture& out) {
  auto& p=*owner.impl_;const auto v=owner.view();
  if(!owner.valid() || p.unavailable || p.exhausted || !p.file.begin_export(out.cutoff))return false;
  out.view=v;out.slots=p.cfg.slots;
  return owner.view_is_current(v);
}
StateExportStep detail::KeyFenceAccess::advance(BoundedUserState& owner,KeyFenceCapture& c,const WorkBudget& b) {
  StateExportStep out;auto& p=*owner.impl_;
  if(!owner.view_is_current(c.view) || p.unavailable){out.step.status=StateStatus::Stale;return out;}
  out=p.file.advance_export(c.cutoff,{b.max_read_calls,0,b.max_read_bytes,0,0,b.max_work_items,b.stop_requested,b.stop_context});
  p.costs.add(out.step.cost);
  if(out.step.status!=StateStatus::Pending && out.step.status!=StateStatus::Found &&
     out.step.status!=StateStatus::Ready && out.step.status!=StateStatus::Stale &&
     out.step.status!=StateStatus::InvalidBudget)p.read_failed();
  if(!owner.view_is_current(c.view)){out.item=out.complete=false;out.step.status=StateStatus::Stale;}
  return out;
}
bool detail::KeyFenceAccess::ram(BoundedUserState& owner,const KeyFenceCapture& c,std::size_t i,StateEntry& out) {
  if(!owner.view_is_current(c.view) || owner.impl_->unavailable || i>=c.slots)return false;
  out=owner.impl_->slots[i];return true;
}
bool detail::KeyFenceAccess::seal(BoundedUserState& owner,const KeyFenceCapture& c) {
  StateExportCursor now;
  if(!owner.view_is_current(c.view) || owner.impl_->unavailable || !owner.impl_->file.begin_export(now))return false;
  const auto& a=c.cutoff;
  return a.activation==now.activation && a.physical==now.physical && a.file==now.file &&
      a.generation==now.generation && a.keys==now.keys && a.tails==now.tails && a.sequence==now.sequence;
}
} // namespace hanime
