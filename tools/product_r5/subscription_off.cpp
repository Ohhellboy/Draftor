#include "hid_keyboard.hpp"
#include "hanime_hid_probe.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <deque>
#include <vector>
#include <atomic>
#include <new>
using String=std::string;
static bool forbidAlloc=false,inCallback=false,hostTask=false,connected=true;
void* operator new(size_t n){assert(!forbidAlloc);if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void* operator new[](size_t n){return ::operator new(n);}void operator delete[](void* p)noexcept{::operator delete(p);}
// GCC may select sized deallocation even with the allocation guard enabled.
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete[](p);}
static unsigned gattCalls=0,writes=0,reads=0;
static void gatt(){assert(!inCallback);++gattCalls;}
struct BLEUUID {uint16_t v;explicit BLEUUID(uint16_t x):v(x){};bool equals(BLEUUID b)const{return v==b.v;}};
struct BLERemoteDescriptor{
 uint16_t handle=0;String value;bool writable=true,throwRead=false,disconnectRead=false,cccd=false;unsigned readCount=0,writeCount=0;String readOverride;
 uint16_t getHandle(){return handle;}
 String readValue(){gatt();++reads;++readCount;if(throwRead)throw std::bad_alloc();if(disconnectRead)connected=false;return !readOverride.empty()?readOverride:value;}
 bool writeValue(uint8_t* b,size_t n,bool response){gatt();assert(cccd&&n==2&&b[0]==1&&b[1]==0&&response);++writes;++writeCount;if(!writable)return false;value.assign((char*)b,n);return true;}
};
struct BLERemoteCharacteristic;
using Notify=void(*)(BLERemoteCharacteristic*,uint8_t*,size_t,bool);
struct BLERemoteCharacteristic{
 uint16_t handle=0,uuid=0x2a4d;bool notifiable=true,readable=true,hasRef=true,hasCccd=true,protocolWorks=true,throwSubscribe=false;String value;BLERemoteDescriptor ref,cccd;Notify callback=nullptr;unsigned reportReads=0;std::vector<uint8_t> protocolWrites;
 BLEUUID getUUID(){return BLEUUID(uuid);}uint16_t getHandle(){return handle;}bool canNotify(){return notifiable;}bool canRead(){return readable;}
 BLERemoteDescriptor* getDescriptor(BLEUUID u){gatt();if(u.v==0x2908)return hasRef?&ref:nullptr;if(u.v==0x2902)return hasCccd?&cccd:nullptr;assert(false);return nullptr;}
 String readValue(){gatt();++reportReads;return value;}
 bool writeValue(uint8_t b,bool response){gatt();assert(uuid==0x2a4e&&!response);protocolWrites.push_back(b);return protocolWorks;}
 bool subscribe(bool n,Notify cb,bool response){gatt();assert(n&&response);callback=cb;if(throwSubscribe)throw std::bad_alloc();uint8_t enabled[]={1,0};return cccd.writeValue(enabled,2,true);}
 void registerForNotify(Notify cb,bool n,bool){(void)subscribe(n,cb,true);}
 void deliver(std::vector<uint8_t> b){assert(callback);inCallback=true;callback(this,b.data(),b.size(),true);inCallback=false;}
};
struct Service{
 BLERemoteCharacteristic map,protocol,boot;bool hasBoot=false,hasProtocol=true;std::map<uint16_t,BLERemoteCharacteristic*> chars;
 BLERemoteCharacteristic* getCharacteristic(BLEUUID u){gatt();if(u.v==0x2a4b)return &map;if(u.v==0x2a4e)return hasProtocol?&protocol:nullptr;if(u.v==0x2a22)return hasBoot?&boot:nullptr;assert(false);return nullptr;}
 auto getCharacteristicsByHandle(){gatt();return &chars;}
};
struct BLEClient {Service service;bool hasService=true;Service* getService(BLEUUID u){gatt();assert(u.v==0x1812);return hasService?&service:nullptr;}bool isConnected(){return connected;}};
static bool ble_hs_is_parent_task(){return hostTask;}
#define BT_PRINTF(...) ((void)0)
#define BT_PRINTLN(...) ((void)0)

