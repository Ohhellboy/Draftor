#include "runtime_findings_diag.hpp"
#if DRAFTOR_WS_RUNTIME_DIAG
#include "hanime_hid_probe.hpp"
#if DRAFTOR_WS_HID_DIAG
namespace s3app::runtime_diag {
void mark(Stage s,uint32_t v)noexcept{hidprobe::r1(unsigned(s),v);}
void service()noexcept{} void f9()noexcept{mark(Stage::F9Pop);}
unsigned lastTickPhase=UINT32_MAX;bool recordingTick=false;
void heartbeat()noexcept{} void tickBegin(unsigned p)noexcept{recordingTick=p!=lastTickPhase;if(recordingTick){lastTickPhase=p;mark(Stage::TickBegin,p);}}
void tickEnd(unsigned p)noexcept{if(recordingTick){mark(Stage::TickEnd,p);recordingTick=false;}}
void endTransport()noexcept{hidprobe::stopTransport();}
void operationBegin()noexcept{} void operationEnd(const s3profile::Record&)noexcept{}
void lcd(uint32_t,bool)noexcept{}
}
#else
#include "board_config.hpp"
#include "operation_profile.hpp"
#include "io_profile.hpp"
#include <esp_attr.h>
#include <esp_timer.h>
#include <hal/usb_serial_jtag_ll.h>
static_assert(draftor::board::Waveshare,"R1 experiment belongs only to Waveshare");
namespace s3app::runtime_diag {
// Internal DRAM only, including the intentional heap-stop path. No allocator,
// PSRAM, mutex, Arduino Serial, USB driver installation, or filesystem access.
DRAM_ATTR Ring ring;
DRAM_ATTR bool transport=true,seenF9=false;
DRAM_ATTR uint32_t lastBeat=0,beats=0,operations=0,lcdSamples=0;
struct Io {uint64_t us=0,calls=0;};
DRAM_ATTR Io before[3];
DRAM_ATTR unsigned lastTickPhase=UINT32_MAX;
DRAM_ATTR bool recordingTick=false;
static uint32_t now() noexcept {return uint32_t(esp_timer_get_time());}
void service() noexcept {
 if(!transport)return;
 ring.service([](const uint8_t* data,size_t size){
  const auto n=usb_serial_jtag_ll_write_txfifo(data,size);
  if(n)usb_serial_jtag_ll_txfifo_flush();
  return size_t(n);
 });
}
void mark(Stage stage,uint32_t value) noexcept {
 if(!transport)return;
 ring.push(stage,now(),value);service(); // <= one 16-byte FIFO write, no wait
}
void f9() noexcept {seenF9=true;mark(Stage::F9Pop);}
void heartbeat() noexcept {
 service();const auto t=now();
 if(seenF9&&beats<8&&uint32_t(t-lastBeat)>=1000000){lastBeat=t;mark(Stage::Heartbeat,++beats);}
}
void tickBegin(unsigned phase) noexcept {
 recordingTick=seenF9&&phase!=lastTickPhase;
 if(recordingTick){lastTickPhase=phase;mark(Stage::TickBegin,phase);}
}
void tickEnd(unsigned phase) noexcept {if(recordingTick){mark(Stage::TickEnd,phase);recordingTick=false;}}
void endTransport() noexcept {
 mark(Stage::TransportEnd);transport=false; // BEFORE TinyUSB/MSC takes the PHY
}
static Io io(unsigned k) noexcept {
 Io total;for(const auto& s:ime_device::ioProfile.samples[unsigned(ime_device::IoKind::DocumentRead)+k]){total.us+=s.us;total.calls+=s.calls;}return total;
}
void operationBegin() noexcept {if(operations<10)for(unsigned k=0;k<3;++k)before[k]=io(k);}
void operationEnd(const s3profile::Record& r) noexcept {
 if(operations++>=10)return;
 uint32_t name=0;for(unsigned i=0;i<4&&r.operation[i];++i)name|=uint32_t(uint8_t(r.operation[i]))<<(8*i);
 mark(Stage::OperationBegin,name);mark(Stage::OperationTotalUs,uint32_t(r.total_us));
 for(unsigned k=0;k<3;++k){const auto after=io(k);mark(Stage(unsigned(Stage::VfsReadUs)+k),uint32_t(after.us-before[k].us));mark(Stage(unsigned(Stage::VfsReadCalls)+k),uint32_t(after.calls-before[k].calls));}
 uint64_t other=0;for(unsigned i=0;i<s3profile::CallCount;++i)if(i!=s3profile::Fsync)other+=r.calls[i].us;
 mark(Stage::VfsOtherUs,uint32_t(other));mark(Stage::VerifyUs,uint32_t(r.us[s3profile::Verify]));mark(Stage::InstallUs,uint32_t(r.us[s3profile::Install]));mark(Stage::LayoutUs,uint32_t(r.us[s3profile::Layout]));mark(Stage::DrawUs,uint32_t(r.us[s3profile::Draw]));
}
void lcd(uint32_t elapsed,bool success) noexcept {if(lcdSamples++<32){mark(Stage::LcdUs,elapsed);mark(Stage::LcdResult,success);}}
}
#endif

#endif // DRAFTOR_WS_HID_DIAG branch
