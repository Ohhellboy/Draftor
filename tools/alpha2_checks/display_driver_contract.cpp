#include "ST7306_OsptekLpm8.h"
#include "display_delta.hpp"
#include "display_schedule.hpp"
#include "display_tx_observe.hpp"
#include "idle_log.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
namespace draftor::memory {
long allocation_fail_after=-1;AllocationStats allocation_stats;
void* allocate(std::size_t n)noexcept{++allocation_stats.calls;if(allocation_fail_after==0)return nullptr;if(allocation_fail_after>0)--allocation_fail_after;return std::malloc(n);}
void release(void* p)noexcept{std::free(p);}
}
namespace model {
std::array<int,32> pins{};
std::array<std::uint8_t,30000> panel{};
bool active=false;std::uint8_t command=0,x0=5,x1=54,y0=0,y1=199;
unsigned parameter=0;std::size_t position=0;
std::uint64_t wire=0,payload=0,transactions=0;
std::function<void()> during;
void resetCounts(){wire=payload=transactions=0;}
}
void pinMode(std::uint8_t,int){}
void digitalWrite(std::uint8_t p,int v){model::pins[p]=v;}
void delay(unsigned long){}
void SPIClass::begin(int sck,int miso,int mosi,int cs){CHECK(sck==12&&miso==-1&&mosi==11&&cs==10);}
void SPIClass::beginTransaction(SPISettings s){CHECK(s.hz==10000000&&s.order==MSBFIRST&&s.mode==SPI_MODE0);CHECK(!model::active);model::active=true;++model::transactions;}
void SPIClass::endTransaction(){CHECK(model::active&&model::pins[10]==HIGH);model::active=false;}
void SPIClass::writeBytes(const std::uint8_t* p,std::size_t n){
  CHECK(model::active&&model::pins[10]==LOW&&n>0);
  // Match the pinned SDK's ceil(n/4) source-word reads, with ASan/UBSan.
  CHECK(reinterpret_cast<std::uintptr_t>(p)%4==0);
  for(std::size_t i=0;i<(n+3)/4;++i){volatile auto word=reinterpret_cast<const std::uint32_t*>(p)[i];(void)word;}
  model::wire+=n;
  if(model::pins[9]==LOW){CHECK(n==1);model::command=p[0];model::parameter=0;if(p[0]==0x2C)model::position=std::size_t(model::y0)*150;return;}
  if(model::command==0x2A){for(std::size_t i=0;i<n;++i){if(model::parameter++==0)model::x0=p[i];else model::x1=p[i];}}
  if(model::command==0x2B){for(std::size_t i=0;i<n;++i){if(model::parameter++==0)model::y0=p[i];else model::y1=p[i];}}
  if(model::command==0x2C){
    CHECK(model::x0==5&&model::x1==54&&model::y0<=model::y1&&model::y1<200);
    CHECK(model::position+n<=std::size_t(model::y1+1)*150);
    if(model::during)model::during();
    std::copy(p,p+n,model::panel.begin()+model::position);model::position+=n;model::payload+=n;
  }
}
struct Output : ime_device::LogPort {
  std::string data;bool connected()const override{return true;}std::size_t available()override{return 31;}
  std::size_t write(const char* p,std::size_t n)override{n=std::min(n,std::size_t(7));data.append(p,n);return n;}
};
int main(){try{
  SPIClass spi;ST7306OsptekLpm8 driver(spi,12,11,10,9,4);
  CHECK(!driver.update());CHECK(driver.begin());
  CHECK(reinterpret_cast<std::uintptr_t>(driver.framebuffer())%4==0);
  ime_device::DisplaySchedule schedule;schedule.prepare(driver.BufferSize);
  auto draw=[&](){return schedule.presentCompared(driver.framebuffer(),driver.BufferSize,1,[&](const auto* prior,std::size_t n){return driver.update(prior,n);});};
  driver.fill(ST7306OsptekLpm8::Color::White);model::resetCounts();CHECK(draw());
  CHECK(model::payload==30000&&driver.lastTransfer().full);CHECK(!std::memcmp(model::panel.data(),driver.framebuffer(),30000));
  model::resetCounts();CHECK(draw());CHECK(model::wire==0&&schedule.reused==1);
  driver.fillRect(20,320,180,8,ST7306OsptekLpm8::Color::Black);model::resetCounts();CHECK(draw());
  CHECK(!std::memcmp(model::panel.data(),driver.framebuffer(),30000));
  const auto sample_payload=model::payload;
#if DRAFTOR_DISPLAY_TRANSACTION
  CHECK(sample_payload==600);
#else
  CHECK(sample_payload==30000);
#endif
  std::uint64_t total=sample_payload;unsigned seed=1307;
  for(unsigned frame=0;frame<100;++frame){
    // Sparse, disjoint and broad H/V-equivalent wire pixels, including edges.
    for(unsigned k=0;k<1+frame%60;++k){seed=seed*1664525+1013904223;driver.pixel(seed%300,(seed>>10)%400,ST7306OsptekLpm8::Color(frame%4));}
    if(frame%17==0)driver.fill(ST7306OsptekLpm8::Color(frame%4));
    model::resetCounts();CHECK(draw());CHECK(!std::memcmp(model::panel.data(),driver.framebuffer(),30000));
    CHECK(model::payload<=30000&&model::transactions<=64);
    if(model::payload){const auto& s=driver.lastTransfer();CHECK(s.wire_bytes==model::wire&&s.payload_bytes==model::payload&&s.bus_transactions==model::transactions);CHECK(s.windows<=8&&s.staging_copy_bytes==7U*s.windows);}
    total+=model::payload;
  }
  // Native Y/packing edges: every tile can change without missing erased pixels.
  for(unsigned tile=0;tile<100;++tile){driver.fillRect(0,396-int(tile)*4,300,4,ST7306OsptekLpm8::Color::Red);CHECK(draw());CHECK(!std::memcmp(model::panel.data(),driver.framebuffer(),30000));}
  driver.fillRect(10,10,12,12,ST7306OsptekLpm8::Color::Black);
  unsigned queued=0;const auto before=schedule.sent;bool still_old=false;
  model::during=[&]{++queued;still_old=schedule.sent==before;CHECK(still_old);CHECK(!driver.update());CHECK(!schedule.presentCompared(driver.framebuffer(),driver.BufferSize,2,[](const auto*,std::size_t){return true;}));};
  CHECK(draw());model::during={};CHECK(queued&&still_old&&schedule.busy_rejected);
  // A received event remained queued throughout synchronous sends. It may now
  // mutate B; neither a new view nor its completion was fabricated in flight.
  const auto a=std::vector<std::uint8_t>(driver.framebuffer(),driver.framebuffer()+30000);
  driver.fillRect(0,0,5,5,ST7306OsptekLpm8::Color::Black);
  CHECK(!schedule.presentCompared(driver.framebuffer(),30000,2,[&](const auto* prior,std::size_t n){CHECK(n==30000&&!std::memcmp(prior,a.data(),n));return false;}));
  model::resetCounts();CHECK(draw());CHECK(model::payload==30000); // known failure retires shadow
  driver.pixel(11,11,ST7306OsptekLpm8::Color::Red);
  CHECK(!schedule.presentCompared(driver.framebuffer(),30000,3,[&](const auto*,std::size_t){schedule.invalidate();return true;}));
  model::resetCounts();CHECK(draw());CHECK(model::payload==30000);
  schedule.invalidate();CHECK(driver.begin());model::resetCounts();CHECK(draw());CHECK(model::payload==30000);
  ime_device::DisplaySchedule failedAllocation;draftor::memory::allocation_fail_after=0;failedAllocation.prepare(30000);draftor::memory::allocation_fail_after=-1;
  CHECK(failedAllocation.memoryBytes()==0);
  for(unsigned n=0;n<2;++n){model::resetCounts();CHECK(failedAllocation.presentCompared(driver.framebuffer(),30000,n,[&](const auto* old,std::size_t size){CHECK(!old&&!size);return driver.update(old,size);}));CHECK(model::payload==30000);}
#if DRAFTOR_DISPLAY_TX_OBSERVE
  ime_device::IdleLog log;Output output;
  for(unsigned i=0;i<30;++i)display_tx_observe::emit(log,i,true,driver.lastTransfer(),10,100,1,2,3,4,5);
  CHECK(log.size()==10&&display_tx_observe::dropped==20);const auto size=log.size();
  log.service(output,{true,false,false,false});CHECK(log.size()==size&&output.data.empty());
  while(log.size())log.service(output,{});
  CHECK(std::count(output.data.begin(),output.data.end(),'\n')==10);CHECK(output.data.find("done=100")!=std::string::npos);
#endif
  std::cout<<"PASS driver_protocol pixels=201plus bounded_bands=8 sparse_payload="<<sample_payload<<" random_payload="<<total<<" no_new_frame_buffer=1 synchronous_only=1 reported_failure_fallback=1 model_not_device=1\n";
  std::cout<<"SIZE driver="<<sizeof(driver)<<" schedule="<<sizeof(schedule)<<" plan="<<sizeof(st7306_osptek_lpm8::DeltaPlan)<<" stats="<<sizeof(ST7306OsptekLpm8::TransferStats)<<"\n";
  return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
