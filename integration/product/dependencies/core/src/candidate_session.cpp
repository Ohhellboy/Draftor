#include "hanime/candidate_session.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace hanime {
struct SessionInputIdentity {};
struct CandidateSnapshotIdentity {};
namespace {
void add(WorkCost& a,WorkCost b) {a.read_calls+=b.read_calls; a.read_bytes+=b.read_bytes; a.work_items+=b.work_items;}
bool valid_budget(const WorkBudget& b) {return b.max_read_calls && b.max_read_bytes && b.max_work_items;}
bool blocked(CommitStatus s) {
  return s==CommitStatus::Preparing || s==CommitStatus::CallingSink ||
      s==CommitStatus::RetryableNoEffect || s==CommitStatus::UnknownOutcome || s==CommitStatus::OutputRestricted;
}
bool valid_config(const SessionConfig& c) {
  if (!c.ranking_slots || !c.snapshot_slots || c.snapshot_slots>c.ranking_slots ||
      !c.raw_batch || !c.output_bytes || !c.form_bytes || !c.code_bytes) return false;
  std::size_t total=0;
  auto account=[&](std::size_t n,std::size_t bytes) {
    if (n>std::numeric_limits<std::size_t>::max()/bytes || n*bytes>std::numeric_limits<std::size_t>::max()-total) return false;
    total+=n*bytes; return true;
  };
  return c.snapshot_slots<=std::numeric_limits<std::size_t>::max()/2 &&
      account(c.ranking_slots,sizeof(RankItem)) && account(c.snapshot_slots,sizeof(RankItem)) &&
      account(2*c.snapshot_slots,sizeof(CandidateSlot)) && account(2*c.snapshot_slots,c.output_bytes) &&
      account(c.output_bytes,1) && account(c.form_bytes,2) && account(c.code_bytes,2) &&
      account(c.raw_batch,sizeof(RawQueryHit));
}
SessionStatus rank_status(RankStatus s) {
  switch (s) {
    case RankStatus::InProgress: return SessionStatus::Pending;
    case RankStatus::WaitingState: return SessionStatus::WaitingState;
    case RankStatus::Ready: return SessionStatus::StagingReady;
    case RankStatus::NoMatches: return SessionStatus::NoMatches;
    case RankStatus::RangeExhausted: return SessionStatus::RangeExhausted;
    case RankStatus::Stale: case RankStatus::StateUnavailable: return SessionStatus::NeedsRankRestart;
    case RankStatus::Busy: return SessionStatus::Busy;
    case RankStatus::CapacityExceeded: return SessionStatus::CapacityExceeded;
    case RankStatus::ResourceError: case RankStatus::GroupOrderError: return SessionStatus::ReadError;
    case RankStatus::InvalidBudget: return SessionStatus::InvalidBudget;
    default: return SessionStatus::InvalidRequest;
  }
}
}
struct CandidateSession::Impl {
  enum class Phase { Idle, Rank, Copy, FirstBoundary, LastBoundary, Handoff,
                     OutputStart, OutputRead, CommitStart, CommitRead, CommitCall };
  struct Bank {
    std::unique_ptr<CandidateSlot[]> slots;
    std::unique_ptr<char[]> text;
    SnapshotHandle token;
    std::shared_ptr<const SessionInputIdentity> input;
    InputHandle rank_input;
    SharedActivationHandle activation;
    RankBoundary first,last;
    std::size_t size{0};
    std::uint64_t config_revision{0};
    bool gate{true}, has_more{false};
    ScoringMode mode{ScoringMode::Exact};
  } banks[2];
  struct Request {
    RangeDirection direction{RangeDirection::First}; RankBoundary anchor;
    std::size_t q{0}, text_count{0}; bool saved{false};
  } request,resume;
  struct Transaction {
    CommitResult result;
    SelectionHandle selection;
    std::shared_ptr<const SessionInputIdentity> input;
    InputHandle rank_input;
    SharedActivationHandle activation;
    StableLexemeKey key;
    RuntimeLexemeRef ref{0};
    std::uint64_t config_revision{0};
    std::size_t text_bytes{0};
  } transaction;
  PagedSchemeIndex* scheme;
  PagedSharedResource& shared;
  CommitCountSource& counts;
  TextCommitSink& sink;
  SessionConfig config;
  std::unique_ptr<RankItem[]> work,result;
  std::unique_ptr<char[]> names,commit_text;
  std::unique_ptr<ExactRanker> rank;
  TextAccessCheck access;
  TextAccessCertificate certificate;
  OutputReader output;
  std::string form, pending_code;
  OutputInvocation invocation{OutputInvocation::Profile};
  std::uint64_t config_revision{1};
  std::shared_ptr<const SessionInputIdentity> input;
  MountEpoch pending_epoch{0};
  ScoringMode pending_mode{ScoringMode::Exact};
  std::size_t pending_q{0}, pending_text_count{0};
  bool active{false}, pending_start{false}, pending_cleanup{false}, access_running{false};
  unsigned callback_depth{0}, published_bank{0}, output_bank{1};
  std::size_t copied{0}, text_pos{0}, text_end{0};
  RankTicket ticket;
  Phase phase{Phase::Idle};
  SessionStatus status{SessionStatus::Idle};
  SessionCosts costs;

