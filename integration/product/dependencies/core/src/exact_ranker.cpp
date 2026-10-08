#include "hanime/exact_ranker.hpp"
#include "hanime/base_order.hpp"

#include <limits>
#include <utility>

namespace hanime {
struct InputQueryIdentity {};
struct RankLifetime {
  std::shared_ptr<const InputQueryIdentity> input;
  SharedActivationHandle shared;
  StateView state;
  RankRule rule;
  ScoringMode mode{ScoringMode::Exact};
  BaseOnlyReason reason{BaseOnlyReason::None};
};
namespace {
bool better(std::int64_t a, const StableLexemeKey& ak,
            std::int64_t b, const StableLexemeKey& bk) {
  return a > b || (a == b && ak < bk);
}
bool same_view(const StateView& a, const StateView& b) {
  return a.owner == b.owner && a.registry_namespace == b.registry_namespace &&
      a.count_revision == b.count_revision && a.availability_revision == b.availability_revision;
}
bool valid_buffer(RankItem* p, std::size_t n) {
  return p && n && n <= std::numeric_limits<std::size_t>::max()/sizeof(RankItem) &&
      reinterpret_cast<std::uintptr_t>(p) <= std::numeric_limits<std::uintptr_t>::max()-n*sizeof(RankItem);
}
bool overlap(RankItem* a, std::size_t n, RankItem* b, std::size_t m) {
  const auto x=reinterpret_cast<std::uintptr_t>(a), y=reinterpret_cast<std::uintptr_t>(b);
  return x < y+m*sizeof(RankItem) && y < x+n*sizeof(RankItem);
}
}
bool RankRule::make(std::uint64_t revision, std::uint64_t lambda, RankRule* out) {
  if (!out || !revision || lambda > 65535) return false;
  *out={revision,static_cast<std::uint16_t>(lambda)};
  return true;
}

struct ExactRanker::Impl {
  enum class Phase { Idle, OrderedLocate, OrderedRead, Scan, SharedStart, SharedAdvance, CountStart, CountAdvance,
                     Score, Filter, Search, Shift, Insert, Copy, Publish, Finished };
  struct Group {
    RuntimeLexemeRef ref{0};
    std::int32_t maximum{0};
    std::uint32_t paths{0}, ordinal{0};
  } group;
  PagedSchemeIndex& scheme;
  PagedSharedResource& shared;
  CountSource& counts;
  RankItem* work;
  std::size_t w, b, code_limit;
  std::shared_ptr<const InputQueryIdentity> input;
  std::shared_ptr<const RankLifetime> lifetime;
  QueryBatchContext latest;
  ExactQueryCursor raw;
  std::shared_ptr<BaseOrderIndex> ordered;
  BaseOrderCursor ordered_cursor;
  // Borrowed only during the synchronous request_first_prefix call.
  BaseOrderPrefixState* offered_prefix{nullptr};
  std::string code;
  std::uint32_t ordered_start{0},ordered_end{0};
  RawQueryBatch batch;
  SharedReadCursor shared_cursor;
  SharedLexemeRecord record;
  CountTicket count_ticket;
  RankItem pending;
  RankBoundary anchor;
  RankItem* output{nullptr};
  std::size_t output_capacity{0}, q{0}, length{0}, pos{0}, lo{0}, hi{0}, shift{0}, copied{0};
  std::uint32_t raw_count{0}, eligible{0};
  std::uint64_t revision{0};
  RangeDirection direction{RangeDirection::First};
  Phase phase{Phase::Idle};
  RankStatus status{RankStatus::InProgress};
  BaseOnlyReason reason{BaseOnlyReason::None};
  bool latched{false}, cancelled{false}, exhausted_raw{false}, first_scan{true}, lease{false};

