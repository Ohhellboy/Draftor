#include "tinyusb_msc.hpp"
#include "usb_runtime_allocation.hpp"
#include "USB.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <unistd.h>
UsbTestState usb_test;
UsbFake USB;
namespace {bool fail_guard=false;std::atomic<unsigned> guard_calls{0};}
// Narrow Host fault seam at the actual C++ ABI entry used by the old endpoint.
// It models the fixed SDK's fatal guard-allocation result, not its allocator.
extern "C" int __cxa_guard_acquire(std::uint64_t* guard) {
 ++guard_calls;
 if(fail_guard){const char msg[]="ACTUAL_ENDPOINT_GUARD_REACHED_WITH_ALLOCATION_FAILURE\n";::write(2,msg,sizeof(msg)-1);std::_Exit(86);}
 return *reinterpret_cast<unsigned char*>(guard)==0;
}
extern "C" void __cxa_guard_release(std::uint64_t* guard){*reinterpret_cast<unsigned char*>(guard)=1;}
extern "C" void __cxa_guard_abort(std::uint64_t*){}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace draftor::usb;
struct Disk:BlockDevice {
 std::uint32_t sectorCount()const noexcept override{return 1;}
 IoResult read(std::uint64_t,void* p,std::size_t n)noexcept override{std::memset(p,0x5a,n);return {Status::Ok,n};}
 IoResult write(std::uint64_t,const void*,std::size_t n)noexcept override{return {Status::Ok,n};}
 Status sync()noexcept override{return Status::Ok;}
};
struct Member {static unsigned live;Member(){++live;}~Member(){--live;}};unsigned Member::live=0;
struct Owner {Member member;explicit Owner(bool fail){if(fail)throw std::bad_alloc();}};
extern "C" std::int32_t tud_msc_read10_cb(std::uint8_t,std::uint32_t,std::uint32_t,void*,std::uint32_t);
int main(int argc,char** argv){
 if(argc>1&&std::strcmp(argv[1],"guard-failure")==0){
  fail_guard=true;auto& p=s3::TinyUsbMsc::instance();CHECK(!p.state().accepting);CHECK(guard_calls==0);
  std::puts("PASS actual endpoint usable with fatal dynamic guard seam armed");return 0;
 }
 auto& p=s3::TinyUsbMsc::instance();CHECK(!p.state().accepting&&p.state().drained);CHECK(usb_test.begins==0);
 std::atomic<bool> same{true};std::vector<std::thread> workers;
 for(unsigned i=0;i<8;++i)workers.emplace_back([&]{for(unsigned n=0;n<10000;++n)if(&s3::TinyUsbMsc::instance()!=&p)same=false;});
 for(auto& t:workers)t.join();CHECK(same);
 // Actual owner allocation helper: failure, member unwinding, retry and reuse.
 Owner* owner=nullptr;const auto initial=draftor::memory::allocation_stats.live_allocations;
 draftor::memory::allocation_fail_after=0;CHECK(!s3app::ensureUsbRuntime(owner,false));CHECK(!owner&&Member::live==0);
 draftor::memory::allocation_fail_after=-1;CHECK(!s3app::ensureUsbRuntime(owner,true));CHECK(!owner&&Member::live==0);CHECK(draftor::memory::allocation_stats.live_allocations==initial);
 CHECK(s3app::ensureUsbRuntime(owner,false));CHECK(owner&&Member::live==1);auto* original=owner;
 CHECK(s3app::ensureUsbRuntime(owner,true));CHECK(owner==original&&Member::live==1);
 owner->~Owner();draftor::memory::release(owner);CHECK(Member::live==0&&draftor::memory::allocation_stats.live_allocations==initial);
 CHECK(p.start());CHECK(p.start());CHECK(usb_test.begins==1&&usb_test.interfaces==1);CHECK(p.prepareSession(0)==Status::Ok);
 Disk disk;std::uint32_t generation=0;CHECK(p.publish(disk,Access::ReadOnly,generation)==Status::Ok);
 char bytes[8]{};CHECK(tud_msc_read10_cb(0,0,0,bytes,sizeof bytes)==8);CHECK(bytes[0]==0x5a);
 CHECK(p.bridge().write(generation,0,bytes,sizeof bytes).status==Status::ReadOnly);
 p.withdraw(EndReason::Ejected);CHECK(p.detach()==Status::Ok);p.busReset();p.inquiryFence();CHECK(p.prepareSession(100)==Status::Ok);
 CHECK(p.publish(disk,Access::ReadOnly,generation)==Status::Ok);p.withdraw(EndReason::Ejected);CHECK(p.detach()==Status::Ok);
 std::puts("PASS endpoint singleton/concurrency, owner allocation/unwind/retry, export callback/session reuse; SDK Host shim only");
}