  Impl(PagedSchemeIndex& s,PagedSharedResource& r,CommitCountSource& c,TextCommitSink& t,SessionConfig cfg)
      :scheme(&s),shared(r),counts(c),sink(t),config(cfg) {
    work=std::make_unique<RankItem[]>(cfg.ranking_slots);
    result=std::make_unique<RankItem[]>(cfg.snapshot_slots);
    names=std::make_unique<char[]>(cfg.form_bytes);
    commit_text=std::make_unique<char[]>(cfg.output_bytes);
    for (auto& bank:banks) {
      bank.slots=std::make_unique<CandidateSlot[]>(cfg.snapshot_slots);
      bank.text=std::make_unique<char[]>(cfg.snapshot_slots*cfg.output_bytes);
    }
    form.reserve(cfg.form_bytes); pending_code.reserve(cfg.code_bytes);
    rank=std::make_unique<ExactRanker>(s,r,c,work.get(),cfg.ranking_slots,cfg.raw_batch,cfg.code_bytes);
  }
  unsigned staging_index() const {return 1-published_bank;}
  bool access_ready() const {return certificate.current(shared,config.output_bytes,config.form_bytes);}
  bool input_current() const {return active && !pending_start && access_ready() && rank->input_is_current(rank->input_handle());}
  bool bank_current(const Bank& b) const {
    return b.token.identity_ && input_current() && b.input==input && b.config_revision==config_revision &&
        shared.activation_is_current(b.activation) && rank->input_is_current(b.rank_input);
  }
  bool transaction_current() const {
    const auto& t=transaction;
    return input_current() && t.input==input && t.config_revision==config_revision &&
        shared.activation_is_current(t.activation) && rank->input_is_current(t.rank_input);
  }
  OutputRequest output_request() const {return {form,invocation};}
  bool in_rank() const {return phase==Phase::Rank || phase==Phase::Copy || phase==Phase::FirstBoundary ||
      phase==Phase::LastBoundary || phase==Phase::Handoff;}
  bool prefetching() const {
    return (phase==Phase::OutputStart || phase==Phase::OutputRead) && output_bank==staging_index();
  }
  void interrupt_range(bool save) {
    const bool was_ranking=in_rank();
    if (was_ranking) {
      if (save) {resume=request; resume.saved=true;}
      auto& bank=banks[staging_index()]; bank.token={}; bank.size=0;
    }
    // Idle/terminal is not proof that independent ports have released work.
    // This original ticket is a no-op if a newer range already replaced it.
    if (rank->abandon_range(ticket) && was_ranking) ++costs.abandoned_ranges;
    rank->release(ticket);
  }
  void stop_work(bool save) {
    interrupt_range(save);
    output.cancel(); phase=Phase::Idle;
  }
  SessionStatus start_range(Request r) {
    const auto rs=rank->request_range(r.direction,r.direction==RangeDirection::First ? nullptr : &r.anchor,
        r.q,result.get(),config.snapshot_slots,&ticket);
    status=rank_status(rs);
    if (rs==RankStatus::InProgress) {
      request=r; phase=Phase::Rank; copied=0;
      auto& b=banks[staging_index()]; b.token={}; b.size=0;
    }
    return status;
  }
  void start_output(unsigned bank,std::size_t start,std::size_t count) {
    output_bank=bank; text_pos=start; text_end=start+count;
    phase=count ? Phase::OutputStart : Phase::Idle;
    status=count ? SessionStatus::Pending : SessionStatus::StagingReady;
  }
  void fail_commit(CommitStatus s,OutputStatus out) {
    auto& t=transaction; t.result.status=s; t.result.output=out; t.result.retry={};
    if (s==CommitStatus::RetryableNoEffect) {
      t.result.retry.transaction_=t.result.transaction; t.result.retry.attempt_=t.result.attempt;
    }
    phase=Phase::Idle;
    status=s==CommitStatus::Stale ? SessionStatus::Stale : SessionStatus::Ready;
  }
};

