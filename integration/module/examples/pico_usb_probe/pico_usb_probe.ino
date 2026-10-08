// Minimal Pico platform probe: real MSC + dedicated TF image + real SdFat.
// No editor, IME, document service or publication is claimed by this firmware.
#include "features.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_TinyUSB.h>
#include <DraftorUsb.h>
#include "tinyusb_msc.hpp"
#include "file_image_disk.hpp"
#include "sdfat_inbox.hpp"
#include "draftor_usb_transfer/export.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include <cstdlib>
#include <algorithm>
#include <cstring>
namespace {
using namespace draftor::usb;
namespace dx=draftor::exchange;
constexpr std::size_t PicoBodyLimit=65536;
#if DRAFTOR_PICO_EXPORT
class FixtureOwner final:public dx::OwnerPort {
 public:
  const char* body=u8"Draftor Pico 2 W USB fixture\n简体 繁體 🙂 é\n";
  bool frozen=false;
  dx::OwnerState probe() noexcept override {dx::OwnerState s;s.has_current=true;s.body_bytes=std::strlen(body);s.identity.collection[0]=1;s.identity.document[0]=2;s.identity.mount=s.identity.document_session=s.identity.revision_epoch=1;return s;}
  bool freeze() noexcept override {if(frozen)return false;frozen=true;return true;}
  void unfreeze() noexcept override{frozen=false;}
  dx::SaveReply beginAutoSave() noexcept override{return{};}dx::SaveReply serviceSave() noexcept override{return{};}
  dx::BorrowReply borrow(size_t at,size_t n,void* context,dx::BorrowConsumer consumer) noexcept override {consumer(context,{body+at,n});return{true,n};}
};
class SnapshotAllocator final:public dx::Allocator {
 public:char* allocate(size_t n) noexcept override{return static_cast<char*>(std::malloc(n));}
 void deallocate(char* p,size_t) noexcept override{std::free(p);}
};
FixtureOwner owner;SnapshotAllocator allocator;dx::ExportSession snapshot(owner,allocator,{PicoBodyLimit});
#endif
#if DRAFTOR_PICO_EXPORT || DRAFTOR_PICO_INBOX
auto& usb=pico::TinyUsbMsc::instance();
#endif
#if DRAFTOR_PICO_EXPORT
UsbExport exporter(snapshot,usb);
#endif
#if DRAFTOR_PICO_INBOX
SdFs sd;pico::FileImageDisk image;pico::SdFatInbox filesystem(image,{PicoBodyLimit});InboxSession inbox(filesystem,filesystem,usb,{PicoBodyLimit});
InboxCandidate first{};bool haveCandidate=false,sdReady=false,creating=false,closeRequested=false;
#else
constexpr bool creating=false;
#endif
char requested=0;
bool idle(){
#if DRAFTOR_PICO_EXPORT
 if(exporter.phase()!=ExportPhase::Idle&&exporter.phase()!=ExportPhase::Closed)return false;
#endif
#if DRAFTOR_PICO_INBOX
 if(inbox.phase()!=InboxPhase::Idle&&inbox.phase()!=InboxPhase::Closed)return false;
#endif
 return true;
}
#if DRAFTOR_PICO_INBOX
void closeInboxWhenDrained(){
 if(inbox.close()==Status::Ok){image.close();closeRequested=false;haveCandidate=false;}
}
void validateFirst(){
 if(!haveCandidate)return;
 ImportSource* source=nullptr;if(inbox.acquire(first,source)!=Status::Ok)return;
 const auto lease=inbox.sourceLease();
 if(!source->confirmed()){Serial1.println("UNCONFIRMED_RECEIPT: recovery bytes only");inbox.release(lease);return;}
 dx::ImportWire parser(PicoBodyLimit);char input[256],output[256];size_t at=0;bool okay=true;
 while(at<first.source.size&&okay){const auto wanted=std::min<size_t>(sizeof input,first.source.size-at);auto r=source->read(at,input,wanted);if(r.status!=Status::Ok){okay=false;break;}
 size_t consumed=0;while(consumed<r.bytes){auto step=parser.feed({input+consumed,r.bytes-consumed},{output,sizeof output});consumed+=step.consumed;if(step.flow==dx::Flow::Error){okay=false;break;}}at+=r.bytes;usb.service();}
 if(okay){auto step=parser.finish({output,sizeof output});okay=step.flow==dx::Flow::Done;}
 Serial1.printf("TXT_VALIDATED_ONLY=%u bytes=%u; no product document created\n",okay,unsigned(parser.outputBytes()));inbox.release(lease);
}
#endif
}
void setup(){
 Serial1.setTX(0);Serial1.setRX(1);Serial1.begin(115200);
 Serial1.println("DRAFTOR_PICO2W_USB_EXPORT_INBOX_V12_20261003 / DEVICE_NOT_RUN build artifact");
 Serial1.printf("features export=%u inbox=%u body=%u wire=%u\n",unsigned(DRAFTOR_PICO_EXPORT),unsigned(DRAFTOR_PICO_INBOX),unsigned(PicoBodyLimit),unsigned(2*PicoBodyLimit+3));
#if DRAFTOR_PICO_EXPORT || DRAFTOR_PICO_INBOX
 Serial1.printf("endpoint=%u\n",unsigned(sizeof usb));
 if(!usb.start())Serial1.println("USB start failed");
#endif
#if DRAFTOR_PICO_EXPORT
 Serial1.printf("snapshot=%u exporter=%u\n",unsigned(sizeof snapshot),unsigned(sizeof exporter));
#endif
#if DRAFTOR_PICO_INBOX
 Serial1.printf("inbox=%u filesystem=%u image=%u image_bytes=%u\n",unsigned(sizeof inbox),unsigned(sizeof filesystem),unsigned(sizeof image),unsigned(pico::FileImageDisk::Bytes));
 pinMode(17,OUTPUT);digitalWrite(17,HIGH); // legacy display CS shares SPI0; keep inactive
 SPI.setRX(16);SPI.setTX(19);SPI.setSCK(18);sdReady=sd.begin(SdSpiConfig(22,SHARED_SPI,SD_SCK_MHZ(4),&SPI));
 Serial1.printf("TF mounted=%u; private image creation requires i command\n",sdReady);
#endif
 Serial1.println("UART e=export fixture, i=new private Inbox image, v=validate first TXT after eject, c=close/cancel. Received images persist; never auto-deleted.");
}
void loop(){
#if DRAFTOR_PICO_EXPORT || DRAFTOR_PICO_INBOX
 usb.service();
#endif
#if DRAFTOR_PICO_EXPORT
 exporter.tick();
#endif
#if DRAFTOR_PICO_INBOX
 inbox.tick();
 if(closeRequested)closeInboxWhenDrained();
 if(inbox.phase()==InboxPhase::Inspecting){InboxCandidate c;const auto result=inbox.next(c);if(result==Status::Ok){Serial1.printf("TXT %s %u bytes\n",c.name.data(),unsigned(c.source.size));if(!haveCandidate){first=c;haveCandidate=true;}}else if(result==Status::NotFound)Serial1.printf("sealed candidates=%u end=%u\n",unsigned(inbox.result().candidates),unsigned(inbox.result().end));}
#endif
 if(Serial1.available()){
  const char c=Serial1.read();
  if(c=='c'){
   requested=0;
#if DRAFTOR_PICO_EXPORT
   exporter.close();
#endif
#if DRAFTOR_PICO_INBOX
   if(creating){image.close();creating=false;}
   closeRequested=true;closeInboxWhenDrained();
#endif
  } else if((DRAFTOR_PICO_EXPORT&&c=='e')||(DRAFTOR_PICO_INBOX&&c=='i')){
   if(idle()&&!creating&&!requested){requested=c;Serial1.println("waiting for actual reset/inquiry fence");}
   else Serial1.println("BUSY: finish current transfer");
  }
#if DRAFTOR_PICO_INBOX
  else if(c=='v')validateFirst();
#endif
 }
#if DRAFTOR_PICO_INBOX
 if(creating){const auto status=image.advanceCreate();if(status!=Status::Busy){creating=false;Serial1.printf("image prepared=%u path=%s\n",unsigned(status),image.path());if(status==Status::Ok)requested='I';else image.close();}}
#endif
#if DRAFTOR_PICO_EXPORT || DRAFTOR_PICO_INBOX
 if(requested&&idle()&&usb.canPublish()){
  const char next=requested;requested=0;
#if DRAFTOR_PICO_EXPORT
  if(next=='e')Serial1.printf("export begin=%u\n",unsigned(exporter.begin()));
#endif
#if DRAFTOR_PICO_INBOX
  if(next=='I'){haveCandidate=false;Serial1.printf("inbox begin=%u\n",unsigned(inbox.begin()));}
  else if(next=='i'&&sdReady){image.close();const auto created=image.beginCreate(sd);creating=created==Status::Ok;Serial1.printf("image begin=%u path=%s\n",unsigned(created),image.path());}
#endif
 }
#endif
 delay(1);
}
