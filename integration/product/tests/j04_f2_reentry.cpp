// Reuse the real product fixtures; no replacement keyboard or IME state owner.
#define DRAFTOR_FIND_TEST_FIXTURE_ONLY 1
#include "find_chinese.cpp"
#undef DRAFTOR_FIND_TEST_FIXTURE_ONLY
#include "docmanager_display.hpp"
#include "input_route.hpp"
#include "support/r2_raster.hpp"

struct F2Rig:FindRig {
 s3hid::InputRoute route;
 bool present(){
  if(app.commandBarrier())return false;
  if(app.filesOpen()){
   if(!ime_device::prepareImePresentation(&ime,app))return false;
   r2preview::Raster canvas;
   CHECK(s3app::display::documentMenu(app,canvas.contentFont,canvas.rect(),canvas.aux(),canvas.content()));
   s3app::display::menuFeedback(app,canvas.rect(),canvas.aux());
   const auto publication=ime_device::publishActivePresentation(app,&ime,app.presentationFrame());
   CHECK(publication.accepted);frame=publication.candidates;
  }else if(!FindRig::display())return false;
  route.framePresented(app.focusToken());return true;
 }
 s3hid::Event captured(uint8_t usage,bool down=true){
  s3hid::Event e{1,++now,usage,0,down,route.epoch(),frame,ime.attemptSerial()};
  e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();return e;
 }
 bool dispatch(s3hid::Event e){
  // Model the existing keyboard::admit gate before route dispatch. This Host
  // fixture has one admitted BLE epoch; it does not exercise radio hardware.
  if(e.epoch!=1)return false;
  if(!route.accept(e,++now)){
   product.releaseAfterRouteDrop(app,&ime,e);
   return false;
  }
  context();
  if(product.layout(app,e)){repeat.clear();context();return true;}
  const bool chinese=ime.chinese();
  if(!product.ime(app,&ime,e)){repeat.event(e);app.key(e,chinese);}else repeat.clear();
  context();return true;
 }
 void key(uint8_t usage){CHECK(dispatch(captured(usage)));CHECK(dispatch(captured(usage,false)));}
 void settle(){
  for(unsigned n=0;n<2000000;++n){
   step();
   if(n>100&&!ime.hasWork()&&!app.commandBarrier()&&!app.manualSavePending()&&
      !app.persistence.backgroundSaveActive()&&!app.find.hasWork()&&present())return;
  }
  CHECK(false);
 }
 void stable(uint8_t usage){key(usage);settle();}
 void acknowledge(){
  CHECK(app.fileRequestPending()&&app.commandBarrier());
  app.fileFramePresented(uint64_t(now)*1000,true);settle();
 }
};

