#include "ST7305_Waveshare.h"
#include "display_schedule.hpp"
#include <array>
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <climits>
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
namespace draftor::memory {
long allocation_fail_after=-1;AllocationStats allocation_stats;
void* allocate(std::size_t n)noexcept{++allocation_stats.calls;return allocation_fail_after==0?nullptr:std::malloc(n);}
void release(void* p)noexcept{std::free(p);}
}
namespace model {
std::array<int,49> pins{};
std::array<std::uint8_t,120000> pixels{};
bool active=false,begin_ok=true;
std::uint8_t command=0,col0=0x12,col1=0x2A,row0=0,row1=199;
unsigned parameter=0;std::size_t position=0,wire=0,payload=0,transactions=0;
std::function<void()> during;
void counts(){wire=payload=transactions=0;}
}
void pinMode(std::uint8_t,int){}
void digitalWrite(std::uint8_t p,int v){CHECK(p==40||p==5||p==41);model::pins[p]=v;}
void delay(unsigned long){}
bool SPIClass::begin(int sck,int miso,int mosi,int cs){CHECK(sck==11&&miso==-1&&mosi==12&&cs==40);return model::begin_ok;}
void SPIClass::beginTransaction(SPISettings s){CHECK(s.hz==10000000&&s.order==MSBFIRST&&s.mode==SPI_MODE0);CHECK(!model::active);model::active=true;++model::transactions;}
void SPIClass::endTransaction(){CHECK(model::active&&model::pins[40]==HIGH);model::active=false;}
void SPIClass::writeBytes(const std::uint8_t* p,std::size_t n){
 CHECK(model::active&&model::pins[40]==LOW&&n);
 // Pinned Arduino reads whole source words, even for the final partial word.
 CHECK(reinterpret_cast<std::uintptr_t>(p)%4==0);
 for(std::size_t j=0;j<(n+3)/4;++j){volatile auto word=reinterpret_cast<const std::uint32_t*>(p)[j];(void)word;}
 model::wire+=n;
 if(model::pins[5]==LOW){CHECK(n==1);model::command=*p;model::parameter=0;model::position=0;return;}
 for(std::size_t i=0;i<n;++i){
  if(model::command==0x2A){if(!model::parameter++)model::col0=p[i];else model::col1=p[i];}
  else if(model::command==0x2B){if(!model::parameter++)model::row0=p[i];else model::row1=p[i];}
  else if(model::command==0x2C){
   CHECK(model::col0>=0x12&&model::col1<=0x2A&&model::col0<=model::col1&&model::row0<=model::row1&&model::row1<200);
   const auto row_bytes=std::size_t(model::col1-model::col0+1)*3;
   CHECK(model::position<row_bytes*(model::row1-model::row0+1));
   const unsigned y=(model::row0+model::position/row_bytes)*2;
   const unsigned x=(model::col0-0x12)*12+(model::position%row_bytes)*4;
   // Decode wire independently into a logical 300x400 reference image.
   unsigned bit=7;
   for(unsigned dx=0;dx<4;++dx)for(unsigned dy=0;dy<2;++dy)model::pixels[(y+dy)*300+x+dx]=(p[i]>>bit--)&1U;
   ++model::position;++model::payload;
  }
 }
 if(model::command==0x2C&&model::during)model::during();
}
int main(){try{
 using Color=ST7305Waveshare::Color;
 SPIClass spi;ST7305Waveshare lcd(spi,11,12,40,5,41);
 CHECK(!lcd.update());model::begin_ok=false;CHECK(!lcd.begin());CHECK(!lcd.update());model::begin_ok=true;CHECK(lcd.begin());
 std::array<std::uint8_t,120000> expected{};expected.fill(1);
 auto pixel=[&](int x,int y,Color c){lcd.pixel(x,y,c);if(x>=0&&y>=0&&x<300&&y<400)expected[y*300+x]=(c==Color::White||c==Color::Off);};
 auto fill=[&](Color c){lcd.fill(c);expected.fill(c==Color::White||c==Color::Off);};
 ime_device::DisplaySchedule schedule;schedule.prepare(lcd.BufferSize);CHECK(schedule.memoryBytes()==15000);
 auto send=[&](){return schedule.presentCompared(lcd.framebuffer(),lcd.BufferSize,1,[&](const auto* old,std::size_t n){return lcd.update(old,n);});};
 auto verify=[&](){CHECK(model::pixels==expected);CHECK(lcd.lastTransfer().windows<=8);};
 fill(Color::White);model::counts();CHECK(send());verify();CHECK(model::payload==15000&&lcd.lastTransfer().full);
 model::counts();CHECK(send());CHECK(!model::wire&&schedule.reused==1);
 for(auto xy:{std::pair<int,int>{0,0},{299,0},{0,399},{299,399},{3,1},{4,2},{11,1},{12,2}}){pixel(xy.first,xy.second,Color::Black);model::counts();CHECK(send());verify();CHECK(model::payload==75);}
 pixel(-1,0,Color::Black);pixel(300,399,Color::Black);pixel(5,400,Color::Black);pixel(INT_MAX,INT_MIN,Color::Black);CHECK(send());verify();
 // Every logical pixel/bit visited, asymmetric pattern catches transposes/mirrors.
 for(int y=0;y<400;++y)for(int x=0;x<300;++x)pixel(x,y,((x*7+y*13)%19)<9?Color::Black:Color::White);
 model::counts();CHECK(send());verify();CHECK(lcd.lastTransfer().full&&model::payload==15000);
 fill(Color::White);CHECK(send());
 for(int y=0;y<400;y+=22)pixel(y%300,y,Color::Red);
 model::counts();CHECK(send());verify();CHECK(lcd.lastTransfer().windows<=8&&!lcd.lastTransfer().full);
 unsigned seed=7305;
 for(unsigned frame=0;frame<160;++frame){
  for(unsigned j=0;j<frame%57+1;++j){seed=seed*1664525+1013904223;pixel(seed%300,(seed>>10)%400,Color(frame%4));}
  if(frame%31==0)fill(Color(frame%4));
  model::counts();CHECK(send());verify();CHECK(model::payload<=15000);
  if(model::payload)CHECK(lcd.lastTransfer().payload_bytes==model::payload&&lcd.lastTransfer().wire_bytes==model::wire&&lcd.lastTransfer().bus_transactions==model::transactions);
 }
 fill(Color::White);CHECK(send());pixel(10,10,Color::Black);
 unsigned queued=0;const auto serial=schedule.sent;
 model::during=[&]{++queued;CHECK(schedule.sent==serial);lcd.fill(Color::Black);lcd.pixel(299,399,Color::Black);CHECK(!lcd.update());CHECK(!schedule.presentCompared(lcd.framebuffer(),15000,2,[](const auto*,std::size_t){return true;}));};
 CHECK(send());model::during={};CHECK(queued);verify();
 pixel(20,20,Color::Black);
 CHECK(!schedule.presentCompared(lcd.framebuffer(),15000,3,[](const auto*,std::size_t){return false;}));
 model::counts();CHECK(send());verify();CHECK(model::payload==15000);
 pixel(21,20,Color::Black);
 CHECK(!schedule.presentCompared(lcd.framebuffer(),15000,4,[&](const auto*,std::size_t){schedule.invalidate();return true;}));
 model::counts();CHECK(send());verify();CHECK(model::payload==15000);
 schedule.invalidate();CHECK(lcd.begin());model::counts();CHECK(send());verify();CHECK(model::payload==15000);
 CHECK(!lcd.update(lcd.framebuffer(),14999));CHECK(lcd.update());verify();
 draftor::memory::allocation_fail_after=0;ime_device::DisplaySchedule noShadow;noShadow.prepare(15000);draftor::memory::allocation_fail_after=-1;
 for(unsigned i=0;i<2;++i){model::counts();CHECK(noShadow.presentCompared(lcd.framebuffer(),15000,i,[&](const auto* p,std::size_t n){CHECK(!p&&!n);return lcd.update(p,n);}));CHECK(model::payload==15000);verify();}
 std::cout<<"PASS ST7305 real driver: 120000 pixels, polarity, corners, native group boundaries, unchanged frame, single 75-byte row, 8-window bound, full fallback, invalidation, failed begin/transfer, reset, word alignment, no-shadow fallback, in-flight mutation rejection\n";
 std::cout<<"SIZE framebuffer=15000 shadow=15000 stack_staging=64 driver="<<sizeof(lcd)<<" plan="<<sizeof(st7305_waveshare::Plan)<<"\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
