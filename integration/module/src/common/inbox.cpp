#include "draftor_usb_transfer/inbox.hpp"
#include <cstring>
#include <exception>
#include <limits>

namespace draftor::usb {
bool isTxtCandidate(const InboxCandidate& c) noexcept {
  if (c.directory || c.hidden || c.name[0] == '.' || !c.name[0]) return false;
  const auto* end = static_cast<const char*>(std::memchr(c.name.data(), 0, c.name.size()));
  if (!end) return false;
  const std::string_view n(c.name.data(), end - c.name.data());
  if (n.find_first_of("/\\:") != n.npos || n.size() < 5) return false;
  const auto ext = n.substr(n.size() - 4);
  return ext[0] == '.' && (ext[1] == 't' || ext[1] == 'T') &&
    (ext[2] == 'x' || ext[2] == 'X') && (ext[3] == 't' || ext[3] == 'T');
}
InboxSession::~InboxSession() {
  if (phase_ != InboxPhase::Idle && phase_ != InboxPhase::Closed) std::terminate();
}
Status InboxSession::begin() noexcept {
  if (phase_ != InboxPhase::Idle && phase_ != InboxPhase::Closed) return Status::Busy;
  if (session_ == std::numeric_limits<std::uint64_t>::max()) return Status::Exhausted;
  exchange::Limits limits;
  if (!quota_.checked(limits)) return Status::InvalidArgument;
  ++session_; result_ = {}; phase_ = InboxPhase::Preparing; return Status::Ok;
}
void InboxSession::fail(Status s) noexcept {
  result_.status = s;
  if (mounted_) fs_.unmount();
  mounted_ = false; backing_.release(); phase_ = InboxPhase::Closed;
}
void InboxSession::endHost(EndReason why) noexcept {
  if (phase_ != InboxPhase::HostMounted && phase_ != InboxPhase::HostWriting &&
      phase_ != InboxPhase::Sealing) return;
  media_.withdraw(why); result_.end = why; phase_ = InboxPhase::Sealing;
}
InboxPhase InboxSession::tick() noexcept {
  switch (phase_) {
    case InboxPhase::Preparing: {
      auto s = backing_.prepare(session_);
      if (s == Status::Ok) s = media_.publish(backing_.device(), Access::ReadWrite, generation_);
      if (s != Status::Ok) fail(s);
      else { published_ = true; phase_ = InboxPhase::HostMounted; }
      break;
    }
    case InboxPhase::HostMounted:
    case InboxPhase::HostWriting: {
      const auto m = media_.state();
      if (m.wrote) phase_ = InboxPhase::HostWriting;
      if (!m.accepting || m.generation != generation_)
        endHost(m.end == EndReason::None ? EndReason::Forced : m.end);
      break;
    }
    case InboxPhase::Sealing: {
      const auto m = media_.state();
      if (!m.drained || media_.detach() != Status::Ok) break;
      published_ = false;
      if (m.error != Status::Ok) { fail(m.error); break; }
      auto s = backing_.device().sync();
      if (s == Status::Ok) s = backing_.seal(session_);
      if (s != Status::Ok) fail(s);
      else phase_ = InboxPhase::Sealed;
      break;
    }
    case InboxPhase::Sealed: {
      const auto s = fs_.mountReadOnly(session_);
      if (s != Status::Ok) fail(s);
      else { mounted_ = true; phase_ = InboxPhase::Inspecting; }
      break;
    }
    default: break;
  }
  return phase_;
}
Status InboxSession::next(InboxCandidate& out) noexcept {
  if (phase_ != InboxPhase::Inspecting && phase_ != InboxPhase::ImportReady) return Status::NotReady;
  if (!backing_.stable(session_)) return Status::Stale;
  InboxCandidate candidate;
  const auto s = fs_.next(candidate);
  if (s == Status::NotFound) { phase_ = InboxPhase::ImportReady; return s; }
  if (s != Status::Ok) { result_.status = s; return s; }
  if (!isTxtCandidate(candidate)) { ++result_.ignored; return Status::Busy; }
  if (candidate.source.session != session_) return Status::Stale;
  ++result_.candidates; out = candidate; return Status::Ok;
}
Status InboxSession::acquire(const InboxCandidate& c, ImportSource*& out) noexcept {
  if (sourceActive_) return Status::Busy;
  if (phase_ != InboxPhase::Inspecting && phase_ != InboxPhase::ImportReady) return Status::NotReady;
  if (c.source.session != session_ || !backing_.stable(session_)) return Status::Stale;
  if (!isTxtCandidate(c)) return Status::InvalidArgument;
  exchange::Limits limits;
  if (!quota_.checked(limits)) return Status::InvalidArgument;
  if (c.source.size > limits.wire) return Status::TooLarge;
  if (sourceLease_ == std::numeric_limits<std::uint64_t>::max()) return Status::Exhausted;
  auto s = fs_.open(c);
  if (s != Status::Ok) return s;
  ++sourceLease_; selected_ = c.source; sourceActive_ = true; out = this; return Status::Ok;
}
bool InboxSession::stable() const noexcept {
  return sourceActive_ && mounted_ && backing_.stable(session_);
}
IoResult InboxSession::read(std::uint64_t at, void* p, std::size_t n) noexcept {
  if (!stable()) return {Status::Stale, 0};
  if (at > selected_.size || n > selected_.size - at) return {Status::Bounds, 0};
  auto r = fs_.read(at, p, n);
  if (!stable()) return {Status::Stale, 0};
  return r;
}
Status InboxSession::release(std::uint64_t token) noexcept {
  if (!sourceActive_ || token != sourceLease_) return Status::Stale;
  fs_.closeFile(); sourceActive_ = false; selected_ = {}; return Status::Ok;
}
Status InboxSession::close() noexcept {
  if (sourceActive_) return Status::Busy;
  if (published_) { endHost(EndReason::Forced); return Status::Busy; }
  if (mounted_) fs_.unmount();
  mounted_ = false; backing_.release(); phase_ = InboxPhase::Closed; return Status::Ok;
}
} // namespace draftor::usb
