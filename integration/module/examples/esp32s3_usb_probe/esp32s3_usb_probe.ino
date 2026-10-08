// ISOLATED MODULE PROBE. No P23, editor, IME, TF, telemetry or product menu.
#include <Arduino.h>
#include <DraftorUsb.h>
#include "draftor_usb_transfer/export.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include "draftor_usb_transfer/import.hpp"
#include "tinyusb_msc.hpp"
#include "fatfs_inbox.hpp"
#include "esp_heap_caps.h"
#include <cstring>

namespace {
using namespace draftor::usb;
namespace dx = draftor::exchange;
class SampleOwner final : public dx::OwnerPort {
 public:
  const char* body = u8"Draftor USB probe\n简体 繁體 𨿍 🙂 é\n";
  bool frozen = false;
  dx::OwnerState probe() noexcept override {
    dx::OwnerState s; s.has_current = true; s.body_bytes = std::strlen(body);
    s.identity.collection[0] = 1; s.identity.document[0] = 1;
    s.identity.mount = s.identity.document_session = s.identity.revision_epoch = 1; return s;
  }
  bool freeze() noexcept override { if (frozen) return false; frozen = true; return true; }
  void unfreeze() noexcept override { frozen = false; }
  dx::SaveReply beginAutoSave() noexcept override { return {}; }
  dx::SaveReply serviceSave() noexcept override { return {}; }
  dx::BorrowReply borrow(std::size_t at, std::size_t n, void* ctx, dx::BorrowConsumer consumer) noexcept override {
    consumer(ctx, {body + at, n}); return {true, n};
  }
};
class PsramSnapshot final : public dx::Allocator {
 public:
  char* allocate(std::size_t n) noexcept override { return static_cast<char*>(heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)); }
  void deallocate(char* p, std::size_t) noexcept override { heap_caps_free(p); }
};
// Volatile probe sink only. Durable semantics are tested by Host PersistentStore;
// the real DocumentImportPort is supplied by the next product integration stage.
class ProbeSink final : public DocumentImportPort {
 public:
  ~ProbeSink() { heap_caps_free(body_); }
  Status reserve(const ImportIntent& i, ImportResult& out) noexcept override {
    if (r_.operation == i.operation && r_.outcome == Outcome::Imported) {
      if (!(i.source == source_)) return Status::Conflict;
      out = r_; return Status::Ok;
    }
    if (r_.outcome == Outcome::Writing || r_.outcome == Outcome::Verified) return Status::Busy;
    if (!body_) body_ = static_cast<char*>(heap_caps_malloc(BodyLimit, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!body_) return Status::Io;
    source_ = i.source; r_ = {Status::Ok, Outcome::Reserved, i.operation, {}, 0, {}}; out = r_; return Status::Ok;
  }
  Status begin(std::uint64_t id) noexcept override { if (id != r_.operation) return Status::Stale; r_.outcome = Outcome::Writing; return Status::Ok; }
  IoResult append(std::uint64_t id, std::string_view s) noexcept override {
    if (id != r_.operation || r_.outcome != Outcome::Writing || s.size() > BodyLimit - r_.bodyBytes) return {Status::Bounds, 0};
    std::memcpy(body_ + r_.bodyBytes, s.data(), s.size()); r_.bodyBytes += s.size(); return {Status::Ok, s.size()};
  }
  Status finish(std::uint64_t id, std::size_t n) noexcept override {
    if (id != r_.operation || n != r_.bodyBytes) return Status::Contract;
    r_.outcome = Outcome::Verified; return Status::Ok;
  }
  Status publish(std::uint64_t id) noexcept override {
    if (id != r_.operation || r_.outcome != Outcome::Verified) return Status::Stale;
    r_.outcome = Outcome::Imported; return Status::Ok;
  }
  void abort(std::uint64_t id) noexcept override { if (id == r_.operation && r_.outcome != Outcome::Imported) r_.outcome = Outcome::Aborted; }
  Status query(std::uint64_t id, ImportResult& out) noexcept override {
    if (id != r_.operation) return Status::NotFound; out = r_; return Status::Ok;
  }
 private:
  ImportResult r_{}; SourceIdentity source_{}; char* body_ = nullptr;
};
SampleOwner owner;
PsramSnapshot allocator;
dx::ExportSession snapshot(owner, allocator, {BodyLimit});
auto& usb = s3::TinyUsbMsc::instance();
UsbExport exporter(snapshot, usb);
s3::PsramDisk disk;
s3::FatFsInbox filesystem(disk);
InboxSession inbox(filesystem, filesystem, usb);
ProbeSink sink;
ImportJob job(sink);
InboxCandidate first;
bool hasCandidate = false, importing = false, closing = false;
char requested = 0;
std::uint64_t operation = 0, lease = 0;
bool idle() {
  return (exporter.phase() == ExportPhase::Idle || exporter.phase() == ExportPhase::Closed) &&
    (inbox.phase() == InboxPhase::Idle || inbox.phase() == InboxPhase::Closed);
}
}
void setup() {
  Serial.begin(115200); // UART debug only; file transfer is the native USB MSC
  Serial.println("ISOLATED MODULE PROBE / DEVICE_NOT_RUN build artifact");
  Serial.printf("sizes session=%u export=%u inbox=%u job=%u fatfs=%u bridge=%u\n",
    unsigned(sizeof snapshot), unsigned(sizeof exporter), unsigned(sizeof inbox), unsigned(sizeof job), unsigned(sizeof filesystem), unsigned(sizeof(MediaBridge)));
  exporter.begin();
  while (exporter.phase() == ExportPhase::Preparing) exporter.tick();
  if (!usb.start()) Serial.println("USB start failed");
  Serial.println("UART: e export, i new inbox, n import first TXT into VOLATILE sink, c close. Software re-enumeration is requested between media sessions; physical USB remains DEVICE_NOT_RUN.");
}
void loop() {
  exporter.tick(); inbox.tick();
  if (closing && inbox.close() == Status::Ok && idle()) {
    disk.release(); usb.cancelPreparation(); closing = false;
  }
  if (requested && idle() && !closing) {
    const auto ready = usb.prepareSession(millis());
    if (ready == Status::Ok) {
      if (requested == 'e') exporter.begin();
      else if (disk.allocate() == Status::Ok) { hasCandidate = false; inbox.begin(); }
      requested = 0;
    } else if (ready != Status::Busy) {
      Serial.println("USB reconnect failed; reconnect native data cable"); requested = 0;
    }
  }
  if (inbox.phase() == InboxPhase::Inspecting) {
    InboxCandidate candidate;
    const auto r = inbox.next(candidate);
    if (r == Status::Ok && !hasCandidate) { first = candidate; hasCandidate = true; }
    if (r == Status::NotFound) Serial.printf("inbox candidates=%u\n", unsigned(inbox.result().candidates));
  }
  if (importing) {
    const auto p = job.tick();
    if (p == ImportPhase::Complete || p == ImportPhase::Failed || p == ImportPhase::Uncertain) {
      Serial.printf("volatile probe result=%u bytes=%u\n", unsigned(job.result().status), unsigned(job.result().bodyBytes));
      inbox.release(lease); importing = false;
    }
  }
  if (Serial.available()) {
    const auto c = Serial.read();
    if (c == 'c') {
      requested = 0; closing = true;
      job.cancel(); if (importing) inbox.release(lease); importing = false;
      exporter.close(); inbox.endHost(EndReason::Forced);
      if (inbox.phase() == InboxPhase::ImportReady || inbox.phase() == InboxPhase::Inspecting) inbox.close();
    } else if ((c == 'e' || c == 'i') && idle() && !closing) requested = c;
    else if (c == 'n' && hasCandidate && !importing) {
      ImportSource* source = nullptr;
      if (inbox.acquire(first, source) == Status::Ok) {
        lease = inbox.sourceLease(); ImportIntent i; i.operation = ++operation; i.source = first.source; i.name = first.name;
        if (job.begin(*source, i) == Status::Ok) importing = true;
        else inbox.release(lease);
      }
    }
  }
  delay(1);
}
