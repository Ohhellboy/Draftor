#include "hid_probe_core.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
#include <new>
#include <cstdlib>
static bool forbid=false;
void* operator new(std::size_t n){assert(!forbid);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
using namespace hidprobe;
int main(){
 Spsc<uint32_t,32> ring;std::thread producer([&]{for(uint32_t i=0;i<100000;++i)while(!ring.push(i))std::this_thread::yield();});for(uint32_t i=0;i<100000;++i){uint32_t v=0;while(!ring.pop(v))std::this_thread::yield();assert(v==i);}producer.join();puts("PASS actual SPSC FIFO across two threads");
 Core core;std::vector<uint8_t> bytes;bytes.reserve(200000);uint8_t map[4096];for(unsigned i=0;i<4096;++i)map[i]=uint8_t(i);core.map(map,sizeof(map),true);core.reference(23,1,1,64,true);
 uint8_t raw[40];for(unsigned i=0;i<40;++i)raw[i]=uint8_t(i+1);
 for(unsigned i=0;i<128;++i)core.raw(23,raw,40,50,10+i);assert(core.stats[RawCount]==128&&core.stats[RawDrop]==96);
 core.raw(23,raw,40,50,140);assert(core.stats[RawCount]==129&&core.stats[RawDrop]==96);
 core.set(LoopBegin,123);core.set(LoopEnd,122);core.notePoint(Admit,150);core.noteRx(CacheWait,23,150);
 unsigned calls=0;forbid=true;core.service(1000,[&](const uint8_t*,size_t){++calls;return size_t(0);});forbid=false;assert(calls==1&&core.offset==0);
 forbid=true;core.service(1001,[&](const uint8_t* p,size_t){bytes.push_back(*p);return size_t(1);});forbid=false;assert(core.offset==4);
 for(uint32_t ms=1050;ms<40000;ms+=50){calls=0;forbid=true;core.service(ms,[&](const uint8_t* p,size_t n){++calls;bytes.insert(bytes.end(),p,p+n);return n;});forbid=false;assert(calls<=4);}
 assert(core.workerBeat>30&&core.stats[LoopBegin]==123&&core.stats[LoopEnd]==122);assert(core.descriptorAt==4096);puts("PASS worker progresses with frozen main counters; bounded writes, partial/zero FIFO, no allocation");
 assert(bytes.size()%16==0);unsigned expected=0,rawHeaders=0,mapWords=0,refs=0,healths=0;bool truncated=false;
 for(size_t at=0;at<bytes.size();at+=16){Packet p;std::memcpy(&p,bytes.data()+at,16);assert(p.magic==0x32524457&&p.sequence==expected++);if(p.kind==2){++rawHeaders;assert((p.value&65535)==23&&(p.value>>16)==40);}if(p.kind==3){assert((p.value&65535)==50&&((p.value>>16)&255)==32);truncated=true;}if(p.kind==7){++mapWords;assert(p.ms<4096);}if(p.kind==8||p.kind==9)++refs;if(p.kind==1)++healths;}
 assert(rawHeaders==32&&mapWords==1024&&refs==2&&healths>30&&truncated);puts("PASS raw head/total/capture lengths, map reconstruction indices, references, drop reporting");
 const auto prior=bytes.size();core.enabled=false;core.service(40000,[&](const uint8_t*,size_t){assert(false);return size_t(0);});assert(bytes.size()==prior);puts("PASS disabled transport performs no hardware writes");
 Core timed;forbid=true;timed.service(100,[](const uint8_t*,size_t n){return n;});timed.service(600100,[](const uint8_t*,size_t n){return n;});forbid=false;assert(timed.finished&&!timed.enabled&&timed.pending.kind==127);puts("PASS 600s window stops explicitly");
 Core budget;budget.running=true;budget.started=0;budget.issued=Core::PacketLimit-1;budget.service(10,[](const uint8_t*,size_t n){return n;});assert(budget.finished&&budget.pending.kind==127);puts("PASS packet budget ends explicitly");
 if(const char* out=std::getenv("HID_R2_TRACE_OUT")){auto* f=std::fopen(out,"wb");assert(f);assert(std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());std::fclose(f);}
 std::printf("CORE_BYTES=%zu RAW_BYTES=%zu PACKET_BYTES=%zu\n",sizeof(Core),sizeof(Raw),sizeof(Packet));
}
