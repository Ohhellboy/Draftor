#include "hid_keyboard.hpp"
#include "hanime_hid_probe.hpp"
#include <map>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>
#if DRAFTOR_WS_HID_DIAG
#include "hid_probe_core.hpp"
namespace hidprobe {
Core fixture;std::vector<Rx> stages;
void rxPoint(Rx p,uint16_t h)noexcept{fixture.noteRx(p,h,100);stages.push_back(p);}
void raw(uint16_t h,const uint8_t* b,size_t head,size_t total,size_t copied)noexcept{fixture.raw(h,b,head,total,100,copied);}
}
#endif
struct Mbuf {uint8_t* om_data;size_t om_len;Mbuf* next;size_t total;};
#define OS_MBUF_PKTLEN(m) ((m)->total)
int os_mbuf_copydata(Mbuf* m,size_t offset,size_t n,uint8_t* out){while(m&&offset>=m->om_len){offset-=m->om_len;m=m->next;}while(n&&m){const auto take=std::min(n,m->om_len-offset);std::memcpy(out,m->om_data+offset,take);out+=take;n-=take;m=m->next;offset=0;}return n?-1:0;}
struct Semaphore{bool held=false;void take(){assert(!held);held=true;}void give(){assert(held);held=false;}};
using String=std::string;
struct Characteristic{Semaphore m_semaphoreReadCharEvt;String m_value;void(*m_notifyCallback)(Characteristic*,uint8_t*,size_t,bool)=nullptr;};
struct Service{std::map<uint16_t,Characteristic*> m_characteristicMapByHandle;uint16_t getEndHandle(){return 100;}};
struct Client{uint16_t m_conn_id=7;bool m_isConnected=true;std::map<uint16_t,Service*> m_servicesMap;};
struct Event {struct {uint16_t conn_handle=7,attr_handle=23;Mbuf* om;bool indication=false;}notify_rx;};
#define log_d(...) do{}while(0)
int receive(Client* client,Event* event){
#include "gap_actual.inc"
}
static s3hid::Report report;static unsigned callbackBytes=0;static bool decoded=false;
void callback(Characteristic*,uint8_t* b,size_t n,bool){callbackBytes=unsigned(n);s3hid::State state;decoded=s3hid::Map::decode(report,b,n,state);}
int main(){
 const uint8_t desc[]={5,1,9,6,0xa1,1,5,7,0x19,0,0x29,0xff,0x75,8,0x95,8,0x81,0,0xc0};s3hid::Map map;assert(map.parse(desc,sizeof(desc)));report=*map.report(0);
 uint8_t bytes[8]={0,0,66,0,0,0,0,0};Mbuf one{bytes,8,nullptr,8};Characteristic characteristic;characteristic.m_notifyCallback=callback;Service service;service.m_characteristicMapByHandle[23]=&characteristic;Client client;client.m_servicesMap[1]=&service;Event event;event.notify_rx.om=&one;
 assert(receive(&client,&event)==0&&callbackBytes==8&&decoded);puts("PASS actual GAP handler contiguous 8-byte report reaches keyboard decode");
 Mbuf second{bytes+3,5,nullptr,0},first{bytes,3,&second,8};event.notify_rx.om=&first;assert(receive(&client,&event)==0&&callbackBytes==3&&!decoded);puts("CONDITIONAL_COUNTEREXAMPLE actual GAP handler forwards only 3/8 bytes of chained mbuf; keyboard decode rejects length (not device proof)");
#if DRAFTOR_WS_HID_DIAG
 hidprobe::Raw a,b;assert(hidprobe::fixture.rawRing.pop(a)&&hidprobe::fixture.rawRing.pop(b));assert(a.head==8&&a.total==8&&a.captured==8);assert(b.head==3&&b.total==8&&b.captured==8&&std::memcmp(b.data.data(),bytes,8)==0);
 const std::vector<hidprobe::Rx> expected={hidprobe::Gap,hidprobe::CacheWait,hidprobe::CacheAcquired,hidprobe::CacheStored,hidprobe::CacheDone,hidprobe::Callback,hidprobe::CallbackDone,hidprobe::GapDone};assert(hidprobe::stages.size()==16);assert(std::equal(expected.begin(),expected.end(),hidprobe::stages.begin()));assert(std::equal(expected.begin(),expected.end(),hidprobe::stages.begin()+8));puts("PASS R2 captures across chain and separates cache lock/acquire/store; original callback unchanged");
#endif
 characteristic.m_notifyCallback=nullptr;assert(receive(&client,&event)==0);puts("PASS unregistered notification still traverses existing cache path; no subscription added");
}