static void reentry(bool vertical,bool chinese,unsigned pause){
 F2Rig r;CHECK(r.app.commitText("J04 首尾。Body must remain unchanged.",r.app.editStamp()));
 CHECK(r.app.session.manualSave(++r.now));r.settle();
 if(vertical)r.stable(61);
 if(chinese)r.stable(57);
 const BodyGuard body(r);const auto id=r.app.session.currentId();
 r.app.deferFileCommands(true);
 CHECK(r.dispatch(r.captured(59)));CHECK(r.app.fileRequestPending()&&!r.app.filesOpen());
 // This physical up was captured before the busy/menu frame completed. The
 // product stops dequeuing while the LCD feedback barrier is pending.
 const auto f2Up=r.captured(59,false);
 const auto staleF2Down=r.captured(59);
 r.app.service(++r.now,false);CHECK(!r.app.filesOpen());
 CHECK(!r.present()); // No busy-LCD acknowledgement, no new route.
 r.acknowledge();CHECK(r.app.filesOpen());
 CHECK(!r.dispatch(f2Up));CHECK(!r.dispatch(staleF2Down));CHECK(r.app.filesOpen());
 CHECK(r.dispatch(r.captured(40)));CHECK(r.app.fileRequestPending());
 const auto enterUp=r.captured(40,false);
 r.acknowledge();CHECK(!r.app.filesOpen()&&r.app.session.currentId()==id);
 CHECK(!r.dispatch(enterUp)); // Existing Enter release exception is retained.
 r.now+=pause;
 CHECK(r.dispatch(r.captured(59)));
 const bool firstWorked=r.app.fileRequestPending();
 std::cout<<"J04_FIRST_F2 layout="<<vertical<<" chinese="<<chinese<<" waited_ms="<<pause
          <<" pending="<<firstWorked<<" files_open="<<r.app.filesOpen()<<std::endl;
 if(!firstWorked){
  // Baseline reproduces the field workaround: first up clears the old latch,
  // then the second F2 down works. Fail the required first-press assertion.
  CHECK(r.dispatch(r.captured(59,false)));CHECK(r.dispatch(r.captured(59)));
  std::cout<<"J04_SECOND_F2 pending="<<r.app.fileRequestPending()<<std::endl;
  CHECK(r.app.fileRequestPending());
 }
 CHECK(firstWorked);
 r.acknowledge();CHECK(r.app.filesOpen());
 CHECK(r.dispatch(r.captured(59,false)));body.check(r);CHECK(r.ime.chinese()==chinese);
 std::cout<<"PASS J04 first F2 after menu Enter, old-route release, preserved busy ACK/body/mode\n";
}

static void releaseGuards(){
 F2Rig r;CHECK(r.app.commitText("Guard body.",r.app.editStamp()));
 CHECK(r.app.session.manualSave(++r.now));r.settle();const BodyGuard body(r);
 r.app.deferFileCommands(true);
 const auto held=r.captured(59);CHECK(r.dispatch(held));r.acknowledge();CHECK(r.app.filesOpen());
 // An older up must not unlatch the held physical F2, even across routes.
 auto older=held;older.down=false;--older.ms;CHECK(!r.dispatch(older));
 CHECK(r.dispatch(r.captured(59)));CHECK(r.app.filesOpen()&&!r.app.fileRequestPending());
 auto otherEpoch=r.captured(59,false);otherEpoch.epoch=2;
 CHECK(!r.dispatch(otherEpoch));
 // Even a caller mistake cannot reset the owner's HID epoch through this
 // release-only route exception. Normal production still admits BLE first.
 r.product.releaseAfterRouteDrop(r.app,&r.ime,otherEpoch);
 CHECK(r.dispatch(r.captured(59)));CHECK(r.app.filesOpen()&&!r.app.fileRequestPending());
 // An unrelated stale release may not reset F2 state either.
 auto otherKey=r.captured(30,false);otherKey.route_epoch=held.route_epoch;
 CHECK(!r.dispatch(otherKey));
 CHECK(r.dispatch(r.captured(59)));CHECK(r.app.filesOpen()&&!r.app.fileRequestPending());
 // Fresh release retires once; repeated older delivery cannot reset a new down.
 auto up=r.captured(59,false);up.route_epoch=held.route_epoch;CHECK(!r.dispatch(up));
 CHECK(r.dispatch(r.captured(59)));CHECK(!r.app.filesOpen());r.settle();
 CHECK(!r.dispatch(up));CHECK(r.dispatch(r.captured(59)));CHECK(!r.app.fileRequestPending());
 CHECK(r.dispatch(r.captured(59,false)));CHECK(r.dispatch(r.captured(59)));
 CHECK(r.app.fileRequestPending());r.acknowledge();CHECK(r.app.filesOpen());body.check(r);
 std::cout<<"PASS J04 stale down, older/repeated up, other BLE epoch and unrelated release remain inert\n";
}

int main(){try{
 for(bool vertical:{false,true})for(bool chinese:{false,true})for(unsigned pause:{0u,10000u})reentry(vertical,chinese,pause);
 releaseGuards();
 std::cout<<"HOST_ONLY actual App/DeviceIme/ProductInput/InputRoute/painters; DEVICE_NOT_TESTED\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
