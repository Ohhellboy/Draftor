#pragma once
#include "document_transfer.hpp"
#include <limits>
namespace draftor::exchange {
enum class TransferChannel { None, UsbExport, UsbImport, Qr, Wifi, Bluetooth };
enum class TransferStage { Idle, Preparing, Transferring, Committing, Closing, Failed };
// One product owner calls this gate. Closing/error retain ownership until all
// callbacks and persistence work release their resources; no queue or preemption.
class TransferSelection final {
 public:
  Status acquire(TransferChannel channel, std::uint64_t& token) noexcept {
    if (stage_ != TransferStage::Idle) return Status::Busy;
    if (channel == TransferChannel::None) return Status::InvalidArgument;
    if (token_ == std::numeric_limits<std::uint64_t>::max()) return Status::Exhausted;
    ++token_; channel_ = channel; stage_ = TransferStage::Preparing; token = token_;
    return Status::Ok;
  }
  Status stage(std::uint64_t token, TransferStage next) noexcept {
    if (!token || token != token_ || stage_ == TransferStage::Idle) return Status::Stale;
    if (next == TransferStage::Idle) return Status::InvalidArgument;
    if ((stage_ == TransferStage::Closing || stage_ == TransferStage::Failed) &&
        next != TransferStage::Closing && next != TransferStage::Failed) return Status::Busy;
    stage_ = next; return Status::Ok;
  }
  Status release(std::uint64_t token, bool resourcesReleased) noexcept {
    if (!token || token != token_ || stage_ == TransferStage::Idle) return Status::Stale;
    if (!resourcesReleased) return Status::Busy;
    if (stage_ != TransferStage::Closing && stage_ != TransferStage::Failed) return Status::Busy;
    channel_ = TransferChannel::None; stage_ = TransferStage::Idle; return Status::Ok;
  }
  TransferChannel channel() const noexcept { return channel_; }
  TransferStage stage() const noexcept { return stage_; }
  std::uint64_t token() const noexcept { return token_; }
 private:
  TransferChannel channel_ = TransferChannel::None;
  TransferStage stage_ = TransferStage::Idle;
  std::uint64_t token_ = 0;
};
} // namespace draftor::exchange
