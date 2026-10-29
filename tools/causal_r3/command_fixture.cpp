#include "usb_causal_probe.hpp"
#include "runtime_findings_diag.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
namespace rd=s3app::runtime_diag;
namespace s3app::runtime_diag {std::vector<Stage> stages;void mark(Stage s,uint32_t)noexcept{stages.push_back(s);}}
static bool integrity=true,allocate=true,heapIntegrityStopped=false,entryAllowed=true,documentValid=true,composition=false;
static unsigned captures=0,allocations=0,dispatches=0,wifiCalls=0;
struct App {bool rename=false,files=false,dirtyDisplay=false;std::string notice;bool renameOpen()const{return rename;}bool filesOpen()const{return files;}} object;App* app=&object;
namespace s3app {
#include "entry_actual.inc"
namespace stability {enum class Kind{UsbBefore,UsbDenied,UsbReady,UsbPhase};template<class... T> bool capture(Kind k,T...){++captures;return k!=Kind::UsbBefore||integrity;}}
}
struct Transfer {unsigned stage=0;unsigned phase()const{return stage;}bool key(const s3hid::Event& e,bool composing){++dispatches;if(!entryAllowed||!documentValid||composing)return true;if(e.down&&e.usage==66&&!e.mods)stage=1;if(e.down&&e.usage==41&&!e.mods)stage=0;return true;}};
struct UsbRuntime{Transfer transfer;} runtime;UsbRuntime* usbRuntime=nullptr;bool usbInput(){return composition;}
namespace s3app {bool ensureUsbRuntime(UsbRuntime*& p){++allocations;if(!allocate)return false;p=&runtime;return true;}}
#include "usb_actual.inc"
bool wifiRuntime=false;
bool wifiKey(const s3hid::Event&){++wifiCalls;return false;}
namespace hidprobe {std::vector<unsigned> actions;void causal(unsigned a,uint32_t)noexcept{actions.push_back(a);}}
#include "causal_actual.inc"
static void reset(){integrity=allocate=entryAllowed=documentValid=true;composition=heapIntegrityStopped=false;captures=allocations=dispatches=wifiCalls=0;usbRuntime=nullptr;runtime={};object={};wifiRuntime=false;rd::stages.clear();hidprobe::actions.clear();}
int main(){
 s3hid::Event alt{13,100,24,3,true,4,5,6,7,800,9,10};
 auto mapped=usb_causal::originalUsbCommand(alt);assert(mapped.usage==66&&!mapped.mods&&mapped.epoch==13&&mapped.route_epoch==4&&mapped.ime_frame==5&&mapped.ime_attempt==6&&mapped.event_id==7&&mapped.received_us==800&&mapped.ime_context==9&&mapped.resource_owner==10);
 reset();assert(usbCausalKey(alt)&&captures>=3&&allocations==1&&dispatches==1&&runtime.transfer.stage==1);s3hid::Event esc=alt;esc.usage=41;esc.mods=0;assert(usbCausalKey(esc)&&runtime.transfer.stage==0);puts("PASS actual wrapper -> unchanged usbKey -> original entry and allocation path, Esc return; event identity retained");
 reset();s3hid::Event f9=alt;f9.usage=66;f9.mods=0;assert(usbCausalKey(f9)&&captures==0&&allocations==0&&dispatches==0&&wifiCalls==0);f9.down=false;assert(usbCausalKey(f9)&&captures==0);puts("PASS physical HID F9 down/up records only at application command boundary");
 reset();integrity=false;assert(usbCausalKey(alt)&&heapIntegrityStopped&&allocations==0&&dispatches==0);assert(rd::stages.back()==rd::Stage::IntegrityFail);puts("PASS integrity failure latches original stop, no allocation/dispatch");
 reset();allocate=false;assert(usbCausalKey(alt)&&!heapIntegrityStopped&&allocations==1&&!dispatches&&object.notice=="USB unavailable: memory low");puts("PASS allocation denial retains original error path");
 reset();object.rename=true;assert(!usbCausalKey(alt)&&captures==0&&dispatches==0);puts("PASS rename admission rejects alternate command");
 reset();wifiRuntime=true;assert(usbCausalKey(alt)&&captures==0&&dispatches==0&&object.notice=="Turn Wi-Fi off before USB transfer");puts("PASS Wi-Fi exclusion preserved");
 reset();documentValid=false;assert(usbCausalKey(alt)&&dispatches==1&&runtime.transfer.stage==0);reset();composition=true;assert(usbCausalKey(alt)&&dispatches==1&&runtime.transfer.stage==0);puts("PASS original transfer dispatch still owns document/composition decisions (stubbed seam; real transfer source unchanged)");
 for(unsigned mods=0;mods<256;++mods){alt.mods=uint8_t(mods);const bool expected=(mods&0x11)&&(mods&0x22)&&!(mods&0xcc);assert((usb_causal::classify(alt)==usb_causal::AlternateUsb)==expected);}puts("PASS exact modifier mask; plain U/Ctrl-U/Alt/GUI not remapped");
}
