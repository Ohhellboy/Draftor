#include "draftor_exchange/import_job.hpp"
#include <algorithm>

namespace draftor::exchange {
ImportJob::~ImportJob() { cancel(); }
Status ImportJob::begin(ImportSource& source, const ImportIntent& intent) noexcept {
  if (phase_ == ImportPhase::Reading || phase_ == ImportPhase::Finishing ||
      phase_ == ImportPhase::Verifying || phase_ == ImportPhase::Publishing) return Status::Busy;
  Limits limits;
  if (!intent.operation || intent.name.back() != 0 || !quota_.checked(limits)) return Status::InvalidArgument;
  if (!source.confirmed()) return Status::Uncertain;
  // reserve validates operation-to-source binding before returning a durable
  // receipt. This allows replay after result delivery loss without rereading.
  ImportResult receipt;
  const auto status = port_.reserve(intent, receipt);
  if (status != Status::Ok) return status;
  intent_ = intent; result_ = receipt; source_ = nullptr;
  if (receipt.outcome == Outcome::Imported || receipt.outcome == Outcome::Opened) {
    phase_ = ImportPhase::Complete; result_.status = Status::Ok; return Status::Ok;
  }
  phase_ = ImportPhase::Reading;
  if (!(source.identity() == intent.source) || !source.stable()) { fail(Status::Stale); return Status::Stale; }
  if (intent.source.size > limits.wire) { fail(Status::TooLarge); return Status::TooLarge; }
  const auto start = port_.begin(intent.operation);
  if (start != Status::Ok) { fail(start); return start; }
  source_ = &source; sourceLease_ = source.sourceLease(); text_ = ImportWire(quota_.body);
  have_ = used_ = offset_ = 0; result_.bodyBytes = 0;
  result_.status = Status::Ok; result_.outcome = Outcome::Writing; return Status::Ok;
}
void ImportJob::fail(Status s) noexcept {
  if (phase_ == ImportPhase::Publishing) {
    // Cooperative publication may already have exposed META. Cancellation or a
    // stale source cannot roll it back or turn a lost receipt into "aborted".
    ImportResult receipt;
    const auto q = port_.query(intent_.operation, receipt);
    source_ = nullptr;
    if (q == Status::Ok && (receipt.outcome == Outcome::Imported || receipt.outcome == Outcome::Opened)) {
      result_ = receipt; result_.status = Status::Ok; phase_ = ImportPhase::Complete;
    } else {
      port_.abort(intent_.operation); // port must preserve any published record
      result_.status = Status::Uncertain; phase_ = ImportPhase::Uncertain;
    }
    return;
  }
  port_.abort(intent_.operation); source_ = nullptr;
  result_.status = s; result_.outcome = Outcome::Aborted; phase_ = ImportPhase::Failed;
}
void ImportJob::cancel() noexcept {
  if (phase_ == ImportPhase::Reading || phase_ == ImportPhase::Finishing ||
      phase_ == ImportPhase::Verifying || phase_ == ImportPhase::Publishing) fail(Status::Cancelled);
}
bool ImportJob::append(const exchange::Step& step) noexcept {
  if (step.flow == exchange::Flow::Error) {
    result_.text = step.failure; fail(Status::InvalidText); return false;
  }
  if (step.produced) {
    const auto r = port_.append(intent_.operation, {output_.data(), step.produced});
    if (r.status != Status::Ok || r.bytes != step.produced) {
      fail(r.status == Status::Ok ? Status::ShortWrite : r.status); return false;
    }
    result_.bodyBytes += step.produced;
  }
  return true;
}
ImportPhase ImportJob::tick() noexcept {
  if (phase_ != ImportPhase::Reading && phase_ != ImportPhase::Finishing &&
      phase_ != ImportPhase::Verifying && phase_ != ImportPhase::Publishing) return phase_;
  if (!source_ || !source_->stable() || source_->sourceLease() != sourceLease_ || !(source_->identity() == intent_.source)) {
    fail(Status::Stale); return phase_;
  }
  if (phase_ == ImportPhase::Reading) {
    if (used_ == have_) {
      if (offset_ == intent_.source.size) { phase_ = ImportPhase::Finishing; return phase_; }
      have_ = std::min<std::size_t>(input_.size(), intent_.source.size - offset_); used_ = 0;
      const auto r = source_->read(offset_, input_.data(), have_);
      if (r.status != Status::Ok || r.bytes != have_) {
        fail(r.status == Status::Ok ? Status::ShortRead : r.status); return phase_;
      }
      if (!source_->stable()) { fail(Status::Stale); return phase_; }
      offset_ += have_;
    }
    const auto step = text_.feed({input_.data() + used_, have_ - used_}, {output_.data(), output_.size()});
    used_ += step.consumed; append(step);
  } else if (phase_ == ImportPhase::Finishing) {
    const auto step = text_.finish({output_.data(), output_.size()});
    if (append(step) && step.flow == exchange::Flow::Done) {
      phase_ = ImportPhase::Verifying;
    }
  } else if (phase_ == ImportPhase::Verifying) {
    const auto status = port_.finish(intent_.operation, result_.bodyBytes);
    if (status == Status::Busy) return phase_;
    if (status != Status::Ok) fail(status);
    else { result_.outcome = Outcome::Verified; phase_ = ImportPhase::Publishing; }
  } else {
    const auto status = port_.publish(intent_.operation);
    if (status == Status::Busy) return phase_;
    ImportResult receipt;
    const auto queried = port_.query(intent_.operation, receipt);
    if (queried == Status::Ok && (receipt.outcome == Outcome::Imported || receipt.outcome == Outcome::Opened)) {
      result_ = receipt; result_.status = Status::Ok; phase_ = ImportPhase::Complete; source_ = nullptr;
    } else if (status == Status::Uncertain || status == Status::Ok || queried != Status::Ok) {
      // Never abort a possibly committed transaction. Same operation ID recovers.
      result_.status = Status::Uncertain; phase_ = ImportPhase::Uncertain; source_ = nullptr;
    } else {
      phase_ = ImportPhase::Verifying; fail(status);
    }
  }
  return phase_;
}
} // namespace draftor::exchange
