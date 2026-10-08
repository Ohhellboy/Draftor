#include <Arduino.h>
#include <DraftorUsb.h>
#include "tinyusb_msc.hpp"
#include "fatfs_inbox.hpp"
#include "esp_heap_caps.h"
#include <cstring>
using namespace draftor::usb;
namespace {
// VOLATILE diagnostic transaction sink only; never linked into the product.
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
auto& usb=s3::TinyUsbMsc::instance();s3::PsramDisk disk;s3::FatFsInbox filesystem(disk);
InboxSession inbox(filesystem,filesystem,usb);ProbeSink sink;ImportJob job(sink);InboxCandidate candidate;
bool requested=true,hasCandidate=false,importing=false,closing=false;std::uint64_t lease=0,operation=0;
}
void setup(){Serial.begin(115200);usb.start("Draftor import-only diagnostic");}
void loop(){
 if(requested&&(inbox.phase()==InboxPhase::Idle||inbox.phase()==InboxPhase::Closed)){
  const auto s=usb.prepareSession(millis());
  if(s==Status::Ok){if(disk.allocate()==Status::Ok)inbox.begin();requested=false;hasCandidate=false;}else if(s!=Status::Busy)requested=false;
 }
 inbox.tick();
 if(closing&&inbox.close()==Status::Ok){disk.release();usb.cancelPreparation();closing=false;}
 if(inbox.phase()==InboxPhase::Inspecting){InboxCandidate next;if(inbox.next(next)==Status::Ok&&!hasCandidate){candidate=next;hasCandidate=true;}}
 if(importing){const auto p=job.tick();if(p==ImportPhase::Complete||p==ImportPhase::Failed||p==ImportPhase::Uncertain){inbox.release(lease);importing=false;}}
 if(Serial.available()){
  const auto c=Serial.read();if(c=='i'&&inbox.phase()==InboxPhase::Closed)requested=true;
  else if(c=='n'&&hasCandidate&&!importing){ImportSource* source=nullptr;if(inbox.acquire(candidate,source)==Status::Ok){lease=inbox.sourceLease();ImportIntent intent;intent.operation=++operation;intent.source=candidate.source;intent.name=candidate.name;if(job.begin(*source,intent)==Status::Ok)importing=true;else inbox.release(lease);}}
  else if(c=='c'){requested=false;job.cancel();if(importing)inbox.release(lease);importing=false;inbox.endHost(EndReason::Forced);closing=true;}
 }
 delay(1);
}
