#!/usr/bin/env python3
"""Compile the unchanged v0.5.1 releaseClient body with bounded RTOS/BLE stubs."""
from pathlib import Path
import subprocess,sys,tempfile
p=Path(__file__).resolve().parents[1]
s=(p/'evidence/F1_BEFORE_releaseClient.cpp').read_text()
preamble=r'''#include <cstdio>
#include <stdexcept>
constexpr int BLE_HS_CONN_HANDLE_NONE=65535,portMAX_DELAY=-1;
int waits=0,requests=0,destroyed=0,clientsCleaned=0,stateMux=0,fenceEvent=0,fenceSemaphore=0;
struct BLEClient{bool ready;int handle;bool isConnected(){return ready;}int getConnId(){return handle;}void disconnect(){if(ready){++requests;ready=false;handle=65535;}}~BLEClient(){++destroyed;}};
struct Sub{} subscriptions[8];int subscriptionCount=0;
void invalidate(){} void lifecycle(const char*){} int nimble_port_get_dflt_eventq(){return 0;}
void ble_npl_eventq_put(int,int*){}void xSemaphoreTake(int,int){}
void portENTER_CRITICAL(int*){}void portEXIT_CRITICAL(int*){}
void delay(int){if(++waits==100)throw std::runtime_error("bounded probe stop");}
'''
main=r'''
int main(){for(int state=0;state<3;++state){waits=requests=destroyed=0;auto* c=new BLEClient{state==1,state==0?65535:7};bool done=true;try{releaseClient(c);}catch(...){done=false;}
std::printf("state=%d completed=%d disconnect_requests=%d waits=%d destroyed=%d\n",state,done,requests,waits,destroyed);
if(state==2){if(done||requests||waits!=100||destroyed)return 2;delete c;}}
std::puts("REPRODUCED F1: valid handle + not ready waits 100 steps without terminate; probe bounded, expected defect observed");}
'''
with tempfile.TemporaryDirectory(prefix='hanime-f1-before-') as d:
 src=Path(d)/'probe.cpp';exe=Path(d)/'probe';src.write_text(preamble+s+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
