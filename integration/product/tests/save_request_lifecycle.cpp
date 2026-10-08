// Real product App + persistence + cooperative writer over POSIX VFS.
// Reuse the existing USB fixture unchanged; the transport is a Host model.
#define main inherited_usb_product_flow_main
#include "usb_product_flow.cpp"
#undef main
#include "hid_keyboard.hpp"
#include "input_route.hpp"
namespace s3app {
struct AppTestAccess {
 static size_t requestBytes(){return sizeof(App::ManualSaveRequest);}
 static bool running(const App& a){return a.manualSave_.running;}
 static uint32_t requested(const App& a){return a.manualSave_.requested_revision;}
 static uint32_t started(const App& a){return a.manualSave_.started_revision;}
};
}
static unsigned manualWrites=0,manualReadbacks=0;
static bool armVerifyFailure=false,sawManualWrite=false;
static void observeFile(const char* path,const char* mode){
 if(!std::strstr(path,"/MANUAL."))return;
 if(mode[0]=='w'){++manualWrites;sawManualWrite=true;}
 else if(mode[0]=='r'&&sawManualWrite){++manualReadbacks;if(armVerifyFailure)s3fs::faults.read=true;}
}
static void observe(){manualWrites=manualReadbacks=0;armVerifyFailure=sawManualWrite=false;s3fs::File::openObserver=observeFile;}
static void append(Rig& r,const char* text){CHECK(r.app.commitText(text,r.app.editStamp()));}
static void layout(Rig& r,bool vertical){if(vertical){r.app.layoutKey({1,++r.now,61,0,true});r.app.layoutKey({1,++r.now,61,0,false});}}
static void request(Rig& r,unsigned entry){
 if(entry==0)r.app.key({1,++r.now,22,0x08,true});
 else if(entry==1)r.app.key({1,++r.now,22,0x01,true});
 else if(entry==2)r.app.key({1,++r.now,63,0,true});
 else {const auto got=r.app.requestSave(++r.now);CHECK(got.status==ds::Status::Busy&&std::string_view(got.stage)=="manual_save_accepted");}
 CHECK(r.app.manualSavePending());
}
static void settle(Rig& r){
 unsigned n=0;
 while(r.app.persistence.backgroundSaveActive()||r.app.manualSavePending()||r.app.commandBarrier()){
  CHECK(++n<4000);r.app.service(++r.now,false);
 }
}
static void lane(Rig& r,const ds::DocumentId& id,ds::SaveLane which,std::string_view expected){
 const auto got=r.app.persistence.loadLane(id,which);CHECK(got.status==ds::Status::Ok);CHECK(std::string_view(got.snapshot.text)==expected);
}
static void manual(Rig& r,std::string_view expected){lane(r,r.app.session.currentId(),ds::SaveLane::Manual,expected);}
static void autoA(Rig& r){append(r,"A");CHECK(r.app.persistence.startAutoSave(++r.now,r.app.editorPort).status==ds::Status::Busy);append(r,"B");}
static void noMoreWrites(Rig& r,unsigned expected){for(unsigned i=0;i<20;++i){r.app.disconnected();r.app.service(++r.now,false);}CHECK(!r.app.manualSavePending()&&manualWrites==expected);}
static void acceptedRequests(){
 for(bool vertical:{false,true})for(unsigned entry=0;entry<4;++entry){
  for(bool disconnect:{false,true}){
   Rig r;layout(r,vertical);autoA(r);const auto id=r.app.session.currentId();const auto receivedRevision=r.app.document.textRevision();
   observe();request(r,entry);CHECK(!r.app.commandBarrier());CHECK(!s3app::AppTestAccess::running(r.app));
   if(disconnect)r.app.disconnected();CHECK(r.app.manualSavePending());settle(r);
   CHECK(!r.app.document.dirty()&&!r.app.persistence.bodyDirty());CHECK(manualWrites==1&&manualReadbacks>0);manual(r,"AB");
   lane(r,id,ds::SaveLane::Auto,"A");CHECK(std::string_view(r.app.notice)=="Saved");noMoreWrites(r,1);
   std::cout<<"PASS auto_A_request_AB H_V="<<vertical<<" entry="<<entry<<" disconnected="<<disconnect<<" accepted_revision="<<receivedRevision<<" manual_writes=1 verify_ack=1 dirty=0\n";
  }
  {Rig r;layout(r,vertical);append(r,"AB");observe();request(r,entry);CHECK(s3app::AppTestAccess::running(r.app));r.app.disconnected();settle(r);manual(r,"AB");CHECK(!r.app.document.dirty());noMoreWrites(r,1);}
  {Rig r;layout(r,vertical);autoA(r);observe();request(r,entry);const auto requested=s3app::AppTestAccess::requested(r.app);
   append(r,"C");CHECK(r.app.document.textRevision()>requested);for(unsigned n=0;n<20;++n)request(r,entry);
   r.app.disconnected();settle(r);manual(r,"ABC");CHECK(!r.app.document.dirty()&&manualWrites==1);noMoreWrites(r,1);}
  {Rig r;layout(r,vertical);append(r,"AB");observe();request(r,entry);const auto captured=s3app::AppTestAccess::started(r.app);
   append(r,"C");r.app.disconnected();settle(r);manual(r,"AB");CHECK(r.app.document.textRevision()>captured&&r.app.document.dirty()&&r.app.persistence.bodyDirty());
   CHECK(std::string_view(r.app.notice)=="Snapshot saved; newer edits");noMoreWrites(r,1);}
  {Rig r;layout(r,vertical);append(r,"AB");observe();request(r,entry);for(unsigned n=0;n<20;++n)request(r,entry);settle(r);CHECK(manualWrites==1);manual(r,"AB");}
  {Rig r;layout(r,vertical);append(r,"AB");observe();request(r,entry);append(r,"C");for(unsigned n=0;n<20;++n)request(r,entry);
   r.app.disconnected();settle(r);CHECK(manualWrites==2&&!r.app.document.dirty());manual(r,"ABC");noMoreWrites(r,2);}
  std::cout<<"PASS coalescing_and_post_capture_edits H_V="<<vertical<<" entry="<<entry<<" bounded_tokens=1\n";
 }
}
static void commandBoundaries(){
 for(bool vertical:{false,true}){
  {Rig r;layout(r,vertical);autoA(r);const auto id=r.app.session.currentId();observe();request(r,0);
   r.app.key({1,++r.now,17,0x08,true});CHECK(r.app.commandBarrier());settle(r);
   CHECK(r.app.session.currentId()!=id);lane(r,id,ds::SaveLane::Manual,"AB");CHECK(manualWrites==1);manualWrites=0;
   for(unsigned n=0;n<20;++n)r.app.service(++r.now,false);CHECK(manualWrites==0);}
  {Rig r;layout(r,vertical);autoA(r);const auto id=r.app.session.currentId();r.app.key({1,++r.now,17,0x08,true});CHECK(r.app.commandBarrier());
   const auto rejected=r.app.requestSave(++r.now);CHECK(rejected.status==ds::Status::Busy&&std::string_view(rejected.stage)=="manual_save_not_accepted");CHECK(!r.app.manualSavePending());
   settle(r);CHECK(r.app.session.currentId()!=id);}
  {Rig r;layout(r,vertical);autoA(r);const auto id=r.app.session.currentId();observe();request(r,0);
   r.app.key({1,++r.now,17,0x08,true});r.app.disconnected();CHECK(!r.app.commandBarrier());settle(r);
   CHECK(r.app.session.currentId()==id);manual(r,"AB");CHECK(manualWrites==1);}
  {Rig r;layout(r,vertical);autoA(r);observe();request(r,0);r.app.key({1,++r.now,41,0,true});CHECK(r.app.manualSavePending());settle(r);manual(r,"AB");}
  // External lifecycle boundary: real session API, after explicitly retiring
  // the writer. A pending accepted request must not follow CURRENT to B.
  for(unsigned boundary=0;boundary<3;++boundary){
   Rig r;layout(r,vertical);autoA(r);request(r,0);const auto id=r.app.session.currentId();
   r.app.persistence.abortForMemoryFailure();observe();
   if(boundary==0){CHECK(r.app.session.create("B",++r.now));CHECK(r.app.session.currentId()!=id);append(r,"B-only");}
   else if(boundary==1){CHECK(r.app.session.moveToTrash(id,++r.now));CHECK(!r.app.session.hasCurrent());}
   else r.storage.noteUnmounted();
   r.app.service(++r.now,false);CHECK(!r.app.manualSavePending());CHECK(r.app.lastOperation.status==ds::Status::SessionChanged);CHECK(manualWrites==0);
   if(boundary==0||boundary==2)CHECK(r.app.document.dirty());
   CHECK(std::string_view(r.app.notice)!="Saved");
  }
  // Real unmount while the AUTO writer is still alive; no artificial abort.
  {Rig r;layout(r,vertical);autoA(r);observe();request(r,0);r.storage.noteUnmounted();r.app.service(++r.now,false);
   CHECK(!r.app.manualSavePending()&&r.app.document.dirty());settle(r);CHECK(manualWrites==0&&r.app.lastOperation.status!=ds::Status::Ok);}
  std::cout<<"PASS command_switch_close_unmount_identity H_V="<<vertical<<" wrong_document_writes=0\n";
 }
}
static void failures(){
 for(bool vertical:{false,true})for(unsigned fault=0;fault<4;++fault){
  Rig r;layout(r,vertical);append(r,"AB");observe();request(r,0);
  if(fault==0)s3fs::faults.write_bytes=0;else if(fault==1)s3fs::faults.flush=true;else if(fault==2)s3fs::faults.close=true;else armVerifyFailure=true;
  settle(r);CHECK(!r.app.manualSavePending()&&r.app.lastOperation.status!=ds::Status::Ok);CHECK(r.app.document.dirty()&&r.app.persistence.bodyDirty());CHECK(std::string_view(r.app.notice)!="Saved");
  if(fault==3)CHECK(manualWrites==1&&manualReadbacks>0);
  const auto writes=manualWrites;for(unsigned i=0;i<20;++i){r.app.disconnected();r.app.service(++r.now,false);}CHECK(manualWrites==writes);
  s3fs::faults={};armVerifyFailure=false;
  std::cout<<"PASS writer_failure H_V="<<vertical<<" fault="<<fault<<" no_false_ack=1 no_manual_retry=1\n";
 }
 for(bool vertical:{false,true}){
  Rig r;layout(r,vertical);autoA(r);observe();request(r,0);s3fs::faults.write_bytes=0;settle(r);
  CHECK(!r.app.manualSavePending()&&r.app.document.dirty()&&manualWrites==0&&r.app.lastOperation.status!=ds::Status::Ok);s3fs::faults={};
  std::cout<<"PASS predecessor_AUTO_failure_visible H_V="<<vertical<<'\n';
 }
 for(bool vertical:{false,true}){
  Rig r;layout(r,vertical);autoA(r);observe();request(r,0);const auto id=r.app.session.currentId();
  r.app.key({1,++r.now,17,0x08,true});CHECK(r.app.commandBarrier());const auto focus=r.app.focusToken(),serial=uint64_t(r.app.operationSerial);
  s3fs::faults.write_bytes=0;settle(r);s3fs::faults={};
  CHECK(r.app.focusToken()>focus&&r.app.operationSerial==serial+1);CHECK(r.app.session.currentId()==id&&r.app.document.dirty()&&manualWrites==0);
  std::cout<<"PASS predecessor_failure_cancels_command_once H_V="<<vertical<<'\n';
 }
}
static void usbAndRawBoundary(){
 for(bool vertical:{false,true}){
  Rig r;layout(r,vertical);autoA(r);observe();
  s3hid::InputRoute route;route.framePresented(r.app.focusToken());
  const s3hid::Event stale{1,++r.now,22,0x08,true,route.epoch()};
  CHECK(route.accept(stale,r.now));request(r,0);
  CHECK(r.transfer.request(false,{})==u::Status::Busy);CHECK(!r.transfer.active());
  s3hid::Tracker tracker;s3hid::Repeat repeat;s3hid::State held;held.set(227);held.set(22);unsigned events=0;
  tracker.update(0,held,1,++r.now,[&](const s3hid::Event& e){repeat.event(e);++events;});CHECK(events>0&&!tracker.allReleased());
  r.app.disconnected();tracker.clear();repeat.clear();route.framePresented(r.app.focusToken());
  CHECK(!route.accept(stale,++r.now)&&route.discarded==1);CHECK(r.app.manualSavePending());
  CHECK(tracker.allReleased());s3hid::Event out;CHECK(!repeat.poll(r.now+1000,2,out));
  settle(r);manual(r,"AB");CHECK(manualWrites==1);exported(r,"AB");CHECK(r.app.document.text()=="AB");
  std::cout<<"PASS USB_admission_export_current_version H_V="<<vertical<<" raw_route_epoch_check=PRODUCTION_InputRoute_NO_BLE_TASK\n";
 }
}
int main(){try{
 std::cout<<"RESOURCE sizeof_App="<<sizeof(s3app::App)<<" sizeof_manual_request="<<s3app::AppTestAccess::requestBytes()<<" max_tokens=1 extra_body_snapshots=0\n";
 acceptedRequests();commandBoundaries();failures();usbAndRawBoundary();
 std::cout<<"SOFTWARE_FIX_VERIFIED queued_manual_save_disconnect_semantics DEVICE_NOT_RUN\n";return 0;
}catch(const std::exception& e){s3fs::faults={};std::cerr<<e.what()<<'\n';return 1;}}