namespace keyboard {
enum class Phase:uint8_t{Starting,Scanning,Connecting,Pairing,AwaitRelease,Ready,Disconnected,Cleaning,Error};
std::atomic<bool> accepting{false};std::atomic<uint32_t> epoch{1},bad{0},overflow{0},received{0},peak{0},passkey{1000000};std::atomic<Phase> phase{Phase::Pairing};
uint32_t nextEventId=0;bool requireRelease=false;s3hid::Map reportMap;s3hid::Tracker tracker;s3hid::IngressContext ingress;
struct Subscription{BLERemoteCharacteristic* characteristic; s3hid::Report report;};Subscription subscriptions[8];unsigned subscriptionCount=0;
std::deque<s3hid::Event> events;void* queue=reinterpret_cast<void*>(1);int stateMux=0,depth=0;
#define portENTER_CRITICAL(x) do{assert(*(x)==0);++*(x);++depth;}while(0)
#define portEXIT_CRITICAL(x) do{assert(*(x)==1);--*(x);--depth;}while(0)
constexpr int pdTRUE=1;uint32_t millis(){return 100;}uint64_t esp_timer_get_time(){return 100000;}
int xQueueSend(void*,const s3hid::Event* e,int){if(events.size()==512)return 0;events.push_back(*e);return 1;}
unsigned uxQueueMessagesWaiting(void*){return unsigned(events.size());}
#include "notify_actual.inc"
#include "invalidate_actual.inc"
#include "hogp_actual.inc"
void primeActual(){
#include "prime_actual.inc"
}
void retiredClearActual(){
#include "clear_actual.inc"
}
}
using namespace keyboard;
static BLEClient client;static BLERemoteCharacteristic chars[24];static String realMap;
static void makeChar(BLERemoteCharacteristic& c,uint16_t handle,uint8_t id,uint8_t type=1){c={};c.handle=handle;c.ref.handle=handle+1;c.ref.value=String({char(id),char(type)});c.cccd.handle=handle+2;c.cccd.cccd=true;c.value=String(id==1?7:19,'\0');}
static void reset(unsigned count=5,bool moved=false){
 client={};client.service.map.uuid=0x2a4b;client.service.map.value=realMap;client.service.protocol.uuid=0x2a4e;
 for(unsigned i=0;i<count;++i){const uint8_t ids[]={1,3,4,5,17};makeChar(chars[i],uint16_t(moved?300+i*5:(i?48+i*4:42)),i<5?ids[i]:3);client.service.chars[chars[i].handle]=&chars[i];}
 accepting=false;connected=true;hostTask=false;subscriptionCount=0;clearInputSubscriptions();tracker.clear();events.clear();epoch=1;bad=overflow=received=peak=0;phase=Phase::Pairing;requireRelease=false;nextEventId=0;gattCalls=writes=reads=0;assert(depth==0);
}
static void press(uint8_t u){chars[0].deliver({0,u,0,0,0,0,0});}
static void expectFailure(hidprobe::SetupStatus expected){assert(!subscribeAllInputs(&client));(void)expected;assert(!accepting);assert(depth==0);}
static void verifyExtraDoesNothing(BLERemoteCharacteristic& c,std::vector<uint8_t> b){
 const auto e=epoch.load(),errors=bad.load(),g=gattCalls,done=received.load();const auto n=events.size();const auto release=tracker.allReleased(),wait=requireRelease;const auto ph=phase.load();
 forbidAlloc=true;inCallback=true;c.callback(&c,b.data(),b.size(),true);inCallback=false;forbidAlloc=false;
 assert(epoch==e&&events.size()==n&&bad==errors&&gattCalls==g&&received==done&&tracker.allReleased()==release&&requireRelease==wait&&phase==ph);
}
int main(int argc,char** argv){
 assert(argc==2);std::ifstream in(argv[1],std::ios::binary);realMap.assign(std::istreambuf_iterator<char>(in),{});assert(realMap.size()==253);
 reset();assert(subscribeAllInputs(&client));assert(subscriptionCount==1&&inputSubscriptionCount==5&&writes==5);
 for(unsigned i=0;i<5;++i){assert(chars[i].callback&&chars[i].cccd.writeCount==1&&chars[i].cccd.readCount==1);assert(chars[i].cccd.value==String("\1\0",2));}
 primeActual();assert(accepting&&phase==Phase::Ready);for(unsigned i=1;i<5;++i)assert(chars[i].reportReads==0);puts("PASS real 253B descriptor: all five inputs configured/read back; only keyboard primed");
 press(4);assert(events.size()==1&&!tracker.allReleased());
 for(unsigned i=1;i<5;++i){verifyExtraDoesNothing(chars[i],{66});verifyExtraDoesNothing(chars[i],{});verifyExtraDoesNothing(chars[i],std::vector<uint8_t>(64,255));}
 assert(events.size()==1&&events[0].usage==4);press(0);press(41);press(0);assert(events.size()==4&&tracker.allReleased()&&events[2].usage==41);puts("PASS extra/empty/oversize inputs allocate nothing; preserve held state, epoch, queue, errors and following ordinary release/down");
 invalidate();press(4);assert(events.size()==4);verifyExtraDoesNothing(chars[1],{1,2,3,4});retiredClearActual();assert(subscriptionCount==0&&inputSubscriptionCount==0);for(auto& s:inputSubscriptions)assert(!s.characteristic&&!s.cccd);puts("PASS cancellation and retired-client slot clearing retain original protection");
 reset(5,true);assert(subscribeAllInputs(&client));for(unsigned i=0;i<5;++i)assert(inputSubscriptions[i].characteristic->getHandle()==300+i*5);puts("PASS rediscovery uses actual handles; no model/handle constants");
 reset(7);chars[5].ref.value[1]=2;chars[6].ref.value[1]=3;assert(subscribeAllInputs(&client)&&inputSubscriptionCount==5);assert(!chars[5].callback&&!chars[6].callback);puts("PASS Output and Feature have no callback or payload write");
 reset(16);assert(subscribeAllInputs(&client)&&inputSubscriptionCount==16&&writes==16);reset(17);expectFailure(hidprobe::SetupInputCapacity);assert(writes==0);reset(9);for(unsigned i=0;i<9;++i)chars[i].ref.value[0]=1;expectFailure(hidprobe::SetupKeyboardCapacity);assert(writes==0);puts("PASS separate 16-input and original 8-keyboard capacities fail closed before CCCD writes");
 reset();chars[3].hasRef=false;expectFailure(hidprobe::SetupNoRef);reset();chars[3].ref.value=String(1,'x');expectFailure(hidprobe::SetupBadRef);reset();chars[3].ref.value[1]=4;expectFailure(hidprobe::SetupBadRef);reset();chars[3].notifiable=false;expectFailure(hidprobe::SetupNoNotify);reset();chars[3].hasCccd=false;expectFailure(hidprobe::SetupNoCccd);assert(writes==0);puts("PASS missing/malformed reference, non-notifiable Input and missing CCCD are explicit failures");
 reset();client.service.protocol.protocolWorks=false;expectFailure(hidprobe::SetupProtocolFailed);reset();chars[2].cccd.writable=false;expectFailure(hidprobe::SetupWriteFailed);assert(chars[0].cccd.readCount==0&&writes==3);retiredClearActual();assert(!inputSubscriptionCount);puts("PASS subscription failure is returned; no readback or admission after failed write; partial setup retires");
 reset();chars[2].cccd.readOverride=String(1,0);expectFailure(hidprobe::SetupReadFailed);reset();chars[2].cccd.readOverride=String(2,0);expectFailure(hidprobe::SetupMismatch);reset();chars[2].cccd.disconnectRead=true;expectFailure(hidprobe::SetupDisconnected);puts("PASS short/error readback, disabled CCCD and disconnect cannot appear verified");
 reset();chars[2].throwSubscribe=true;bool threw=false;try{subscribeAllInputs(&client);}catch(const std::bad_alloc&){threw=true;}assert(threw&&!accepting);retiredClearActual();reset();chars[2].cccd.throwRead=true;threw=false;try{subscribeAllInputs(&client);}catch(const std::bad_alloc&){threw=true;}assert(threw&&!accepting);puts("PASS setup allocation exceptions retain original worker catch/retirement contract");
 reset();hostTask=true;auto g=gattCalls;assert(!subscribeAllInputs(&client)&&gattCalls==g);hostTask=false;accepting=true;subscriptionCount=2;assert(!subscribeAllInputs(&client)&&subscriptionCount==2&&gattCalls==g);puts("PASS host-task/active-input call cannot perform GATT or reset admitted slots");
 reset();client.service.map.value.clear();client.service.hasBoot=true;makeChar(client.service.boot,900,0);client.service.boot.uuid=0x2a22;client.service.boot.value=String(8,0);assert(subscribeAllInputs(&client)&&inputSubscriptionCount==1&&subscriptionCount==1);assert(client.service.protocol.protocolWrites.back()==0&&client.service.boot.cccd.readCount==1);primeActual();assert(phase==Phase::Ready);puts("PASS explicit Boot fallback and release priming preserved");
}
