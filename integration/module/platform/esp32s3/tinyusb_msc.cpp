#include <new>
#include "tinyusb_msc.hpp"
#include "USB.h"
#include "esp32-hal-tinyusb.h"
#include "device/dcd.h"
#include "device/usbd.h"
#include <cstring>
#include <limits>

#if ARDUINO_USB_MODE != 0 || ARDUINO_USB_MSC_ON_BOOT || ARDUINO_USB_DFU_ON_BOOT
#error "Use USB OTG (TinyUSB), MSCOnBoot disabled and DFUOnBoot disabled"
#endif
namespace draftor::usb::s3 {
namespace {
std::uint16_t descriptor(std::uint8_t* dst, std::uint8_t* itf) {
  const auto str = tinyusb_add_string_descriptor("Draftor document transfer");
  const auto ep = tinyusb_get_free_duplex_endpoint();
  if (!ep) return 0;
  const std::uint8_t bytes[] = {TUD_MSC_DESCRIPTOR(*itf, str, ep, std::uint8_t(0x80 | ep), CFG_TUD_ENDPOINT_SIZE)};
  ++*itf; std::memcpy(dst, bytes, sizeof bytes); return sizeof bytes;
}
}
// Raw storage has no automatic destructor registration. The fixed constructor
// runs once before application startup, before any USB callback is registered.
// Its constexpr default definition must remain allocation-free (C++17); target
// code is checked for guard/allocator/atexit calls. This firmware-lifetime object
// is never destroyed; session teardown remains explicit withdraw/detach.
alignas(TinyUsbMsc) unsigned char TinyUsbMsc::storage_[sizeof(TinyUsbMsc)];
void TinyUsbMsc::bootConstruct() { new (storage_) TinyUsbMsc; }
TinyUsbMsc& TinyUsbMsc::instance() noexcept { return *std::launder(reinterpret_cast<TinyUsbMsc*>(storage_)); }

bool TinyUsbMsc::start(const char* productName) noexcept {
  if (started_) return true;
  if (tinyusb_enable_interface(USB_INTERFACE_MSC, TUD_MSC_DESC_LEN, descriptor) != ESP_OK) return false;
  USB.productName(productName);
  started_ = USB.begin(); return started_;
}
Status TinyUsbMsc::prepareSession(std::uint32_t now) noexcept {
  if (!started_) return Status::NotReady;
  const auto state = bridge_.state();
  if (state.accepting || !state.drained || bridge_.sectors()) return Status::Busy;
  if (!publishedOnce_) return Status::Ok;
  const auto fence = enumerated_.load();
  if (reconnect_ != Reconnect::Detached && fence > usedEpoch_.load() && fence == reset_.load()) {
    reconnect_ = Reconnect::Idle; return Status::Ok;
  }
  if (reconnect_ == Reconnect::Idle) {
    if (!tud_disconnect()) return Status::NotReady;
    reconnectAt_ = now; reconnect_ = Reconnect::Detached; return Status::Busy;
  }
  if (reconnect_ == Reconnect::Detached) {
    if (std::uint32_t(now - reconnectAt_) < 300) return Status::Busy;
    if (!tud_connect()) { reconnect_ = Reconnect::Idle; return Status::NotReady; }
    reconnectAt_ = now; reconnect_ = Reconnect::AwaitingInquiry; return Status::Busy;
  }
  if (std::uint32_t(now - reconnectAt_) >= 10000) {
    reconnect_ = Reconnect::Idle; return Status::NotReady;
  }
  return Status::Busy;
}
void TinyUsbMsc::cancelPreparation() noexcept {
  // Restore the physical link if cancelled during our deliberate detach window.
  if (reconnect_ == Reconnect::Detached) tud_connect();
  reconnect_ = Reconnect::Idle;
}
Status TinyUsbMsc::publish(BlockDevice& disk, Access access, std::uint32_t& out) noexcept {
  // Raw MSC callbacks carry no session ID. Require a fresh reset + inquiry
  // between media; this fences residual fragmented commands from the old disk.
  const auto fence = enumerated_.load();
  if (publishedOnce_) {
    if (fence <= usedEpoch_.load() || fence != reset_.load()) return Status::NotReady;
    const auto ack = bridge_.acknowledgeIdle();
    if (ack != Status::Ok) return ack;
    // A reset before this check invalidates the observed inquiry. A reset
    // after it marks the bridge interrupted, so publish cannot reopen it.
    if (fence != reset_.load()) return Status::NotReady;
  }
  const auto status = bridge_.publish(disk, access, out);
  if (status == Status::Ok) {
    publishedOnce_ = true; usedEpoch_.store(enumerated_.load());
  }
  return status;
}
void TinyUsbMsc::busReset() noexcept {
  const auto n = reset_.load();
  if (n != std::numeric_limits<std::uint32_t>::max()) reset_.store(n + 1);
  if (used_.load()) bridge_.withdraw(EndReason::Disconnected);
}
void TinyUsbMsc::inquiryFence() noexcept { enumerated_.store(reset_.load()); }
void TinyUsbMsc::noteRequest() noexcept {
  if (bridge_.state().accepting) { usedEpoch_.store(reset_.load()); used_.store(true); }
}
} // namespace draftor::usb::s3

