#include "hanime/member_eligibility.hpp"
#include <array>
#include <algorithm>
namespace hanime {
struct MemberProof {
  std::shared_ptr<const int> owner;
  MemberRequest request;
  std::array<char,128> code{},form{};
  bool eligible{false};
};
struct MemberEligibility::Impl {
  BaseOrderIndex& index; PagedSharedResource& shared;
  char* names; std::size_t f; char* text; std::size_t l;
  std::shared_ptr<const int> identity{std::make_shared<const int>(0)};
  std::shared_ptr<MemberProof> proof;
  BaseOrderCursor cursor;
  OutputReader output;
  MemberStatus status{MemberStatus::Cancelled};
  OutputStatus output_status{OutputStatus::Pending};
  enum class Phase {Locate,Member,OutputStart,Output,Done} phase{Phase::Done};
  Impl(BaseOrderIndex& i,PagedSharedResource& s,char* n,std::size_t nf,char* t,std::size_t nt)
      :index(i),shared(s),names(n),f(nf),text(t),l(nt) {}
  void release() {index.cancel(cursor);output.cancel();}
  MemberStep finish(MemberStatus s,WorkCost c={}) {
    status=s;phase=Phase::Done;release();if(proof)proof->eligible=s==MemberStatus::Eligible;
    return {s,output_status,c};
  }
};
MemberEligibility::MemberEligibility(BaseOrderIndex& i,PagedSharedResource& s,char* n,std::size_t nf,char* t,std::size_t nt)
    :impl_(std::make_unique<Impl>(i,s,n,nf,t,nt)) {}
MemberEligibility::~MemberEligibility() {impl_->release();}
MemberStatus MemberEligibility::begin(const MemberRequest& r,MemberTicket* t) {
  auto& p=*impl_;
  if (!t || !p.names || !p.text || !p.f || !p.l || !r.input_revision || !r.selection_revision ||
      !r.ref || r.key.empty() || r.begin>=r.end || r.end>r.source_bytes || r.code.empty() || r.code.size()>128 ||
      r.output.form.size()>128 || !is_valid_utf8(r.code) ||
      (!r.output.form.empty() && !is_valid_utf8(r.output.form))) return MemberStatus::InvalidRequest;
  if (!p.index.matches(p.shared) || !p.shared.activation_is_current(r.shared)) return MemberStatus::Stale;
  p.release();p.proof=std::make_shared<MemberProof>();p.proof->owner=p.identity;
  p.proof->request=r;
  std::copy(r.code.begin(),r.code.end(),p.proof->code.begin());
  std::copy(r.output.form.begin(),r.output.form.end(),p.proof->form.begin());
  p.proof->request.code={p.proof->code.data(),r.code.size()};
  p.proof->request.output.form={p.proof->form.data(),r.output.form.size()};
  if (p.index.begin(p.proof->request.code,p.cursor)!=BaseOrderStatus::Pending) return p.finish(MemberStatus::ResourceError).status;
  p.phase=Impl::Phase::Locate;p.status=MemberStatus::Pending;p.output_status=OutputStatus::Pending;
  t->proof_=p.proof;return p.status;
}
MemberStep MemberEligibility::advance(const MemberTicket& t,const WorkBudget& b) {
  auto& p=*impl_;
  if (!t.proof_ || t.proof_!=p.proof) return {};
  if (!p.index.matches(p.shared) || !p.shared.activation_is_current(p.proof->request.shared)) return p.finish(MemberStatus::Stale);
  if (p.phase==Impl::Phase::Done) return {p.status,p.output_status,{}};
  if (!b.max_read_calls || !b.max_read_bytes || !b.max_work_items) return {MemberStatus::InvalidRequest,p.output_status,{}};
  using Phase=Impl::Phase;
  if (p.phase==Phase::Locate || p.phase==Phase::Member) {
    const auto step=p.phase==Phase::Locate ? p.index.locate(p.cursor,b) : p.index.find_member(p.cursor,p.proof->request.ref,b);
    if (step.status==BaseOrderStatus::Ready) {
      if (p.phase==Phase::Locate) p.phase=Phase::Member;
      else if (step.item.key!=p.proof->request.key) return p.finish(MemberStatus::IdentityMismatch,step.cost);
      else p.phase=Phase::OutputStart;
    } else if (step.status==BaseOrderStatus::NotFound) return p.finish(MemberStatus::NotMapped,step.cost);
    else if (step.status!=BaseOrderStatus::Pending) return p.finish(step.status==BaseOrderStatus::Stale?MemberStatus::Stale:MemberStatus::ResourceError,step.cost);
    return {MemberStatus::Pending,p.output_status,step.cost};
  }
  if (p.phase==Phase::OutputStart) {
    const auto& r=p.proof->request;
    p.output_status=p.output.begin(p.shared,r.shared,r.ref,r.key,r.output,p.names,p.f,p.text,p.l);
    if (p.output_status!=OutputStatus::Pending) return p.finish(MemberStatus::OutputRestricted,{0,0,1});
    p.phase=Phase::Output;return {MemberStatus::Pending,p.output_status,{0,0,1}};
  }
  const auto step=p.output.advance(b);p.output_status=step.status;
  if (step.status==OutputStatus::Pending) return {MemberStatus::Pending,step.status,step.cost};
  if (output_has_text(step.status)) return p.finish(MemberStatus::Eligible,step.cost);
  return p.finish(step.status==OutputStatus::Stale ? MemberStatus::Stale :
      step.status==OutputStatus::ReadError ? MemberStatus::ResourceError : MemberStatus::OutputRestricted,step.cost);
}
bool MemberEligibility::eligible(const MemberTicket& t,const MemberRequest& current) const {
  const auto& p=*impl_;
  if (!t.proof_ || t.proof_->owner!=p.identity || !t.proof_->eligible ||
      !p.index.matches(p.shared) || !p.shared.activation_is_current(t.proof_->request.shared) ||
      !p.shared.activation_is_current(current.shared)) return false;
  const auto& saved=t.proof_->request;
  return saved.input_revision==current.input_revision && saved.selection_revision==current.selection_revision &&
      saved.ref==current.ref && saved.key==current.key && saved.code==current.code &&
      saved.begin==current.begin && saved.end==current.end && saved.source_bytes==current.source_bytes &&
      saved.output.form==current.output.form && saved.output.invocation==current.output.invocation;
}
void MemberEligibility::cancel(const MemberTicket& t) {
  if (t.proof_ && t.proof_==impl_->proof) impl_->finish(MemberStatus::Cancelled);
}
std::size_t MemberEligibility::memory_bytes() const {
  return sizeof(*this)+sizeof(Impl)+impl_->cursor.memory_bytes()-sizeof(BaseOrderCursor)+sizeof(MemberProof);
}
std::size_t MemberEligibility::ticket_payload_bytes() { return sizeof(MemberProof); }
} // namespace hanime
