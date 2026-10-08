#include "display_schedule.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace ime_device;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
int main(){try{
 DisplaySchedule d;d.prepare(30000);CHECK(d.memoryBytes()==30000);
 std::array<uint8_t,30000> pixels{};unsigned transfers=0,published=0;
 auto draw=[&](bool ok){const bool accepted=d.present(pixels.data(),pixels.size(),100,[&]{CHECK(published==0);++transfers;return ok;});if(accepted)++published;return accepted;};
 CHECK(draw(true));CHECK(transfers==1&&published==1);published=0;
 CHECK(draw(true));CHECK(transfers==1&&published==1&&d.reused==1);published=0;
 pixels.back()=1;CHECK(!draw(false));CHECK(!published&&d.failed==1);
 CHECK(draw(true));CHECK(published==1&&transfers==3);published=0;
 d.completed(100);
 CHECK(!d.due(110,true,false,false));CHECK(d.due(110,true,true,true));
 CHECK(!d.due(140,true,true,false));CHECK(d.due(150,true,true,false));
 CHECK(!d.due(500,false,false,false));
 // A failed mirror allocation falls back to actual transfers, never false reuse.
 DisplaySchedule noCache;draftor::memory::allocation_fail_after=0;noCache.prepare(30000);draftor::memory::allocation_fail_after=-1;
 CHECK(!noCache.memoryBytes());unsigned n=0;
 CHECK(noCache.present(pixels.data(),pixels.size(),1,[&]{++n;return true;}));
 CHECK(noCache.present(pixels.data(),pixels.size(),2,[&]{++n;return true;}));CHECK(n==2);
 std::cout<<"PASS exact pixels, failed transfer blocks publication, fallback, 33/50ms fairness, ready bypass; simulated clock only\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
