#include "behavior_observer.hpp"
#include "ble_keyboard_host.hpp"
#include "hanime_hid_probe.hpp"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <host/ble_hs_adv.h>
#include <host/ble_hs_id.h>
#include <cstring>
#include <cctype>
#include <BLESecurity.h>
#include <Preferences.h>
#include <atomic>
#include <freertos/queue.h>
#include <nimble/nimble_port.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <hanime_ble_ownership.hpp>
#include "ble_reconnect_policy.hpp"
#include <host/ble_store.h>
#include <host/ble_hs_pvcy.h>
#if DRAFTOR_WS_HOGP_INPUTS
// Same pinned IDF export used by the inherited characteristic/descriptor reads.
extern "C" int ble_hs_is_parent_task(void);
#endif
namespace keyboard {
namespace {
constexpr size_t Capacity=512;
StaticQueue_t queueControl;
uint8_t queueBytes[Capacity*sizeof(s3hid::Event)];
QueueHandle_t queue=nullptr;
std::atomic<Phase> phase{Phase::Starting};
s3hid::IngressContext ingress; // read/write only while holding stateMux
std::atomic<uint32_t> epoch{1},received{0},overflow{0},bad{0},peak{0},passkey{1000000};
uint32_t nextEventId=0; // stateMux protects receipt IDs, including overflow gaps.
std::atomic<bool> accepting{false},authOK{false},authDone{false};
s3hid::Map reportMap;
s3hid::Tracker tracker;
portMUX_TYPE stateMux=portMUX_INITIALIZER_UNLOCKED;
struct Subscription {BLERemoteCharacteristic* characteristic=nullptr;s3hid::Report report{};};
Subscription subscriptions[8];unsigned subscriptionCount=0;
bool requireRelease=false;
std::atomic<bool> pairingNotSaved{false};
std::atomic<unsigned> storeFailures{0};
ble_store_write_fn* originalStoreWrite=nullptr;
int observedStoreWrite(int type,const union ble_store_value* value){
 const int rc=originalStoreWrite?originalStoreWrite(type,value):BLE_HS_ENOTSUP;
 if(type==BLE_STORE_OBJ_TYPE_OUR_SEC||type==BLE_STORE_OBJ_TYPE_PEER_SEC){
  const unsigned mask=1u<<type;
  if(rc){storeFailures.fetch_or(mask);pairingNotSaved=true;}else storeFailures.fetch_and(~mask);
  BT_PRINTF("[BLE_STORE] type=%d rc=%d (27=store capacity)\n",type,rc);
 }
 return rc; // preserve NimBLE's existing capacity/replacement callback contract
}
class DeviceCallbacks:public BLEDeviceCallbacks {
 int onStoreStatus(ble_store_status_event* event,void* arg)override{
  const int rc=BLEDeviceCallbacks::onStoreStatus(event,arg);
  int ours=-1,peers=-1;
  ble_store_util_count(BLE_STORE_OBJ_TYPE_OUR_SEC,&ours);ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC,&peers);
  BT_PRINTF("[BLE_STORE_STATUS] event=%d rc=%d our=%d peer=%d max_bonds=%d\n",event->event_code,rc,ours,peers,CONFIG_BT_NIMBLE_MAX_BONDS);
  return rc;
 }
} deviceCallbacks;
bool storedPeerKey(const ble_addr_t& identity){
 ble_store_key_sec key{};key.peer_addr=identity;key.peer_addr.type&=1;
 ble_store_value_sec value{};
 const int rc=ble_store_read_peer_sec(&key,&value);
 BT_PRINTF("[BLE_BOND_READ] peer=%s type=%u rc=%d ltk=%u irk=%u authenticated=%u\n",BLEAddress(identity).toString().c_str(),key.peer_addr.type,rc,rc?0:value.ltk_present,rc?0:value.irk_present,rc?0:value.authenticated);
 return rc==0&&value.ltk_present&&value.authenticated;
}
void invalidate(){
 accepting=false;
 portENTER_CRITICAL(&stateMux);
 ++epoch;tracker.clear();requireRelease=false;
 portEXIT_CRITICAL(&stateMux);
}
void notify(BLERemoteCharacteristic* c,uint8_t* data,size_t size,bool){
 if(!accepting.load())return;
 hidprobe::rxPoint(hidprobe::StateLockWait);
 portENTER_CRITICAL(&stateMux);
 hidprobe::rxPoint(hidprobe::StateLockHeld);
 if(!accepting.load()){portEXIT_CRITICAL(&stateMux);return;}
 unsigned slot=0;while(slot<subscriptionCount&&subscriptions[slot].characteristic!=c)++slot;
 if(slot==subscriptionCount){portEXIT_CRITICAL(&stateMux);return;}
 s3hid::State state;
 if(!s3hid::Map::decode(subscriptions[slot].report,data,size,state)){
  hidprobe::inc(hidprobe::Bad);hidprobe::rxPoint(hidprobe::DecodeFailed);
  ++bad;++epoch;s3hid::State unknown;unknown.set(4);tracker.update(slot,unknown,epoch.load(),millis(),[](s3hid::Event){});requireRelease=true;phase=Phase::AwaitRelease;portEXIT_CRITICAL(&stateMux);return;
 }
 hidprobe::inc(hidprobe::Decoded);
 if(requireRelease){
  hidprobe::inc(hidprobe::AwaitDrop);hidprobe::rxPoint(hidprobe::AwaitRelease);
  tracker.update(slot,state,epoch.load(),millis(),[](s3hid::Event){});
  if(!tracker.allReleased()){portEXIT_CRITICAL(&stateMux);return;}
  tracker.clear();requireRelease=false;phase=Phase::Ready;
 }
 hidprobe::rxPoint(hidprobe::Dispatch);
 bool over=false;
 s3hid::dispatchReport(tracker,slot,state,epoch.load(),millis(),ingress,[&](s3hid::Event e){
  e.event_id=++nextEventId;e.received_us=uint64_t(esp_timer_get_time());
  if(over)return;
  if(xQueueSend(queue,&e,0)!=pdTRUE){hidprobe::inc(hidprobe::QueueOverflow);++overflow;over=true;return;}
  hidprobe::inc(hidprobe::Enqueued);
  ++received;auto count=uxQueueMessagesWaiting(queue);if(count>peak)peak=count;
 });
 if(over){++epoch;requireRelease=!tracker.allReleased();if(requireRelease)phase=Phase::AwaitRelease;else tracker.clear();}
 portEXIT_CRITICAL(&stateMux);
 hidprobe::rxPoint(hidprobe::NotifyDone);
}
class ClientCallbacks:public BLEClientCallbacks{
 void onConnect(BLEClient*)override{phase=Phase::Pairing;}
 void onDisconnect(BLEClient*)override{invalidate();passkey=1000000;phase=Phase::Disconnected;}
} clientCallbacks;
class SecurityCallbacks:public BLESecurityCallbacks{
 void onPassKeyNotify(uint32_t pin)override{passkey=pin;phase=Phase::Pairing;BT_PRINTF("[BLE] Enter %06lu on POP ICON KEYS, then Enter\n",(unsigned long)pin);}
 uint32_t onPassKeyRequest()override{return 0;} // DisplayOnly host never requests typed PIN.
 bool onSecurityRequest()override{return true;}
 bool onConfirmPIN(uint32_t)override{return false;} // No silent numeric-comparison approval.
 void onAuthenticationComplete(ble_gap_conn_desc* result)override{
  authOK=result && result->sec_state.encrypted && result->sec_state.authenticated;authDone=true;
  BT_PRINTF("[BLE_AUTH] encrypted=%u authenticated=%u bonded=%u\n",result?result->sec_state.encrypted:0,result?result->sec_state.authenticated:0,result?result->sec_state.bonded:0);
 }
} securityCallbacks;
// DRAFTOR_HOGP_INPUTS_BEGIN
#ifndef DRAFTOR_WS_HOGP_INPUTS
#define DRAFTOR_WS_HOGP_INPUTS 0
#endif
#if DRAFTOR_WS_HOGP_INPUTS
// Keyboard slots retain the original 8-slot tracker. All Input Reports have
// separate bounded worker-owned subscription records; no callback borrows them.
constexpr unsigned InputSubscriptionCapacity=16;
struct InputSubscription {BLERemoteCharacteristic* characteristic=nullptr;BLERemoteDescriptor* cccd=nullptr;uint8_t id=0;bool keyboard=false;};
InputSubscription inputSubscriptions[InputSubscriptionCapacity];unsigned inputSubscriptionCount=0;
void clearInputSubscriptions(){for(auto& s:inputSubscriptions)s={};inputSubscriptionCount=0;}
void notifyOtherInput(BLERemoteCharacteristic*,uint8_t*,size_t,bool){
 // BLEClient already captured a bounded raw prefix. No GATT, tracker, queue,
 // epoch, command, allocation or host-task wait is allowed on this path.
 hidprobe::inc(hidprobe::OtherInput);
}
bool inputSetupFailure(hidprobe::SetupStatus status,uint16_t handle=0){hidprobe::setupDone(status,handle,epoch.load());return false;}
void inputReceipt(const InputSubscription& s,hidprobe::InputStatus status,size_t n=0,uint16_t value=0){
 hidprobe::inputSubscription(s.characteristic->getHandle(),s.cccd->getHandle(),s.id,s.keyboard,status,n,value);
}
bool configureInputs(BLEClient* client,bool boot){
 for(unsigned i=0;i<inputSubscriptionCount;++i){
  const auto& s=inputSubscriptions[i];
  if(!client->isConnected()){inputReceipt(s,hidprobe::InputDisconnected);return inputSetupFailure(hidprobe::SetupDisconnected,s.characteristic->getHandle());}
  // The pinned NimBLE registerForNotify() discards this boolean. subscribe()
  // reports the CCCD write result and writes 0x0001 once, with response.
  if(!s.characteristic->subscribe(true,s.keyboard?notify:notifyOtherInput,true)){
   inputReceipt(s,hidprobe::InputWriteFailed);return inputSetupFailure(hidprobe::SetupWriteFailed,s.characteristic->getHandle());
  }
  inputReceipt(s,hidprobe::InputEnabled);
 }
 // One readback per CCCD, on this worker after all subscriptions, before input
 // admission. A write ACK alone is not reported as verified configuration.
 for(unsigned i=0;i<inputSubscriptionCount;++i){
  const auto& s=inputSubscriptions[i];
  if(!client->isConnected()){inputReceipt(s,hidprobe::InputDisconnected);return inputSetupFailure(hidprobe::SetupDisconnected,s.characteristic->getHandle());}
  const String value=s.cccd->readValue();
  if(!client->isConnected()){inputReceipt(s,hidprobe::InputDisconnected);return inputSetupFailure(hidprobe::SetupDisconnected,s.characteristic->getHandle());}
  if(value.length()!=2){inputReceipt(s,hidprobe::InputReadFailed,value.length());return inputSetupFailure(hidprobe::SetupReadFailed,s.characteristic->getHandle());}
  const uint16_t flags=uint8_t(value[0])|(uint16_t(uint8_t(value[1]))<<8);
  if(flags!=1){inputReceipt(s,hidprobe::InputMismatch,2,flags);return inputSetupFailure(hidprobe::SetupMismatch,s.characteristic->getHandle());}
  inputReceipt(s,hidprobe::InputVerified,2,flags);
 }
 hidprobe::setupDone(boot?hidprobe::SetupBoot:hidprobe::SetupReport,0,epoch.load());return true;
}
bool subscribeAllInputs(BLEClient* client){
 // Never synchronously perform discovery/read/write on NimBLE's host task.
 // An invalid caller must not reset an already admitted keyboard or mutate
 // the worker-owned diagnostic metadata from the host task.
 if(ble_hs_is_parent_task()||accepting.load())return false;
 hidprobe::discovery();clearInputSubscriptions();subscriptionCount=0;
 try{
  if(!client->isConnected())return inputSetupFailure(hidprobe::SetupDisconnected);
  auto service=client->getService(BLEUUID(uint16_t(0x1812)));if(!service)return inputSetupFailure(hidprobe::SetupNoService);
  auto mapChar=service->getCharacteristic(BLEUUID(uint16_t(0x2a4b)));
  auto protocol=service->getCharacteristic(BLEUUID(uint16_t(0x2a4e)));bool parsed=false;
  if(mapChar){String bytes=mapChar->readValue();parsed=reportMap.parse((const uint8_t*)bytes.c_str(),bytes.length());hidprobe::map((const uint8_t*)bytes.c_str(),bytes.length(),parsed);}
  if(parsed){
   if(protocol&&!protocol->writeValue(uint8_t(1),false))return inputSetupFailure(hidprobe::SetupProtocolFailed);
   auto characteristics=service->getCharacteristicsByHandle();
   if(!characteristics)return inputSetupFailure(hidprobe::SetupNoService);
   for(auto& pair:*characteristics){
    auto c=pair.second;if(!c->getUUID().equals(BLEUUID(uint16_t(0x2a4d))))continue;
    auto ref=c->getDescriptor(BLEUUID(uint16_t(0x2908)));if(!ref)return inputSetupFailure(hidprobe::SetupNoRef,c->getHandle());
    String reference=ref->readValue();
    if(reference.length()!=2||uint8_t(reference[1])<1||uint8_t(reference[1])>3)return inputSetupFailure(hidprobe::SetupBadRef,c->getHandle());
    if(uint8_t(reference[1])!=1)continue; // Output/Feature: never subscribe or send a payload.
    if(!c->canNotify())return inputSetupFailure(hidprobe::SetupNoNotify,c->getHandle());
    if(inputSubscriptionCount==InputSubscriptionCapacity)return inputSetupFailure(hidprobe::SetupInputCapacity,c->getHandle());
    auto report=reportMap.report(uint8_t(reference[0]));
    if(report&&subscriptionCount==8)return inputSetupFailure(hidprobe::SetupKeyboardCapacity,c->getHandle());
    auto cccd=c->getDescriptor(BLEUUID(uint16_t(0x2902)));if(!cccd)return inputSetupFailure(hidprobe::SetupNoCccd,c->getHandle());
    hidprobe::reference(c->getHandle(),uint8_t(reference[0]),1,report?report->bits:0,report!=nullptr);
    auto& s=inputSubscriptions[inputSubscriptionCount++];s={c,cccd,uint8_t(reference[0]),report!=nullptr};inputReceipt(s,hidprobe::InputFound);
    if(report){auto& sub=subscriptions[subscriptionCount++];sub.characteristic=c;sub.report=*report;}
   }
   if(subscriptionCount)return configureInputs(client,false);
  }
  // Preserve the explicit Boot fallback when no keyboard Report is usable.
  // No Report CCCD has been written yet. Replace stale discovery metadata.
  clearInputSubscriptions();subscriptionCount=0;hidprobe::discovery();
  auto boot=service->getCharacteristic(BLEUUID(uint16_t(0x2a22)));
  if(!boot||!protocol||!boot->canNotify())return inputSetupFailure(hidprobe::SetupNoBoot);
  if(!protocol->writeValue(uint8_t(0),false))return inputSetupFailure(hidprobe::SetupProtocolFailed);
  static const uint8_t bootMap[]={0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,0x75,0x01,0x95,0x08,0x81,0x02,0x75,0x08,0x95,0x01,0x81,0x01,0x19,0x00,0x29,0xff,0x75,0x08,0x95,0x06,0x81,0x00,0xc0};
  if(!reportMap.parse(bootMap,sizeof(bootMap)))return inputSetupFailure(hidprobe::SetupNoBoot);
  auto cccd=boot->getDescriptor(BLEUUID(uint16_t(0x2902)));if(!cccd)return inputSetupFailure(hidprobe::SetupNoCccd,boot->getHandle());
  const auto report=reportMap.report(0);subscriptions[0]={boot,*report};subscriptionCount=1;
  hidprobe::map(bootMap,sizeof(bootMap),true);hidprobe::reference(boot->getHandle(),0,1,report->bits,true);
  inputSubscriptions[0]={boot,cccd,0,true};inputSubscriptionCount=1;inputReceipt(inputSubscriptions[0],hidprobe::InputFound);
  return configureInputs(client,true);
 }catch(const std::bad_alloc&){hidprobe::setupDone(hidprobe::SetupAllocationFailed,0,epoch.load());throw;} // original worker retires the client
}
#endif
// DRAFTOR_HOGP_INPUTS_END
bool subscribe(BLEClient* client){
#if DRAFTOR_WS_HOGP_INPUTS
 return subscribeAllInputs(client);
#else
 hidprobe::discovery();
 subscriptionCount=0;
 auto service=client->getService(BLEUUID(uint16_t(0x1812)));if(!service)return false;
 auto mapChar=service->getCharacteristic(BLEUUID(uint16_t(0x2a4b)));
 auto protocol=service->getCharacteristic(BLEUUID(uint16_t(0x2a4e)));
 bool parsed=false;
 if(mapChar){String bytes=mapChar->readValue();parsed=reportMap.parse((const uint8_t*)bytes.c_str(),bytes.length());hidprobe::map((const uint8_t*)bytes.c_str(),bytes.length(),parsed);BT_PRINTF("[HID_MAP] bytes=%u parsed=%u\n",bytes.length(),parsed);}
 if(parsed){
  if(protocol&&!protocol->writeValue(uint8_t(1),false))return false;
  auto characteristics=service->getCharacteristicsByHandle(); // UUID map loses duplicate 2A4D reports.
  for(auto& pair:*characteristics){auto c=pair.second;if(!c->getUUID().equals(BLEUUID(uint16_t(0x2a4d)))||!c->canNotify())continue;
   auto ref=c->getDescriptor(BLEUUID(uint16_t(0x2908)));if(!ref)continue;
   String reference=ref->readValue();if(reference.length()==2){const auto known=reportMap.report(uint8_t(reference[0]));hidprobe::reference(pair.first,uint8_t(reference[0]),uint8_t(reference[1]),known?known->bits:0,known&&uint8_t(reference[1])==1);}
   if(reference.length()!=2||uint8_t(reference[1])!=1)continue;
   auto report=reportMap.report(uint8_t(reference[0]));if(!report)continue;
   if(subscriptionCount==8)return false;
   auto cccd=c->getDescriptor(BLEUUID(uint16_t(0x2902)));if(!cccd)return false;
   auto& sub=subscriptions[subscriptionCount++];sub.characteristic=c;sub.report=*report;
   c->registerForNotify(notify,true,false);
   uint8_t enabled[2]={1,0};if(!cccd->writeValue(enabled,2,true))return false;
   BT_PRINTF("[HID_INPUT] handle=%u report_id=%u bytes=%u\n",pair.first,report->id,(report->bits+7)/8);
  }
 }
 if(!subscriptionCount){
  auto boot=service->getCharacteristic(BLEUUID(uint16_t(0x2a22)));
  if(!boot||!protocol||!boot->canNotify()||!protocol->writeValue(uint8_t(0),false))return false;
  static const uint8_t bootMap[]={0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,0x75,0x01,0x95,0x08,0x81,0x02,0x75,0x08,0x95,0x01,0x81,0x01,0x19,0x00,0x29,0xff,0x75,0x08,0x95,0x06,0x81,0x00,0xc0};
  if(!reportMap.parse(bootMap,sizeof(bootMap)))return false;
  hidprobe::map(bootMap,sizeof(bootMap),true);hidprobe::reference(boot->getHandle(),0,1,reportMap.report(0)->bits,true);
  subscriptions[0]={boot,*reportMap.report(0)};subscriptionCount=1;
  auto cccd=boot->getDescriptor(BLEUUID(uint16_t(0x2902)));if(!cccd)return false;
  boot->registerForNotify(notify,true,false);uint8_t enabled[]={1,0};if(!cccd->writeValue(enabled,2,true))return false;
  BT_PRINTLN("[HID_INPUT] explicit Boot protocol fallback");
 }
 return true;
#endif
}
std::atomic<bool> scanFound{false};
ble_addr_t scanAddress{},knownAddress{};
bool haveKnownAddress=false;
int scanEvent(ble_gap_event* event,void*) {
 if(event->type!=BLE_GAP_EVENT_DISC||scanFound.load())return 0;
 ble_hs_adv_fields fields{};
 if(haveKnownAddress&&sameIdentity(knownAddress.type,knownAddress.val,event->disc.addr.type,event->disc.addr.val)){
  scanAddress=event->disc.addr;scanFound=true;return 0;
 }
 if(ble_hs_adv_parse_fields(&fields,event->disc.data,event->disc.length_data)!=0||!fields.name)return 0;
 char name[64]{};unsigned count=std::min<unsigned>(fields.name_len,sizeof(name)-1);
 for(unsigned i=0;i<count;++i)name[i]=std::toupper(static_cast<unsigned char>(fields.name[i]));
 if(std::strstr(name,"POP")&&std::strstr(name,"KEY")){scanAddress=event->disc.addr;scanFound=true;}
 return 0;
}
uint32_t clientsCreated=0,clientsCleaned=0,readyRounds=0;
// At most one retained client and one event. The keyboard task owns the pointer;
// the event borrows it until Complete. No next client is created before deletion.
enum class ReleaseStage : uint8_t { Idle, Queued, Complete };
struct ReleaseRequest {
 std::atomic<ReleaseStage> stage{ReleaseStage::Idle};
 BLEClient* client=nullptr;
 bool safe=false;
 uint32_t started=0,lastWarning=0;
} releaseRequest;
ble_npl_event releaseEvent{};
void releaseCallback(ble_npl_event*) {
 if(releaseRequest.stage.load()!=ReleaseStage::Queued)return;
 releaseRequest.safe=releaseRequest.client->retireStep(millis());
 releaseRequest.stage.store(ReleaseStage::Complete); // last client access in callback
}
void lifecycle(const char* stage){
 BT_PRINTF("[BLE_LIFECYCLE] stage=%s ready_round=%u created=%u cleaned=%u live_client=%u services=%u characteristics=%u descriptors=%u internal_free=%u internal_largest=%u psram_free=%u\n",
  stage,unsigned(readyRounds),unsigned(clientsCreated),unsigned(clientsCleaned),
  unsigned(hanime_ble::live.clients.load()),unsigned(hanime_ble::live.services.load()),unsigned(hanime_ble::live.characteristics.load()),unsigned(hanime_ble::live.descriptors.load()),
  heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
bool releaseClient(BLEClient*& client){
 if(!client)return true;
 if(!releaseRequest.client){releaseRequest.started=millis();releaseRequest.lastWarning=millis();phase=Phase::Cleaning;}
 const auto stage=releaseRequest.stage.load();
 if(stage==ReleaseStage::Complete){
  if(releaseRequest.safe){
   portENTER_CRITICAL(&stateMux);
   for(auto& sub:subscriptions){sub={};}
   subscriptionCount=0;
#if DRAFTOR_WS_HOGP_INPUTS
   clearInputSubscriptions();
#endif
   portEXIT_CRITICAL(&stateMux);
   delete client;client=nullptr;releaseRequest.client=nullptr;
   releaseRequest.stage=ReleaseStage::Idle;
   phase=Phase::Disconnected;
   ++clientsCleaned;lifecycle("cleaned");return true;
  }
  releaseRequest.stage=ReleaseStage::Idle;
 }
 if(releaseRequest.stage==ReleaseStage::Idle){
  invalidate();releaseRequest.client=client;releaseRequest.safe=false;
  releaseRequest.stage=ReleaseStage::Queued;
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&releaseEvent);
 }
 // Never enqueue twice or free while the host still owns this callback arg.
 if(uint32_t(millis()-releaseRequest.started)>=5000&&uint32_t(millis()-releaseRequest.lastWarning)>=5000){
  releaseRequest.lastWarning=millis();phase=Phase::Error;
  BT_PRINTLN("[BLE_RETIRE] cleanup pending; client retained, active recovery continues");
 }
 return false;
}

void task(void*){
 if(!BLEDevice::init("HANIME S3 Writing v0.6 SharedCore")){phase=Phase::Error;vTaskDelete(nullptr);return;}
 originalStoreWrite=ble_hs_cfg.store_write_cb;ble_hs_cfg.store_write_cb=observedStoreWrite;
 BLEDevice::setDeviceCallbacks(&deviceCallbacks);
 BT_PRINTF("[BLE_IDENTITY] local=%s resolve_rc=%d persist=%d max_bonds=%d\n",BLEDevice::getAddress().toString().c_str(),ble_hs_pvcy_set_resolve_enabled(1),CONFIG_BT_NIMBLE_NVS_PERSIST,CONFIG_BT_NIMBLE_MAX_BONDS);
 ble_npl_event_init(&releaseEvent,releaseCallback,nullptr);
 BLEDevice::setSecurityCallbacks(&securityCallbacks);
 BLESecurity::setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
 BLESecurity::setCapability(ESP_IO_CAP_OUT);
 BLESecurity::setInitEncryptionKey();BLESecurity::setRespEncryptionKey();
 BLESecurity::setKeySize(16);
 BLESecurity::setPassKey(false); // random PIN; never ship a fixed default passkey
 // Start authentication explicitly below; allow 90s for entering displayed PIN.
 BLESecurity::setForceAuthentication(false);
 Preferences preferences;bool prefs=preferences.begin("hanime-s3-kbd",false);
 String saved=prefs?preferences.getString("address",""):String();
 uint8_t savedType=prefs?preferences.getUChar("type",0):0;
 uint8_t peerRecord[7]{};
 if(prefs&&preferences.getBytesLength("peer")==sizeof(peerRecord)&&preferences.getBytes("peer",peerRecord,sizeof(peerRecord))==sizeof(peerRecord)&&peerRecord[0]<=1){
  ble_addr_t identity{};identity.type=peerRecord[0];std::memcpy(identity.val,peerRecord+1,6);
  saved=BLEAddress(identity).toString();savedType=identity.type;
 } // old address/type keys are a read-only migration fallback


 BT_PRINTF("[BLE_HOST] stack=NimBLE central=1 saved_peer=%u bonding=NVS\n",saved.length()!=0);
 BLEClient* client=nullptr;
 RetryPlan retry;
 if(saved.length()){BLEAddress old(saved);knownAddress.type=savedType&1;std::memcpy(knownAddress.val,old.getNative(),6);haveKnownAddress=true;storedPeerKey(knownAddress);}
 for(;;){
  hidprobe::monitor(unsigned(phase.load()),epoch.load(),accepting.load());
  if(client){
   if(!releaseClient(client)){delay(250);continue;}
   delay(1500);
  }
  try {
   client=BLEDevice::createClient();if(!client){phase=Phase::Error;delay(1500);continue;}++clientsCreated;client->setClientCallbacks(&clientCallbacks);
   String address=saved;uint8_t type=savedType;bool connected=false;
   const bool scanNow=retry.scan(address.length()!=0);
   if(!scanNow){
    BT_PRINTF("[BLE_RETRY] known=%s type=%u\n",address.c_str(),type);
    phase=Phase::Connecting;connected=client->connect(BLEAddress(address),type,4000);
    if(!connected){continue;} // retire before scanning or reconnecting
   }
   if(!connected){
    phase=Phase::Scanning;scanFound=false;
    ble_gap_disc_params params{};params.itvl=128;params.window=96;params.passive=0;params.filter_duplicates=1;
    uint8_t ownType=0;
    if(ble_hs_id_infer_auto(0,&ownType)==0&&ble_gap_disc(ownType,4000,&params,&scanEvent,nullptr)==0){
     uint32_t start=millis();while(ble_gap_disc_active()&&!scanFound&&millis()-start<4500)delay(20);
     if(ble_gap_disc_active())ble_gap_disc_cancel();
     if(scanFound){BLEAddress found(scanAddress);address=found.toString();type=scanAddress.type;
      BT_PRINTF("[BLE_FOUND] peer=%s type=%u known=%u; verify security and HID\n",address.c_str(),type,haveKnownAddress&&sameIdentity(knownAddress.type,knownAddress.val,scanAddress.type,scanAddress.val));phase=Phase::Connecting;
      connected=client->connect(found,type,5000);
     }
    }
   }
   if(connected){
    retry.ready();
    BLESecurity::setPassKey(false);
    invalidate();authDone=false;authOK=false;passkey=1000000;phase=Phase::Pairing;
    int error=0;
    BLESecurity::startSecurity(client->getConnId(),&error);
    BT_PRINTF("[BLE_SECURITY] start_rc=%d\n",error);
    uint32_t start=millis();
    while((error==0||error==BLE_HS_EALREADY)&&client->isConnected()&&!authDone&&millis()-start<90000){
     // A bonded peer can finish encryption before the connect call returns.
     // Read actual connection state so an earlier auth callback is not lost.
     ble_gap_conn_desc current{};
     if(ble_gap_conn_find(client->getConnId(),&current)==0&&current.sec_state.encrypted&&current.sec_state.authenticated){authOK=true;authDone=true;break;}
     delay(20);
    }
    if(authOK&&client->isConnected()&&subscribe(client)&&client->isConnected()){
     // Prime every keyboard report without emitting events. Held pairing Enter
     // or reconnect keys must be released before text input is enabled.
     s3hid::State initial[8];
     for(unsigned i=0;i<subscriptionCount;++i){
      auto c=subscriptions[i].characteristic;bool valid=false;
      if(c->canRead()){String bytes=c->readValue();valid=s3hid::Map::decode(subscriptions[i].report,(const uint8_t*)bytes.c_str(),bytes.length(),initial[i]);}
      if(!valid)initial[i].set(4); // unknown report requires a release sample
     }
     portENTER_CRITICAL(&stateMux);tracker.clear();++epoch;
     for(unsigned i=0;i<subscriptionCount;++i)tracker.update(i,initial[i],epoch.load(),millis(),[](s3hid::Event){});
     requireRelease=!tracker.allReleased();bool waiting=requireRelease;
     phase=waiting?Phase::AwaitRelease:Phase::Ready;accepting=true;
     portEXIT_CRITICAL(&stateMux);hidprobe::bind(epoch.load());passkey=1000000;
     ble_gap_conn_desc identity{};
     if(ble_gap_conn_find(client->getConnId(),&identity)==0){address=BLEAddress(identity.peer_id_addr).toString();type=identity.peer_id_addr.type;}
     type&=1;saved=address;savedType=type;
     BLEAddress stable(address);knownAddress.type=type;std::memcpy(knownAddress.val,stable.getNative(),6);haveKnownAddress=true;
     uint8_t encoded[7]{type},verified[7]{};std::memcpy(encoded+1,knownAddress.val,6);
     bool identityWritten=prefs&&preferences.putBytes("peer",encoded,sizeof(encoded))==sizeof(encoded)&&
       preferences.getBytes("peer",verified,sizeof(verified))==sizeof(verified)&&std::memcmp(encoded,verified,sizeof(encoded))==0;
     pairingNotSaved=!pairingDurable(identityWritten,storedPeerKey(knownAddress),storeFailures.load());
     BT_PRINTF("[BLE_IDENTITY_SAVE] identity=%s type=%u prefs=%u durable=%u security_failures=%u\n",address.c_str(),type,identityWritten,!pairingNotSaved.load(),storeFailures.load());
     client->updateConnParams(12,24,0,500);
     if(waiting)BT_PRINTLN("[BLE_WAIT_RELEASE] release all keys; tap and release Shift if needed");
     else BT_PRINTF("[BLE_READY] keyboard_reports=%u\n",subscriptionCount);
     if(!waiting){++readyRounds;lifecycle("ready");}
     while(client->isConnected()){
      hidprobe::monitor(unsigned(phase.load()),epoch.load(),accepting.load());
      if(waiting&&phase==Phase::Ready){BT_PRINTF("[BLE_READY] keyboard_reports=%u\n",subscriptionCount);waiting=false;++readyRounds;lifecycle("ready");}
      delay(50);
     }
    }else{phase=Phase::Error;BT_PRINTLN("[BLE_ERROR] authentication or HID discovery failed; retry known identity / scan");}
   }
  }catch(const std::bad_alloc&){phase=Phase::Error;invalidate();BT_PRINTLN("[BLE_ERROR] allocation failed; retiring client before retry");}
  // The next outer iteration advances retirement before any new attempt.
 }
}
}
static TaskHandle_t workerHandle=nullptr;
uint32_t workerStackMinimumBytes(){return workerHandle?uint32_t(uxTaskGetStackHighWaterMark(workerHandle)):0;}
bool begin(){queue=xQueueCreateStatic(Capacity,sizeof(s3hid::Event),queueBytes,&queueControl);return queue&&xTaskCreatePinnedToCore(task,"ble-keyboard",8192,nullptr,2,&workerHandle,0)==pdPASS;}
bool pop(s3hid::Event& e){const bool ok=queue&&xQueueReceive(queue,&e,0)==pdTRUE;if(ok)hidprobe::inc(hidprobe::Popped);return ok;}
bool admit(const s3hid::Event& event,uint32_t& observedEpoch){
 portENTER_CRITICAL(&stateMux);
 observedEpoch=epoch.load();
 const bool current=accepting.load()&&event.epoch==observedEpoch;
 portEXIT_CRITICAL(&stateMux);
 return current;
}
void publishFrame(uint32_t route,uint32_t generation){portENTER_CRITICAL(&stateMux);ingress.route=route;ingress.frame=generation;portEXIT_CRITICAL(&stateMux);}
void publishImeContext(uint32_t serial,uint32_t resource_owner){
 // Publish only after event/service returns. No retired owner is exposed as a
 // new EN owner while its cleanup is still running; notify copies this pair.
 portENTER_CRITICAL(&stateMux);ingress.ime_context=serial;ingress.resource_owner=resource_owner;portEXIT_CRITICAL(&stateMux);
}
void setImeAttempt(uint32_t serial){portENTER_CRITICAL(&stateMux);ingress.attempt=serial;portEXIT_CRITICAL(&stateMux);}
bool pending(){return queue&&uxQueueMessagesWaiting(queue)>0;}
Status status(){return {phase.load(),passkey.load(),epoch.load(),received.load(),overflow.load(),bad.load(),peak.load(),pairingNotSaved.load()};}
const char* phaseName(Phase p){switch(p){case Phase::Starting:return "BLE starting";case Phase::Scanning:return "Scanning for keyboard";case Phase::Cleaning:return "BLE reconnecting";case Phase::Connecting:return "BLE connecting";case Phase::Pairing:return "BLE pairing";case Phase::AwaitRelease:return "Release keys; tap Shift";case Phase::Ready:return pairingNotSaved.load()?"Connected; pairing not saved":"Keyboard connected";case Phase::Disconnected:return "Keyboard disconnected";default:return "BLE error; see log";}}
}
