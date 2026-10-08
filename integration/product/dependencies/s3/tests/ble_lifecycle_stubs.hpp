#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <stdexcept>
#include <string>
#include "../libraries/BLE/src/hanime_ble_retirement.hpp"
#define REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(2);} } while(0)
#define log_v(...) ((void)0)
#define log_d(...) ((void)0)
#define log_i(...) ((void)0)
#define log_e(...) ((void)0)
constexpr int BLE_HS_EBUSY=1,BLE_HS_EUNKNOWN=2,BLE_HS_EDONE=3,BLE_HS_EALREADY=4,BLE_HS_ETIMEOUT=5,BLE_HS_ENOTCONN=6,BLE_HS_ECONTROLLER=7,BLE_HS_ETIMEOUT_HCI=8,BLE_HS_ENOTSYNCED=9,BLE_HS_EOS=10;
constexpr uint16_t BLE_HS_CONN_HANDLE_NONE=65535;
constexpr uint8_t BLE_ERR_REM_USER_CONN_TERM=19;
constexpr uint32_t portMAX_DELAY=0xffffffff;
enum {BLE_GAP_EVENT_DISCONNECT=1,BLE_GAP_EVENT_CONNECT,BLE_GAP_EVENT_TERM_FAILURE,BLE_GAP_EVENT_MTU};
struct ble_gap_event {int type=0;struct{int status=0;uint16_t conn_handle=0;}connect;
 struct {int reason=0;struct {uint16_t conn_handle=0;}conn;}disconnect;
 struct {int status=0;uint16_t conn_handle=0;}term_failure;
 struct {uint16_t conn_handle=0,value=0;}mtu;
};
struct ble_addr_t{uint8_t val[6]{};uint8_t type=0;};
struct ble_gap_conn_params{};
struct BLEAddress{std::string text;BLEAddress(const char* p):text(p){}std::string toString()const{return text;}uint8_t getType()const{return 0;}uint8_t* getNative(){static uint8_t bytes[6]{};return bytes;}bool operator==(const BLEAddress& o)const{return text==o.text;}};
struct BLETaskData{void* m_pInstance;int m_flags=0;explicit BLETaskData(void* p):m_pInstance(p){}};
struct BLEClient;
struct ClientCallbacks{void onConnect(BLEClient*){}void onDisconnect(BLEClient*){}};
struct FakeScan{bool stop(){return true;}};
struct BLEDevice{inline static bool m_synced=true;inline static uint8_t m_ownAddrType=0;static FakeScan* getScan(){static FakeScan s;return &s;}static void onReset(int){}static void addPeerDevice(BLEClient*,bool,int){}static void removePeerDevice(int,bool){}};
struct BLESecurity{inline static bool m_securityEnabled=false,m_forceSecurity=false;static void startSecurity(int){}static void resetSecurity(){}};
struct BLEUtils{static const char* returnCodeToString(int){return "stub";}static const char* gapEventToString(int){return "stub";}static bool taskWait(const BLETaskData&,uint32_t);static void taskRelease(const BLETaskData&,int);};
int ble_gap_conn_find_by_addr(const ble_addr_t*,void*);
int ble_gap_connect(int,const ble_addr_t*,int,const ble_gap_conn_params*,int(*)(ble_gap_event*,void*),void*);
int ble_gap_conn_cancel();int ble_gap_terminate(uint16_t,uint8_t);int ble_gattc_exchange_mtu(uint16_t,void*,void*);void ble_hs_sched_reset(int);
struct BLEClient {
 std::atomic<uint16_t> m_conn_id{BLE_HS_CONN_HANDLE_NONE};std::atomic<bool> m_isConnected{false};
 BLEAddress m_peerAddress{""};ClientCallbacks callbacks;ClientCallbacks* m_pClientCallbacks=&callbacks;int m_gattAppId=1;
 int m_lastErr=0;int32_t m_connectTimeout=30000;uint8_t m_terminateFailCount=0;ble_gap_conn_params m_pConnParams;
 mutable std::atomic<BLETaskData*> m_pTaskData{nullptr};BLETaskData m_connectTask{this};std::atomic<int> m_connectResult{BLE_HS_ETIMEOUT};
 std::atomic<bool> m_connectPending{false},m_retiring{false};bool m_connectAttempted=false;hanime_ble::Retirement m_retirement;
 bool isConnected(){return m_isConnected;}uint16_t getConnId(){return m_conn_id;}
 bool connect(BLEAddress,uint8_t,uint32_t);int disconnect(uint8_t reason=BLE_ERR_REM_USER_CONN_TERM);bool retireStep(uint32_t);
 static int handleGAPEvent(ble_gap_event*,void*);~BLEClient();
};
struct ble_npl_event{void(*fn)(ble_npl_event*)=nullptr;};
inline uint32_t nowMs=0;uint32_t millis(){return nowMs;}
inline std::deque<std::function<void()>> hostQueue;
int nimble_port_get_dflt_eventq(){return 0;}
void ble_npl_eventq_put(int,ble_npl_event* e){hostQueue.push_back([e]{e->fn(e);});}
void ble_npl_event_init(ble_npl_event* e,void(*f)(ble_npl_event*),void*){e->fn=f;}
struct Sub{};inline Sub subscriptions[8];inline unsigned subscriptionCount=0,clientsCleaned=0;
inline int stateMux=0;void portENTER_CRITICAL(int*){}void portEXIT_CRITICAL(int*){}
enum class Phase{Cleaning,Disconnected,Error};inline Phase phase=Phase::Error;
struct SerialStub{void println(const char*){}};inline SerialStub Serial;
void invalidate(){}void lifecycle(const char*){}
