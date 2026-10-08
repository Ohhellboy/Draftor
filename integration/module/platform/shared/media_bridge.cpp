#include "media_bridge.hpp"
#include <exception>
#include <limits>

namespace draftor::usb {
MediaBridge::~MediaBridge() {
  if (device_ || (gate_.load() & (Publishing | 1)) || !(gate_.load() & Closing)) std::terminate();
}
Status MediaBridge::publish(BlockDevice& device, Access access, std::uint32_t& out) noexcept {
  if (device_ || gate_.load(std::memory_order_acquire) != Closing) return Status::Busy;
  const auto g = generation_.load();
  if (g == std::numeric_limits<std::uint32_t>::max()) return Status::Exhausted;
  if (!device.sectorCount()) return Status::InvalidArgument;
  auto expected = Closing;
  if (!gate_.compare_exchange_strong(expected, Closing | Publishing)) return Status::Busy;
  device_ = &device; sectors_.store(device.sectorCount());
  generation_.store(g + 1); writable_.store(access == Access::ReadWrite);
  wrote_.store(false); end_.store(EndReason::None); error_.store(Status::Ok);
  expected = Closing | Publishing;
  if (!gate_.compare_exchange_strong(expected, 0, std::memory_order_release)) {
    // A reset/cancel during publication must not be overwritten by opening the
    // gate. No reader was admitted; retire this generation without exposing it.
    device_ = nullptr; sectors_.store(0); gate_.store(Closing); return Status::NotReady;
  }
  out = g + 1; return Status::Ok;
}
Status MediaBridge::acquire(std::uint32_t g) noexcept {
  std::uint32_t expected = 0;
  if (!gate_.compare_exchange_strong(expected, 1, std::memory_order_acquire))
    return (expected & Closing) ? Status::NotReady : Status::Busy;
  if (g != generation_.load()) { release(); return Status::Stale; }
  return Status::Ok;
}
void MediaBridge::withdraw(EndReason why) noexcept {
  gate_.fetch_or(Closing | Interrupted, std::memory_order_acq_rel);
  auto none = EndReason::None;
  end_.compare_exchange_strong(none, why);
}
MediaState MediaBridge::state() const noexcept {
  const auto gate = gate_.load(std::memory_order_acquire);
  return {!(gate & Closing), !(gate & (Publishing | 1)), wrote_.load(), end_.load(),
          error_.load(), generation_.load()};
}
Status MediaBridge::detach() noexcept {
  const auto gate = gate_.load(std::memory_order_acquire);
  if (!(gate & Closing) || (gate & (Publishing | 1))) return Status::Busy;
  device_ = nullptr; sectors_.store(0); gate_.store(Closing); return Status::Ok;
}
Status MediaBridge::acknowledgeIdle() noexcept {
  if (device_) return Status::Busy;
  auto gate = gate_.load(std::memory_order_acquire);
  if (!(gate & Closing) || (gate & (Publishing | 1))) return Status::Busy;
  return gate_.compare_exchange_strong(gate, Closing) ? Status::Ok : Status::Busy;
}
void MediaBridge::record(Status status) noexcept {
  if (status == Status::Io || status == Status::Full || status == Status::ShortRead ||
      status == Status::ShortWrite || status == Status::SyncFailed) {
    auto ok = Status::Ok; error_.compare_exchange_strong(ok, status);
  }
}
IoResult MediaBridge::read(std::uint32_t g, std::uint64_t at, void* out, std::size_t n) noexcept {
  const auto status = acquire(g);
  if (status != Status::Ok) return {status, 0};
  auto r = device_->read(at, out, n);
  if (r.status == Status::Ok && r.bytes != n) r.status = Status::ShortRead;
  record(r.status); release(); return r;
}
IoResult MediaBridge::write(std::uint32_t g, std::uint64_t at, const void* in, std::size_t n) noexcept {
  const auto status = acquire(g);
  if (status != Status::Ok) return {status, 0};
  if (!writable_.load()) { release(); return {Status::ReadOnly, 0}; }
  auto r = device_->write(at, in, n);
  if (r.status == Status::Ok && r.bytes != n) r.status = Status::ShortWrite;
  if (r.bytes) wrote_.store(true);
  record(r.status); release(); return r;
}
Status MediaBridge::sync(std::uint32_t g) noexcept {
  auto r = acquire(g);
  if (r != Status::Ok) return r;
  r = device_->sync(); record(r); release(); return r;
}
} // namespace draftor::usb
