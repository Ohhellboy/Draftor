#include "draftor_exchange/export_session.hpp"
#include <algorithm>
#include <exception>
#include <limits>

namespace draftor::exchange {
bool Identity::operator==(const Identity& b) const noexcept {
  return collection == b.collection && document == b.document &&
      mount == b.mount && document_session == b.document_session &&
      revision_epoch == b.revision_epoch && body_revision == b.body_revision;
}
ExportSession::ExportSession(OwnerPort& owner, Allocator& allocator, SessionConfig cfg) noexcept
    : owner_(owner), allocator_(allocator), config_(cfg), serializer_(cfg.body_limit),
      session_counter_(cfg.session_seed), reader_counter_(cfg.reader_seed) {}
ExportSession::~ExportSession() {
  if (reader_active_) std::terminate();
  stop();
}
SessionError ExportSession::checkIdentity(const OwnerState& s) const noexcept {
  if (!s.revision_reliable) return SessionError::RevisionUncertain;
  if (!s.has_current || s.identity != identity_ || s.body_bytes != stats_.source_bytes)
    return SessionError::IdentityChanged;
  if (s.composition || s.semantic_pending || s.file_operation || s.pending_body_edits)
    return SessionError::PortContract;
  return SessionError::None;
}
SessionError ExportSession::begin() noexcept {
  if (phase_ != Phase::Idle && phase_ != Phase::Closed) return SessionError::Busy;
  Limits limits;
  if (!Limits::checked(config_.body_limit, limits) ||
      !config_.work_bytes || config_.work_bytes > 1024) return SessionError::InvalidConfig;
  const auto state = owner_.probe();
  if (!state.has_current) return SessionError::NoCurrent;
  if (state.composition || state.semantic_pending) return SessionError::Composition;
  if (state.file_operation) return SessionError::FileOperation;
  if (state.pending_body_edits) return SessionError::PendingEdits;
  if (!state.revision_reliable) return SessionError::RevisionUncertain;
  if (state.body_bytes > config_.body_limit) return SessionError::SourceTooLarge;
  if (session_counter_ == std::numeric_limits<std::uint64_t>::max())
    return SessionError::GenerationExhausted;
  if (!owner_.freeze()) return SessionError::Busy;
  frozen_ = true;
  ++session_counter_;
  phase_ = Phase::Saving; error_ = SessionError::None; cancelled_ = false;
  stats_ = {}; stats_.source_bytes = state.body_bytes;
  identity_ = state.identity; last_save_ = {}; text_failure_ = {};
  source_at_ = output_at_ = 0;
  serializer_ = ExportBody(config_.body_limit);
  const auto check = checkIdentity(owner_.probe());
  if (check != SessionError::None) fail(check);
  return check;
}
void ExportSession::fail(SessionError error) noexcept {
  error_ = error; phase_ = Phase::Closing; closeIfDrained();
}
void ExportSession::closeIfDrained() noexcept {
  if (phase_ != Phase::Closing || reader_active_) return;
  if (data_) allocator_.deallocate(data_, stats_.allocation_bytes);
  data_ = nullptr;
  if (frozen_) { owner_.unfreeze(); frozen_ = false; }
  phase_ = Phase::Closed;
}
void ExportSession::stop() noexcept {
  if (phase_ == Phase::Idle || phase_ == Phase::Closed) return;
  phase_ = Phase::Closing; closeIfDrained();
}
void ExportSession::cancel() noexcept {
  if (phase_ == Phase::Idle || phase_ == Phase::Closed) return;
  cancelled_ = true; stop();
}
bool ExportSession::acceptSave(SaveReply reply) noexcept {
  last_save_ = reply;
  if (reply.state == SaveState::Partial) { fail(SessionError::SavePartial); return false; }
  if (reply.state == SaveState::Failed) { fail(SessionError::SaveFailed); return false; }
  return true;
}
void ExportSession::consume(void* context, std::string_view input) noexcept {
  auto& self = *static_cast<ExportSession*>(context);
  ++self.callback_count_;
  if (self.callback_count_ != 1 || input.empty() || input.size() > self.offered_) return;
  self.callback_bytes_ = input.size();
  const auto space = self.stats_.allocation_bytes - self.output_at_;
  self.step_ = self.serializer_.feed(input, {self.data_ ? self.data_ + self.output_at_ : nullptr, space});
}
Phase ExportSession::tick() noexcept {
  if (phase_ == Phase::Closing) { closeIfDrained(); return phase_; }
  if (phase_ == Phase::Ready) {
    const auto check = checkIdentity(owner_.probe());
    if (check != SessionError::None) fail(check);
    return phase_;
  }
  if (phase_ != Phase::Saving && phase_ != Phase::Copying) return phase_;
  auto state = owner_.probe();
  const auto check = checkIdentity(state);
  if (check != SessionError::None) { fail(check); return phase_; }
  if (phase_ == Phase::Saving) {
    // pending HID is deliberately not a gate here. The product owner has
    // consumed/discarded modal edits; only pending BODY edits block entry.
    if (state.save_busy || state.body_dirty || state.position_dirty) {
      ++stats_.save_calls;
      const auto reply = state.save_busy ? owner_.serviceSave() : owner_.beginAutoSave();
      if (!acceptSave(reply)) return phase_;
      state = owner_.probe();
      const auto after = checkIdentity(state);
      if (after != SessionError::None) { fail(after); return phase_; }
      if (reply.state == SaveState::Pending && !state.save_busy) fail(SessionError::PortContract);
      return phase_; // A completed old save can leave a newer body dirty.
    }
    if (state.body_bytes) {
      data_ = allocator_.allocate(state.body_bytes);
      if (!data_) { fail(SessionError::AllocationFailed); return phase_; }
    }
    stats_.allocation_bytes = state.body_bytes;
    phase_ = Phase::Copying;
    return phase_;
  }
  ++stats_.copy_ticks;
  if (source_at_ < stats_.source_bytes) {
    offered_ = std::min(config_.work_bytes, stats_.source_bytes - source_at_);
    callback_count_ = callback_bytes_ = 0; step_ = {};
    const auto result = owner_.borrow(source_at_, offered_, this, &ExportSession::consume);
    ++stats_.borrow_calls;
    if (!result.ok) { fail(SessionError::ReadFailed); return phase_; }
    if (callback_count_ != 1 || !callback_bytes_ || result.bytes != callback_bytes_) {
      fail(SessionError::PortContract); return phase_;
    }
    stats_.borrowed_bytes += callback_bytes_;
    stats_.max_borrow = std::max(stats_.max_borrow, callback_bytes_);
    source_at_ += step_.consumed; output_at_ += step_.produced;
    if (step_.flow == Flow::Error) {
      text_failure_ = step_.failure; fail(SessionError::Text); return phase_;
    }
    if (step_.consumed != callback_bytes_ || step_.flow != Flow::NeedInput) {
      fail(SessionError::PortContract); return phase_;
    }
  }
  const auto after = checkIdentity(owner_.probe());
  if (after != SessionError::None) { fail(after); return phase_; }
  if (source_at_ == stats_.source_bytes) {
    const auto r = serializer_.finish({data_ ? data_ + output_at_ : nullptr,
                                       stats_.allocation_bytes - output_at_});
    output_at_ += r.produced;
    if (r.flow == Flow::Error) { text_failure_ = r.failure; fail(SessionError::Text); }
    else if (r.flow != Flow::Done) fail(SessionError::PortContract);
    else { stats_.output_bytes = output_at_; phase_ = Phase::Ready; }
  }
  return phase_;
}
ReaderStatus ExportSession::acquire(Lease& out) noexcept {
  if (phase_ != Phase::Ready) return ReaderStatus::NotReady;
  const auto check = checkIdentity(owner_.probe());
  if (check != SessionError::None) { fail(check); return ReaderStatus::NotReady; }
  if (reader_active_) return ReaderStatus::Busy;
  if (reader_counter_ == std::numeric_limits<std::uint64_t>::max()) {
    fail(SessionError::GenerationExhausted); return ReaderStatus::Exhausted;
  }
  reader_token_ = {session_counter_, ++reader_counter_};
  reader_active_ = true; ++stats_.readers;
  out = {data_, stats_.output_bytes, reader_token_, identity_, stats_.source_bytes};
  return ReaderStatus::Ok;
}
ReaderStatus ExportSession::release(ReaderToken token, ReaderEnd end) noexcept {
  if (!reader_active_ || !(token == reader_token_)) return ReaderStatus::Stale;
  reader_active_ = false; reader_end_ = end;
  closeIfDrained();
  return ReaderStatus::Ok;
}
} // namespace draftor::exchange