CandidateSession::CandidateSession(PagedSchemeIndex& s,PagedSharedResource& r,CommitCountSource& c,
    TextCommitSink& sink,const SessionConfig& cfg) {
  if (!valid_config(cfg)) return;
  try {impl_=std::make_unique<Impl>(s,r,c,sink,cfg);} catch (const std::bad_alloc&) {}
}
CandidateSession::~CandidateSession() {if (impl_) {impl_->stop_work(false); impl_->access.cancel();}}
bool CandidateSession::valid() const {return bool(impl_);}
bool CandidateSession::text_access_ready() const {return impl_ && impl_->access_ready();}
TextAccessStatus CandidateSession::begin_text_access() {
  if (!impl_) return TextAccessStatus::InvalidRequest;
  auto& p=*impl_;
  if (p.active || p.callback_depth || blocked(p.transaction.result.status)) return TextAccessStatus::InvalidRequest;
  p.certificate={}; p.access_running=true;
  return p.access.begin(p.shared,p.config.output_bytes,p.config.form_bytes);
}
TextAccessStep CandidateSession::advance_text_access(const WorkBudget& b) {
  if (!impl_ || !impl_->access_running || impl_->callback_depth) {TextAccessStep r; r.status=TextAccessStatus::InvalidRequest; return r;}
  auto& p=*impl_; auto r=p.access.advance(b); add(p.costs.admission,r.cost);
  if (r.status==TextAccessStatus::Ready) {p.certificate=p.access.certificate(); p.access_running=false;}
  return r;
}
SessionStatus CandidateSession::set_output(OutputRequest r) {
  if (!impl_) return SessionStatus::InvalidRequest;
  auto& p=*impl_;
  if (p.active || p.callback_depth || blocked(p.transaction.result.status)) return SessionStatus::Busy;
  if (r.form.size()>p.config.form_bytes || (!r.form.empty() && !is_valid_utf8(r.form)) ||
      (r.invocation!=OutputInvocation::Profile && r.invocation!=OutputInvocation::Explicit && r.invocation!=OutputInvocation::Expanded) ||
      p.config_revision==std::numeric_limits<std::uint64_t>::max()) return SessionStatus::InvalidRequest;
  p.form.assign(r.form); p.invocation=r.invocation; ++p.config_revision; return SessionStatus::Ready;
}
SessionStatus CandidateSession::bind_scheme(PagedSchemeIndex& scheme) {
  if (!impl_) return SessionStatus::InvalidRequest;
  auto& p=*impl_;
  if (p.active || p.callback_depth || blocked(p.transaction.result.status)) return SessionStatus::Busy;
  if (!p.access_ready() || !scheme.ready() || scheme.info().registry_namespace!=p.shared.info().registry_namespace ||
      scheme.info().required_shared_bundle_id!=p.shared.info().shared_bundle_id) return SessionStatus::Stale;
  try {
    auto rank=std::make_unique<ExactRanker>(scheme,p.shared,p.counts,p.work.get(),p.config.ranking_slots,p.config.raw_batch,p.config.code_bytes);
    p.stop_work(false); p.rank=std::move(rank); p.scheme=&scheme;
  } catch (const std::bad_alloc&) {return SessionStatus::AllocationFailure;}
  return SessionStatus::Ready;
}
SessionStatus CandidateSession::begin_input(std::string_view code,MountEpoch epoch,std::size_t q,
    std::size_t text_count,ScoringMode mode) {
  if (!impl_) return SessionStatus::InvalidRequest;
  auto& p=*impl_;
  if (!q || q>p.config.snapshot_slots || text_count>q) return SessionStatus::CapacityExceeded;
  if (code.empty() || code.size()>p.config.code_bytes || !is_valid_utf8(code) ||
      (mode!=ScoringMode::Exact && mode!=ScoringMode::BaseOnly)) return SessionStatus::InvalidRequest;
  if (!p.access_ready() || epoch!=p.certificate.activation().mount_epoch()) return SessionStatus::Stale;
  try {p.input=std::make_shared<SessionInputIdentity>();} catch (const std::bad_alloc&) {return SessionStatus::AllocationFailure;}
  p.pending_code.assign(code); p.pending_epoch=epoch; p.pending_mode=mode;
  p.pending_q=q; p.pending_text_count=text_count; p.active=true; p.pending_start=true;
  // Logical invalidation is immediate; physical buffer reuse waits until tick,
  // after any presenter/sink callback has returned.
  if (blocked(p.transaction.result.status) && p.transaction.result.status!=CommitStatus::CallingSink)
    p.transaction.result.status=CommitStatus::Stale;
  return p.status=SessionStatus::Pending;
}
SessionInputHandle CandidateSession::input_handle() const {
  SessionInputHandle h; if (impl_) h.identity_=impl_->input; return h;
}
void CandidateSession::cancel_input(const SessionInputHandle& h) {
  if (!impl_ || !h.identity_ || h.identity_!=impl_->input) return;
  auto& p=*impl_; p.active=false; p.pending_start=false; p.pending_cleanup=true;
  if (blocked(p.transaction.result.status) && p.transaction.result.status!=CommitStatus::CallingSink)
    p.transaction.result.status=CommitStatus::Stale;
}
SessionStatus CandidateSession::request_range(RangeDirection d,std::size_t q,std::size_t texts) {
  if (!impl_) return SessionStatus::InvalidRequest;
  auto& p=*impl_;
  if (!q || q>p.config.snapshot_slots || texts>q) return SessionStatus::CapacityExceeded;
  if (d!=RangeDirection::First && d!=RangeDirection::After && d!=RangeDirection::Before) return SessionStatus::InvalidRequest;
  if (p.callback_depth || blocked(p.transaction.result.status) ||
      (p.phase!=Impl::Phase::Idle && !p.prefetching())) return SessionStatus::Busy;
  if (!p.input_current()) return SessionStatus::Stale;
  Impl::Request r; r.direction=d; r.q=q; r.text_count=texts;
  if (d!=RangeDirection::First) {
    const auto& bank=p.banks[p.published_bank];
    if (!p.bank_current(bank)) return SessionStatus::Stale;
    r.anchor=d==RangeDirection::After ? bank.last : bank.first;
    if (!p.rank->boundary_is_current(r.anchor)) return SessionStatus::NeedsRankRestart;
  }
  if (p.prefetching()) p.stop_work(false);
  return p.start_range(r);
}
SessionStatus CandidateSession::restart_first(std::size_t q,std::size_t texts,ScoringMode mode) {
  if (!impl_) return SessionStatus::InvalidRequest;
  auto& p=*impl_;
  if (!q || q>p.config.snapshot_slots || texts>q) return SessionStatus::CapacityExceeded;
  if (p.callback_depth || blocked(p.transaction.result.status)) return SessionStatus::Busy;
  if (!p.input_current()) return SessionStatus::Stale;
  p.stop_work(false); p.resume={};
  const auto rs=p.rank->restart_rank(mode);
  if (rs!=RankStatus::InProgress) return p.status=rank_status(rs);
  return p.start_range({RangeDirection::First,{},q,texts,false});
}
SessionStatus CandidateSession::resume_range() {
  if (!impl_) return SessionStatus::InvalidRequest;
  auto& p=*impl_;
  if (p.callback_depth || blocked(p.transaction.result.status) || p.phase!=Impl::Phase::Idle) return SessionStatus::Busy;
  if (!p.input_current() || !p.resume.saved) return SessionStatus::Stale;
  const auto status=p.start_range(p.resume);
  if (status==SessionStatus::Pending) {p.resume={}; ++p.costs.resumed_ranges;}
  return status;
}
SnapshotHandle CandidateSession::staging() const {
  if (!impl_) return {};
  const auto& p=*impl_; const auto& bank=p.banks[p.staging_index()];
  return p.bank_current(bank) ? bank.token : SnapshotHandle{};
}
SnapshotHandle CandidateSession::published() const {
  if (!impl_) return {};
  const auto& p=*impl_; const auto& bank=p.banks[p.published_bank];
  return p.bank_current(bank) ? bank.token : SnapshotHandle{};
}
SessionStatus CandidateSession::request_text(const SnapshotHandle& h,std::size_t start,std::size_t count) {
  if (!impl_ || !h.identity_) return SessionStatus::Stale;
  auto& p=*impl_;
  if (p.callback_depth || blocked(p.transaction.result.status)) return SessionStatus::Busy;
  unsigned which=2;
  for (unsigned i=0;i<2;++i) if (p.banks[i].token.identity_==h.identity_ && p.bank_current(p.banks[i])) which=i;
  if (which==2) return SessionStatus::Stale;
  if (!count || start>p.banks[which].size || count>p.banks[which].size-start) return SessionStatus::InvalidRequest;
  // Staging text is disposable prefetch; it cannot displace visible text or
  // an explicit rank request. Published text may interrupt either safely.
  if (which==p.staging_index() && p.phase!=Impl::Phase::Idle && !p.prefetching()) return SessionStatus::Busy;
  p.stop_work(true); p.start_output(which,start,count); return p.status;
}
SessionStatus CandidateSession::publish(const SnapshotHandle& h) {
  if (!impl_ || !h.identity_) return SessionStatus::Stale;
  auto& p=*impl_; auto& bank=p.banks[p.staging_index()];
  if (p.callback_depth || blocked(p.transaction.result.status) || p.phase!=Impl::Phase::Idle) return SessionStatus::Busy;
  if (bank.token.identity_!=h.identity_ || !p.bank_current(bank)) return SessionStatus::Stale;
  p.published_bank=p.staging_index();
  auto& old=p.banks[p.staging_index()]; old.token={}; old.size=0;
  return p.status=SessionStatus::Ready;
}
SessionStatus CandidateSession::present(const std::function<void(const CandidateViewPage&)>& callback) {
  if (!impl_ || !callback) return SessionStatus::InvalidRequest;
  auto& p=*impl_; const auto& bank=p.banks[p.published_bank];
  if (p.callback_depth) return SessionStatus::Busy;
  if (!p.bank_current(bank)) return SessionStatus::Stale;
  CandidateViewPage view{bank.token,bank.slots.get(),bank.size,bank.text.get(),p.config.output_bytes,
      !p.rank->boundary_is_current(bank.first),bank.has_more,bank.mode};
  ++p.callback_depth; ++p.costs.present_calls;
  try {callback(view);} catch (...) {--p.callback_depth; throw;}
  --p.callback_depth; return SessionStatus::Ready;
}
SelectionHandle CandidateSession::selection(std::size_t slot) const {return {published(),slot};}
CommitResult CandidateSession::confirm(const SelectionHandle& selection) {
  CommitResult rejected; rejected.status=CommitStatus::Stale;
  if (!impl_ || !selection.snapshot.identity_) return rejected;
  auto& p=*impl_; auto& t=p.transaction;
  if (t.selection.snapshot.identity_==selection.snapshot.identity_ && t.selection.slot==selection.slot)
    return t.result; // The original event is never a retry, even after failure.
  if (p.callback_depth || blocked(t.result.status)) {rejected.status=CommitStatus::Busy; return rejected;}
  auto& bank=p.banks[p.published_bank];
  if (!p.bank_current(bank) || bank.token.identity_!=selection.snapshot.identity_ || selection.slot>=bank.size || !bank.gate) return rejected;
  CommitHandle handle;
  try {handle.identity_=std::make_shared<CommitIdentity>();} catch (const std::bad_alloc&) {rejected.status=CommitStatus::Busy; return rejected;}
  p.stop_work(true); bank.gate=false;
  t={}; t.selection=selection; t.input=bank.input; t.rank_input=bank.rank_input; t.activation=bank.activation;
  t.key=bank.slots[selection.slot].ranked.key; t.ref=bank.slots[selection.slot].ranked.ref;
  t.config_revision=bank.config_revision; t.result.transaction=handle;
  t.result.status=CommitStatus::Preparing; t.result.attempt=1;
  p.phase=Impl::Phase::CommitStart; p.status=SessionStatus::Pending;
  return t.result;
}
CommitResult CandidateSession::retry(const RetryPermit& permit) {
  CommitResult rejected; rejected.status=CommitStatus::Stale;
  if (!impl_) return rejected;
  auto& p=*impl_; auto& t=p.transaction;
  if (p.callback_depth) {rejected.status=CommitStatus::Busy; return rejected;}
  if (!permit.transaction_.identity_ || permit.transaction_.identity_!=t.result.transaction.identity_ ||
      permit.attempt_!=t.result.attempt || t.result.status!=CommitStatus::RetryableNoEffect) return rejected;
  if (!p.transaction_current()) {p.fail_commit(CommitStatus::Stale,OutputStatus::Stale); return t.result;}
  if (t.result.attempt==std::numeric_limits<std::uint64_t>::max()) {
    p.fail_commit(CommitStatus::Cancelled,OutputStatus::Cancelled); return t.result;
  }
  ++t.result.attempt; t.result.retry={}; t.result.status=CommitStatus::Preparing;
  t.result.output=OutputStatus::Pending; t.text_bytes=0;
  p.phase=Impl::Phase::CommitStart; p.status=SessionStatus::Pending; return t.result;
}
CommitResult CandidateSession::cancel_commit(const CommitHandle& handle) {
  CommitResult rejected; rejected.status=CommitStatus::Stale;
  if (!impl_ || !handle.identity_) return rejected;
  auto& p=*impl_; auto& t=p.transaction;
  if (handle.identity_!=t.result.transaction.identity_) return rejected;
  if (p.callback_depth || t.result.status==CommitStatus::CallingSink) {rejected.status=CommitStatus::Busy; return rejected;}
  if (t.result.status==CommitStatus::Accepted || t.result.status==CommitStatus::Stale ||
      t.result.status==CommitStatus::Cancelled || t.result.status==CommitStatus::None) return t.result;
  if (t.result.status==CommitStatus::UnknownOutcome) return t.result; // Cannot re-arm possibly accepted text.
  if (!p.transaction_current()) {t.result.status=CommitStatus::Stale; t.result.retry={}; return t.result;}
  p.output.cancel(); p.fail_commit(CommitStatus::Cancelled,OutputStatus::Cancelled);
  auto& bank=p.banks[p.published_bank];
  if (p.bank_current(bank) && bank.token.identity_==t.selection.snapshot.identity_) {
    try {bank.token.identity_=std::make_shared<CandidateSnapshotIdentity>(); bank.gate=true;}
    catch (const std::bad_alloc&) {bank.token={};}
  }
  return t.result;
}
CommitResult CandidateSession::commit_result() const {return impl_ ? impl_->transaction.result : CommitResult{};}

