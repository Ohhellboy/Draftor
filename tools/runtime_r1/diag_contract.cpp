#include "runtime_findings_diag.hpp"
#include "operation_profile.hpp"
#include "io_profile.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace rd=s3app::runtime_diag;
static unsigned writes=0,flushes=0,used=0,limit=16;static bool forbid=false;
static uint8_t wire[20000]{};static int64_t timeUs=0;
void* operator new(size_t n){assert(!forbid);auto* p=std::malloc(n);if(!p)throw std::bad_alloc();return p;}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
int64_t esp_timer_get_time(){return ++timeUs;}
uint32_t usb_serial_jtag_ll_write_txfifo(const uint8_t* p,uint32_t n){assert(n<=16);++writes;if(n>limit)n=limit;assert(used+n<=sizeof(wire));std::memcpy(wire+used,p,n);used+=n;return n;}
void usb_serial_jtag_ll_txfifo_flush(){++flushes;}
int main(){
 forbid=true;limit=0;rd::f9();rd::mark(rd::Stage::IntegrityFail);auto initial=writes;rd::service();assert(writes==initial+1&&used==0&&flushes==0);
 // Drain the real stop-path service in three-byte fragments, with allocation forbidden.
 limit=3;for(unsigned i=0;i<32;++i)rd::service();assert(used==32);
 rd::Record first{},second{};std::memcpy(&first,wire,16);std::memcpy(&second,wire+16,16);assert(first.stage==rd::Stage::F9Pop&&second.stage==rd::Stage::IntegrityFail);
 limit=16;rd::tickBegin(1);rd::tickEnd(1);auto tickWrites=writes;rd::tickBegin(1);rd::tickEnd(1);assert(writes==tickWrites);
 rd::operationBegin();auto& samples=ime_device::ioProfile.samples;
 samples[unsigned(ime_device::IoKind::DocumentRead)][0].add(8,123,true);
 samples[unsigned(ime_device::IoKind::DocumentWrite)][0].add(8,234,true);
 samples[unsigned(ime_device::IoKind::DocumentSync)][0].add(0,345,true);
 s3profile::Record op;op.operation="trash";op.total_us=4000;op.calls[s3profile::Stat].us=456;op.us[s3profile::Verify]=567;rd::operationEnd(op);rd::lcd(678,true);
 bool read=false,write=false,sync=false,other=false,lcd=false;
 for(unsigned i=0;i<used;i+=16){rd::Record r;std::memcpy(&r,wire+i,16);assert(r.magic==0x31524457);if(r.stage==rd::Stage::VfsReadUs){assert(r.value==123);read=true;}if(r.stage==rd::Stage::VfsWriteUs){assert(r.value==234);write=true;}if(r.stage==rd::Stage::VfsSyncUs){assert(r.value==345);sync=true;}if(r.stage==rd::Stage::VfsOtherUs){assert(r.value==456);other=true;}if(r.stage==rd::Stage::LcdUs){assert(r.value==678);lcd=true;}}
 assert(read&&write&&sync&&other&&lcd);
 rd::endTransport();initial=writes;
 for(unsigned n=0;n<100;++n){rd::mark(rd::Stage::UsbKey);rd::heartbeat();rd::service();rd::operationEnd(op);}
 assert(writes==initial);forbid=false;
 std::puts("PASS actual diagnostic implementation: no heap use; blocked/partial FIFO bounded; separated VFS/LCD values; no USB-JTAG writes after MSC handoff. Hardware transport NOT_TESTED.");
}
