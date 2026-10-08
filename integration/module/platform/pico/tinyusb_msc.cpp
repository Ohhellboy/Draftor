#include "tinyusb_msc.hpp"
#include <hardware/sync.h>
#include <pico/mutex.h>
#include <device/dcd.h>
#include <cstring>
#include <limits>

extern mutex_t __usb_mutex; // pinned Arduino-Pico TinyUSB port's task mutex
namespace {
bool sense(draftor::usb::Status status) {
  using draftor::usb::Status;
  if (status==Status::Ok) return true;
  uint8_t key=SCSI_SENSE_MEDIUM_ERROR,asc=0x11;
  if (status==Status::ReadOnly) { key=SCSI_SENSE_DATA_PROTECT;asc=0x27; }
  else if (status==Status::Busy||status==Status::NotReady||status==Status::Stale) { key=SCSI_SENSE_NOT_READY;asc=0x3a; }
  else if (status==Status::Bounds||status==Status::InvalidArgument) { key=SCSI_SENSE_ILLEGAL_REQUEST;asc=0x21; }
  tud_msc_set_sense(0,key,asc,0);return false;
}
}
namespace draftor::usb::pico {
static_assert(std::atomic<unsigned>::is_always_lock_free,"Pico callback atomics must be lock-free");
TinyUsbMsc& TinyUsbMsc::instance() noexcept { static TinyUsbMsc endpoint;return endpoint; }
uint16_t TinyUsbMsc::getInterfaceDescriptor(uint8_t,uint8_t* out,uint16_t capacity) {
  if (!out) return TUD_MSC_DESC_LEN;
  if (capacity<TUD_MSC_DESC_LEN) return 0;
  const auto interface=TinyUSBDevice.allocInterface(1);
  const auto in=TinyUSBDevice.allocEndpoint(TUSB_DIR_IN),outEp=TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);
  const uint8_t descriptor[]={TUD_MSC_DESCRIPTOR(interface,_strid,outEp,in,64)};
  std::memcpy(out,descriptor,sizeof descriptor);return sizeof descriptor;
}
bool TinyUsbMsc::start() noexcept {
  if (started_) return true;
  setStringDescriptor("Draftor USB Export and Inbox");
  if (!TinyUSBDevice.isInitialized()) TinyUSBDevice.begin(0);
  if (!TinyUSBDevice.addInterface(*this)) return false;
  started_=true;
  TinyUSBDevice.detach(); reconnecting_=true;reconnectAt_=millis()+50;
  return true;
}
bool TinyUsbMsc::canPublish() const noexcept {
  return started_&&!reconnecting_&&pending_.load()==Pending::Empty&&
    inquiryEpoch_.load()==epoch_.load()&&(!used_||epoch_.load()>usedEpoch_);
}
Status TinyUsbMsc::publish(BlockDevice& disk,Access access,std::uint32_t& generation) noexcept {
  if (!canPublish()) return Status::NotReady;
  const auto fence=epoch_.load();
  const auto acknowledged=bridge_.acknowledgeIdle();
  if (acknowledged!=Status::Ok) return acknowledged;
  // Reset IRQ marks the bridge interrupted; publish's compare-exchange cannot
  // reopen an admission gate whose generation was interrupted mid-publication.
  if (fence!=epoch_.load()) return Status::NotReady;
  const auto status=bridge_.publish(disk,access,generation);
  if (status==Status::Ok) {used_=true;usedEpoch_=fence;}
  return status;
}
MediaState TinyUsbMsc::state() const noexcept {
  auto state=bridge_.state();state.drained=state.drained&&pending_.load()==Pending::Empty;return state;
}
Status TinyUsbMsc::detach() noexcept {
  if (pending_.load()!=Pending::Empty) return Status::Busy;
  const auto result=bridge_.detach();
  if (result!=Status::Ok) return result;
  // Soft disconnect is an SDK operation, never an invented reset. The next
  // publication still requires the host's actual bus-reset + inquiry fence.
  if (!reconnecting_) {
    if (!mutex_try_enter(&__usb_mutex,nullptr)) return Status::Busy;
    TinyUSBDevice.detach();mutex_exit(&__usb_mutex);
    reconnecting_=true;reconnectAt_=millis()+50;
  }
  return Status::Ok;
}
void TinyUsbMsc::reset() noexcept {
  const auto current=epoch_.load();
  if (current!=std::numeric_limits<uint32_t>::max()) epoch_.store(current+1);
  bridge_.withdraw(EndReason::Disconnected);
}
std::int32_t TinyUsbMsc::enqueue(bool write,uint32_t lba,uint32_t offset,void* buffer,uint32_t size) noexcept {
  const auto observedEpoch=epoch_.load();
  if (!buffer||!size||size>bounce_.size()||offset>=SectorBytes) {sense(Status::Bounds);return -1;}
  if (!bridge_.state().accepting) {sense(Status::NotReady);return -1;}
  if (write&&!bridge_.writable()) {sense(Status::ReadOnly);return -1;}
  const auto irq=save_and_disable_interrupts();
  auto expected=Pending::Empty;
  if (!pending_.compare_exchange_strong(expected,Pending::Filling)) {restore_interrupts(irq);return TUD_MSC_RET_BUSY;}
  // A reset between admission and this short copy must retain the old epoch.
  requestEpoch_=observedEpoch;generation_=bridge_.generation();
  offset_=std::uint64_t(lba)*SectorBytes+offset;bytes_=size;usbBuffer_=buffer;write_=write;
  if (write) std::memcpy(bounce_.data(),buffer,size);
  pending_.store(Pending::Queued,std::memory_order_release);restore_interrupts(irq);return TUD_MSC_RET_ASYNC;
}
bool TinyUsbMsc::synchronize(bool eject) noexcept {
  // Every successful WRITE10 has already completed FsFile::sync in service().
  // This SDK cannot asynchronously finish SYNCHRONIZE CACHE or START STOP.
  if (pending_.load()!=Pending::Empty) return sense(Status::Busy);
  const auto status=bridge_.state();
  if (!status.accepting) return sense(Status::NotReady);
  if (eject) bridge_.withdraw(EndReason::Ejected);
  return sense(status.error);
}
void TinyUsbMsc::service() noexcept {
  if (reconnecting_&&static_cast<int32_t>(millis()-reconnectAt_)>=0) {
    if (mutex_try_enter(&__usb_mutex,nullptr)) {
      TinyUSBDevice.attach();mutex_exit(&__usb_mutex);reconnecting_=false;
    }
  }
  auto expected=Pending::Queued;
  if (pending_.compare_exchange_strong(expected,Pending::Executing,std::memory_order_acquire)) {
    if (requestEpoch_!=epoch_.load()) completion_={Status::Stale,0};
    else if (write_) {
      completion_=bridge_.write(generation_,offset_,bounce_.data(),bytes_);
      if (completion_.status==Status::Ok) completion_.status=bridge_.sync(generation_);
    } else completion_=bridge_.read(generation_,offset_,bounce_.data(),bytes_);
    pending_.store(Pending::Completing,std::memory_order_release);
  }
  if (pending_.load()!=Pending::Completing||!mutex_try_enter(&__usb_mutex,nullptr)) return;
  // Service already queued class/config resets before touching a retained SDK
  // pointer. IRQ masking keeps the fixed SDK queue empty through async enqueue.
  // This adapter is serviced on Arduino core 0, as is its TinyUSB soft IRQ.
  const auto irq=save_and_disable_interrupts();
  tud_task_ext(0,false);
  const bool current=requestEpoch_==epoch_.load();
  bool completed=!current;
  if (current) {
    sense(completion_.status);
    // In pinned TinyUSB false means pending_io is absent, not queue capacity.
    // Retain Completing on rejection: never report drain or reuse the pointer.
    completed=tud_msc_async_io_done(completion_.status==Status::Ok?static_cast<int32_t>(completion_.bytes):static_cast<int32_t>(TUD_MSC_RET_ERROR),false);
    if (completed&&!write_&&completion_.status==Status::Ok) std::memcpy(usbBuffer_,bounce_.data(),bytes_);
  }
  if (completed) {usbBuffer_=nullptr;pending_.store(Pending::Empty,std::memory_order_release);}
  restore_interrupts(irq);
  // Complete in the same SDK task ownership period. A rejected completion is
  // retried on the next service tick after checking its epoch again.
  tud_task_ext(0,false);mutex_exit(&__usb_mutex);
}
}
namespace { auto& endpoint(){return draftor::usb::pico::TinyUsbMsc::instance();} }
extern "C" {
// GNU link wrappers observe actual SDK resets without editing the SDK. Bus
// events alone omit BOT reset and SET_CONFIGURATION's class reset.
bool __real_mscd_control_xfer_cb(uint8_t,uint8_t,tusb_control_request_t const*);
void __real_mscd_reset(uint8_t);
bool __wrap_mscd_control_xfer_cb(uint8_t port,uint8_t stage,tusb_control_request_t const* request) {
  if (stage==CONTROL_STAGE_SETUP&&request->bmRequestType_bit.type==TUSB_REQ_TYPE_CLASS&&
      request->bRequest==MSC_REQ_RESET&&request->wValue==0&&request->wLength==0) endpoint().reset();
  return __real_mscd_control_xfer_cb(port,stage,request);
}
void __wrap_mscd_reset(uint8_t port) {endpoint().reset();__real_mscd_reset(port);}
void tud_event_hook_cb(uint8_t,uint32_t event,bool) {
  if(event==DCD_EVENT_BUS_RESET||event==DCD_EVENT_UNPLUGGED) endpoint().reset();
}
uint8_t tud_msc_get_maxlun_cb(){return 1;}
void tud_msc_inquiry_cb(uint8_t,uint8_t vendor[8],uint8_t product[16],uint8_t revision[4]) {
  endpoint().inquiry();std::memcpy(vendor,"DRAFTOR ",8);std::memcpy(product,"PICO TRANSFER   ",16);std::memcpy(revision,"0012",4);
}
bool tud_msc_test_unit_ready_cb(uint8_t lun){return lun==0&&sense(endpoint().state().accepting?draftor::usb::Status::Ok:draftor::usb::Status::NotReady);}
void tud_msc_capacity_cb(uint8_t lun,uint32_t* count,uint16_t* size){*count=lun==0&&endpoint().state().accepting?endpoint().sectors():0;*size=*count?512:0;}
bool tud_msc_is_writable_cb(uint8_t lun){return lun==0&&endpoint().writable();}
int32_t tud_msc_read10_cb(uint8_t lun,uint32_t lba,uint32_t offset,void* p,uint32_t n){return lun?static_cast<int32_t>(TUD_MSC_RET_ERROR):endpoint().enqueue(false,lba,offset,p,n);}
int32_t tud_msc_write10_cb(uint8_t lun,uint32_t lba,uint32_t offset,uint8_t* p,uint32_t n){return lun?static_cast<int32_t>(TUD_MSC_RET_ERROR):endpoint().enqueue(true,lba,offset,p,n);}
bool tud_msc_start_stop_cb(uint8_t lun,uint8_t,bool start,bool eject){return lun==0&&endpoint().synchronize(eject&&!start);}
int32_t tud_msc_scsi_cb(uint8_t lun,const uint8_t command[16],void*,uint16_t){
  if(lun) return -1;
  if(command[0]==0x35) return endpoint().synchronize(false)?0:-1;
  if(command[0]==SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL) return endpoint().state().accepting?0:-1;
  tud_msc_set_sense(lun,SCSI_SENSE_ILLEGAL_REQUEST,0x20,0);return -1;
}
}
