#include "tinyusb_msc.hpp"
#include "check.hpp"
#include "device/usbd.h"
#include <array>
using namespace draftor::usb;
static std::uint8_t key, asc;
extern "C" {
bool tud_msc_set_sense(std::uint8_t, std::uint8_t k, std::uint8_t a, std::uint8_t) { key = k; asc = a; return true; }
void tud_event_hook_cb(std::uint8_t, std::uint32_t, bool);
std::uint8_t tud_msc_get_maxlun_cb();
void tud_msc_inquiry_cb(std::uint8_t, std::uint8_t*, std::uint8_t*, std::uint8_t*);
bool tud_msc_test_unit_ready_cb(std::uint8_t);
void tud_msc_capacity_cb(std::uint8_t, std::uint32_t*, std::uint16_t*);
bool tud_msc_is_writable_cb(std::uint8_t);
std::int32_t tud_msc_read10_cb(std::uint8_t, std::uint32_t, std::uint32_t, void*, std::uint32_t);
std::int32_t tud_msc_write10_cb(std::uint8_t, std::uint32_t, std::uint32_t, std::uint8_t*, std::uint32_t);
bool tud_msc_start_stop_cb(std::uint8_t, std::uint8_t, bool, bool);
std::int32_t tud_msc_scsi_cb(std::uint8_t, const std::uint8_t*, void*, std::uint16_t);
}
struct Disk : BlockDevice {
  std::array<char, 1024> data{}; Status flush = Status::Ok;
  std::uint32_t sectorCount() const noexcept override { return 2; }
  IoResult read(std::uint64_t at, void* out, std::size_t n) noexcept override {
    if (at > data.size() || n > data.size() - at) return {Status::Bounds, 0};
    std::memcpy(out, data.data() + at, n); return {Status::Ok, n};
  }
  IoResult write(std::uint64_t at, const void* in, std::size_t n) noexcept override {
    if (at > data.size() || n > data.size() - at) return {Status::Bounds, 0};
    std::memcpy(data.data() + at, in, n); return {Status::Ok, n};
  }
  Status sync() noexcept override { return flush; }
};
static void enumerate() {
  tud_event_hook_cb(0, 1, true);
  std::uint8_t vid[8], pid[16], rev[4]; tud_msc_inquiry_cb(0, vid, pid, rev);
}
int main() {
  auto& ep = s3::TinyUsbMsc::instance(); Disk disk; std::uint32_t gen;
  CHECK(ep.start() && tud_msc_get_maxlun_cb() == 1);
  CHECK(ep.publish(disk, Access::ReadOnly, gen) == Status::Ok); enumerate();
  CHECK(tud_msc_test_unit_ready_cb(0) && !tud_msc_is_writable_cb(0));
  std::uint32_t sectors; std::uint16_t size; tud_msc_capacity_cb(0, &sectors, &size); CHECK(sectors == 2 && size == 512);
  std::uint8_t byte = 'x';
  CHECK(tud_msc_write10_cb(0, 0, 0, &byte, 1) == -1 && key == 7 && asc == 0x27 && disk.data[0] == 0);
  CHECK(tud_msc_read10_cb(0, 2, 0, &byte, 1) == -1 && key == 5 && asc == 0x21);
  CHECK(tud_msc_start_stop_cb(0, 0, false, true)); CHECK(ep.detach() == Status::Ok);
  CHECK(tud_msc_write10_cb(0, 0, 0, &byte, 1) == -1 && key == 2);
  CHECK(ep.publish(disk, Access::ReadWrite, gen) == Status::NotReady);
  enumerate(); tud_event_hook_cb(0, 1, true);
  CHECK(ep.publish(disk, Access::ReadWrite, gen) == Status::NotReady);
  std::uint8_t v[8], p[16], revision[4]; tud_msc_inquiry_cb(0, v, p, revision);
  CHECK(ep.publish(disk, Access::ReadWrite, gen) == Status::Ok);
  CHECK(tud_msc_is_writable_cb(0)); byte = 'N';
  CHECK(tud_msc_write10_cb(0, 0, 1, &byte, 1) == 1 && disk.data[1] == 'N');
  byte = 0; CHECK(tud_msc_read10_cb(0, 0, 1, &byte, 1) == 1 && byte == 'N');
  std::uint8_t cmd[16]{}; cmd[0] = 0x35;
  CHECK(tud_msc_scsi_cb(0, cmd, nullptr, 0) == 0);
  disk.flush = Status::SyncFailed;
  CHECK(tud_msc_scsi_cb(0, cmd, nullptr, 0) == -1 && key == 3 && asc == 0x0c);
  CHECK(!tud_msc_start_stop_cb(0, 0, false, true));
  CHECK(ep.state().error == Status::SyncFailed && ep.detach() == Status::Ok);
  enumerate(); disk.flush = Status::Ok;
  CHECK(ep.publish(disk, Access::ReadOnly, gen) == Status::Ok && tud_msc_test_unit_ready_cb(0));
  cmd[0] = 0xff; CHECK(tud_msc_scsi_cb(0, cmd, nullptr, 0) == -1 && key == 5 && asc == 0x20);
  tud_event_hook_cb(0, 1, true); CHECK(!tud_msc_test_unit_ready_cb(0));
  CHECK(tud_msc_read10_cb(0, 0, 0, &byte, 1) == -1);
  CHECK(ep.detach() == Status::Ok);
  CHECK(ep.prepareSession(100) == Status::Busy && host_disconnect_calls == 1);
  CHECK(ep.prepareSession(399) == Status::Busy && host_connect_calls == 0);
  CHECK(ep.prepareSession(400) == Status::Busy && host_connect_calls == 1);
  CHECK(ep.publish(disk, Access::ReadWrite, gen) == Status::NotReady);
  enumerate(); CHECK(ep.prepareSession(401) == Status::Ok);
  CHECK(ep.publish(disk, Access::ReadWrite, gen) == Status::Ok && tud_msc_test_unit_ready_cb(0));
  CHECK(tud_msc_start_stop_cb(0, 0, false, true) && ep.detach() == Status::Ok);
  CHECK(ep.prepareSession(500) == Status::Busy); ep.cancelPreparation();
  CHECK(host_connect_calls == 2);
  host_connect_supported = false; CHECK(ep.prepareSession(600) == Status::NotReady);
  host_connect_supported = true;
  CHECK(ep.prepareSession(700) == Status::Busy && ep.prepareSession(1000) == Status::Busy);
  CHECK(ep.prepareSession(11000) == Status::NotReady);
  std::printf("PASS real adapter callbacks with Host SDK seam checks=%zu\n", checks);
}
