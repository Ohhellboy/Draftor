// Joint-only lifecycle check. Reuse B1's real resources/DeviceIme/Find painter
// and R2's real menu painter; no replacement controller or submission model.
#define DRAFTOR_FIND_TEST_FIXTURE_ONLY 1
#include "find_chinese.cpp"
#undef DRAFTOR_FIND_TEST_FIXTURE_ONLY
#include "docmanager_display.hpp"
#include "input_route.hpp"
#include "support/r2_raster.hpp"

struct JointRig:FindRig {
 s3hid::InputRoute route;
 std::vector<unsigned char> menuPixels;
 bool present(){
  if(app.commandBarrier())return false;
  if(app.filesOpen()){
   if(!ime_device::prepareImePresentation(&ime,app))return false;
   r2preview::Raster canvas;
   CHECK(s3app::display::documentMenu(app,canvas.contentFont,canvas.rect(),canvas.aux(),canvas.content()));
   s3app::display::menuFeedback(app,canvas.rect(),canvas.aux());
   menuPixels=canvas.pixels;
   const auto publication=ime_device::publishActivePresentation(app,&ime,app.presentationFrame());
   CHECK(publication.accepted);frame=publication.candidates;
  }else if(!FindRig::display())return false;
  // Same order as the product: publish the new route after completed drawing.
  route.framePresented(app.focusToken());return true;
 }
 s3hid::Event captured(uint8_t usage,uint8_t mods=0,bool down=true){
  s3hid::Event e{1,++now,usage,mods,down,route.epoch(),frame,ime.attemptSerial()};
  e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();return e;
 }
 bool dispatch(s3hid::Event e){
  if(!route.accept(e,++now))return false;
  context();
  if(product.layout(app,e)){repeat.clear();context();return true;}
  const bool chinese=ime.chinese();
  if(!product.ime(app,&ime,e)){repeat.event(e);app.key(e,chinese);}else repeat.clear();
  context();return true;
 }
 void key(uint8_t usage,uint8_t mods=0){
  CHECK(dispatch(captured(usage,mods)));CHECK(dispatch(captured(usage,mods,false)));
 }
 void settleJoint(){
  for(unsigned n=0;n<2000000;++n){
   step();
   if(n>100&&!ime.hasWork()&&!app.commandBarrier()&&!app.manualSavePending()&&
      !app.persistence.backgroundSaveActive()&&!app.find.hasWork()&&present())return;
  }
  CHECK(false);
 }
 void stable(uint8_t usage,uint8_t mods=0){key(usage,mods);settleJoint();}
};

