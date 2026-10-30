#include "hid_keyboard.hpp"
#include "hanime_hid_probe.hpp"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <deque>
#include <cstring>
#if DRAFTOR_WS_HID_DIAG
#include "hid_probe_core.hpp"
namespace hidprobe {Core fixtureProbe;void inc(Stat s)noexcept{fixtureProbe.add(s);}void rxPoint(Rx p,uint16_t h)noexcept{fixtureProbe.noteRx(p,h,100);}}
#endif
struct BLERemoteCharacteristic {};
namespace keyboard {
enum class Phase:uint8_t{Starting,Scanning,Connecting,Pairing,AwaitRelease,Ready,Disconnected,Cleaning,Error};
std::atomic<bool> accepting{true};std::atomic<uint32_t> epoch{1},bad{0},overflow{0},received{0},peak{0};std::atomic<Phase> phase{Phase::Ready};
uint32_t nextEventId=0;bool requireRelease=false;s3hid::Tracker tracker;s3hid::IngressContext ingress;
struct Subscription{BLERemoteCharacteristic* characteristic; s3hid::Report report;};Subscription subscriptions[8];unsigned subscriptionCount=1;
std::deque<s3hid::Event> events;void* queue=reinterpret_cast<void*>(1);int stateMux=0,depth=0;
#define portENTER_CRITICAL(x) do{assert(*(x)==0);++*(x);++depth;}while(0)
#define portEXIT_CRITICAL(x) do{assert(*(x)==1);--*(x);--depth;}while(0)
constexpr int pdTRUE=1;uint32_t millis(){return 100;}uint64_t esp_timer_get_time(){return 100000;}
int xQueueSend(void*,const s3hid::Event* e,int){if(events.size()==512)return 0;events.push_back(*e);return 1;}
unsigned uxQueueMessagesWaiting(void*){return unsigned(events.size());}
int xQueueReceive(void*,s3hid::Event* e,int){if(events.empty())return 0;*e=events.front();events.pop_front();return 1;}
#include "notify_actual.inc"
#include "admit_actual.inc"
}
using namespace keyboard;
static BLERemoteCharacteristic chars[2];
static const uint8_t mapBytes[]={5,1,9,6,0xa1,1,5,7,0x19,0xe0,0x29,0xe7,0x75,1,0x95,8,0x81,2,0x75,8,0x95,1,0x81,1,0x19,0,0x29,0xff,0x75,8,0x95,6,0x81,0,0xc0};
void reset(unsigned slots=1){s3hid::Map m;assert(m.parse(mapBytes,sizeof(mapBytes)));tracker.clear();events.clear();epoch=1;bad=overflow=received=peak=0;accepting=true;phase=Phase::Ready;requireRelease=false;nextEventId=0;subscriptionCount=slots;for(unsigned i=0;i<slots;++i)subscriptions[i]={&chars[i],*m.report(0)};assert(depth==0);}
void send(unsigned slot,uint8_t key,unsigned bytes=8){uint8_t b[8]={0,0,key};notify(&chars[slot],b,bytes,true);assert(depth==0);}
int main(){
 reset();send(0,4);send(0,0);assert(events.size()==2&&events[0].usage==4&&events[0].down&&!events[1].down);events.clear();send(0,66);send(0,0);assert(events.size()==2&&events[0].usage==66);puts("PASS ordinary and HID F9 decode/diff; release returns");
 reset();send(0,4,2);assert(bad==1&&phase==Phase::AwaitRelease&&epoch==2);send(0,4);assert(events.empty()&&requireRelease);send(0,0);assert(phase==Phase::Ready&&!requireRelease&&events.empty());send(0,4);assert(events.size()==1);puts("PASS malformed length fails closed then same-slot release recovers");
 reset();send(0,1);assert(bad==1&&requireRelease);send(0,0);send(0,41);assert(!requireRelease&&events.size()==1&&events[0].usage==41);puts("PASS rollover requires release, then Esc works");
 reset(2);send(1,4,1);send(0,0);assert(requireRelease&&phase==Phase::AwaitRelease);send(0,4);send(0,0);assert(events.empty());send(1,0);assert(!requireRelease&&phase==Phase::Ready);send(0,4);assert(events.size()==1);puts("PASS unknown slot cannot be cleared by unrelated slot; later offending-slot release recovers");
 reset();for(unsigned i=0;i<512;++i)events.push_back({1,100,4,0,true});send(0,4);assert(overflow==1&&epoch==2&&requireRelease);uint32_t seen=0;assert(!admit(events.front(),seen)&&seen==2);events.clear();send(0,0);send(0,4);assert(!requireRelease&&events.size()==1&&admit(events.front(),seen));puts("PASS full queue invalidates epoch; release recovers without replay");
 s3hid::Map m;const uint8_t composite[]={5,1,9,6,0xa1,1,0x85,1,5,7,0x19,0,0x29,0xff,0x75,8,0x95,1,0x81,0,0xc0,5,0xc,9,1,0xa1,1,0x85,2,0x19,0,0x2a,0xff,3,0x75,16,0x95,1,0x81,0,0xc0};assert(m.parse(composite,sizeof(composite)));assert(m.report(1)&&!m.report(2));s3hid::State state;const uint8_t key[]={66};assert(s3hid::Map::decode(*m.report(1),key,1,state)&&state.has(66));puts("PASS separate consumer report is not selected as keyboard; synthetic mapping only");
 reset();accepting=false;send(0,4);assert(events.empty());puts("PASS disabled ingress retained; no callback loop/hang in directed cases");
#if DRAFTOR_WS_HID_DIAG
 assert(hidprobe::fixtureProbe.stats[hidprobe::Bad]==3);assert(hidprobe::fixtureProbe.stats[hidprobe::QueueOverflow]==1);puts("PASS enabled diagnostic counters observe faults without changing recovery");
#endif
}