namespace {
using namespace draftor::usb;
auto& endpoint() { return s3::TinyUsbMsc::instance(); }
bool sense(Status s) {
  if (s == Status::Ok) return true;
  std::uint8_t key = SCSI_SENSE_MEDIUM_ERROR, asc = 0x11;
  switch (s) {
    case Status::ReadOnly: key = SCSI_SENSE_DATA_PROTECT; asc = 0x27; break;
    case Status::NotReady: case Status::Stale: case Status::Busy:
      key = SCSI_SENSE_NOT_READY; asc = 0x3a; break;
    case Status::Bounds: case Status::InvalidArgument:
      key = SCSI_SENSE_ILLEGAL_REQUEST; asc = 0x21; break;
    case Status::Full: case Status::ShortWrite: case Status::SyncFailed:
      key = SCSI_SENSE_MEDIUM_ERROR; asc = 0x0c; break;
    default: break;
  }
  tud_msc_set_sense(0, key, asc, 0); return false;
}
bool lunOkay(std::uint8_t lun) { return lun == 0; }
}
extern "C" {
void tud_event_hook_cb(std::uint8_t, std::uint32_t event, bool) {
  if (event == DCD_EVENT_BUS_RESET) endpoint().busReset();
  else if (event == DCD_EVENT_UNPLUGGED) endpoint().withdraw(EndReason::Disconnected);
}
std::uint8_t tud_msc_get_maxlun_cb() { return 1; } // TinyUSB expects LUN count
void tud_msc_inquiry_cb(std::uint8_t, std::uint8_t vid[8], std::uint8_t pid[16], std::uint8_t rev[4]) {
  endpoint().inquiryFence();
  std::memcpy(vid, "DRAFTOR ", 8); std::memcpy(pid, "USB TRANSFER    ", 16); std::memcpy(rev, "0001", 4);
}
bool tud_msc_test_unit_ready_cb(std::uint8_t lun) {
  endpoint().noteRequest();
  return lunOkay(lun) && sense(endpoint().state().accepting ? Status::Ok : Status::NotReady);
}
void tud_msc_capacity_cb(std::uint8_t lun, std::uint32_t* count, std::uint16_t* size) {
  endpoint().noteRequest();
  *count = lunOkay(lun) && endpoint().state().accepting ? endpoint().bridge().sectors() : 0;
  *size = *count ? SectorBytes : 0;
}
bool tud_msc_is_writable_cb(std::uint8_t lun) {
  return lunOkay(lun) && endpoint().bridge().writable();
}
std::int32_t tud_msc_read10_cb(std::uint8_t lun, std::uint32_t lba, std::uint32_t offset, void* p, std::uint32_t n) {
  if (!lunOkay(lun) || offset >= SectorBytes || n > INT32_MAX) return -1;
  endpoint().noteRequest(); auto& b = endpoint().bridge();
  const auto r = b.read(b.generation(), std::uint64_t(lba) * SectorBytes + offset, p, n);
  return sense(r.status) ? static_cast<std::int32_t>(r.bytes) : -1;
}
std::int32_t tud_msc_write10_cb(std::uint8_t lun, std::uint32_t lba, std::uint32_t offset, std::uint8_t* p, std::uint32_t n) {
  if (!lunOkay(lun) || offset >= SectorBytes || n > INT32_MAX) return -1;
  endpoint().noteRequest(); auto& b = endpoint().bridge();
  const auto r = b.write(b.generation(), std::uint64_t(lba) * SectorBytes + offset, p, n);
  return sense(r.status) ? static_cast<std::int32_t>(r.bytes) : -1;
}
bool tud_msc_start_stop_cb(std::uint8_t lun, std::uint8_t, bool start, bool eject) {
  if (!lunOkay(lun)) return false;
  if (eject && !start) {
    auto& b = endpoint().bridge();
    const auto r = b.sync(b.generation());
    // Even failed sync withdraws admission; the sticky media error prevents
    // parsing/publishing an inbox whose writes were not safely acknowledged.
    b.withdraw(EndReason::Ejected); return sense(r);
  }
  return sense(endpoint().state().accepting ? Status::Ok : Status::NotReady);
}
std::int32_t tud_msc_scsi_cb(std::uint8_t lun, const std::uint8_t cmd[16], void*, std::uint16_t) {
  if (!lunOkay(lun)) return -1;
  if (cmd[0] == 0x35) { // SCSI SYNCHRONIZE CACHE(10); real SDK has no USBMSC hook
    auto& b = endpoint().bridge(); return sense(b.sync(b.generation())) ? 0 : -1;
  }
  if (cmd[0] == SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL)
    return sense(endpoint().state().accepting ? Status::Ok : Status::NotReady) ? 0 : -1;
  tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0); return -1;
}
}
