// Actual App/persistence, actual extracted BLE admission/invalidate; Host mutex.
#define main inherited_usb_product_flow_main
#include "usb_product_flow.cpp"
#undef main
static unsigned manualWrites=0;
static void observeFile(const char* path,const char* mode){if(std::strstr(path,"/MANUAL.")&&mode[0]=='w')++manualWrites;}
static void observe(){manualWrites=0;s3fs::File::openObserver=observeFile;}
static void layout(Rig& r,bool vertical){if(vertical){r.app.layoutKey({1,++r.now,61,0,true});r.app.layoutKey({1,++r.now,61,0,false});}}
static void autoA(Rig& r){CHECK(r.app.commitText("A",r.app.editStamp()));CHECK(r.app.persistence.startAutoSave(++r.now,r.app.editorPort).status==ds::Status::Busy);CHECK(r.app.commitText("B",r.app.editStamp()));}
static void settle(Rig& r){unsigned n=0;while(r.app.persistence.backgroundSaveActive()||r.app.manualSavePending()||r.app.commandBarrier()){CHECK(++n<4000);r.app.service(++r.now,false);}}
static void lane(Rig& r,const ds::DocumentId& id,ds::SaveLane which,std::string_view expected){const auto got=r.app.persistence.loadLane(id,which);CHECK(got.status==ds::Status::Ok);CHECK(std::string_view(got.snapshot.text)==expected);}
static void manual(Rig& r,std::string_view expected){lane(r,r.app.session.currentId(),ds::SaveLane::Manual,expected);}
#include "ble_epoch_fixture.hpp"
int main(){try{
 namespace bf=ble_epoch_fixture;
 for(bool vertical:{false,true})for(unsigned order=0;order<3;++order){
  Rig r;layout(r,vertical);autoA(r);observe();bf::reset();
  s3hid::State held;held.set(22);bf::tracker.update(0,held,1,++r.now,[](const s3hid::Event&){});CHECK(!bf::tracker.allReleased());
  s3hid::Event raw{1,++r.now,22,0x08,true};uint32_t seen=0;
  if(order==0){ // App acceptance precedes disconnect.
   CHECK(bf::admit(raw,seen));r.app.key(raw);CHECK(r.app.manualSavePending());
   bf::invalidate();r.app.disconnected();
  }else if(order==1){ // Disconnect precedes raw admission: no command exists.
   bf::invalidate();r.app.disconnected();CHECK(!bf::admit(raw,seen));
   CHECK(!r.app.manualSavePending());
  }else{ // Raw admitted first may finish dispatch after invalidation.
   CHECK(bf::admit(raw,seen));bf::invalidate();r.app.key(raw);
   CHECK(r.app.manualSavePending());r.app.disconnected();
  }
  CHECK(!bf::admit(raw,seen)&&seen==2);CHECK(bf::tracker.allReleased());
  settle(r);lane(r,r.app.session.currentId(),ds::SaveLane::Auto,"A");
  if(order==1){CHECK(manualWrites==0&&r.app.document.dirty());}
  else{manual(r,"AB");CHECK(manualWrites==1&&!r.app.document.dirty());exported(r,"AB");}
  std::cout<<"PASS actual_B_admit_C_save H_V="<<vertical<<" order="<<order<<" manual_writes="<<manualWrites<<'\n';
 }
 std::cout<<"LIMIT Host mutex/TF transport; deviceLoop and FreeRTOS not executed\n";
 return 0;
}catch(const std::exception& e){s3fs::faults={};std::cerr<<e.what()<<'\n';return 1;}}
