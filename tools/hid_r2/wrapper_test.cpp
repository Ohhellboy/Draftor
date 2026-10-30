#include "hid_probe_core.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
unsigned probe_lock_held=0;
static bool forbid=false;static uint64_t clockUs=0;static uint8_t wire[65536];static size_t used=0;static unsigned calls=0;
void* operator new(std::size_t n){assert(!forbid);if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void operator delete(void* p)noexcept{std::free(p);}void* operator new[](size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
int64_t esp_timer_get_time(){return clockUs;}
uint32_t usb_serial_jtag_ll_write_txfifo(const uint8_t* p,uint32_t n){assert(probe_lock_held==1&&n<=16);assert(used+n<=sizeof(wire));std::memcpy(wire+used,p,n);used+=n;++calls;return n;}
void usb_serial_jtag_ll_txfifo_flush(){assert(probe_lock_held==1);}
namespace hidprobe{extern Core core;}
int main(){using namespace hidprobe;
 forbid=true;monitor(0,1,false);assert(used==0&&!core.running);clockUs=50000000;monitor(5,2,true);assert(core.running&&core.started==50000&&calls==4);puts("PASS actual monitor waits for first input admission and writes at most four packets");
 for(unsigned i=0;i<60;++i){clockUs+=50000;calls=0;monitor(6,3,false);assert(calls<=4&&probe_lock_held==0);}assert(core.workerBeat>=3&&core.stats[BleState]==6&&core.stats[Epoch]==3);puts("PASS actual monitor continues during disconnected/reconnect coordinator turns; main counters remain flat");
 point(Stopped);assert(core.stats[HeapStop]==1);rxPoint(CacheWait,23);raw(23,wire,3,8,8);key(66,0,true);keyDone();r1(7,0);assert(core.stats[RawCount]==1&&core.stats[LastKey]==(66|(1<<16)));const auto before=used;stopTransport();for(unsigned i=0;i<10;++i){clockUs+=50000;monitor(5,3,true);}assert(used==before&&probe_lock_held==0);forbid=false;puts("PASS actual wrappers allocate nothing; heap-stop observation retained; PHY handoff prevents all later writes");
}
