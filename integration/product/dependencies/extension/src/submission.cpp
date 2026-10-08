#include "hanime_extension/submission.hpp"
#include "fixed_member_array.hpp"
#include <limits>
#include <stdexcept>

namespace hanime_extension {
using namespace hanime;
namespace {
SubmissionResult error(CompositionStatus status) { SubmissionResult r; r.core.status=status; return r; }
struct Guard { bool& flag; explicit Guard(bool& f):flag(f){flag=true;} ~Guard(){flag=false;} };
CompositionConfig checked(CompositionConfig c) {
  if (!c.max_members||c.max_members>64||!c.form_bytes||c.form_bytes>512||
      !c.member_text_bytes||c.member_text_bytes>4096||!c.total_text_bytes||c.total_text_bytes>16384||
      c.learning!=CompositionLearningPolicy::Disabled) throw std::invalid_argument("Invalid bounded submission configuration");
  return c;
}
}
CompositionResult SubmissionHandle::result() const {
  const auto r=core_.result();
  if(r.attempt==attempt_)return r;
  // The core receipt is mutable across retries. Do not lend its new result or
  // retry permit to an event that was authorized for an earlier attempt.
  auto stale=error(CompositionStatus::Stale).core;
  stale.attempt=attempt_;stale.origin=r.origin;
  return stale;
}
struct Submission::Impl {
  Composition& input;
  PagedSharedResource& shared;
  CompositionConfig config;
  CompositionCommitter core;
  detail::FixedMemberArray<CompositionMember> capture;
  std::shared_ptr<const OwnerIdentity> owner{std::make_shared<OwnerIdentity>()};
  SubmissionHandle active;
  SharedActivationHandle binding;
  std::uint64_t epoch{0},sequence{0};
  bool locked{false},calling{false},closed{false};
  Impl(Composition& c,PagedSharedResource& r,TextCommitSink& sink,CompositionConfig cfg,StorageBackend backend)
    :input(c),shared(r),config(checked(cfg)),core(r,sink,config),
     capture(config.max_members,StorageRole::SubmissionCapture,backend) {}
  bool ours(const SubmissionHandle& h) const {
    return h.owner_==owner&&h.sequence_&&h.sequence_==sequence&&h.attempt_==active.attempt_;
  }
  bool current(const CompositionResult& r) const {
    const auto s=input.snapshot();
    return input.valid()&&shared.activation_is_current(binding)&&s.scheme_epoch==epoch&&
      s.input_id==r.origin.input_revision&&s.revision==r.origin.selection_revision&&
      s.state==CompositionState::AllSelected&&r.origin.source.begin==0&&r.origin.source.end==s.raw.size();
  }
  SubmissionResult wrap(CompositionResult r) const {return {std::move(r),active};}
  SubmissionResult historical(const SubmissionHandle& h) const {
    if(h.owner_!=owner)return error(CompositionStatus::Stale);
    return {h.result(),h};
  }
  void unlock(bool unknown=false) {
    if(locked){input.end_submission(unknown);locked=false;}
  }
  SubmissionResult finish(CompositionResult r) {
    if(r.status!=CompositionStatus::Preparing&&r.status!=CompositionStatus::Ready&&r.status!=CompositionStatus::CallingSink) {
      const auto s=input.snapshot();
      const bool accepted=r.status==CompositionStatus::Accepted&&s.input_id==r.origin.input_revision&&
        s.revision==r.origin.selection_revision;
      unlock(r.status==CompositionStatus::UnknownOutcome);
      if(accepted) input.cancel_input(r.origin.input_revision);
    }
    return wrap(std::move(r));
  }
};
Submission::Submission(Composition& c,PagedSharedResource& r,TextCommitSink& sink,CompositionConfig cfg)
  :Submission(c,r,sink,cfg,StorageBackend{}) {}
Submission::Submission(Composition& c,PagedSharedResource& r,TextCommitSink& sink,CompositionConfig cfg,StorageBackend backend) {
  detail::validate_storage_backend(backend);
  p_=std::make_unique<Impl>(c,r,sink,cfg,backend);
}
Submission::~Submission(){close();}
StorageAllocation Submission::storage_usage() const noexcept { return p_->capture.allocation(); }
bool Submission::valid() const {return p_->core.valid()&&!p_->closed;}
SubmissionResult Submission::result() const {return p_->wrap(p_->active.result());}
SubmissionResult Submission::prepare() {
  auto& p=*p_;
  if(p.calling)return error(CompositionStatus::Busy);
  if(p.closed)return error(CompositionStatus::Closed);
  if(!p.core.valid())return error(CompositionStatus::AllocationFailure);
  auto previous=p.active.result();
  if(previous.status==CompositionStatus::UnknownOutcome)return p.wrap(previous);
  if(p.sequence&&p.current(previous)&&(previous.status==CompositionStatus::Preparing||
     previous.status==CompositionStatus::Ready||previous.status==CompositionStatus::RetryableNoEffect||
     previous.status==CompositionStatus::Accepted))return p.wrap(previous);
  if(p.locked){p.core.cancel(p.active.core_);p.unlock();}
  const auto s=p.input.snapshot();
  if(!p.input.valid()||!p.shared.activation_is_current(s.activation))return error(CompositionStatus::Stale);
  if(s.state!=CompositionState::AllSelected)return error(CompositionStatus::InvalidRequest);
  if(p.sequence==std::numeric_limits<std::uint64_t>::max())return error(CompositionStatus::CapacityExceeded);
  if(s.member_count>p.capture.size()) {
    auto r=error(CompositionStatus::CapacityExceeded);r.core.capacity=CompositionCapacity::Members;
    r.core.required_capacity=s.member_count;return r;
  }
  if(!p.input.begin_submission())return error(CompositionStatus::Busy);
  p.locked=true;
  for(std::size_t i=0;i<s.member_count;++i) {
    const auto& m=s.members[i];
    p.capture[i]={m.ref,m.key,{m.begin,m.end},{s.output_forms.substr(m.form_begin,m.form_size),m.invocation}};
  }
  const auto r=p.core.prepare(s.activation,{s.input_id,s.revision,{0,s.raw.size()}},p.capture.data(),s.member_count);
  if(r.status!=CompositionStatus::Preparing){p.unlock();return {r,{}};}
  p.active.owner_=p.owner;p.active.sequence_=++p.sequence;p.active.core_=r.transaction;
  p.active.attempt_=r.attempt;
  p.binding=s.activation;p.epoch=s.scheme_epoch;
  return p.wrap(r);
}
SubmissionStep Submission::advance(const SubmissionHandle& h,const WorkBudget& b) {
  auto& p=*p_;
  if(p.calling)return {error(CompositionStatus::Busy),{}};
  if(!p.ours(h))return {p.historical(h),{}};
  if(h.result().status==CompositionStatus::Preparing&&!p.current(h.result())) {
    p.core.cancel(h.core_);p.unlock();return {error(CompositionStatus::Stale),{}};
  }
  const auto step=p.core.advance(h.core_,b);
  // InvalidBudget is not a terminal transaction result and must keep the lock.
  if(step.result.status==CompositionStatus::InvalidBudget)return {{step.result,h},step.cost};
  return {p.finish(step.result),step.cost};
}
CompositionStatus Submission::prepared(const SubmissionHandle& h,
    const std::function<void(const PreparedCompositionView&)>& callback) {
  auto& p=*p_;
  if(p.calling)return CompositionStatus::Busy;
  if(!p.ours(h))return CompositionStatus::Stale;
  if(!p.current(h.result())){p.core.cancel(h.core_);p.unlock();return CompositionStatus::Stale;}
  Guard guard(p.calling);return p.core.prepared(h.core_,callback);
}
SubmissionResult Submission::confirm(const SubmissionHandle& h) {
  auto& p=*p_;
  if(p.calling)return error(CompositionStatus::Busy);
  if(!p.ours(h))return p.historical(h);
  if(h.result().status==CompositionStatus::Ready&&!p.current(h.result())) {
    p.core.cancel(h.core_);p.unlock();return error(CompositionStatus::Stale);
  }
  Guard guard(p.calling);return p.finish(p.core.confirm(h.core_));
}
SubmissionResult Submission::retry(const SubmissionHandle& h) {
  auto& p=*p_;
  if(p.calling)return error(CompositionStatus::Busy);
  if(p.closed)return error(CompositionStatus::Closed);
  if(!p.ours(h))return p.historical(h);
  const auto r=h.result();
  if(r.status!=CompositionStatus::RetryableNoEffect)return p.wrap(r);
  if(!p.current(r)){p.core.cancel(h.core_);return error(CompositionStatus::Stale);}
  if(!p.input.begin_submission())return error(CompositionStatus::Busy);
  p.locked=true;
  const auto next=p.core.retry(r.retry);
  if(next.status==CompositionStatus::Preparing)p.active.attempt_=next.attempt;
  return p.finish(next);
}
SubmissionResult Submission::cancel(const SubmissionHandle& h) {
  auto& p=*p_;
  if(p.calling)return error(CompositionStatus::Busy);
  if(!p.ours(h))return p.historical(h);
  return p.finish(p.core.cancel(h.core_));
}
SubmissionResult Submission::close() {
  auto& p=*p_;
  if(p.calling)return error(CompositionStatus::Busy);
  p.closed=true;return p.finish(p.core.close());
}
const CompositionCosts& Submission::costs() const {return p_->core.costs();}
SubmissionMemory Submission::memory_usage() const {
  return {sizeof(*this),sizeof(Impl),p_->capture.capacity()*sizeof(CompositionMember),p_->core.memory_usage()};
}
} // namespace hanime_extension
