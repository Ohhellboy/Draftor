#include "tinyusb_msc.hpp"
#include <pico/mutex.h>
#include <hardware/sync.h>
#include <array>
#include <cstdlib>
#include <iostream>
mutex_t __usb_mutex;
extern "C" bool __real_mscd_control_xfer_cb(uint8_t,uint8_t,tusb_control_request_t const*){return true;}
extern "C" void __real_mscd_reset(uint8_t){}
extern "C" bool __wrap_mscd_control_xfer_cb(uint8_t,uint8_t,tusb_control_request_t const*);
extern "C" void __wrap_mscd_reset(uint8_t);
void botReset(){tusb_control_request_t req{};req.bmRequestType_bit.type=TUSB_REQ_TYPE_CLASS;req.bRequest=MSC_REQ_RESET;__wrap_mscd_control_xfer_cb(0,CONTROL_STAGE_SETUP,&req);}
void busReset(){draftor::usb::pico::TinyUsbMsc::instance().reset();}
using namespace draftor::usb;
extern "C" {int32_t tud_msc_read10_cb(uint8_t,uint32_t,uint32_t,void*,uint32_t);int32_t tud_msc_write10_cb(uint8_t,uint32_t,uint32_t,uint8_t*,uint32_t);bool tud_msc_start_stop_cb(uint8_t,uint8_t,bool,bool);int32_t tud_msc_scsi_cb(uint8_t,const uint8_t[16],void*,uint16_t);}
#define CHECK(c) do{if(!(c)){std::cerr<<__LINE__<<": "#c"\n";std::exit(1);}}while(false)
class Disk final:public BlockDevice {public:std::array<uint8_t,1024> bytes{};unsigned reads=0,writes=0,syncs=0;bool resetOnRead=false,failSync=false;
 uint32_t sectorCount()const noexcept override{return 2;}
 IoResult read(uint64_t at,void*p,size_t n)noexcept override{++reads;if(resetOnRead)pico::TinyUsbMsc::instance().reset();if(at>bytes.size()||n>bytes.size()-at)return{Status::Bounds,0};std::memcpy(p,bytes.data()+at,n);return{Status::Ok,n};}
 IoResult write(uint64_t at,const void*p,size_t n)noexcept override{++writes;if(at>bytes.size()||n>bytes.size()-at)return{Status::Bounds,0};std::memcpy(bytes.data()+at,p,n);return{Status::Ok,n};}
 Status sync()noexcept override{++syncs;return failSync?Status::SyncFailed:Status::Ok;}
};
int main(){auto& endpoint=pico::TinyUsbMsc::instance();CHECK(endpoint.start());CHECK(!endpoint.canPublish());shim::now=100;endpoint.service();endpoint.reset();endpoint.inquiry();CHECK(endpoint.canPublish());
 Disk disk;uint32_t generation=0;CHECK(endpoint.publish(disk,Access::ReadOnly,generation)==Status::Ok);uint8_t buffer[512]{};
 CHECK(tud_msc_write10_cb(0,0,0,buffer,512)==-1);CHECK(shim::sense==SCSI_SENSE_DATA_PROTECT);CHECK(disk.writes==0);
 CHECK(tud_msc_read10_cb(0,0,512,buffer,1)==-1);CHECK(tud_msc_read10_cb(0,0,0,buffer,513)==-1);
 disk.bytes[17]=91;CHECK(tud_msc_read10_cb(0,0,17,buffer,20)==TUD_MSC_RET_ASYNC);CHECK(!endpoint.state().drained);CHECK(disk.reads==0);
 CHECK(tud_msc_read10_cb(0,0,0,buffer,1)==TUD_MSC_RET_BUSY);endpoint.service();CHECK(buffer[0]==91&&shim::lastCompletion==20);CHECK(endpoint.state().drained);
 CHECK(tud_msc_read10_cb(0,4,0,buffer,1)==TUD_MSC_RET_ASYNC);endpoint.service();CHECK(shim::lastCompletion==-1);
 CHECK(tud_msc_start_stop_cb(0,0,false,true));CHECK(!endpoint.state().accepting);CHECK(endpoint.detach()==Status::Ok);CHECK(!endpoint.canPublish());
 shim::now+=100;endpoint.service();CHECK(!endpoint.canPublish());endpoint.reset();endpoint.inquiry();CHECK(endpoint.canPublish());
 CHECK(endpoint.publish(disk,Access::ReadWrite,generation)==Status::Ok);buffer[0]=55;CHECK(tud_msc_write10_cb(0,0,3,buffer,1)==TUD_MSC_RET_ASYNC);buffer[0]=88;endpoint.service();CHECK(disk.bytes[3]==55&&disk.syncs==1);
 uint8_t command[16]={0x35};CHECK(tud_msc_scsi_cb(0,command,nullptr,0)==0);
 // Reset invalidates the retained SDK pointer before any I/O, and drain remains
 // false until the old async request has been safely retired.
 std::memset(buffer,0xa5,sizeof buffer);CHECK(tud_msc_read10_cb(0,0,0,buffer,512)==TUD_MSC_RET_ASYNC);endpoint.reset();CHECK(endpoint.detach()==Status::Busy);const auto done=shim::completions;endpoint.service();CHECK(buffer[0]==0xa5&&shim::completions==done);CHECK(endpoint.detach()==Status::Ok);
 shim::now+=100;endpoint.service();endpoint.reset();endpoint.inquiry();CHECK(endpoint.publish(disk,Access::ReadWrite,generation)==Status::Ok);
 disk.resetOnRead=true;CHECK(tud_msc_read10_cb(0,0,0,buffer,512)==TUD_MSC_RET_ASYNC);endpoint.service();CHECK(buffer[0]==0xa5&&shim::completions==done);CHECK(endpoint.detach()==Status::Ok);
 shim::now+=100;endpoint.service();endpoint.reset();endpoint.inquiry();CHECK(endpoint.publish(disk,Access::ReadWrite,generation)==Status::Ok);disk.failSync=true;
 CHECK(tud_msc_write10_cb(0,0,0,buffer,1)==TUD_MSC_RET_ASYNC);endpoint.service();CHECK(shim::lastCompletion==-1&&endpoint.state().error==Status::SyncFailed);CHECK(!tud_msc_start_stop_cb(0,0,false,true));CHECK(endpoint.detach()==Status::Ok);
 // Deterministic IRQ boundary: reset after admission, before request capture.
 disk.resetOnRead=false;disk.failSync=false;
 shim::now+=100;endpoint.service();endpoint.reset();endpoint.inquiry();CHECK(endpoint.publish(disk,Access::ReadWrite,generation)==Status::Ok);
 shim::beforeDisable=busReset;const auto admissions=shim::completions,reads=disk.reads;
 CHECK(tud_msc_read10_cb(0,0,0,buffer,512)==TUD_MSC_RET_ASYNC);endpoint.service();CHECK(shim::completions==admissions&&disk.reads==reads);CHECK(endpoint.detach()==Status::Ok);
 // Rejected SDK completion retains pending state and the SDK pointer untouched.
 shim::now+=100;endpoint.service();endpoint.reset();endpoint.inquiry();CHECK(endpoint.publish(disk,Access::ReadWrite,generation)==Status::Ok);
 std::memset(buffer,0x9b,sizeof buffer);shim::rejectCompletion=true;CHECK(tud_msc_read10_cb(0,0,0,buffer,512)==TUD_MSC_RET_ASYNC);endpoint.service();CHECK(!endpoint.state().drained&&buffer[0]==0x9b&&endpoint.detach()==Status::Busy);shim::rejectCompletion=false;endpoint.service();CHECK(endpoint.state().drained&&shim::completions==admissions+1);
 // Actual BOT reset processed in the queued SDK task invalidates old read data.
 std::memset(buffer,0x6d,sizeof buffer);CHECK(tud_msc_read10_cb(0,0,0,buffer,512)==TUD_MSC_RET_ASYNC);shim::beforeTask=botReset;endpoint.service();CHECK(buffer[0]==0x6d&&shim::completions==admissions+1&&!endpoint.state().accepting);CHECK(endpoint.detach()==Status::Ok);
 // SET_CONFIGURATION class reset invalidates a queued write before disk access.
 shim::now+=100;endpoint.service();endpoint.reset();endpoint.inquiry();CHECK(endpoint.publish(disk,Access::ReadWrite,generation)==Status::Ok);const auto writes=disk.writes;
 CHECK(tud_msc_write10_cb(0,0,0,buffer,512)==TUD_MSC_RET_ASYNC);__wrap_mscd_reset(0);endpoint.service();CHECK(disk.writes==writes&&shim::completions==admissions+1);CHECK(endpoint.detach()==Status::Ok);
 std::cout<<"PASS actual Pico adapter callbacks with SDK shim (not USB enumeration)\n";}
