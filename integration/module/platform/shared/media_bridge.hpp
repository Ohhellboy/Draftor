#pragma once
#include "draftor_usb_transfer/ports.hpp"
#include <atomic>

namespace draftor::usb {
// Platform-side lifetime fence. Single owner publishes/detaches; arbitrary
// callback context may withdraw or do I/O. One synchronous request at a time:
// BUSY is returned immediately, never a spin/mutex wait. No business callbacks.
class MediaBridge final : public MediaPort {
 public:
  ~MediaBridge();
  Status publish(BlockDevice&, Access, std::uint32_t&) noexcept override;
  void withdraw(EndReason) noexcept override;
  MediaState state() const noexcept override;
  Status detach() noexcept override;
  // Platform owner acknowledges an observed bus fence, only with no attachment.
  Status acknowledgeIdle() noexcept;
  IoResult read(std::uint32_t generation, std::uint64_t, void*, std::size_t) noexcept;
  IoResult write(std::uint32_t generation, std::uint64_t, const void*, std::size_t) noexcept;
  Status sync(std::uint32_t generation) noexcept;
  std::uint32_t generation() const noexcept { return generation_.load(); }
  std::uint32_t sectors() const noexcept { return sectors_.load(); }
  bool writable() const noexcept { return state().accepting && writable_.load(); }
 private:
  static constexpr std::uint32_t Closing = 0x80000000u;
  static constexpr std::uint32_t Publishing = 0x40000000u, Interrupted = 0x20000000u;
  Status acquire(std::uint32_t) noexcept;
  void release() noexcept { gate_.fetch_sub(1, std::memory_order_release); }
  void record(Status) noexcept;
  std::atomic<std::uint32_t> gate_{Closing}, generation_{0}, sectors_{0};
  std::atomic<bool> writable_{false}, wrote_{false};
  std::atomic<EndReason> end_{EndReason::None};
  std::atomic<Status> error_{Status::Ok};
  BlockDevice* device_ = nullptr; // protected by gate admission/drain
};
} // namespace draftor::usb
