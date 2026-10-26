#define main inherited_r2_usb_fixture_main
#include "usb_product_flow.cpp"
#undef main
#include "docmanager_display.hpp"
#include "input_route.hpp"
#include "support/r2_raster.hpp"
#include "support/r2_names.hpp"
#include <chrono>
static unsigned openedFiles=0,frames=0;
static uint64_t hostDrawUs=0,hostDrawMaxUs=0;
static void tap(Rig& r,uint8_t usage,uint8_t mods=0){r.app.key({1,++r.now,usage,mods,true});r.app.key({1,++r.now,usage,mods,false});}
static void pumpApp(Rig& r){for(unsigned i=0;i<10000;++i){r.app.service(++r.now,false);if(!r.app.commandBarrier()&&!r.app.manualSavePending()&&!r.app.persistence.backgroundSaveActive())return;}CHECK(false);}
static void seed(Rig& r){for(const auto* name:R2Names)CHECK(r.app.session.create(name,++r.now));CHECK(r.app.session.create(std::string(96,'W'),++r.now));std::string closing;for(int i=0;i<32;++i)closing+="）";CHECK(r.app.session.create(closing,++r.now));tap(r,59);CHECK(r.app.filesOpen());}
static r2preview::Raster render(Rig& r,const char* output=nullptr,int renameBottom=328){
 r2preview::Raster canvas;const auto& s=r.app.manager.state();const auto selected=s.selected,offset=s.page.offset,count=s.page.items.size();
 const auto id=selected<count?s.page.items[selected].id:ds::DocumentId{};
 const auto name=selected<count?std::string(s.page.items[selected].name):std::string();
 const auto bodyId=r.app.session.currentId();const auto revision=r.app.document.revision();std::string selectedDraw;
 const bool list=r.app.filesOpen()&&!r.app.menuHelpOpen()&&!r.app.renameOpen()&&s.pending==ds::PendingAction::None;
 const auto lines=s3app::display::wrapMenuName(name,canvas.contentFont);
 if(list&&count)r.app.menuViewport.place(s,lines.count);
 const int selectedY=44+28*int(selected-r.app.menuViewport.first);
 s3fs::File::openObserver=[](const char*,const char*){++openedFiles;};const auto before=openedFiles;
 const auto started=std::chrono::steady_clock::now();
 const bool shown=s3app::display::documentMenu(r.app,canvas.contentFont,canvas.rect(),canvas.aux(),[&](std::string_view text,int x,int y,bool inverse){
  canvas.content()(text,x,y,inverse);
  if(list&&x==28&&y>=selectedY&&y<selectedY+28*lines.count)selectedDraw+=text;
 },renameBottom);s3app::display::menuFeedback(r.app,canvas.rect(),canvas.aux());s3fs::File::openObserver=nullptr;
 const auto elapsed=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());hostDrawUs+=elapsed;hostDrawMaxUs=std::max(hostDrawMaxUs,elapsed);
 CHECK(shown&&openedFiles==before&&selected==s.selected&&offset==s.page.offset&&count==s.page.items.size());
 CHECK((count?s.page.items[selected].id:ds::DocumentId{})==id&&r.app.session.currentId()==bodyId&&r.app.document.revision()==revision);
 if(list&&count){CHECK(selectedDraw==name);unsigned triangle=0;for(int y=28;y<360;++y)for(int x=10;x<16;++x)triangle+=canvas.pixels[y*300+x]==0;CHECK(triangle==36);}
 CHECK(!canvas.has("每批"));if(output)canvas.save(output);++frames;return canvas;
}
static void hidReport(){
 // The same boot-report descriptor fixture used by frame_consistency.cpp.
 // This proves the parser path only, never the physical keyboard's Fn layer.
 const uint8_t descriptor[]={0x05,1,0x09,6,0xa1,1,0x05,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,6,0x75,8,0x15,0,0x25,0x65,0x05,7,0x19,0,0x29,0x65,0x81,0,0xc0};
 s3hid::Map map;CHECK(map.parse(descriptor,sizeof(descriptor)));CHECK(map.report(0));
 for(uint8_t mods:{uint8_t(0),uint8_t(1),uint8_t(8)}){
  const uint8_t usage=mods?56:58;uint8_t bytes[8]={mods,0,usage,0,0,0,0,0};s3hid::State state;
  CHECK(s3hid::Map::decode(*map.report(0),bytes,sizeof(bytes),state));CHECK(state.has(usage)&&state.mods==mods);
  s3hid::Tracker tracker;unsigned presses=0,releases=0;
  auto emit=[&](s3hid::Event e){if(e.usage==usage){CHECK(e.mods==mods);if(e.down)++presses;else ++releases;}};
  tracker.update(0,state,1,1,emit);tracker.update(0,state,1,2,emit);state.keys={};tracker.update(0,state,1,3,emit);
  CHECK(presses==1&&releases==1);
 }
 std::cout<<"PASS synthetic HID F1/Ctrl-slash/Cmd-slash press release dedup; DEVICE_NOT_TESTED\n";
}
static void walk(){Rig r;seed(r);size_t visited=0;unsigned page=0;
 do{
  CHECK(r.app.manager.state().page.items.size()<=16);const auto n=r.app.manager.state().page.items.size();
  for(size_t i=0;i<n;++i){CHECK(r.app.manager.state().selected==i);const auto c=render(r,visited==2?"r2-files-view9":nullptr);(void)c;
   if(std::string_view(r.app.manager.state().page.items[i].name)==std::string(96,'W'))render(r,"r2-files-96W");
   if(r.app.manager.state().page.items[i].name.size()>60&&std::string_view(r.app.manager.state().page.items[i].name).find("（恢复）")!=std::string_view::npos)render(r,"r2-files-restored-suffix");
   if(std::string_view(r.app.manager.state().page.items[i].name).find("城市公共空间与居民日常生活研究甲")==0)render(r,"r2-files-disambiguation");
   if(std::string_view(r.app.manager.state().page.items[i].name).substr(0,3)=="）")render(r,"r2-files-bracket-relaxed");
   ++visited;if(i+1<n)tap(r,81);
  }
  const auto offset=r.app.manager.state().page.offset;const auto selected=r.app.manager.state().selected;
  const auto id=r.app.manager.state().page.items[selected].id;const auto first=r.app.menuViewport.first;
  s3hid::InputRoute route;route.framePresented(r.app.focusToken());const auto old=route.epoch();
  tap(r,58);CHECK(r.app.menuHelpOpen());render(r,"r2-help-files-1");route.framePresented(r.app.focusToken());
  CHECK(!route.accept({1,r.now,40,0,true,old},r.now));tap(r,79);CHECK(r.app.menuHelpPage()==1);render(r,"r2-help-files-2");
  tap(r,80);CHECK(r.app.menuHelpPage()==0);tap(r,58);CHECK(r.app.menuHelpOpen());tap(r,41);
  CHECK(!r.app.menuHelpOpen()&&r.app.menuViewport.first==first&&r.app.manager.state().page.offset==offset&&r.app.manager.state().selected==selected&&r.app.manager.state().page.items[selected].id==id);render(r);
  tap(r,56,1);CHECK(r.app.menuHelpOpen());tap(r,41);tap(r,56,8);CHECK(r.app.menuHelpOpen());tap(r,41);tap(r,56,9);CHECK(!r.app.menuHelpOpen());
  // Fast reverse and forward changes keep real selected name/ID in each frame.
  for(size_t i=1;i<n;++i){tap(r,82);render(r);}for(size_t i=1;i<n;++i){tap(r,81);render(r);}
  if(!r.app.manager.state().page.has_more)break;
  tap(r,79);CHECK(r.app.manager.state().page.offset==offset+16&&r.app.manager.state().selected==0);render(r,"r2-files-next-page");
  tap(r,80);CHECK(r.app.manager.state().page.offset==offset&&r.app.manager.state().selected==0);render(r);
  tap(r,79);CHECK(r.app.manager.state().page.offset==offset+16&&r.app.manager.state().selected==0);++page;
 }while(page<8);
 CHECK(visited>=34);std::cout<<"PASS product names="<<visited<<" frames="<<frames<<" real_16_batch VIEW9 full_names anchor help_restore no_drawing_IO\n";
}
static void actions(){Rig r;seed(r);auto chosen=r.app.manager.state().page.items[0].id;
 tap(r,58);tap(r,40);CHECK(!r.app.menuHelpOpen()&&!r.app.filesOpen()&&r.app.session.currentId()==chosen);
 tap(r,58);CHECK(!r.app.menuHelpOpen()); // F1 has no global meaning.
 tap(r,59);tap(r,58);tap(r,21);CHECK(!r.app.menuHelpOpen()&&r.app.renameOpen());
 tap(r,4,1);CHECK(r.app.commitName("重命名测试（恢复）（1）",r.app.nameStamp()));render(r,"r2-rename");tap(r,40);CHECK(!r.app.renameOpen());
 ds::DocumentMetadata metadata;CHECK(r.app.catalog.get(chosen,&metadata)==ds::Status::Ok&&metadata.name=="重命名测试（恢复）（1）");
 tap(r,7);CHECK(r.app.manager.state().pending==ds::PendingAction::TrashSelected);render(r,"r2-trash-confirm");tap(r,58);CHECK(!r.app.menuHelpOpen());tap(r,41);CHECK(r.app.manager.state().pending==ds::PendingAction::None);
 tap(r,23);CHECK(r.app.manager.state().view==ds::FileManagerView::Trash);render(r,"r2-trash-empty");tap(r,58);render(r,"r2-help-trash-1");tap(r,79);render(r,"r2-help-trash-2");tap(r,41);tap(r,23);
 chosen=r.app.manager.state().page.items[r.app.manager.state().selected].id;tap(r,7);tap(r,40);pumpApp(r);CHECK(r.app.lastOperation);
 tap(r,23);CHECK(r.app.manager.state().view==ds::FileManagerView::Trash);render(r,"r2-trash");
 const auto body=r.app.session.currentId();tap(r,58);tap(r,40);pumpApp(r);CHECK(!r.app.menuHelpOpen()&&r.app.session.currentId()==body);CHECK(r.app.catalog.get(chosen,&metadata)==ds::Status::Ok&&metadata.area==ds::DocumentArea::Normal);render(r,"r2-restored-result");
 tap(r,58);CHECK(r.app.menuHelpOpen());r.app.disconnected();CHECK(!r.app.menuHelpOpen());
 tap(r,58);CHECK(r.app.menuHelpOpen());CHECK(r.app.enterTransfer());CHECK(!r.app.menuHelpOpen());r.app.leaveTransfer();
 tap(r,58);CHECK(r.app.menuHelpOpen());tap(r,61);CHECK(!r.app.menuHelpOpen());
 std::cout<<"PASS help passes Enter/R unchanged, confirmationguard, no global F1, real rename/trash/restore, disconnect/transfer/F4 focus\n";
}
static void errorFeedback(){Rig r;seed(r);const std::string duplicate(r.app.manager.state().page.items[1].name);
 tap(r,21);CHECK(r.app.renameOpen());CHECK(!r.app.commitName(std::string(97,'W'),r.app.nameStamp()));
 for(int bottom:{328,266,198})CHECK(render(r,bottom==198?"r2-rename-limit-expanded":nullptr,bottom).has("最多 96"));
 tap(r,4,1);CHECK(r.app.commitName(duplicate,r.app.nameStamp()));tap(r,40);CHECK(r.app.renameOpen()&&r.app.lastOperation.status==ds::Status::NameExists);
 for(int bottom:{328,266,198})CHECK(render(r,bottom==266?"r2-rename-name-exists-cn":nullptr,bottom).has("已有同名"));
 tap(r,4,1);tap(r,42);tap(r,40);CHECK(r.app.lastOperation.status==ds::Status::InvalidName);
 CHECK(render(r,nullptr,198).has("名称不能为空"));
 CHECK(r.app.commitName("真实写入失败检查",r.app.nameStamp()));s3fs::faults.write_bytes=0;tap(r,40);s3fs::faults={};
 CHECK(r.app.renameOpen()&&!r.app.lastOperation);CHECK(render(r,"r2-rename-io-error-cn",266).has(s3app::display::menuStatus(r.app.lastOperation.status)));
 tap(r,41);r.app.notice="Turn Wi-Fi off before USB transfer";CHECK(render(r,"r2-files-transfer-denied").has("先关闭 Wi-Fi"));
 tap(r,58);CHECK(r.app.menuHelpOpen());CHECK(render(r,"r2-help-transfer-denied").has("先关闭 Wi-Fi"));
 std::cout<<"PASS real rename limit/nameexists/invalid/writefailure feedback in EN and CN compact/expanded viewport; transfer denial notice remains visible\n";
}
static void deferredHelp(){Rig r;seed(r);r.app.deferFileCommands(true);
 const auto old=r.app.session.currentId();tap(r,81);const auto chosen=r.app.manager.state().page.items[r.app.manager.state().selected].id;CHECK(chosen!=old);
 tap(r,58);tap(r,40);CHECK(r.app.fileRequestPending()&&!r.app.menuHelpOpen());
 const auto serial=r.app.fileTiming.serial;tap(r,58);CHECK(r.app.fileTiming.serial==serial&&!r.app.menuHelpOpen());
 r.app.service(++r.now,false);CHECK(r.app.session.currentId()==old);tap(r,41);CHECK(!r.app.fileRequestPending()&&r.app.session.currentId()==old);
 tap(r,58);CHECK(r.app.menuHelpOpen());tap(r,40);CHECK(r.app.fileRequestPending());r.app.service(++r.now,false);CHECK(r.app.session.currentId()==old);
 r.app.fileFramePresented(100,true);pumpApp(r);CHECK(r.app.session.currentId()==chosen&&!r.app.filesOpen());
 std::cout<<"PASS help close passes one deferred command; F1 blocked while queued; Esc cancels; existing LCD-feedback barrier retained\n";
}
int main(){try{hidReport();walk();actions();errorFeedback();deferredHelp();std::cout<<"HOST_ONLY ASAN raster frames="<<frames<<" total_us="<<hostDrawUs<<" max_us="<<hostDrawMaxUs<<" DEVICE_COST_NOT_MEASURED\n";return 0;}catch(const std::exception& e){s3fs::File::openObserver=nullptr;std::cerr<<e.what()<<'\n';return 1;}}
