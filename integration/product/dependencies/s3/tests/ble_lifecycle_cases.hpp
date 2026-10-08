// Deterministic backend; no real radio, RTOS, serial or memory-usage claim.
enum class Mode{Normal,ConnectFail,MtuFail,MtuTimeout,AttemptTimeout,Immediate};
Mode mode=Mode::Normal;
BLEClient* backend=nullptr;
int connectCalls=0,cancelCalls=0,terminateCalls=0,resetCalls=0,destroyed=0,callbackDepth=0,notifyCalls=0;
int startError=0,mtuError=0;
bool physicalPending=false,autoDisconnect=true,resetCompletes=true,notified=false;
uint16_t physicalHandle=BLE_HS_CONN_HANDLE_NONE,nextHandle=7;
std::deque<int> terminateResults,cancelResults;
std::deque<uint16_t> terminatedHandles;
int takeResult(std::deque<int>& q){if(q.empty())return 0;int n=q.front();q.pop_front();return n;}
void dispatch(ble_gap_event e){REQUIRE(backend);++callbackDepth;BLEClient::handleGAPEvent(&e,backend);--callbackDepth;}
void connected(int status=0){physicalPending=false;if(!status)physicalHandle=nextHandle;ble_gap_event e;e.type=BLE_GAP_EVENT_CONNECT;e.connect={status,nextHandle};dispatch(e);}
void mtu(){ble_gap_event e;e.type=BLE_GAP_EVENT_MTU;e.mtu={physicalHandle,247};dispatch(e);}
void disconnected(){if(physicalHandle==BLE_HS_CONN_HANDLE_NONE)return;ble_gap_event e;e.type=BLE_GAP_EVENT_DISCONNECT;e.disconnect.reason=19;e.disconnect.conn.conn_handle=physicalHandle;physicalHandle=BLE_HS_CONN_HANDLE_NONE;dispatch(e);}
BLEClient::~BLEClient(){REQUIRE(callbackDepth==0);REQUIRE(!physicalPending);REQUIRE(physicalHandle==BLE_HS_CONN_HANDLE_NONE);REQUIRE(!m_connectPending);REQUIRE(m_conn_id==BLE_HS_CONN_HANDLE_NONE);REQUIRE(m_pTaskData==nullptr);++destroyed;if(backend==this)backend=nullptr;}
int ble_gap_conn_find_by_addr(const ble_addr_t*,void*){return BLE_HS_ENOTCONN;}
int ble_gap_connect(int,const ble_addr_t*,int duration,const ble_gap_conn_params*,int(*)(ble_gap_event*,void*),void* arg){++connectCalls;REQUIRE(duration==4000);if(startError)return startError;backend=static_cast<BLEClient*>(arg);physicalPending=true;if(mode==Mode::Immediate){connected();mtu();}return 0;}
int ble_gattc_exchange_mtu(uint16_t handle,void*,void*){REQUIRE(handle==physicalHandle);return mtuError;}
int ble_gap_conn_cancel(){++cancelCalls;REQUIRE(backend&&backend->m_connectPending);int rc=takeResult(cancelResults);if(!rc){physicalPending=false;hostQueue.push_back([]{connected(BLE_HS_ETIMEOUT);});}return rc;}
int ble_gap_terminate(uint16_t handle,uint8_t){++terminateCalls;terminatedHandles.push_back(handle);REQUIRE(backend&&handle==backend->getConnId());int rc=takeResult(terminateResults);if(!rc&&autoDisconnect)hostQueue.push_back([]{disconnected();});return rc;}
void ble_hs_sched_reset(int reason){REQUIRE(reason==BLE_HS_ECONTROLLER);++resetCalls;if(resetCompletes)hostQueue.push_back([]{if(physicalPending)connected(BLE_HS_ECONTROLLER);disconnected();});}
void BLEUtils::taskRelease(const BLETaskData& t,int rc){REQUIRE(backend);REQUIRE(&t==&backend->m_connectTask);++notifyCalls;notified=true;(void)rc;}
bool BLEUtils::taskWait(const BLETaskData&,uint32_t timeout){REQUIRE(timeout==5000);switch(mode){case Mode::Normal:connected();mtu();break;case Mode::ConnectFail:connected(42);break;case Mode::MtuFail:connected();break;case Mode::MtuTimeout:connected();return false;case Mode::AttemptTimeout:return false;case Mode::Immediate:break;}return notified;}
void runOne(){if(hostQueue.empty())return;auto f=std::move(hostQueue.front());hostQueue.pop_front();f();}
void drain(){unsigned n=0;while(!hostQueue.empty()){REQUIRE(++n<100);runOne();}}
void setup(){REQUIRE(hostQueue.empty());REQUIRE(!backend);releaseRequest.stage=ReleaseStage::Idle;releaseRequest.client=nullptr;releaseRequest.safe=false;releaseRequest.lastWarning=0;ble_npl_event_init(&releaseEvent,releaseCallback,nullptr);mode=Mode::Normal;nowMs=0;connectCalls=cancelCalls=terminateCalls=resetCalls=destroyed=notifyCalls=0;mtuError=startError=0;physicalPending=false;physicalHandle=BLE_HS_CONN_HANDLE_NONE;autoDisconnect=resetCompletes=true;notified=false;nextHandle=7;terminateResults.clear();cancelResults.clear();terminatedHandles.clear();}
bool connectClient(BLEClient* c){return c->connect(BLEAddress("test"),0,4000);}
void finish(BLEClient*& c,unsigned maxSteps=100){for(unsigned i=0;i<maxSteps&&c;++i){releaseClient(c);drain();nowMs+=250;}REQUIRE(!c);REQUIRE(destroyed==1);REQUIRE(releaseClient(c));REQUIRE(destroyed==1);REQUIRE(hostQueue.empty());}
int main(){
 setup();auto* c=new BLEClient;finish(c);REQUIRE(!terminateCalls&&!cancelCalls);std::puts("PASS never connected: no request, one delete");
 setup();c=new BLEClient;startError=42;REQUIRE(!connectClient(c));finish(c);REQUIRE(!terminateCalls&&!cancelCalls);std::puts("PASS ordinary immediate GAP failure");
 setup();c=new BLEClient;mode=Mode::ConnectFail;REQUIRE(!connectClient(c));finish(c);REQUIRE(!terminateCalls&&!cancelCalls);std::puts("PASS delivered connection failure");
 setup();c=new BLEClient;mode=Mode::MtuFail;mtuError=42;REQUIRE(!connectClient(c));REQUIRE(!c->isConnected()&&c->getConnId()==7);finish(c);REQUIRE(terminateCalls==1&&!cancelCalls);std::puts("PASS F1 actual connect + MTU immediate error -> terminate -> callback -> one delete");
 setup();c=new BLEClient;mode=Mode::MtuTimeout;REQUIRE(!connectClient(c));REQUIRE(!c->isConnected()&&c->getConnId()==7);finish(c);REQUIRE(terminateCalls==1&&!cancelCalls);std::puts("PASS MTU wait timeout cancels live link, not connection attempt");
 setup();c=new BLEClient;mode=Mode::AttemptTimeout;REQUIRE(!connectClient(c));REQUIRE(c->m_connectPending);finish(c);REQUIRE(cancelCalls==1&&!terminateCalls);std::puts("PASS pending attempt timeout -> cancel -> terminal callback");
 setup();c=new BLEClient;REQUIRE(connectClient(c));REQUIRE(c->isConnected());autoDisconnect=false;REQUIRE(!releaseClient(c));for(unsigned i=0;i<20;++i)REQUIRE(!releaseClient(c));REQUIRE(hostQueue.size()==1);drain();REQUIRE(!destroyed);disconnected();finish(c);REQUIRE(terminateCalls==1);std::puts("PASS normal disconnect: no premature delete, one queued callback, repeated release safe");
 setup();c=new BLEClient;mode=Mode::MtuFail;mtuError=42;terminateResults={42,42,0};REQUIRE(!connectClient(c));finish(c);REQUIRE(terminateCalls==3&&!resetCalls);std::puts("PASS terminate rejection twice then success, bounded backoff");
 setup();c=new BLEClient;mode=Mode::MtuFail;mtuError=42;terminateResults={42,42,42};REQUIRE(!connectClient(c));finish(c);REQUIRE(terminateCalls==3&&resetCalls==1);std::puts("PASS repeated terminate error -> existing NimBLE host recovery -> real terminal event");
 setup();c=new BLEClient;mode=Mode::AttemptTimeout;cancelResults={42,42,0};REQUIRE(!connectClient(c));finish(c);REQUIRE(cancelCalls==3&&!terminateCalls);std::puts("PASS cancellation failure then recovery");
 setup();c=new BLEClient;mode=Mode::AttemptTimeout;REQUIRE(!connectClient(c));REQUIRE(!releaseClient(c));connected();REQUIRE(c->getConnId()==7);finish(c);REQUIRE(terminateCalls>=1&&!cancelCalls&&notifyCalls==0);std::puts("PASS late successful CONNECT after timeout is tracked and terminated, no dead waiter");
 setup();c=new BLEClient;mode=Mode::MtuTimeout;REQUIRE(!connectClient(c));mtu();REQUIRE(!c->isConnected()&&!notifyCalls);finish(c);std::puts("PASS late MTU cannot publish ready after timeout");
 setup();c=new BLEClient;mode=Mode::Immediate;REQUIRE(connectClient(c));REQUIRE(notifyCalls==1);finish(c);std::puts("PASS synchronous CONNECT/MTU before taskWait sees published waiter");
 setup();c=new BLEClient;REQUIRE(connectClient(c));autoDisconnect=false;REQUIRE(!releaseClient(c));drain();ble_gap_event tf;tf.type=BLE_GAP_EVENT_TERM_FAILURE;tf.term_failure={42,7};dispatch(tf);REQUIRE(!destroyed);autoDisconnect=true;nowMs=1000;finish(c);REQUIRE(terminateCalls==2);std::puts("PASS delivered TERM_FAILURE retries without deleting client");
 setup();c=new BLEClient;REQUIRE(connectClient(c));autoDisconnect=false;finish(c);REQUIRE(resetCalls==1);std::puts("PASS accepted terminate without terminal callback triggers 10s host recovery");
 setup();c=new BLEClient;REQUIRE(connectClient(c));autoDisconnect=false;resetCompletes=false;for(unsigned i=0;i<70;++i){REQUIRE(!releaseClient(c));drain();nowMs+=250;}REQUIRE(!destroyed&&resetCalls==1&&terminateCalls>=10);disconnected();finish(c);std::puts("PASS unavailable recovery retains one owned client, progresses bounded steps, eventual callback releases");
 setup();c=new BLEClient;REQUIRE(connectClient(c));disconnected();finish(c);REQUIRE(destroyed==1);mode=Mode::Normal;notified=false;nextHandle=8;destroyed=0;c=new BLEClient;REQUIRE(connectClient(c));ble_gap_event old;old.type=BLE_GAP_EVENT_DISCONNECT;old.disconnect.reason=19;old.disconnect.conn.conn_handle=7;dispatch(old);REQUIRE(c->isConnected()&&c->getConnId()==8);finish(c);REQUIRE(connectCalls==2);std::puts("PASS next connection starts after old lifetime ends; wrong-handle disconnect ignored");
 setup();c=new BLEClient;c->m_pTaskData=&c->m_connectTask;finish(c);std::puts("PASS unwound connect waiter detached only after owner returned");
 setup();c=new BLEClient;mode=Mode::MtuTimeout;REQUIRE(!connectClient(c));physicalHandle=BLE_HS_CONN_HANDLE_NONE;terminateResults={BLE_HS_ENOTCONN};autoDisconnect=false;REQUIRE(!releaseClient(c));drain();REQUIRE(!destroyed&&c->getConnId()==7);ble_gap_event absent;absent.type=BLE_GAP_EVENT_DISCONNECT;absent.disconnect.reason=19;absent.disconnect.conn.conn_handle=7;dispatch(absent);finish(c);std::puts("PASS ENOTCONN is not permission to erase handle; delivered disconnect required");
 setup();c=new BLEClient;mode=Mode::AttemptTimeout;REQUIRE(!connectClient(c));physicalPending=false;cancelResults={BLE_HS_EALREADY};REQUIRE(!releaseClient(c));drain();REQUIRE(!destroyed&&c->m_connectPending);connected(BLE_HS_ETIMEOUT);finish(c);std::puts("PASS cancel EALREADY retains callback argument until terminal connect event");
 setup();c=new BLEClient;startError=BLE_HS_EBUSY;REQUIRE(!connectClient(c));REQUIRE(connectCalls==3);finish(c);std::puts("PASS scan-busy connect retries bounded at three calls");
 setup();c=new BLEClient;startError=BLE_HS_EALREADY;REQUIRE(!connectClient(c));finish(c);REQUIRE(!cancelCalls&&!terminateCalls);std::puts("PASS another connection attempt is not cancelled by this failed client");
 setup();c=new BLEClient;nowMs=100000;REQUIRE(!releaseClient(c));REQUIRE(phase==Phase::Cleaning);drain();nowMs+=250;REQUIRE(releaseClient(c));REQUIRE(phase==Phase::Disconnected);std::puts("PASS cleanup timer starts per operation, old uptime causes no premature error");
 std::printf("PASS production BLE lifecycle 23 scenarios; Retirement sizeof=%zu B; actual dependency compile and device checks separate\n",sizeof(hanime_ble::Retirement));
}
