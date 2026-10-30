#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "hanime_hid_probe.hpp"
namespace hidprobe {
// Each counter and each SPSC index has exactly one producer. Internal DRAM,
// aligned 32-bit loads/stores plus MEMW avoid the SDK's generic atomic helpers
// (which also support external RAM and are not always lock-free).
class Word {
#ifdef ARDUINO_ARCH_ESP32
 alignas(4) volatile uint32_t value_=0;
#else
 std::atomic<uint32_t> value_{0};
#endif
 public:
 Word(uint32_t value=0)noexcept:value_(value){}
 uint32_t load(std::memory_order order=std::memory_order_seq_cst)const noexcept{
#ifdef ARDUINO_ARCH_ESP32
  (void)order;__asm__ __volatile__("memw" ::: "memory");const auto v=value_;__asm__ __volatile__("memw" ::: "memory");return v;
#else
  return value_.load(order);
#endif
 }
 void store(uint32_t value,std::memory_order order=std::memory_order_seq_cst)noexcept{
#ifdef ARDUINO_ARCH_ESP32
  (void)order;__asm__ __volatile__("memw" ::: "memory");value_=value;__asm__ __volatile__("memw" ::: "memory");
#else
  value_.store(value,order);
#endif
 }
 // Single-writer only: never used for contention or cross-context admission.
 uint32_t fetch_add(uint32_t value,std::memory_order order=std::memory_order_seq_cst)noexcept{const auto old=load(order);store(old+value,order);return old;}
 operator uint32_t()const noexcept{return load();}
 Word& operator=(uint32_t v)noexcept{store(v);return *this;}
};
static_assert(sizeof(Word)==4&&alignof(Word)>=4,"Internal aligned control word");
struct Packet {uint32_t magic=0x34524457,ms=0;uint16_t sequence=0;uint8_t kind=0,aux=0;uint32_t value=0;};
static_assert(sizeof(Packet)==16,"HID R4 trace ABI");
template<class T,unsigned N> struct Spsc {
 std::array<T,N> data{};Word head{0},tail{0};
 bool push(const T& value)noexcept{auto h=head.load(std::memory_order_relaxed);if(h-tail.load(std::memory_order_acquire)>=N)return false;data[h%N]=value;head.store(h+1,std::memory_order_release);return true;}
 bool pop(T& value)noexcept{auto t=tail.load(std::memory_order_relaxed);if(t==head.load(std::memory_order_acquire))return false;value=data[t%N];tail.store(t+1,std::memory_order_release);return true;}
};
struct Raw {uint32_t ms=0,generation=0;uint16_t handle=0,head=0,total=0;uint8_t captured=0,id=0;std::array<uint8_t,32> data{};};
struct Ref {uint16_t handle=0,bits=0;uint8_t id=0,type=0,selected=0;};
struct InputReceipt {uint16_t handle=0,cccd=0,value=0;uint8_t id=0,status=0,keyboard=0,length=0;};
// Two single-producer rings: NimBLE host -> coordinator; main loop -> coordinator.
// Only the existing BLE coordinator serializes/writes. No callback output/locks.
struct Core {
 static constexpr uint32_t RawLimit=128,WindowMs=600000,PacketLimit=16384;
 std::array<Word,StatCount> stats{};
 Word enabled{1};Spsc<Raw,32> rawRing;Spsc<Packet,32> mainRing;
 std::array<uint8_t,4096> descriptor{};uint32_t descriptorSize=0,descriptorOriginal=0;bool descriptorParsed=false,mapSet=false,metadataReady=false;Word generation{0};uint32_t boundEpoch=0;
 std::array<Ref,16> refs{};unsigned refCount=0;
 std::array<InputReceipt,16> inputs{};unsigned inputCount=0,inputAt=0,inputPart=0;SetupStatus setupStatus=SetupPending;uint16_t setupHandle=0;bool setupSent=false;
 uint32_t started=0,lastHealth=0,workerBeat=0,issued=0;bool running=false,finished=false;
 std::array<Packet,StatCount+1> health{};unsigned healthAt=StatCount+1;
 Raw currentRaw{};unsigned rawPart=0;bool rawPending=false;
 unsigned descriptorAt=0,refAt=0,refPart=0;bool mapHeader=false;unsigned metaPrefix=0,lane=0;
 static constexpr uint32_t HealthPeriodMs=5000;
 Packet pending{};unsigned offset=16;
 void add(Stat s)noexcept{stats[s].fetch_add(1,std::memory_order_relaxed);}
 void set(Stat s,uint32_t v)noexcept{stats[s].store(v,std::memory_order_relaxed);}
 void notePoint(Point p,uint32_t ms)noexcept{set(MainPointMs,ms);set(MainPoint,p);}
 void noteRx(Rx p,uint16_t handle,uint32_t ms)noexcept{set(RxPointMs,ms);set(RxPoint,p);if(handle)set(Handle,handle);}
 void raw(uint16_t handle,const uint8_t* bytes,size_t head,size_t total,uint32_t ms,size_t available=size_t(-1))noexcept{
  auto id=stats[RawCount].fetch_add(1,std::memory_order_relaxed);
  if(!enabled.load(std::memory_order_relaxed)||id>=RawLimit)return;
  Raw r;r.ms=ms;r.generation=generation.load();r.handle=handle;r.head=uint16_t(head>65535?65535:head);r.total=uint16_t(total>65535?65535:total);r.id=uint8_t(id);
  if(available==size_t(-1))available=head;
  r.captured=uint8_t(bytes?(available<32?available:32):0);if(r.captured)std::memcpy(r.data.data(),bytes,r.captured);
  if(!rawRing.push(r))add(RawDrop);
 }
 void event(uint8_t kind,uint8_t aux,uint32_t v,uint32_t ms)noexcept{
  if(enabled.load(std::memory_order_relaxed)&&!mainRing.push(Packet{0x34524457,ms,0,kind,aux,v}))add(MainDrop);
 }
 // Discovery runs on the coordinator before monitor starts. Preserve first map.
 void map(const uint8_t* b,size_t n,bool parsed)noexcept{
  if(metadataReady)return;mapSet=true;descriptorAt=0;mapHeader=false;descriptorOriginal=uint32_t(n);descriptorSize=uint32_t(b?(n<descriptor.size()?n:descriptor.size()):0);descriptorParsed=parsed;
  if(descriptorSize)std::memcpy(descriptor.data(),b,descriptorSize);
 }
 void reference(uint16_t h,uint8_t id,uint8_t type,uint16_t bits,bool selected)noexcept{
  if(!metadataReady&&refCount<refs.size())refs[refCount++]={h,bits,id,type,uint8_t(selected)};
 }
 Packet make(uint8_t kind,uint8_t aux,uint32_t v,uint32_t ms)noexcept{return {0x34524457,ms,0,kind,aux,v};}
 void beginDiscovery()noexcept{
  generation.fetch_add(1);metadataReady=false;boundEpoch=0;descriptorSize=descriptorOriginal=0;descriptorParsed=mapSet=false;
  descriptorAt=refAt=refPart=refCount=metaPrefix=0;mapHeader=false;inputCount=inputAt=inputPart=0;setupStatus=SetupPending;setupHandle=0;setupSent=false;
 }
 void inputSubscription(uint16_t h,uint16_t cccd,uint8_t id,bool keyboard,InputStatus status,size_t n,uint16_t value)noexcept{
  if(metadataReady)return;
  unsigned i=0;while(i<inputCount&&inputs[i].handle!=h)++i;
  if(i==inputCount){if(inputCount==inputs.size())return;++inputCount;}
  inputs[i]={h,cccd,value,id,uint8_t(status),uint8_t(keyboard),uint8_t(n>255?255:n)};
 }
 void setupDone(SetupStatus status,uint16_t handle,uint32_t e)noexcept{setupStatus=status;setupHandle=handle;boundEpoch=e;metadataReady=true;}
 void bindEpoch(uint32_t e)noexcept{boundEpoch=e;metadataReady=true;}
 bool metadata(Packet& out,uint32_t ms)noexcept{
  if(!metadataReady)return false;
  if(metaPrefix==0){++metaPrefix;out=make(10,0,generation.load(),ms);return true;}
  if(metaPrefix==1){++metaPrefix;out=make(11,0,boundEpoch,ms);return true;}
  if(!mapHeader){mapHeader=true;out=make(6,uint8_t(descriptorParsed),descriptorOriginal,ms);return true;}
  if(descriptorAt<descriptorSize){uint32_t value=0;for(unsigned i=0;i<4&&descriptorAt+i<descriptorSize;++i)value|=uint32_t(descriptor[descriptorAt+i])<<(i*8);out=make(7,0,value,descriptorAt);descriptorAt+=4;return true;}
  if(refAt<refCount){auto& ref=refs[refAt];if(refPart++==0)out=make(8,uint8_t(refAt),uint32_t(ref.handle)|(uint32_t(ref.id)<<16)|(uint32_t(ref.type)<<24),ms);else{out=make(9,uint8_t(refAt),uint32_t(ref.bits)|(uint32_t(ref.selected)<<16),ms);refPart=0;++refAt;}return true;}
  if(inputAt<inputCount){auto& s=inputs[inputAt];
   if(inputPart==0)out=make(40,uint8_t(inputAt),uint32_t(s.handle)|(uint32_t(s.id)<<16)|(uint32_t(s.status)<<24),ms);
   else if(inputPart==1)out=make(41,uint8_t(inputAt),uint32_t(s.cccd)|(uint32_t(s.value)<<16),ms);
   else out=make(42,uint8_t(inputAt),uint32_t(s.length)|(uint32_t(s.keyboard)<<8),ms);
   if(++inputPart==3){inputPart=0;++inputAt;}return true;
  }
  if(!setupSent){setupSent=true;out=make(43,uint8_t(setupStatus),uint32_t(setupHandle)|(uint32_t(inputCount)<<16),ms);return true;}
  return false;
 }
 bool rawPacket(Packet& out)noexcept{
  if(!rawPending){rawPending=rawRing.pop(currentRaw);rawPart=0;}
  if(!rawPending)return false;
  auto& r=currentRaw;
  if(rawPart==0)out=make(2,r.id,uint32_t(r.handle)|(uint32_t(r.head)<<16),r.ms);
  else if(rawPart==1)out=make(3,r.id,uint32_t(r.total)|(uint32_t(r.captured)<<16),r.ms);
  else if(rawPart==2)out=make(12,r.id,r.generation,r.ms);
  else {uint32_t value=0;unsigned at=(rawPart-3)*4;for(unsigned i=0;i<4&&at+i<r.captured;++i)value|=uint32_t(r.data[at+i])<<(8*i);out=make(uint8_t(16+rawPart-3),r.id,value,r.ms);}
  if(++rawPart>=3+(r.captured+3u)/4u)rawPending=false;
  return true;
 }
 bool healthPacket(Packet& out,uint32_t ms)noexcept{
  if(healthAt==health.size()){
   if(workerBeat&&uint32_t(ms-lastHealth)<HealthPeriodMs)return false;
   health[0]=make(1,0,++workerBeat,ms);
   for(unsigned i=0;i<StatCount;++i)health[i+1]=make(uint8_t(64+i),0,stats[i].load(std::memory_order_relaxed),ms);
   healthAt=0;
  }
  out=health[healthAt++];if(healthAt==health.size())lastHealth=ms;
  return true;
 }
 bool next(Packet& out,uint32_t ms)noexcept{
  if(uint32_t(ms-started)>=WindowMs||issued>=PacketLimit-1){out=make(127,0,uint32_t(ms-started),ms);return true;}
  // Packet-level round robin; partial writes retain the current packet.
  // A pending lane wins within four completed packet selections (64 bytes).
  // Health has one in-flight snapshot, no historical backlog, and a cooldown
  // after its last packet. It cannot starve metadata, raw or key events.
  for(unsigned i=0;i<4;++i){const auto selected=lane;lane=(lane+1)%4;
   if(selected==0&&metadata(out,ms))return true;
   if(selected==1&&mainRing.pop(out))return true;
   if(selected==2&&rawPacket(out))return true;
   if(selected==3&&healthPacket(out,ms))return true;
  }
  return false;
 }
 template<class Write> void service(uint32_t ms,Write write)noexcept{
  if(!enabled.load(std::memory_order_relaxed)||finished)return;
  if(!running){running=true;started=ms;}
  // Four nonblocking attempts, <=64 bytes per 50ms worker iteration.
  for(unsigned i=0;i<4;++i){
   if(offset==16){if(!next(pending,ms))return;pending.sequence=uint16_t(issued++);offset=0;}
   auto n=write(reinterpret_cast<const uint8_t*>(&pending)+offset,16-offset);if(n>16-offset)n=16-offset;
   offset+=unsigned(n);if(!n)return;
   if(offset==16&&pending.kind==127){finished=true;enabled=false;return;}
  }
 }
};
}