  Impl(PagedSchemeIndex& s, PagedSharedResource& r, CountSource& c,
       RankItem* a, std::size_t n, std::size_t batch_n, std::size_t limit)
      : scheme(s), shared(r), counts(c), work(a), w(n), b(batch_n), code_limit(limit) {}
  bool resources_current() const {
    return lifetime && !cancelled && shared.activation_is_current(lifetime->shared) &&
        scheme.query_is_current(latest) && (!ordered || ordered->ready());
  }
  bool current() const {
    // Crucial: BaseOnly does not consult the state owner at all.
    return resources_current() && status != RankStatus::Stale && status != RankStatus::StateUnavailable &&
        status != RankStatus::GroupOrderError && status != RankStatus::ResourceError &&
        (lifetime->mode == ScoringMode::BaseOnly || counts.view_is_current(lifetime->state));
  }
  bool ticket_matches(const RankTicket& t) const {
    return lifetime && t.lifetime_ == lifetime && t.revision_ == revision;
  }
  void release_operations() {
    // Cleanup is independent of scoring/resource freshness. Each borrowed
    // port conditionally releases only our saved operation, never its successor.
    scheme.release_query_read(raw);
    if (ordered) ordered->cancel(ordered_cursor);
    shared.cancel_read(&shared_cursor);
    if (count_ticket.revision) counts.cancel_count(count_ticket);
    shared_cursor={}; count_ticket={};
  }
  ~Impl() { release_operations(); } // Owners outlive us; no I/O or flush.
  void new_lifetime(RankRule rule, ScoringMode requested, SharedActivationHandle activation) {
    release_operations();
    if (!latched && requested == ScoringMode::BaseOnly) reason=BaseOnlyReason::HostSelected;
    latched = latched || requested == ScoringMode::BaseOnly;
    auto fresh=std::make_shared<RankLifetime>();
    fresh->input=input; fresh->shared=std::move(activation); fresh->rule=rule;
    fresh->mode=latched ? ScoringMode::BaseOnly : ScoringMode::Exact;
    fresh->reason=reason;
    if (!latched) fresh->state=counts.view();
    lifetime=std::move(fresh);
    ordered=lifetime->mode==ScoringMode::BaseOnly && rule.revision==1 ? scheme.base_order() : nullptr;
    phase=Phase::Idle; status=RankStatus::InProgress; first_scan=true;
    revision=0; batch={}; count_ticket={}; anchor={};
  }
  RankStep fail(RankStatus s, WorkCost c={}) {
    release_operations();
    status=s; phase=Phase::Finished; return {s,c};
  }
};

ExactRanker::ExactRanker(PagedSchemeIndex& s, PagedSharedResource& r, CountSource& c,
                       RankItem* work, std::size_t w, std::size_t b, std::size_t code_limit)
    : impl_(std::make_unique<Impl>(s,r,c,work,w,b,code_limit)) {}
ExactRanker::~ExactRanker() = default;

RankStatus ExactRanker::begin_input(std::string code, MountEpoch epoch, RankRule rule, ScoringMode mode) {
  auto& p=*impl_;
  if (!valid_buffer(p.work,p.w) || !p.b || p.b > std::numeric_limits<std::size_t>::max()/sizeof(RawQueryHit) ||
      !rule.revision || (mode!=ScoringMode::Exact && mode!=ScoringMode::BaseOnly) ||
      code.empty() || code.size()>p.code_limit || !is_valid_utf8(code)) return RankStatus::InvalidRequest;
  if (p.lease) return RankStatus::Busy;
  const auto activation=p.shared.activation_handle();
  if (!p.shared.activation_is_current(activation) || !p.scheme.ready() ||
      activation.mount_epoch()!=epoch || activation.registry_namespace()!=p.scheme.info().registry_namespace ||
      activation.shared_bundle_id()!=p.scheme.info().required_shared_bundle_id) return RankStatus::Stale;
  if (mode==ScoringMode::Exact && p.counts.view().registry_namespace!=activation.registry_namespace()) return RankStatus::InvalidRequest;
  std::string error;
  if (!p.scheme.begin_exact_query(code,epoch,&p.raw,&error)) return RankStatus::ResourceError;
  p.code=std::move(code);
  p.input=std::make_shared<InputQueryIdentity>(); p.latched=false; p.cancelled=false; p.reason=BaseOnlyReason::None;
  p.latest=p.scheme.query_context(p.raw);
  p.new_lifetime(rule,mode,activation);
  return RankStatus::InProgress;
}
RankStatus ExactRanker::restart_rank(ScoringMode mode) {
  auto& p=*impl_;
  if (mode!=ScoringMode::Exact && mode!=ScoringMode::BaseOnly) return RankStatus::InvalidRequest;
  if (p.lease) return RankStatus::Busy;
  if (!p.resources_current()) return RankStatus::Stale;
  if (!p.latched && mode==ScoringMode::Exact && p.counts.view().registry_namespace!=p.lifetime->shared.registry_namespace()) return RankStatus::InvalidRequest;
  std::string error;
  if (!p.scheme.restart_exact_query(p.latest,&p.raw,&error)) return RankStatus::Stale;
  const auto rule=p.lifetime->rule; const auto activation=p.lifetime->shared;
  p.latest=p.scheme.query_context(p.raw); p.new_lifetime(rule,mode,activation);
  return RankStatus::InProgress;
}
RankStatus ExactRanker::request_range(RangeDirection direction, const RankBoundary* anchor,
    std::size_t q, RankItem* output, std::size_t capacity, RankTicket* ticket) {
  auto& p=*impl_;
  if (!q || !ticket || !valid_buffer(output,capacity) ||
      !valid_buffer(p.work,p.w) || overlap(output,capacity,p.work,p.w) ||
      (direction!=RangeDirection::First && direction!=RangeDirection::After && direction!=RangeDirection::Before)) return RankStatus::InvalidRequest;
  if (q>p.w || q>capacity) return RankStatus::CapacityExceeded;
  if (!p.current()) return RankStatus::Stale;
  if (direction!=RangeDirection::First && (!anchor || !boundary_is_current(*anchor))) return RankStatus::Stale;
  // A new scoring lifetime must start at First, never at an old-mode anchor.
  if (p.first_scan && direction!=RangeDirection::First) return RankStatus::InvalidRequest;
  if (p.lease || (p.phase!=Impl::Phase::Idle && p.phase!=Impl::Phase::Finished)) return RankStatus::Busy;
  if (p.revision==std::numeric_limits<std::uint64_t>::max()) return RankStatus::InvalidRequest;
  if (!p.first_scan && !p.ordered) {
    std::string error;
    if (!p.scheme.restart_exact_query(p.latest,&p.raw,&error)) return RankStatus::Stale;
    p.latest=p.scheme.query_context(p.raw);
  }
  const bool was_first=p.first_scan;
  p.first_scan=false; ++p.revision;
  p.direction=direction; p.anchor=anchor ? *anchor : RankBoundary{};
  p.output=output; p.output_capacity=capacity; p.q=q;
  p.length=p.pos=p.copied=0; p.group={}; p.raw_count=p.eligible=0;
  p.batch={}; p.exhausted_raw=false; p.phase=Impl::Phase::Scan; p.status=RankStatus::InProgress;
  if (p.ordered) {
    if (was_first || !p.ordered->continue_range(p.ordered_cursor)) {
      const auto started = p.offered_prefix
          ? p.ordered->begin_prefix(p.code,p.ordered_cursor,*p.offered_prefix)
          : p.ordered->begin(p.code,p.ordered_cursor);
      if (started != BaseOrderStatus::Pending) return p.fail(RankStatus::ResourceError).status;
    }
    p.phase=Impl::Phase::OrderedLocate;
  }
  ticket->lifetime_=p.lifetime; ticket->revision_=p.revision;
  return RankStatus::InProgress;
}

RankStatus ExactRanker::request_first_prefix(BaseOrderPrefixState& state, std::size_t q,
                                           RankItem* result, std::size_t capacity, RankTicket* ticket) {
  auto& p = *impl_;
  if (!p.first_scan) {state.reset();return RankStatus::InvalidRequest;}
  p.offered_prefix = &state;
  const auto status = request_range(RangeDirection::First,nullptr,q,result,capacity,ticket);
  p.offered_prefix = nullptr;
  // begin_prefix already consumed it. Also retire it if the normal request
  // validation rejected the call or the current scoring mode has no BaseOrder.
  state.reset();
  return status;
}
RankStep ExactRanker::save_prefix(const RankTicket& ticket, BaseOrderPrefixState& state,
                                 const WorkBudget& budget) {
  auto& p = *impl_;
  state.reset();
  if (!budget.max_work_items) return {RankStatus::InvalidBudget,{}};
  if (!range_is_current(ticket) || !p.lease || p.phase != Impl::Phase::Finished ||
      (p.status != RankStatus::Ready && p.status != RankStatus::NoMatches) || !p.ordered)
    return {RankStatus::Stale,{}};
  // Publish releases the Reader operation. Only this validated completed
  // rank lease can export its already located cursor after that release.
  const auto saved = p.ordered->save_prefix_impl(p.ordered_cursor,state,true);
  return {saved.status == BaseOrderStatus::Ready ? RankStatus::Ready : RankStatus::Stale,saved.cost};
}

RankStep ExactRanker::advance(const RankTicket& ticket, const WorkBudget& budget) {
  auto& p=*impl_;
  if (!budget.max_read_calls || !budget.max_read_bytes || !budget.max_work_items) return {RankStatus::InvalidBudget,{}};
  if (!p.ticket_matches(ticket)) return {RankStatus::Stale,{}};
  if (p.cancelled) return {RankStatus::Cancelled,{}};
  if (p.phase==Impl::Phase::Finished && (p.status==RankStatus::StateUnavailable ||
      p.status==RankStatus::GroupOrderError || p.status==RankStatus::ResourceError)) return {p.status,{}};
  if (!p.current()) return p.fail(RankStatus::Stale);
  if (p.phase==Impl::Phase::Finished) return {p.status,{}};
  const WorkCost local{0,0,1}; // One local fixed-width operation per advance.
  using Phase=Impl::Phase;
  switch (p.phase) {
    case Phase::OrderedLocate: {
      auto step=p.ordered->locate(p.ordered_cursor,budget);
      if (step.status==BaseOrderStatus::NotFound) {p.raw_count=0;p.phase=Phase::Publish;}
      else if (step.status==BaseOrderStatus::Ready) {
        const auto total=p.ordered_cursor.count(); p.raw_count=total;
        if (p.direction==RangeDirection::Before) {
          p.ordered_end=p.anchor.position_; p.eligible=p.ordered_end;
          p.ordered_start=p.ordered_end>p.q ? p.ordered_end-static_cast<std::uint32_t>(p.q) : 0;
        } else {
          p.ordered_start=p.direction==RangeDirection::First ? 0 : p.anchor.position_+1;
          if (p.ordered_start>total) return p.fail(RankStatus::ResourceError,step.cost);
          p.eligible=total-p.ordered_start;
          p.ordered_end=p.ordered_start+static_cast<std::uint32_t>(std::min<std::size_t>(p.q,p.eligible));
        }
        if (p.ordered_end>total) return p.fail(RankStatus::ResourceError,step.cost);
        p.phase=Phase::OrderedRead;
      } else if (step.status!=BaseOrderStatus::Pending) return p.fail(RankStatus::ResourceError,step.cost);
      return {RankStatus::InProgress,step.cost};
    }
    case Phase::OrderedRead: {
      if (p.ordered_start+p.length==p.ordered_end) {p.phase=Phase::Publish;break;}
      auto step=p.ordered->read_item(p.ordered_cursor,p.ordered_start+static_cast<std::uint32_t>(p.length),budget);
      if (step.status==BaseOrderStatus::Ready) p.output[p.length++]=step.item;
      else if (step.status!=BaseOrderStatus::Pending) return p.fail(RankStatus::ResourceError,step.cost);
      return {RankStatus::InProgress,step.cost};
    }
    case Phase::Scan: {
      if (p.pos<p.batch.hits.size()) {
        const auto& h=p.batch.hits[p.pos];
        if (p.group.paths && h.runtime_ref<p.group.ref) return p.fail(RankStatus::GroupOrderError,local);
        if (p.group.paths && h.runtime_ref!=p.group.ref) {p.phase=Phase::SharedStart; break;}
        if (p.raw_count==std::numeric_limits<std::uint32_t>::max()) return p.fail(RankStatus::ResourceError,local);
        if (!p.group.paths) p.group={h.runtime_ref,h.scheme_weight,1,h.mapping_ordinal};
        else {
          ++p.group.paths;
          if (h.scheme_weight>p.group.maximum) {p.group.maximum=h.scheme_weight; p.group.ordinal=h.mapping_ordinal;}
          else if (h.scheme_weight==p.group.maximum && h.mapping_ordinal<p.group.ordinal) p.group.ordinal=h.mapping_ordinal;
        }
        ++p.pos; ++p.raw_count; break;
      }
      if (p.exhausted_raw) {p.phase=p.group.paths ? Phase::SharedStart : Phase::Copy; break;}
      // Drop previous vector before requesting the next; never aggregate hits.
      p.batch={}; p.pos=0;
      auto step=p.scheme.advance_exact_query(&p.raw,budget,p.b);
      if (!p.scheme.batch_is_current(step.batch)) return p.fail(RankStatus::Stale,step.cost);
      if (step.status==ExactQueryStepStatus::Error) return p.fail(RankStatus::ResourceError,step.cost);
      if (step.status==ExactQueryStepStatus::Stale || step.status==ExactQueryStepStatus::Cancelled) return p.fail(RankStatus::Stale,step.cost);
      p.exhausted_raw=step.status==ExactQueryStepStatus::Complete || step.status==ExactQueryStepStatus::NoResults;
      p.batch=std::move(step.batch);
      return {RankStatus::InProgress,step.cost};
    }
    case Phase::SharedStart:
      if (p.shared.begin_read(SharedRecordKind::Lexeme,p.group.ref,&p.shared_cursor)!=SharedReadStatus::InProgress) return p.fail(RankStatus::ResourceError,local);
      p.phase=Phase::SharedAdvance; break;
    case Phase::SharedAdvance: {
      auto step=p.shared.advance_read(&p.shared_cursor,budget);
      if (step.status==SharedReadStatus::Ready) {
        if (!p.shared.result_is_current(step) || !p.current()) return p.fail(RankStatus::Stale,step.cost);
        p.record=step.lexeme; p.phase=Phase::CountStart;
      } else if (step.status!=SharedReadStatus::InProgress) return p.fail(step.status==SharedReadStatus::Stale ? RankStatus::Stale : RankStatus::ResourceError,step.cost);
      return {RankStatus::InProgress,step.cost};
    }
    case Phase::CountStart:
      p.pending.effective_count=0;
      if (p.lifetime->mode==ScoringMode::BaseOnly) p.phase=Phase::Score;
      else {p.count_ticket=p.counts.begin_count(p.lifetime->state,p.record.identity.stable_key); p.phase=Phase::CountAdvance;}
      break;
    case Phase::CountAdvance: {
      auto step=p.counts.advance_count(p.count_ticket,budget);
      if (step.ticket.revision!=p.count_ticket.revision || !same_view(step.ticket.view,p.count_ticket.view) ||
          !same_view(step.ticket.view,p.lifetime->state)) return p.fail(RankStatus::Stale,step.cost);
      if (step.status==CountStatus::Unavailable) {
        p.latched=true; p.reason=BaseOnlyReason::StateUnavailable;
        return p.fail(RankStatus::StateUnavailable,step.cost);
      }
      if (step.status==CountStatus::Stale || !p.current()) return p.fail(RankStatus::Stale,step.cost);
      if (step.status==CountStatus::Pending) return {RankStatus::WaitingState,step.cost};
      p.pending.effective_count=step.status==CountStatus::Found ? step.count : 0;
      p.phase=Phase::Score; return {RankStatus::InProgress,step.cost};
    }
    case Phase::Score:
      p.pending.key=p.record.identity.stable_key; p.pending.ref=p.group.ref;
      p.pending.scheme_weight=p.group.maximum; p.pending.path_count=p.group.paths;
      p.pending.representative_ordinal=p.group.ordinal;
      p.pending.score=std::int64_t(p.record.identity.base_weight)+std::int64_t(p.group.maximum)+
          std::int64_t(p.pending.effective_count)*std::int64_t(p.lifetime->rule.lambda);
      p.phase=Phase::Filter; break;
    case Phase::Filter: {
      const bool accepted=p.direction==RangeDirection::First ||
          (p.direction==RangeDirection::After ? better(p.anchor.score_,p.anchor.key_,p.pending.score,p.pending.key) :
           better(p.pending.score,p.pending.key,p.anchor.score_,p.anchor.key_));
      if (!accepted) {p.group={}; p.phase=Phase::Scan; break;}
      ++p.eligible; p.lo=0; p.hi=p.length; p.phase=Phase::Search; break;
    }
    case Phase::Search:
      if (p.lo<p.hi) {
        const auto mid=p.lo+(p.hi-p.lo)/2;
        const auto& x=p.work[mid];
        const bool before=p.direction==RangeDirection::Before ?
            better(x.score,x.key,p.pending.score,p.pending.key) : better(p.pending.score,p.pending.key,x.score,x.key);
        if (before) p.hi=mid; else p.lo=mid+1;
      } else if (p.lo>=p.q) {p.group={}; p.phase=Phase::Scan;}
      else {p.shift=p.length<p.q ? p.length : p.q-1; p.phase=Phase::Shift;}
      break;
    case Phase::Shift:
      if (p.shift>p.lo) {p.work[p.shift]=p.work[p.shift-1]; --p.shift;}
      else p.phase=Phase::Insert;
      break;
    case Phase::Insert:
      p.work[p.lo]=p.pending; if (p.length<p.q) ++p.length;
      p.group={}; p.phase=Phase::Scan; break;
    case Phase::Copy:
      if (p.copied<p.length) {
        p.output[p.copied]=p.work[p.direction==RangeDirection::Before ? p.length-1-p.copied : p.copied]; ++p.copied;
      } else p.phase=Phase::Publish;
      break;
    case Phase::Publish:
      p.lease=true;
      return p.fail(p.length ? RankStatus::Ready : (p.raw_count ? RankStatus::RangeExhausted : RankStatus::NoMatches),local);
    case Phase::Idle: return {RankStatus::InvalidRequest,{}};
    case Phase::Finished: return {p.status,{}};
  }
  return {RankStatus::InProgress,local};
}
bool ExactRanker::range_is_current(const RankTicket& t) const {return impl_->ticket_matches(t) && impl_->current();}
bool ExactRanker::boundary_is_current(const RankBoundary& b) const {return b.lifetime_ && b.lifetime_==impl_->lifetime && impl_->current();}
RankResultView ExactRanker::result(const RankTicket& t) const {
  const auto& p=*impl_;
  if (!range_is_current(t) || !p.lease) return {};
  return {p.output,p.length,p.eligible>p.q,p.lifetime->mode,p.lifetime->reason};
}
RankBoundary ExactRanker::boundary(const RankTicket& t,std::size_t slot) const {
  RankBoundary b; const auto v=result(t);
  if (slot<v.size) {b.lifetime_=impl_->lifetime; b.key_=v.items[slot].key; b.score_=v.items[slot].score;
    b.position_=impl_->ordered_start+static_cast<std::uint32_t>(slot);}
  return b;
}
void ExactRanker::release(const RankTicket& t) {if (impl_->ticket_matches(t)) impl_->lease=false;}
void ExactRanker::cancel(const RankTicket& t) {
  if (impl_->ticket_matches(t)) {
    impl_->cancelled=true;
    impl_->fail(RankStatus::Cancelled);
  }
}
InputHandle ExactRanker::input_handle() const {
  InputHandle handle; handle.identity_=impl_->input; return handle;
}
bool ExactRanker::input_is_current(const InputHandle& handle) const {
  return handle.identity_ && handle.identity_==impl_->input && impl_->resources_current();
}
bool ExactRanker::abandon_range(const RankTicket& t) {
  auto& p=*impl_;
  if (!p.ticket_matches(t)) return false;
  p.release_operations();
  p.batch={}; p.group={}; p.lease=false;
  if (p.revision==std::numeric_limits<std::uint64_t>::max()) {
    p.cancelled=true; p.fail(RankStatus::Cancelled); return true;
  }
  ++p.revision; p.phase=Impl::Phase::Finished;
  // Do not erase an Unavailable latch or pretend a failed scoring view is fresh.
  if (p.status!=RankStatus::Stale && p.status!=RankStatus::StateUnavailable && p.status!=RankStatus::ResourceError &&
      p.status!=RankStatus::GroupOrderError) p.status=RankStatus::InProgress;
  return true;
}
ScoringMode ExactRanker::mode() const {return impl_->lifetime ? impl_->lifetime->mode : ScoringMode::Exact;}
bool ExactRanker::uses_base_order() const {return bool(impl_->ordered);}
RankMemoryUsage ExactRanker::memory_usage() const {
  const auto& p=*impl_;
  return {sizeof(*this),sizeof(Impl),sizeof(RankLifetime),p.w*sizeof(RankItem),
      p.output_capacity*sizeof(RankItem),p.batch.hits.capacity()*sizeof(RawQueryHit),
      sizeof(Impl::Group),sizeof(RankItem),sizeof(SharedReadCursor),sizeof(SharedLexemeRecord),
      sizeof(CountTicket),sizeof(RankBoundary),p.raw.memory_usage(),p.ordered_cursor.memory_bytes(),p.code.capacity()+1};
}
} // namespace hanime
