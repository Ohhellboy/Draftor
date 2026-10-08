#include "hanime/composition_commit.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace hanime {
struct CompositionOwnerIdentity {};
struct CompositionIdentity {
  std::shared_ptr<const CompositionOwnerIdentity> owner;
  SharedActivationHandle binding;
  CompositionOrigin origin;
  CompositionStatus status{CompositionStatus::Preparing};
  std::uint64_t attempt{1};
  std::size_t member_index{0}, count{0}, text_bytes{0};
  CompositionCapacity capacity{CompositionCapacity::None};
  std::size_t required{0};
  OutputStatus output{OutputStatus::Pending};
};
CompositionResult CompositionHandle::result() const {
  CompositionResult r;
  if (!identity_) return r;
  const auto& t=*identity_;
  r.transaction=*this; r.status=t.status; r.origin=t.origin; r.attempt=t.attempt;
  r.member_index=t.member_index; r.member_count=t.count; r.text_bytes=t.text_bytes;
  r.capacity=t.capacity; r.required_capacity=t.required; r.output=t.output;
  if (t.status==CompositionStatus::RetryableNoEffect) {
    r.retry.identity_=identity_; r.retry.attempt_=t.attempt;
  }
  return r;
}
namespace {
CompositionResult error(CompositionStatus status) {CompositionResult r; r.status=status; return r;}
void add(WorkCost& a,WorkCost b) {
  a.read_calls+=b.read_calls; a.read_bytes+=b.read_bytes; a.work_items+=b.work_items;
}
bool config_valid(const CompositionConfig& c) {
  if (!c.max_members || !c.form_bytes || !c.member_text_bytes || !c.total_text_bytes ||
      c.learning!=CompositionLearningPolicy::Disabled) return false;
  std::size_t size=0;
  auto account=[&](std::size_t n,std::size_t w) {
    if (n>std::numeric_limits<std::size_t>::max()/w || n*w>std::numeric_limits<std::size_t>::max()-size) return false;
    size+=n*w; return true;
  };
  return account(c.max_members,sizeof(PreparedCompositionMember)) && account(c.max_members,c.form_bytes) &&
      account(1,c.form_bytes) && account(1,c.member_text_bytes) && account(1,c.total_text_bytes);
}
struct BusyGuard {
  bool& busy;
  explicit BusyGuard(bool& b):busy(b) {busy=true;}
  ~BusyGuard() {busy=false;}
};
bool cancellable(CompositionStatus s) {
  return s==CompositionStatus::Preparing || s==CompositionStatus::Ready || s==CompositionStatus::RetryableNoEffect;
}
}
struct CompositionCommitter::Impl {
  enum class Phase { IdentityStart, IdentityRead, OutputStart, OutputRead, Copy, Next, Done };
  PagedSharedResource& shared;
  TextCommitSink& sink;
  CompositionConfig config;
  std::shared_ptr<const CompositionOwnerIdentity> owner{std::make_shared<CompositionOwnerIdentity>()};
  CompositionHandle active;
  std::unique_ptr<PreparedCompositionMember[]> members;
  std::unique_ptr<char[]> requests,names,member_text,text;
  OutputReader output;
  SharedReadCursor cursor;
  CompositionCosts costs;
  std::size_t position{0}, copied{0}, member_length{0};
  bool reading{false}, busy{false}, closed{false};
  Phase phase{Phase::Done};
  Impl(PagedSharedResource& r,TextCommitSink& s,CompositionConfig c):shared(r),sink(s),config(c) {
    members=std::make_unique<PreparedCompositionMember[]>(c.max_members);
    requests=std::make_unique<char[]>(c.max_members*c.form_bytes);
    names=std::make_unique<char[]>(c.form_bytes);
    member_text=std::make_unique<char[]>(c.member_text_bytes);
    text=std::make_unique<char[]>(c.total_text_bytes);
  }
  bool owns(const CompositionHandle& h) const {
    return h.identity_ && h.identity_==active.identity_ && h.identity_->owner==owner;
  }
  void release() {
    if (reading) shared.cancel_read(&cursor);
    reading=false; output.cancel(); phase=Phase::Done;
  }
  void finish(CompositionStatus s) {release(); active.identity_->status=s;}
  void reset() {
    release(); position=copied=member_length=0;
    auto& t=*active.identity_; t.text_bytes=0; t.member_index=0;
    t.output=OutputStatus::Pending; t.capacity=CompositionCapacity::None; t.required=0;
    t.status=CompositionStatus::Preparing; phase=Phase::IdentityStart;
  }
  CompositionStep step(const WorkBudget& b) {
    auto& t=*active.identity_; WorkCost cost{0,0,1};
    auto& m=members[position];
    switch (phase) {
      case Phase::IdentityStart: {
        const auto s=shared.begin_read(SharedRecordKind::Lexeme,m.selected.ref,&cursor);
        if (s!=SharedReadStatus::InProgress) finish(CompositionStatus::ReadError);
        else {reading=true; phase=Phase::IdentityRead;}
        break;
      }
      case Phase::IdentityRead: {
        const auto s=shared.advance_read(&cursor,b); cost=s.cost;
        if (s.status==SharedReadStatus::InProgress) break;
        reading=false;
        if (s.status==SharedReadStatus::Stale || s.status==SharedReadStatus::Cancelled) finish(CompositionStatus::Stale);
        else if (s.status!=SharedReadStatus::Ready) finish(CompositionStatus::ReadError);
        else if (!shared.result_is_current(s)) finish(CompositionStatus::Stale);
        else if (s.lexeme.identity.stable_key!=m.selected.expected_key) finish(CompositionStatus::IdentityMismatch);
        else phase=Phase::OutputStart;
        break;
      }
      case Phase::OutputStart: {
        const auto s=output.begin(shared,t.binding,m.selected.ref,m.selected.expected_key,m.selected.output,
            names.get(),config.form_bytes,member_text.get(),config.member_text_bytes);
        if (s==OutputStatus::Pending) phase=Phase::OutputRead;
        else {t.output=s; finish(s==OutputStatus::Stale ? CompositionStatus::Stale : CompositionStatus::InvalidRequest);}
        break;
      }
      case Phase::OutputRead: {
        const auto s=output.advance(b); cost=s.cost; t.output=s.status;
        if (s.status==OutputStatus::Pending) break;
        if (output_has_text(s.status)) {
          member_length=s.text.size(); copied=0;
          if (member_length>config.total_text_bytes-t.text_bytes) {
            t.capacity=CompositionCapacity::TotalText; t.required=t.text_bytes+member_length;
            finish(CompositionStatus::CapacityExceeded);
          } else {m.text={t.text_bytes,t.text_bytes+member_length}; m.resolved=s.status; phase=Phase::Copy;}
        } else if (s.status==OutputStatus::TooLarge) {
          const auto needed=output.capacity_requirement();
          t.capacity=needed.name ? CompositionCapacity::ResolverName : CompositionCapacity::MemberText;
          t.required=needed.bytes; finish(CompositionStatus::CapacityExceeded);
        } else if (s.status==OutputStatus::ReadError) finish(CompositionStatus::ReadError);
        else if (s.status==OutputStatus::Stale || s.status==OutputStatus::Cancelled) finish(CompositionStatus::Stale);
        else if (s.status==OutputStatus::InvalidRequest) finish(CompositionStatus::InvalidRequest);
        else finish(CompositionStatus::OutputRestricted);
        break;
      }
      case Phase::Copy: {
        const auto n=std::min(b.max_work_items,member_length-copied);
        if (n) std::memcpy(text.get()+t.text_bytes+copied,member_text.get()+copied,n);
        copied+=n; cost.work_items=n ? n : 1; costs.copied_text_bytes+=n;
        if (copied==member_length) {t.text_bytes+=member_length; phase=Phase::Next;}
        break;
      }
      case Phase::Next:
        ++position;
        if (position==t.count) finish(CompositionStatus::Ready);
        else {t.member_index=position; phase=Phase::IdentityStart;}
        break;
      case Phase::Done: break;
    }
    // Reader callbacks must not reactivate resources, but still reject late
    // values if the host violates that sequencing; never publish partial text.
    if (!shared.activation_is_current(t.binding)) finish(CompositionStatus::Stale);
    add(costs.preparation,cost);
    return {active.result(),cost};
  }
};
CompositionCommitter::CompositionCommitter(PagedSharedResource& r,TextCommitSink& s,const CompositionConfig& c) {
  if (!config_valid(c)) return;
  try {impl_=std::make_unique<Impl>(r,s,c);} catch (const std::bad_alloc&) {}
}
CompositionCommitter::~CompositionCommitter() {if (impl_) close();}
bool CompositionCommitter::valid() const {return bool(impl_);}
CompositionResult CompositionCommitter::prepare(const SharedActivationHandle& binding,CompositionOrigin origin,
    const CompositionMember* input,std::size_t count) {
  if (!impl_) return error(CompositionStatus::AllocationFailure);
  auto& p=*impl_;
  if (p.busy) return error(CompositionStatus::Busy);
  if (p.closed) return error(CompositionStatus::Closed);
  if (p.active.identity_ && p.active.identity_->status==CompositionStatus::UnknownOutcome) return p.active.result();
  if (!count || !input || !origin.input_revision || !origin.selection_revision || origin.source.begin>=origin.source.end)
    return error(CompositionStatus::InvalidRequest);
  if (!p.shared.activation_is_current(binding)) return error(CompositionStatus::Stale);
  if (count>p.config.max_members) {
    auto r=error(CompositionStatus::CapacityExceeded); r.capacity=CompositionCapacity::Members; r.required_capacity=count; return r;
  }
  std::size_t previous=origin.source.begin,bytes=0;
  for (std::size_t i=0;i<count;++i) {
    const auto& m=input[i];
    if (m.source.begin!=previous || m.source.begin>=m.source.end || m.source.end>origin.source.end ||
        !m.ref || m.ref>p.shared.info().sections[0].count || m.expected_key.empty()) return error(CompositionStatus::InvalidRequest);
    previous=m.source.end;
    if (m.output.form.size()>p.config.form_bytes) {
      auto r=error(CompositionStatus::CapacityExceeded); r.member_index=i;
      r.capacity=CompositionCapacity::RequestName; r.required_capacity=m.output.form.size(); return r;
    }
    if ((!m.output.form.empty() && !is_valid_utf8(m.output.form)) ||
        (m.output.invocation!=OutputInvocation::Profile && m.output.invocation!=OutputInvocation::Explicit &&
         m.output.invocation!=OutputInvocation::Expanded)) return error(CompositionStatus::InvalidRequest);
    bytes+=sizeof(PreparedCompositionMember)+m.output.form.size();
  }
  if (previous!=origin.source.end) return error(CompositionStatus::InvalidRequest);
  std::shared_ptr<CompositionIdentity> next;
  try {next=std::make_shared<CompositionIdentity>();} catch (const std::bad_alloc&) {return error(CompositionStatus::AllocationFailure);}
  BusyGuard guard(p.busy);
  if (p.active.identity_ && cancellable(p.active.identity_->status)) p.finish(CompositionStatus::Cancelled);
  else p.release();
  p.active.identity_=std::move(next);
  auto& t=*p.active.identity_; t.owner=p.owner; t.binding=binding; t.origin=origin; t.count=count;
  for (std::size_t i=0;i<count;++i) {
    auto& m=p.members[i]; m={}; m.selected=input[i];
    char* dest=p.requests.get()+i*p.config.form_bytes;
    if (!input[i].output.form.empty()) std::memcpy(dest,input[i].output.form.data(),input[i].output.form.size());
    m.selected.output.form={dest,input[i].output.form.size()};
  }
  p.costs.captured_bytes+=bytes; p.reset(); return p.active.result();
}
CompositionStep CompositionCommitter::advance(const CompositionHandle& h,const WorkBudget& b) {
  if (!impl_) return {error(CompositionStatus::AllocationFailure),{}};
  auto& p=*impl_;
  if (p.busy) return {error(CompositionStatus::Busy),{}};
  if (!p.owns(h)) return {error(CompositionStatus::Stale),{}};
  if (!b.max_read_calls || !b.max_read_bytes || !b.max_work_items) return {error(CompositionStatus::InvalidBudget),{}};
  if (h.identity_->status!=CompositionStatus::Preparing) return {h.result(),{}};
  BusyGuard guard(p.busy);
  if (!p.shared.activation_is_current(h.identity_->binding)) {p.finish(CompositionStatus::Stale); return {h.result(),{0,0,1}};}
  return p.step(b);
}
CompositionStatus CompositionCommitter::prepared(const CompositionHandle& h,
    const std::function<void(const PreparedCompositionView&)>& callback) {
  if (!impl_) return CompositionStatus::AllocationFailure;
  auto& p=*impl_;
  if (p.busy) return CompositionStatus::Busy;
  if (!p.owns(h)) return CompositionStatus::Stale;
  if (p.closed) return CompositionStatus::Closed;
  const auto s=h.identity_->status;
  if (s!=CompositionStatus::Ready && s!=CompositionStatus::Accepted && s!=CompositionStatus::UnknownOutcome &&
      s!=CompositionStatus::RetryableNoEffect) return s;
  if (!callback) return CompositionStatus::InvalidRequest;
  if (s==CompositionStatus::Ready && !p.shared.activation_is_current(h.identity_->binding)) {
    p.finish(CompositionStatus::Stale); return CompositionStatus::Stale;
  }
  BusyGuard guard(p.busy); const auto& t=*h.identity_;
  callback({t.binding,t.origin,p.members.get(),t.count,{p.text.get(),t.text_bytes}});
  return s;
}
CompositionResult CompositionCommitter::confirm(const CompositionHandle& h) {
  if (!impl_) return error(CompositionStatus::AllocationFailure);
  auto& p=*impl_;
  if (p.busy) return error(CompositionStatus::Busy);
  if (!p.owns(h)) return error(CompositionStatus::Stale);
  auto& t=*h.identity_;
  if (t.status!=CompositionStatus::Ready) return h.result();
  if (!p.shared.activation_is_current(t.binding)) {p.finish(CompositionStatus::Stale); return h.result();}
  BusyGuard guard(p.busy); t.status=CompositionStatus::CallingSink; ++p.costs.sink_calls;
  bool accepted=false;
  try {accepted=p.sink.commit({p.text.get(),t.text_bytes});}
  catch (...) {p.finish(CompositionStatus::UnknownOutcome); return h.result();}
  if (accepted) p.finish(CompositionStatus::Accepted);
  else if (!p.shared.activation_is_current(t.binding)) p.finish(CompositionStatus::Stale);
  else p.finish(CompositionStatus::RetryableNoEffect);
  return h.result();
}
CompositionResult CompositionCommitter::retry(const CompositionRetryPermit& permit) {
  if (!impl_) return error(CompositionStatus::AllocationFailure);
  auto& p=*impl_; CompositionHandle h; h.identity_=permit.identity_;
  if (p.busy) return error(CompositionStatus::Busy);
  if (!p.owns(h)) return error(CompositionStatus::Stale);
  auto& t=*h.identity_;
  if (t.status!=CompositionStatus::RetryableNoEffect || t.attempt!=permit.attempt_) return h.result();
  if (!p.shared.activation_is_current(t.binding)) {p.finish(CompositionStatus::Stale); return h.result();}
  if (t.attempt==std::numeric_limits<std::uint64_t>::max()) {p.finish(CompositionStatus::Cancelled); return h.result();}
  BusyGuard guard(p.busy); ++t.attempt; p.reset(); return h.result();
}
CompositionResult CompositionCommitter::cancel(const CompositionHandle& h) {
  if (!impl_) return error(CompositionStatus::AllocationFailure);
  auto& p=*impl_;
  if (p.busy) return error(CompositionStatus::Busy);
  if (!p.owns(h)) return error(CompositionStatus::Stale);
  if (cancellable(h.identity_->status)) p.finish(CompositionStatus::Cancelled);
  else p.release();
  return h.result();
}
CompositionResult CompositionCommitter::close() {
  if (!impl_) return error(CompositionStatus::AllocationFailure);
  auto& p=*impl_;
  if (p.busy) return error(CompositionStatus::Busy);
  p.closed=true;
  if (!p.active.identity_) {p.release(); return error(CompositionStatus::Closed);}
  return cancel(p.active);
}
const CompositionCosts& CompositionCommitter::costs() const {
  static const CompositionCosts empty;
  return impl_ ? impl_->costs : empty;
}
CompositionMemoryUsage CompositionCommitter::memory_usage() const {
  if (!impl_) return {};
  const auto& c=impl_->config;
  return {sizeof(*this),sizeof(Impl),c.max_members*sizeof(PreparedCompositionMember),c.max_members*c.form_bytes,
    c.form_bytes,c.member_text_bytes,c.total_text_bytes,sizeof(CompositionOwnerIdentity),sizeof(CompositionIdentity),
    sizeof(OutputReader),sizeof(SharedReadCursor)};
}
} // namespace hanime
