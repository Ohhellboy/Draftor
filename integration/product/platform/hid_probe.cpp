#include "hanime_hid_probe.hpp"
#if DRAFTOR_WS_HID_DIAG
#include "hid_probe_core.hpp"
#include "board_config.hpp"
#include <esp_attr.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <hal/usb_serial_jtag_ll.h>
static_assert(draftor::board::Waveshare,"HID R2 diagnostic is Waveshare only");
namespace hidprobe {
DRAM_ATTR Core core;
DRAM_ATTR portMUX_TYPE transportMux=portMUX_INITIALIZER_UNLOCKED;
static uint32_t now()noexcept{return uint32_t(esp_timer_get_time()/1000);}
void inc(Stat s)noexcept{core.add(s);}
void hostEvent(unsigned type,bool entering)noexcept{
 core.add(entering?HostEntered:HostExited);core.set(HostType,type);core.set(HostPointMs,now());core.set(HostPoint,entering?1:2);
}
void notifyGate(uint16_t actual,uint16_t expected,bool connected,uint16_t handle,uint32_t flags)noexcept{
 core.add(NotifySeen);core.set(ActualConn,actual);core.set(ExpectedConn,expected);core.set(NotifyHandle,handle);core.set(ClientFlags,flags);
 (void)connected; // rejection counts are incremented in the actual return branches.
}
void discovery()noexcept{core.beginDiscovery();}
void inputSubscription(uint16_t h,uint16_t cccd,uint8_t id,bool keyboard,InputStatus status,size_t n,uint16_t value)noexcept{core.inputSubscription(h,cccd,id,keyboard,status,n,value);}
void setupDone(SetupStatus status,uint16_t handle,uint32_t e)noexcept{core.setupDone(status,handle,e);}
void bind(uint32_t epoch)noexcept{core.bindEpoch(epoch);}
void causal(unsigned action,uint32_t value)noexcept{core.event(33,uint8_t(action),value,now());}

void point(Point p)noexcept{core.notePoint(p,now());if(p==Stopped)core.set(HeapStop,1);}
void rxPoint(Rx p,uint16_t h)noexcept{core.noteRx(p,h,now());}
void raw(uint16_t h,const uint8_t* b,size_t n,size_t total,size_t copied)noexcept{core.raw(h,b,n,total,now(),copied);}
void map(const uint8_t* b,size_t n,bool parsed)noexcept{core.map(b,n,parsed);}
void reference(uint16_t h,uint8_t id,uint8_t type,uint16_t bits,bool selected)noexcept{core.reference(h,id,type,bits,selected);}
void key(uint8_t u,uint8_t m,bool down)noexcept{const auto v=uint32_t(u)|(uint32_t(m)<<8)|(uint32_t(down)<<16);core.set(LastKey,v);core.event(4,0,v,now());}
void keyDone()noexcept{core.add(KeyDone);core.event(5,0,core.stats[LastKey].load(),now());}
void r1(unsigned stage,uint32_t v)noexcept{if(stage<=22)core.event(32,uint8_t(stage),v,now());}
void monitor(unsigned phase,uint32_t epoch,bool accepting)noexcept{
 if(!core.running&&!accepting&&!core.metadataReady)return; // Start once input admission is enabled; continue through existing reconnect turns.
 core.set(BleState,uint32_t(phase)|(uint32_t(accepting)<<8));core.set(Epoch,epoch);
 core.service(now(),[](const uint8_t* b,size_t n){
  // This lock protects only a bounded hardware FIFO write against PHY handoff.
  // It is never stateMux, a queue lock, heap inspection, or a wait-for-space loop.
  portENTER_CRITICAL(&transportMux);size_t sent=0;
  if(core.enabled.load()){sent=usb_serial_jtag_ll_write_txfifo(b,n);if(sent)usb_serial_jtag_ll_txfifo_flush();}
  portEXIT_CRITICAL(&transportMux);return sent;
 });
}
void stopTransport()noexcept{portENTER_CRITICAL(&transportMux);core.enabled=false;portEXIT_CRITICAL(&transportMux);}
}
extern "C" __attribute__((used)) const uint32_t draftor_hid_r2_static_cost[]={sizeof(hidprobe::Core),sizeof(hidprobe::transportMux),hidprobe::Core::RawLimit,hidprobe::Core::WindowMs,hidprobe::Core::PacketLimit};
#endif
