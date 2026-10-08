#pragma once
#include "ports.hpp"

namespace draftor::usb {
enum class InboxPhase { Idle, Preparing, HostMounted, HostWriting, Sealing,
  Sealed, Inspecting, ImportReady, Closed };
struct InboxResult {
  Status status = Status::Ok;
  EndReason end = EndReason::None;
  std::size_t candidates = 0, ignored = 0;
};
bool isTxtCandidate(const InboxCandidate&) noexcept;
class InboxSession final : public ImportSource {
 public:
  InboxSession(InboxMediaPort& b, InboxFilesystemPort& f, MediaPort& m, TransferQuota quota = {}) noexcept
    : backing_(b), fs_(f), media_(m), quota_(quota) {}
  ~InboxSession();
  InboxSession(const InboxSession&) = delete;
  InboxSession& operator=(const InboxSession&) = delete;
  Status begin() noexcept;
  InboxPhase tick() noexcept;
  void endHost(EndReason) noexcept;
  Status close() noexcept;
  Status next(InboxCandidate&) noexcept;
  Status acquire(const InboxCandidate&, ImportSource*& out) noexcept;
  std::uint64_t sourceLease() const noexcept override { return sourceActive_ ? sourceLease_ : 0; }
  Status release(std::uint64_t sourceLease) noexcept;
  SourceIdentity identity() const noexcept override { return selected_; }
  bool stable() const noexcept override;
  bool confirmed() const noexcept override { return result_.end == EndReason::Ejected && result_.status == Status::Ok; }
  IoResult read(std::uint64_t, void*, std::size_t) noexcept override;
  InboxPhase phase() const noexcept { return phase_; }
  InboxResult result() const noexcept { return result_; }
 private:
  void fail(Status) noexcept;
  InboxMediaPort& backing_;
  InboxFilesystemPort& fs_;
  MediaPort& media_;
  TransferQuota quota_;
  InboxPhase phase_ = InboxPhase::Idle;
  InboxResult result_{};
  SourceIdentity selected_{};
  std::uint64_t session_ = 0, sourceLease_ = 0;
  std::uint32_t generation_ = 0;
  bool mounted_ = false, sourceActive_ = false, published_ = false;
};
} // namespace draftor::usb
