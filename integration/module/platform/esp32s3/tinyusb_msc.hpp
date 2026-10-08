#pragma once
#include "media_bridge.hpp"

namespace draftor::usb::s3 {
// One firmware-lifetime endpoint is required by the SDK's C callback ABI.
// Business controllers attach only their media, then withdraw/drain/detach.
class TinyUsbMsc final : public MediaPort {
 public:
  static TinyUsbMsc& instance() noexcept;
  bool start(const char* productName = "Draftor USB Document Transfer") noexcept;
  // Owner-loop operation: real disconnect/connect then fresh bus reset+inquiry.
  // Busy while advancing; NotReady on timeout/unsupported API (physical reconnect remains possible).
  Status prepareSession(std::uint32_t nowMs) noexcept;
  void cancelPreparation() noexcept;
  Status publish(BlockDevice&, Access, std::uint32_t&) noexcept override;
  void withdraw(EndReason why) noexcept override { bridge_.withdraw(why); }
  MediaState state() const noexcept override { return bridge_.state(); }
  Status detach() noexcept override { return bridge_.detach(); }
  MediaBridge& bridge() noexcept { return bridge_; }
  void busReset() noexcept; // lock-free ISR notification, no filesystem/business work
  void inquiryFence() noexcept; // runs in TinyUSB task after enumeration
  void noteRequest() noexcept;
 private:
  constexpr TinyUsbMsc() = default;
  // Firmware-lifetime storage, constructed once before application startup.
  // No first-call guard or automatic destructor registration.
  static unsigned char storage_[];
  static void bootConstruct() __attribute__((constructor(101)));
  MediaBridge bridge_;
  std::atomic<std::uint32_t> reset_{0}, enumerated_{0}, usedEpoch_{0};
  std::atomic<bool> used_{false};
  enum class Reconnect { Idle, Detached, AwaitingInquiry };
  Reconnect reconnect_ = Reconnect::Idle;
  std::uint32_t reconnectAt_ = 0;
  bool started_ = false, publishedOnce_ = false;
};
} // namespace draftor::usb::s3
