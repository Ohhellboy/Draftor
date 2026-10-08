#pragma once
#include "media_bridge.hpp"
#include <Adafruit_TinyUSB.h>

namespace draftor::usb::pico {
class TinyUsbMsc final : public MediaPort, public Adafruit_USBD_Interface {
 public:
  static TinyUsbMsc& instance() noexcept;
  bool start() noexcept;
  void service() noexcept;
  bool canPublish() const noexcept;
  Status publish(BlockDevice&,Access,std::uint32_t&) noexcept override;
  void withdraw(EndReason reason) noexcept override { bridge_.withdraw(reason); }
  MediaState state() const noexcept override;
  Status detach() noexcept override;
  uint16_t getInterfaceDescriptor(uint8_t,uint8_t*,uint16_t) override;
  void reset() noexcept;
  void inquiry() noexcept { inquiryEpoch_.store(epoch_.load()); }
  std::int32_t enqueue(bool write,std::uint32_t lba,std::uint32_t offset,void*,std::uint32_t n) noexcept;
  bool synchronize(bool eject) noexcept;
  std::uint32_t sectors() const noexcept { return bridge_.sectors(); }
  bool writable() const noexcept { return bridge_.writable(); }
 private:
  enum class Pending : unsigned { Empty,Filling,Queued,Executing,Completing };
  TinyUsbMsc() = default;
  MediaBridge bridge_;
  std::atomic<Pending> pending_{Pending::Empty};
  std::atomic<std::uint32_t> epoch_{1},inquiryEpoch_{0};
  std::array<uint8_t,SectorBytes> bounce_{};
  void* usbBuffer_ = nullptr;
  std::uint64_t offset_ = 0;
  std::uint32_t bytes_=0,generation_=0,requestEpoch_=0,usedEpoch_=0,reconnectAt_=0;
  IoResult completion_{};
  std::atomic<bool> used_{false};
  bool write_=false,started_=false,reconnecting_=false;
};
}
