#include <Arduino.h>
#include <DraftorUsb.h>
#include "tinyusb_msc.hpp"
#include "esp_heap_caps.h"
#include <cstring>
using namespace draftor::usb;
namespace dx = draftor::exchange;
namespace {
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
SampleOwner owner;PsramSnapshot allocator;
dx::ExportSession snapshot(owner,allocator,{BodyLimit});
auto& usb=s3::TinyUsbMsc::instance();UsbExport exporter(snapshot,usb);bool requested=false;
}
void setup(){Serial.begin(115200);exporter.begin();while(exporter.phase()==ExportPhase::Preparing)exporter.tick();usb.start("Draftor export-only diagnostic");}
void loop(){
 exporter.tick();
 if(requested&&(exporter.phase()==ExportPhase::Closed||exporter.phase()==ExportPhase::Idle)){
  const auto s=usb.prepareSession(millis());if(s==Status::Ok){exporter.begin();requested=false;}else if(s!=Status::Busy)requested=false;
 }
 if(Serial.available()){const auto c=Serial.read();if(c=='e')requested=true;else if(c=='c'){requested=false;exporter.close();usb.cancelPreparation();}}
 delay(1);
}
