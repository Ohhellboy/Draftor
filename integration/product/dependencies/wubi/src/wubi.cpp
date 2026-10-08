#include "hanime_wubi/wubi.hpp"
#include <limits>

namespace hanime::wubi {
EditStatus Wubi86Decoder::append(char c) {
  if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  if (profile().alphabet.find(c) == std::string_view::npos) return EditStatus::InvalidCode;
  if (size_ == code_.size()) return EditStatus::Full;
  if (revision_ == std::numeric_limits<std::uint64_t>::max()) return EditStatus::Exhausted;
  code_[size_++] = c;
  ++revision_;
  return EditStatus::Changed;
}
EditStatus Wubi86Decoder::backspace() {
  if (!size_) return EditStatus::Unchanged;
  if (revision_ == std::numeric_limits<std::uint64_t>::max()) return EditStatus::Exhausted;
  code_[--size_] = 0;
  ++revision_;
  return EditStatus::Changed;
}
EditStatus Wubi86Decoder::clear() {
  if (!size_) return EditStatus::Unchanged;
  if (revision_ == std::numeric_limits<std::uint64_t>::max()) return EditStatus::Exhausted;
  code_.fill(0);
  size_ = 0;
  ++revision_;
  return EditStatus::Changed;
}
DirectQueryPlan Wubi86Decoder::plan() const {
  return {{code_.data(), size_}, revision_, size_ == profile().normal_code_limit};
}

DirectQueryBinding::DirectQueryBinding(Decoder& d, CandidateSession& s, PagedSchemeIndex& i,
    PagedSharedResource& r, std::size_t page) : decoder_(d), session_(s), index_(i), shared_(r), page_size_(page) {}
DirectQueryBinding::~DirectQueryBinding() { session_.cancel_input(input_); }
bool DirectQueryBinding::current() const {
  return active_ && index_.activation_is_current(index_activation_) &&
      shared_.activation_is_current(shared_activation_);
}
bool DirectQueryBinding::commit_busy() const {
  const auto s = session_.commit_result().status;
  return s == CommitStatus::Preparing || s == CommitStatus::CallingSink ||
      s == CommitStatus::UnknownOutcome;
}
SessionStatus DirectQueryBinding::activate() {
  if (active_ || !decoder_.plan().code.empty()) return SessionStatus::Busy;
  if (!page_size_ || !index_.ready() || index_.info().scheme_id != decoder_.profile().resource_scheme)
    return SessionStatus::InvalidRequest;
  status_ = session_.bind_scheme(index_);
  if (status_ == SessionStatus::Ready) {
    index_activation_ = index_.activation_context();
    shared_activation_ = shared_.activation_handle();
    active_ = true;
  }
  return status_;
}
void DirectQueryBinding::begin_plan() {
  // Invalidation precedes any possible begin_input failure; no old selection
  // can be applied to a new code when admission/allocation fails.
  session_.cancel_input(input_);
  awaiting_commit_ = false;
  reason_ = CommitReason::None;
  const auto p = decoder_.plan();
  query_revision_ = p.revision;
  first_page_ = !p.code.empty();
  if (p.code.empty()) { status_ = SessionStatus::Cancelled; return; }
  status_ = session_.begin_input(p.code, shared_activation_.mount_epoch(), page_size_, page_size_);
  if (status_ == SessionStatus::Pending) input_ = session_.input_handle();
}
EditStatus DirectQueryBinding::append(char c) {
  if (!active_) return EditStatus::Closed;
  if (!current()) return EditStatus::Stale;
  if (commit_busy()) return EditStatus::Busy;
  const auto s = decoder_.append(c);
  if (s == EditStatus::Changed) begin_plan();
  return s;
}
EditStatus DirectQueryBinding::backspace() {
  if (!active_) return EditStatus::Closed;
  if (!current()) return EditStatus::Stale;
  if (commit_busy()) return EditStatus::Busy;
  const auto s = decoder_.backspace();
  if (s == EditStatus::Changed) begin_plan();
  return s;
}
EditStatus DirectQueryBinding::cancel() {
  const auto t = session_.commit_result().status;
  if (t == CommitStatus::CallingSink || t == CommitStatus::UnknownOutcome) return EditStatus::Busy;
  const auto s = decoder_.clear();
  if (s == EditStatus::Exhausted) return s;
  session_.cancel_input(input_);
  awaiting_commit_ = first_page_ = false;
  reason_ = CommitReason::None;
  status_ = SessionStatus::Cancelled;
  return s;
}
EditStatus DirectQueryBinding::leave() {
  const auto s = cancel();
  if (s != EditStatus::Busy && s != EditStatus::Exhausted) active_ = false;
  return s;
}
Step DirectQueryBinding::tick(const WorkBudget& b) {
  Step out;
  out.code_revision = decoder_.plan().revision;
  if (!active_) { out.shared.status = status_; return out; }
  if (active_ && !current()) {
    session_.cancel_input(input_);
    awaiting_commit_ = first_page_ = false;
    out.shared.status = status_ = SessionStatus::Stale;
    return out;
  }
  out.shared = session_.tick(b);
  status_ = out.shared.status;
  if (awaiting_commit_ && session_.commit_result().status == CommitStatus::Accepted) {
    if (decoder_.plan().revision == commit_revision_) decoder_.clear();
    awaiting_commit_ = first_page_ = false;
  }
  if (status_ != SessionStatus::StagingReady) return out;
  status_ = session_.publish(session_.staging());
  out.shared.status = status_;
  if (status_ != SessionStatus::Ready || !first_page_) return out;
  first_page_ = false;
  const auto plan = decoder_.plan();
  if (plan.revision != query_revision_ || !plan.full_code || !decoder_.profile().auto_unique_full_code) return out;
  // One visible slot alone is not a uniqueness proof: has_more must be false.
  // This reads already-materialised evidence, issuing no extra resource query.
  session_.present([&](const CandidateViewPage& page) {
    if (page.size == 1 && !page.has_more && !page.order_stale && output_has_text(page.items[0].output)) {
      out.request = CommitReason::AutoUniqueFullCode;
      out.selection = session_.selection(0);
    }
  });
  return out;
}
CommitResult DirectQueryBinding::confirm(const SelectionHandle& h, CommitReason reason) {
  if (!current()) { CommitResult r; r.status = CommitStatus::Stale; return r; }
  auto r = session_.confirm(h);
  if (r.status == CommitStatus::Preparing) {
    awaiting_commit_ = true;
    commit_revision_ = decoder_.plan().revision;
    reason_ = reason;
  }
  return r;
}
CommitResult DirectQueryBinding::retry(const RetryPermit& permit) {
  if (!current()) { CommitResult r; r.status = CommitStatus::Stale; return r; }
  auto r = session_.retry(permit);
  if (r.status == CommitStatus::Preparing) {
    awaiting_commit_ = true;
    commit_revision_ = decoder_.plan().revision;
  }
  return r;
}
} // namespace hanime::wubi