static void joint(bool vertical){
 JointRig r;CHECK(r.app.commitText("清风与正文身份。This text must stay in its document.",r.app.editStamp()));
 CHECK(r.app.session.manualSave(++r.now));r.settleJoint();const auto original=r.app.session.currentId();
 CHECK(r.app.session.create("联合测试中文长文件名称（恢复）（1）",++r.now));
 const auto other=r.app.session.currentId();CHECK(r.app.commitText("另一稿的独特正文标记",r.app.editStamp()));
 CHECK(r.app.session.manualSave(++r.now));r.settleJoint();CHECK(r.app.session.open(original,++r.now));r.settleJoint();
 if(vertical)r.stable(61);
 r.stable(9,1);CHECK(r.app.findOpen());const BodyGuard body(r);
 r.stable(57);CHECK(r.ime.chinese());r.stable(24);r.stable(12); // Original Xiaohe "ui".
 CHECK(r.ime.candidateAvailable()&&r.frame&&r.painted.candidates);
 const auto oldChoice=r.captured(30);const auto oldPaint=r.painted;const auto oldPresentation=r.app.presentationFrame();
 EditorSink oldQuery(r.app);CHECK(oldQuery.bind(11,11)&&oldQuery.target().kind==TextTarget::Find);
 const std::string query(r.app.find.view().query);
 r.stable(58);CHECK(r.app.findOpen()&&!r.app.menuHelpOpen());
 r.stable(56,1);CHECK(r.app.findOpen()&&!r.app.menuHelpOpen()&&r.app.find.view().query==query);body.check(r);
 r.stable(41);CHECK(r.app.findOpen()&&!r.ime.active());
 r.stable(59);CHECK(r.app.filesOpen()&&!r.app.findOpen());
 CHECK(!oldQuery.commit("错目标")&&!r.dispatch(oldChoice));
 const auto oldPublication=ime_device::publishActivePresentation(r.app,&r.ime,oldPresentation,{},oldPaint);
 CHECK(!oldPublication.candidates);body.check(r);
 for(unsigned n=0;r.app.manager.state().page.items[r.app.manager.state().selected].id!=other&&n<16;++n)r.stable(81);
 const auto& state=r.app.manager.state();CHECK(state.page.items[state.selected].id==other);
 const auto selected=state.selected,offset=state.page.offset,first=r.app.menuViewport.first;
 const auto before=r.menuPixels;
 r.stable(58);CHECK(r.app.menuHelpOpen());r.stable(79);r.stable(41);
 CHECK(!r.app.menuHelpOpen()&&state.selected==selected&&state.page.offset==offset&&
       state.page.items[state.selected].id==other&&r.app.menuViewport.first==first&&r.menuPixels==before);
 CHECK(!oldQuery.commit("错目标"));body.check(r);
 std::cout<<"PASS joint Find -> F2 -> help -> Esc preserves ID/batch/anchor/pixels and rejects old query/candidate layout="<<vertical<<'\n';

 r.stable(58);r.stable(21);CHECK(r.app.renameOpen()&&!r.app.menuHelpOpen());
 CHECK(!oldQuery.commit("错目标"));r.stable(4,1);
 EditorSink name(r.app);CHECK(name.bind(12,12)&&name.target().kind==TextTarget::Rename);
 CHECK(name.commit("联合名称目标"));CHECK(std::string_view(r.app.renameText())=="联合名称目标");body.check(r);
 r.stable(41);CHECK(!r.app.renameOpen());
 hanime::documents::DocumentMetadata meta;
 CHECK(r.app.catalog.get(other,&meta)==hanime::documents::Status::Ok&&meta.name=="联合测试中文长文件名称（恢复）（1）");
 CHECK(!name.commit("过期名称"));body.check(r);
 std::cout<<"PASS joint help -> rename uses fresh Rename target; cancel retains real name/body layout="<<vertical<<'\n';

 r.app.deferFileCommands(true);r.stable(58);r.key(40);
 CHECK(r.app.fileRequestPending()&&!r.app.menuHelpOpen());const auto serial=r.app.fileTiming.serial;
 r.key(58);r.key(40);CHECK(r.app.fileTiming.serial==serial&&!r.app.menuHelpOpen());
 r.app.service(++r.now,false);CHECK(r.app.session.currentId()==original);r.key(41);r.settleJoint();
 CHECK(!r.app.fileRequestPending()&&r.app.session.currentId()==original);body.check(r);
 r.stable(58);r.key(40);CHECK(r.app.fileRequestPending());
 r.app.service(++r.now,false);CHECK(r.app.session.currentId()==original);
 r.app.fileFramePresented(100,true);r.settleJoint();
 CHECK(r.app.session.currentId()==other&&!r.app.filesOpen()&&r.app.document.text()=="另一稿的独特正文标记");
 CHECK(!oldQuery.commit("错目标")&&!r.dispatch(oldChoice));
 std::cout<<"PASS joint help Enter obeys one-request LCD barrier/cancel and retires old Find authority layout="<<vertical<<'\n';

 r.stable(9,1);CHECK(r.app.findOpen()&&r.app.find.view().query.empty());const BodyGuard newBody(r);
 EditorSink fresh(r.app);CHECK(fresh.bind(13,13)&&fresh.target().kind==TextTarget::Find);
 CHECK(fresh.commit("另一稿"));r.settleJoint();newBody.check(r);
 r.stable(61);r.stable(58);CHECK(r.app.findOpen()&&!r.app.menuHelpOpen());newBody.check(r);
 r.stable(41);CHECK(!r.app.findOpen());r.key(59);
 // Deferred file commands remain enabled from the preceding bridge. Model
 // the actual busy-page LCD completion here too, rather than waiting forever
 // for a command whose admission correctly requires that receipt.
 CHECK(r.app.fileRequestPending()&&r.app.commandBarrier()&&!r.app.filesOpen());
 r.app.service(++r.now,false);CHECK(!r.app.filesOpen());
 std::cout<<"JOINT_BUSY_ACK final_F2 pending=1 barrier=1 opened_before_ack=0 layout="<<vertical<<'\n';
 r.app.fileFramePresented(200,true);r.settleJoint();r.stable(58);CHECK(r.app.menuHelpOpen());
 r.app.disconnected();r.repeat.clear();r.settleJoint();CHECK(!r.app.menuHelpOpen()&&!r.dispatch(oldChoice));
 std::cout<<"PASS joint fresh Find target after document switch/F4; help disconnect retires focus layout="<<vertical<<'\n';
}

int main(){try{for(bool vertical:{false,true})joint(vertical);std::cout<<"HOST_ONLY joint real product routing/target/painters; DEVICE_NOT_TESTED\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
