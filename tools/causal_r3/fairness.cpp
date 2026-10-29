#include "hid_probe_core.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include <algorithm>
#include <new>
#include <cstdlib>
static bool forbid=false;
void* operator new(size_t n){assert(!forbid);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void* operator new[](size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
using namespace hidprobe;
int main(){
 for(unsigned mode=0;mode<3;++mode){
  Core c;std::vector<uint8_t> wire;wire.reserve(300000);uint8_t map[4096];for(unsigned i=0;i<4096;++i)map[i]=uint8_t(i);
#ifndef EXPECT_R2_STARVATION
  c.beginDiscovery();
#endif
  c.map(map,sizeof(map),true);c.reference(42,1,1,64,true);
#ifndef EXPECT_R2_STARVATION
  c.bindEpoch(13);
#endif
  uint8_t report[8]={0,0,4,0,0,0,0,0};
  for(unsigned i=0;i<32;++i){c.event(4,0,100+i,10+i);c.raw(42,report,8,8,10+i);}
  unsigned positive=0,serviceCalls=0;
  const unsigned rounds=mode?160000:2400;
  for(unsigned k=0;k<rounds;++k){
   size_t budget=mode==0?16:mode==1?(k%7==0?0:3):(k%13<5?0:1);unsigned calls=0;
   forbid=true;
   c.service(100+(mode?k:k*50),[&](const uint8_t* b,size_t n){++calls;if(!budget)return size_t(0);auto sent=std::min(budget,n);wire.insert(wire.end(),b,b+sent);budget-=sent;++positive;return sent;});
   forbid=false;assert(calls<=4);++serviceCalls;
  }
  unsigned key=0,raw=0,desc=0,ref=0,health=0;size_t firstKey=0,firstRaw=0,lastMap=0;uint16_t seq=0;
  for(size_t at=0;at+16<=wire.size();at+=16){Packet p;std::memcpy(&p,wire.data()+at,16);assert(p.sequence==seq++);
   if(p.kind==4){if(!key)firstKey=at/16+1;++key;}
   if(p.kind==2){if(!raw)firstRaw=at/16+1;++raw;}
   if(p.kind==7){++desc;assert(p.ms/4<1024);lastMap=at/16+1;}
   if(p.kind==8||p.kind==9)++ref;if(p.kind==1)++health;
  }
#ifdef EXPECT_R2_STARVATION
  (void)serviceCalls;(void)ref;(void)health;(void)firstKey;(void)firstRaw;(void)lastMap;
  if(mode==0){assert(wire.size()==2400*16&&key==0&&raw==0&&desc==0);puts("REPRODUCED exact R2 Core: 2400/2400 completed packets health, critical/raw/map starved");return 0;}
#else
  assert(key==32&&raw==32&&desc==1024&&ref==2&&health);
  assert(firstKey<=4&&firstRaw<=4&&lastMap<=4*(3+1024+2));
  assert(c.stats[RawDrop]==0&&c.stats[MainDrop]==0);
  std::printf("PASS mode=%u rounds=%u accepted_bytes=%zu positive_writes=%u key_first_packet=%zu raw_first_packet=%zu map_last_packet=%zu (bound 4116), keys=%u raw=%u map_words=%u health=%u no_alloc\n",mode,serviceCalls,wire.size(),positive,firstKey,firstRaw,lastMap,key,raw,desc,health);
#endif
 }
#ifndef EXPECT_R2_STARVATION
 Core disconnected;disconnected.service(1,[](const uint8_t*,size_t){return size_t(0);});assert(disconnected.issued==1&&disconnected.offset==0);disconnected.service(600001,[](const uint8_t*,size_t n){return n;});assert(disconnected.finished);puts("PASS disconnected writer has no false delivery guarantee; budget ends explicitly");
 Core next;uint8_t old[]={1,2},fresh[]={3,4,5,6};next.beginDiscovery();next.map(old,2,false);next.bindEpoch(7);next.beginDiscovery();next.map(fresh,4,true);next.bindEpoch(13);assert(next.generation==2&&next.descriptorOriginal==4&&next.boundEpoch==13&&next.descriptor[0]==3);puts("PASS discovery generation replaces stale descriptor; binds current admission epoch");
#endif
}