SessionStep CandidateSession::tick(const WorkBudget& budget) {
  if (!impl_) return {SessionStatus::InvalidRequest,{}};
  auto& p=*impl_; using Phase=Impl::Phase;
  if (!valid_budget(budget)) return {SessionStatus::InvalidBudget,{}};
  if (p.callback_depth) return {SessionStatus::Busy,{}};
  const WorkCost local{0,0,1};
  if (p.pending_cleanup || p.pending_start) {
    p.stop_work(false); p.resume={}; p.pending_cleanup=false;
    if (!p.pending_start) {p.status=SessionStatus::Cancelled; return {p.status,local};}
    p.pending_start=false;
    if (!p.access_ready()) {p.active=false; return {p.status=SessionStatus::Stale,local};}
    try {
      const auto s=p.rank->begin_input(p.pending_code,p.pending_epoch,{},p.pending_mode);
      if (s!=RankStatus::InProgress) {p.active=false; return {p.status=rank_status(s),local};}
      p.start_range({RangeDirection::First,{},p.pending_q,p.pending_text_count,false});
    } catch (const std::bad_alloc&) {p.active=false; p.status=SessionStatus::AllocationFailure;}
    add(p.costs.local,local); return {p.status,local};
  }
  if (p.phase==Phase::Idle) return {p.status,{}};
  if (!p.input_current()) {
    p.stop_work(false);
    if (p.transaction.result.status==CommitStatus::Preparing) p.fail_commit(CommitStatus::Stale,OutputStatus::Stale);
    return {p.status=SessionStatus::Stale,{}};
  }
  if (p.phase==Phase::Rank) {
    const auto s=p.rank->advance(p.ticket,budget); add(p.costs.ranking,s.cost); p.status=rank_status(s.status);
    if (s.status==RankStatus::Ready) {p.phase=Phase::Copy; p.status=SessionStatus::Pending;}
    else if (s.status!=RankStatus::InProgress && s.status!=RankStatus::WaitingState) p.phase=Phase::Idle;
    return {p.status,s.cost};
  }
  if (p.phase==Phase::Copy || p.phase==Phase::FirstBoundary || p.phase==Phase::LastBoundary || p.phase==Phase::Handoff) {
    auto& bank=p.banks[p.staging_index()];
    const auto view=p.rank->result(p.ticket);
    if (!view.items || !view.size) {
      p.rank->release(p.ticket); bank.token={}; bank.size=0; p.phase=Phase::Idle;
      return {p.status=SessionStatus::NeedsRankRestart,{}};
    }
    if (p.phase==Phase::Copy) {
      bank.slots[p.copied]={}; bank.slots[p.copied].ranked=view.items[p.copied];
      ++p.copied; ++p.costs.copied_items;
      if (p.copied==view.size) p.phase=Phase::FirstBoundary;
    } else if (p.phase==Phase::FirstBoundary) {bank.first=p.rank->boundary(p.ticket,0); p.phase=Phase::LastBoundary;}
    else if (p.phase==Phase::LastBoundary) {bank.last=p.rank->boundary(p.ticket,view.size-1); p.phase=Phase::Handoff;}
    else {
      try {bank.token.identity_=std::make_shared<CandidateSnapshotIdentity>();}
      catch (const std::bad_alloc&) {p.rank->release(p.ticket); p.phase=Phase::Idle; return {p.status=SessionStatus::AllocationFailure,local};}
      bank.input=p.input; bank.rank_input=p.rank->input_handle(); bank.activation=p.certificate.activation();
      bank.config_revision=p.config_revision; bank.size=view.size; bank.gate=true;
      bank.mode=view.mode; bank.has_more=view.has_more;
      p.rank->release(p.ticket);
      p.start_output(p.staging_index(),0,std::min(p.request.text_count,bank.size));
    }
    add(p.costs.local,local); return {p.status,local};
  }
  if (p.phase==Phase::OutputStart) {
    auto& bank=p.banks[p.output_bank];
    if (!p.bank_current(bank)) {p.phase=Phase::Idle; return {p.status=SessionStatus::Stale,{}};}
    if (p.text_pos==p.text_end) {p.phase=Phase::Idle; p.status=SessionStatus::StagingReady;}
    else {
      auto& slot=bank.slots[p.text_pos];
      if (output_has_text(slot.output) || (slot.output!=OutputStatus::Pending && slot.output!=OutputStatus::ReadError)) ++p.text_pos;
      else {
        slot.output=OutputStatus::Pending; slot.text_bytes=0;
        const auto s=p.output.begin(p.shared,bank.activation,slot.ranked.ref,slot.ranked.key,p.output_request(),
            p.names.get(),p.config.form_bytes,bank.text.get()+p.text_pos*p.config.output_bytes,p.config.output_bytes);
        if (s==OutputStatus::Pending) p.phase=Phase::OutputRead;
        else {slot.output=s; p.phase=Phase::Idle; p.status=SessionStatus::ReadError;}
      }
    }
    add(p.costs.local,local); return {p.status,local};
  }
  if (p.phase==Phase::OutputRead) {
    const auto s=p.output.advance(budget); add(p.costs.display,s.cost);
    auto& bank=p.banks[p.output_bank];
    if (!p.bank_current(bank)) {p.phase=Phase::Idle; return {p.status=SessionStatus::Stale,s.cost};}
    if (s.status!=OutputStatus::Pending) {
      auto& slot=bank.slots[p.text_pos]; slot.output=s.status; slot.payload=s.payload;
      slot.text_bytes=output_has_text(s.status) ? s.text.size() : 0;
      if (s.status==OutputStatus::ReadError || s.status==OutputStatus::Stale || s.status==OutputStatus::TooLarge) {
        p.phase=Phase::Idle; p.status=s.status==OutputStatus::Stale ? SessionStatus::Stale : SessionStatus::ReadError;
      } else {++p.text_pos; p.phase=Phase::OutputStart;}
    }
    return {p.status,s.cost};
  }
  auto& t=p.transaction;
  if (!p.transaction_current()) {p.output.cancel(); p.fail_commit(CommitStatus::Stale,OutputStatus::Stale); return {p.status,{}};}
  if (p.phase==Phase::CommitStart) {
    const auto s=p.output.begin(p.shared,t.activation,t.ref,t.key,p.output_request(),p.names.get(),p.config.form_bytes,
                               p.commit_text.get(),p.config.output_bytes);
    if (s==OutputStatus::Pending) p.phase=Phase::CommitRead;
    else p.fail_commit(s==OutputStatus::Stale ? CommitStatus::Stale : CommitStatus::RetryableNoEffect,s);
    add(p.costs.local,local); return {p.status,local};
  }
  if (p.phase==Phase::CommitRead) {
    const auto s=p.output.advance(budget); add(p.costs.commit,s.cost);
    if (!p.transaction_current()) p.fail_commit(CommitStatus::Stale,OutputStatus::Stale);
    else if (output_has_text(s.status)) {t.text_bytes=s.text.size(); t.result.output=s.status; p.phase=Phase::CommitCall;}
    else if (s.status!=OutputStatus::Pending) {
      const auto state=s.status==OutputStatus::ReadError ? CommitStatus::RetryableNoEffect :
          (s.status==OutputStatus::Stale ? CommitStatus::Stale : CommitStatus::OutputRestricted);
      p.fail_commit(state,s.status);
    }
    return {p.status,s.cost};
  }
  // All selection/resource/output checks precede this irreversible call.
  // No re-entrant tick, and begin_input only registers a new logical input.
  t.result.status=CommitStatus::CallingSink; ++p.callback_depth; ++p.costs.sink_calls;
  bool accepted=false;
  try {accepted=p.sink.commit({p.commit_text.get(),t.text_bytes});}
  catch (...) {--p.callback_depth; p.fail_commit(CommitStatus::UnknownOutcome,t.result.output); return {p.status,local};}
  --p.callback_depth;
  if (accepted) {
    t.result.status=CommitStatus::Accepted; t.result.retry={};
    t.result.transaction.identity_->accepted=true;
    t.result.learning=p.counts.notify_committed(t.result.transaction,t.activation.registry_namespace(),t.key);
    if (t.input==p.input) {p.active=false; p.pending_start=false;}
    p.phase=Phase::Idle; p.status=p.pending_start ? SessionStatus::Pending : SessionStatus::Ready;
  } else if (!p.transaction_current()) p.fail_commit(CommitStatus::Stale,t.result.output);
  else p.fail_commit(CommitStatus::RetryableNoEffect,t.result.output);
  add(p.costs.local,local); return {p.status,local};
}
const SessionCosts& CandidateSession::costs() const {static const SessionCosts empty; return impl_ ? impl_->costs : empty;}
OutputCosts CandidateSession::output_costs() const {return impl_ ? impl_->output.costs() : OutputCosts{};}
SessionMemoryUsage CandidateSession::memory_usage() const {
  SessionMemoryUsage m; if (!impl_) return m;
  const auto& p=*impl_; m.owner_bytes=sizeof(*this); m.impl_bytes=sizeof(Impl);
  m.rank_work_bytes=p.config.ranking_slots*sizeof(RankItem); m.rank_result_bytes=p.config.snapshot_slots*sizeof(RankItem);
  m.snapshot_bytes=2*p.config.snapshot_slots*sizeof(CandidateSlot); m.text_pool_bytes=2*p.config.snapshot_slots*p.config.output_bytes;
  m.commit_pool_bytes=p.config.output_bytes; m.name_bytes=p.config.form_bytes;
  m.form_capacity=p.form.capacity()+1; m.code_capacity=p.pending_code.capacity()+1;
  m.output_reader_bytes=sizeof(OutputReader); m.access_check_bytes=sizeof(TextAccessCheck);
  m.text_cursor_bytes=sizeof(SharedTextCursor); m.transaction_bytes=sizeof(Impl::Transaction);
  m.snapshot_common_bytes=2*sizeof(Impl::Bank); m.input_token_bytes=sizeof(SessionInputIdentity);
  m.snapshot_token_bytes=sizeof(CandidateSnapshotIdentity); m.commit_token_bytes=sizeof(CommitIdentity);
  m.ranking=p.rank->memory_usage(); return m;
}
} // namespace hanime
