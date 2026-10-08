#!/usr/bin/env python3
from pathlib import Path
import sys
p=Path(__file__).resolve().parents[1];source=(p/'ble_keyboard_host.cpp').read_text()
def function(signature):
 start=source.index(signature);brace=source.index('{',start);level=1;at=brace+1
 while level:
  level+=(source[at]=='{')-(source[at]=='}');at+=1
 return source[start:at]
code='''#include "ble_keyboard_host.hpp"
#include "ble_reconnect_policy.hpp"
#include <atomic>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
constexpr int BLE_GAP_EVENT_DISC=1,BLE_STORE_OBJ_TYPE_OUR_SEC=1,BLE_STORE_OBJ_TYPE_PEER_SEC=2,BLE_HS_ENOTSUP=8;
struct ble_addr_t {uint8_t type;uint8_t val[6];};
struct ble_gap_event {int type;struct {ble_addr_t addr;const uint8_t* data;unsigned length_data;} disc;};
struct ble_hs_adv_fields {const uint8_t* name=nullptr;unsigned name_len=0;};
inline int ble_hs_adv_parse_fields(ble_hs_adv_fields* f,const uint8_t* data,unsigned size){f->name=data;f->name_len=size;return 0;}
union ble_store_value {int unused;};
struct SerialType {template<class... T> void printf(const char*,T...) {}};SerialType Serial;
namespace keyboard {
std::atomic<bool> scanFound{false},pairingNotSaved{false};
ble_addr_t scanAddress{},knownAddress{};bool haveKnownAddress=false;
std::atomic<unsigned> storeFailures{0};
int resultCode=0;unsigned writes=0;
int backend(int,const ble_store_value*){++writes;return resultCode;}
int (*originalStoreWrite)(int,const ble_store_value*)=backend;
'''
code+='\n'.join(function(sig) for sig in ['int scanEvent(','int observedStoreWrite(','const char* phaseName('])
code+='''
}
int main(){using namespace keyboard;
 knownAddress={1,{1,2,3,4,5,6}};haveKnownAddress=true;
 ble_gap_event event{};event.type=BLE_GAP_EVENT_DISC;event.disc.addr=knownAddress;
 scanEvent(&event,nullptr);assert(scanFound&&sameIdentity(knownAddress.type,knownAddress.val,scanAddress.type,scanAddress.val));
 scanFound=false;event.disc.addr.type=3;scanEvent(&event,nullptr);assert(scanFound); // controller-resolved identity
 scanFound=false;event.disc.addr.val[0]=9;scanEvent(&event,nullptr);assert(!scanFound);
 const char* name="POP ICON KEYS";event.disc.data=reinterpret_cast<const uint8_t*>(name);event.disc.length_data=std::strlen(name);scanEvent(&event,nullptr);assert(scanFound);
 resultCode=27;ble_store_value v{};assert(observedStoreWrite(1,&v)==27&&writes==1&&storeFailures==2&&pairingNotSaved);
 assert(std::strstr(phaseName(Phase::Ready),"not saved"));
 resultCode=0;assert(observedStoreWrite(1,&v)==0&&storeFailures==0&&writes==2);
 pairingNotSaved=!pairingDurable(true,true,storeFailures);assert(!pairingNotSaved&&std::strcmp(phaseName(Phase::Ready),"Keyboard connected")==0);
 std::puts("PASS actual scanEvent/store observer/UI label: nameless known + resolved identity, unrelated nameless rejected, named new peer requires later HID validation; rc27 preserved, failure displayed, successful write recovery. Not RF/NVS/device simulation.");
}
'''
Path(sys.argv[1]).write_text(code)
print('Extracted actual scanEvent, observedStoreWrite and phaseName; minimal BLE/Preferences-independent stubs')
