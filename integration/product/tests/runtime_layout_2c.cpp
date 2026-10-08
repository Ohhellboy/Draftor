// Real P23 App/IME/Host/resource pipeline; only framebuffer and storage are Host ports.
#define main phase2c_unused_legacy_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"
#include "vertical_view.hpp"
#include "presentation_frame.hpp"
#include "support/app_test_access.hpp"
#include <map>
#include <fstream>
#include <vector>
extern bool phase2c_forbid_new;
extern std::size_t phase2c_new_calls;
namespace v=draftor::vertical;
using Mode=s3app::LayoutMode;
using Affinity=hanime::renderer::CaretAffinity;
struct BodyFact {
 std::string text;std::size_t cursor,undo,redo;std::uint32_t revision,text_revision;
 editor::body_edit::SelectionView selection;Affinity affinity;bool dirty;
 const void* history;std::uint64_t focus,session;hanime::documents::DocumentId file;
 explicit BodyFact(const s3app::App& a):text(a.document.text()),cursor(a.document.cursorByte()),undo(a.editor.undoCount()),redo(a.editor.redoCount()),revision(a.document.revision()),text_revision(a.document.textRevision()),selection(a.editor.selection()),affinity(a.editor.caretAffinity()),dirty(a.document.dirty()),history(a.editor.historyAllocation()),focus(a.focusToken()),session(a.session.sessionSerial()),file(a.session.currentId()){}
 void check(const s3app::App& a)const{
  CHECK(a.document.text()==text.c_str()&&a.document.cursorByte()==cursor);
  CHECK(a.document.revision()==revision&&a.document.textRevision()==text_revision&&a.document.dirty()==dirty);
  const auto s=a.editor.selection();CHECK(s.begin==selection.begin&&s.end==selection.end&&s.anchor==selection.anchor&&s.active==selection.active&&s.version==selection.version&&s.active_affinity==selection.active_affinity&&s.has_selection==selection.has_selection);
  CHECK(a.editor.undoCount()==undo&&a.editor.redoCount()==redo&&a.editor.historyAllocation()==history&&a.editor.caretAffinity()==affinity);
  CHECK(a.focusToken()==focus&&a.session.sessionSerial()==session&&a.session.currentId()==file);
 }
};
bool equalItem(const VisibleItem& a,const VisibleItem& b){
 return a.key==b.key&&a.ref==b.ref&&a.input==b.input&&a.revision==b.revision&&a.scheme==b.scheme&&
   a.page_generation==b.page_generation&&a.page==b.page&&a.slot==b.slot&&a.begin==b.begin&&a.end==b.end&&
   a.activation.mount_epoch()==b.activation.mount_epoch()&&a.activation.shared_bundle_id()==b.activation.shared_bundle_id()&&
   a.bytes==b.bytes&&std::equal(a.text.begin(),a.text.begin()+a.bytes,b.text.begin());
}
struct ImeFact {
 const void* composition;const void* window;std::string raw,selected;std::uint64_t input,revision,focus,scheme;
 std::uint32_t generation,query,layout,attempt,ingress,resource,action;bool chinese,working,selecting;std::size_t arrows,input_queue;
 CandidateView view;
 explicit ImeFact(DeviceIme& ime):composition(ime.composition()),window(ime.window()),raw(ime.raw()),selected(ime.selected()),
  input(ime.composition()->snapshot().input_id),revision(ime.composition()->snapshot().revision),focus(ime.composition()->snapshot().focus),scheme(ime.composition()->snapshot().scheme_epoch),generation(ime.view()->generation),query(ime.window()->querySerial()),layout(ime.window()->layoutSerial()),attempt(ime.attemptSerial()),ingress(ime.ingressContext()),resource(ime.resourceOwner()),action(ime.actionId()),chinese(ime.chinese()),working(ime.hasWork()),selecting(ime.window()->selecting()),arrows(ime.queuedArrows()),input_queue(ime.queuedInput()),view(*ime.view()){}
 void check(DeviceIme& ime)const{
  const auto s=ime.composition()->snapshot();CHECK(ime.composition()==composition&&ime.window()==window&&ime.raw()==raw&&ime.selected()==selected);
  CHECK(s.input_id==input&&s.revision==revision&&s.focus==focus&&s.scheme_epoch==scheme);
  CHECK(ime.window()->querySerial()==query&&ime.window()->layoutSerial()==layout);
  CHECK(ime.view()->generation>=generation);
  CHECK(ime.attemptSerial()==attempt&&ime.ingressContext()==ingress&&ime.resourceOwner()==resource&&ime.actionId()==action);
  CHECK(ime.chinese()==chinese&&ime.hasWork()==working&&ime.window()->selecting()==selecting&&ime.queuedArrows()==arrows&&ime.queuedInput()==input_queue);
  const auto& now=*ime.view();
  if(now.generation!=generation&&view.count&&now.count){
   const auto& old=view.rows[view.row-view.rows[0].index].items[view.col];
   const auto& current=now.rows[now.row-now.rows[0].index].items[now.col];
   CHECK(equalItem(old,current)&&old.ordinal==current.ordinal);
   for(size_t r=0;r<now.count;++r)for(size_t c=1;c<now.rows[r].count;++c)CHECK(now.rows[r].items[c].ordinal==now.rows[r].items[c-1].ordinal+1);
   return; // R3-R1: physical groups/generation change, core identity does not.
  }
  CHECK(now.count==view.count&&now.row==view.row&&now.col==view.col&&now.expanded==view.expanded);
  for(size_t r=0;r<view.count;++r){CHECK(now.rows[r].count==view.rows[r].count&&now.rows[r].index==view.rows[r].index);
   for(size_t c=0;c<view.rows[r].count;++c)CHECK(equalItem(now.rows[r].items[c],view.rows[r].items[c]));}
 }
};
struct SwitchRig:Rig {
 s3app::ProductInput input;
 std::vector<VisibleItem> numbers;
 std::array<std::uint8_t,300*400> pixels{};
 ime_device::VerticalImeFrame vertical_frame{};
 s3hid::Event event(std::uint8_t key,bool down=true,std::uint8_t mods=0){
  s3hid::Event e{1,++now,key,mods,down,0,frame,ime.attemptSerial()};e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();return e;
 }
 void deliver(const s3hid::Event& e){if(input.layout(app,e))return;const bool chinese=ime.chinese();if(!input.ime(app,&ime,e))app.key(e,chinese);}
 void tap(std::uint8_t key,std::uint8_t mods=0){deliver(event(key,true,mods));deliver(event(key,false,mods));}
 void toggle(){ // Deliberately no IME getter, scheduler, renderer or file service in F4.
  for(bool down:{true,false}){s3hid::Event e{1,++now,61,0,down};CHECK(input.layout(app,e));}
 }
 void type(std::string_view text){for(char c:text)tap(std::uint8_t(c-'a'+4));}
 void settle(bool paint=true){unsigned n=0;do{step();CHECK(++n<2000000);}while(!resources.ready()||!ime.composition()||ime.hasWork());if(paint)draw();}
 void begin(std::string_view raw){tap(57);settle();type(raw);settle();}
 void draw(){
  const auto token=app.presentationFrame();
  // Production retries at the next foreground slice; Host drains only the
  // demanded repacking operation before accepting this physical frame.
  for(unsigned slices=0;!prepareImePresentation(&ime,app);++slices){CHECK(slices<2000000);step();}
  numbers.clear();pixels.fill(0);
  if(app.horizontalActive()){
   app.layout.setHeightPx(candidateSurfaceVisible(&ime,&app)?(ime.expanded()?210:278)-34:306);
   const auto h=app.navigator.sync();CHECK(h.visual_line>=h.first_visible_line&&h.visual_line<h.first_visible_line+h.visible_lines);
   if(ime.candidateAvailable()){const auto& view=*ime.view();const auto& row=view.rows[view.row-view.rows[0].index];for(size_t i=0;i<row.count;++i)numbers.push_back(row.items[i]);}
  }else{
   const s3app::VerticalPaintPort port{
    [](void* p,v::Rect box,s3app::VerticalColor color){auto& r=*static_cast<SwitchRig*>(p);CHECK(unsigned(box.x)+box.width<=300&&unsigned(box.y)+box.height<=400);for(unsigned y=box.y;y<unsigned(box.y)+box.height;++y)for(unsigned x=box.x;x<unsigned(box.x)+box.width;++x)r.pixels[y*300+x]=std::uint8_t(color);},
    [](void* p,std::uint16_t x,std::uint16_t y,s3app::VerticalColor color){CHECK(x<300&&y<400);static_cast<SwitchRig*>(p)->pixels[y*300+x]=std::uint8_t(color);},this,
    [](void* p,const v::AbRun& run){auto& r=*static_cast<SwitchRig*>(p);if(run.role==v::AbRole::CurrentCandidate){CHECK(run.number==r.numbers.size()+1&&run.identity);r.numbers.push_back(*static_cast<const VisibleItem*>(run.identity));}else if(run.role==v::AbRole::Preview)CHECK(run.number==0&&!run.identity&&!run.focused);}};
   const auto painted=s3app::paintVerticalView(app,app.verticalPresentation(),&ime,font,now,port);
   if(painted.status!=v::Status::Ready)std::cerr<<"vertical draw status="<<int(painted.status)<<" raw="<<ime.raw()<<" work="<<ime.hasWork()<<'\n';
   CHECK(painted.status==v::Status::Ready);vertical_frame=painted.ime;
   const auto& vp=app.verticalPresentation();CHECK(vp.viewport().visible(vp.caret().caret.column));
  }
  const auto published=publishActivePresentation(app,&ime,token,vertical_frame);
  CHECK(published.accepted);frame=published.candidates;
 }
 void body(std::string_view t){CHECK(app.commitText(t,app.editStamp()));draw();}
 void move(size_t b,bool extend=false){const auto out=app.moveToByte(b,extend);CHECK(out.code==editor::body_edit::ResultCode::Applied||out.code==editor::body_edit::ResultCode::NoAction);draw();}
};
std::string longBody(){std::string b;for(unsigned i=0;i<140;++i)b+="天地玄黄 hello world 2026\n";return b;}
void S1(){SwitchRig r;r.body(longBody());r.move(720);r.move(750,true);r.draw();const BodyFact before(r.app);const auto h=r.app.navigator.sync();r.toggle();before.check(r.app);CHECK(r.app.layoutMode()==Mode::Vertical);r.draw();r.toggle();r.draw();before.check(r.app);const auto back=r.app.navigator.sync();CHECK(h.first_visible_line==back.first_visible_line&&h.cursor_x==back.cursor_x&&h.affinity==back.affinity&&h.preferred_x==back.preferred_x);}
void S2(){SwitchRig r;r.body(longBody());r.toggle();r.move(900);r.draw();const auto first=r.app.verticalPresentation().viewport().firstColumn();const auto caret=r.app.verticalPresentation().caret();const BodyFact fact(r.app);r.toggle();r.draw();r.toggle();r.draw();fact.check(r.app);CHECK(first==r.app.verticalPresentation().viewport().firstColumn()&&caret.caret.y_px==r.app.verticalPresentation().caret().caret.y_px);}
void S3(){SwitchRig r;r.body(longBody());r.move(1800);const auto cursor=r.app.document.cursorByte();r.toggle();r.draw();CHECK(r.app.verticalPresentation().caret().caret.position.byte==cursor&&r.app.verticalPresentation().viewport().firstColumn()>0);
 SwitchRig firstH;firstH.toggle();firstH.body(longBody());firstH.move(1800);firstH.toggle();firstH.draw();CHECK(firstH.app.navigator.sync().first_visible_line>0&&firstH.app.document.cursorByte()==1800);}
void S4(){SwitchRig r;r.body(longBody());r.move(0);r.toggle();r.move(r.app.document.text().size());r.body("末尾");const auto cursor=r.app.document.cursorByte();r.toggle();r.draw();CHECK(r.app.document.cursorByte()==cursor&&r.app.navigator.sync().first_visible_line>0);r.tap(29,1);r.tap(28,1);r.move(0);r.body("start ");r.toggle();r.draw();CHECK(r.app.verticalPresentation().viewport().firstColumn()==0);}
void S5(){SwitchRig r;r.body("天地玄黄\n宇宙洪荒\n日月盈昃");for(auto range:{std::pair<size_t,size_t>{0,3},{3,22},{22,3}}){r.move(range.first);r.move(range.second,true);const BodyFact b(r.app);r.toggle();r.draw();b.check(r.app);r.toggle();r.draw();b.check(r.app);}}
void S6(){SwitchRig r;r.body("A");r.toggle();r.toggle();CHECK(r.app.editor.undoCount()==1);r.tap(29,1);CHECK(r.app.document.text().empty()&&r.app.editor.redoCount()==1);r.toggle();r.tap(28,1);CHECK(r.app.document.text()=="A"&&r.app.editor.undoCount()==1);}
void S7(){SwitchRig r;r.body(longBody()+"终点");r.move(0);r.tap(9,1);CHECK(r.app.findOpen());CHECK(r.app.find.setQuery("终点")==hanime::find::QueryEditResult::Applied);r.tap(40);for(unsigned i=0;r.app.find.view().status==hanime::find::Status::Searching;++i){CHECK(i<10000);r.app.service(++r.now,false);}const auto view=r.app.find.view();const BodyFact b(r.app);const auto scans=r.app.scans.find_completed;for(unsigned i=0;i<2;++i){r.toggle();r.draw();b.check(r.app);const auto now=r.app.find.view();CHECK(now.open&&now.query==view.query&&now.query_version==view.query_version&&now.match.begin==view.match.begin&&now.match.end==view.match.end&&r.app.scans.find_completed==scans);}}
void S8(){SwitchRig r;r.begin("wowo");const BodyFact b(r.app);const ImeFact ime(r.ime);for(unsigned i=0;i<2;++i){r.toggle();ime.check(r.ime);b.check(r.app);r.draw();ime.check(r.ime);}r.type("wo");r.settle();CHECK(r.ime.raw()=="wowowo");}
void S9(){SwitchRig r;r.begin("ui");for(size_t row:{0u,1u,4u}){if(row){r.ime.window()->request(row,0,true);r.settle();}const auto before=r.numbers;CHECK(!before.empty()&&before.size()<=6);const ImeFact ime(r.ime);r.toggle();r.draw();ime.check(r.ime);CHECK(r.numbers.size()<=6&&!r.numbers.empty());
 for(const auto& item:r.numbers)for(const auto& old:before)if(item.ordinal==old.ordinal)CHECK(equalItem(item,old));std::cout<<"MAPPING row="<<row<<" selectable="<<before.size()<<'\n';r.toggle();r.draw();}}
void S10(){for(bool redraw:{false,true}){SwitchRig r;r.begin("ui");CHECK(r.numbers.size()>1);const auto item=r.numbers[1];const std::string expected(item.text.data(),item.bytes);r.toggle();if(redraw)r.draw();r.tap(31);r.settle();CHECK(r.app.document.text()==expected.c_str()&&r.app.editor.undoCount()==1&&r.ime.raw().empty());for(unsigned i=0;i<10;++i)r.step();CHECK(r.app.editor.undoCount()==1);}}
void S11(){SwitchRig r;r.tap(57);r.settle();r.type("ui");CHECK(r.ime.hasWork()&&r.ime.window()->busy());const ImeFact ime(r.ime);r.toggle();ime.check(r.ime);r.draw();ime.check(r.ime);r.settle();CHECK(!r.ime.hasWork()&&r.ime.raw()=="ui"&&!r.numbers.empty());CHECK(r.app.layoutMode()==Mode::Vertical&&r.ime.window()->querySerial()==ime.query);
 r.tap(80);CHECK(r.ime.hasWork());const ImeFact navigation(r.ime);r.toggle();navigation.check(r.ime);r.settle();CHECK(r.ime.view()->row==0&&r.ime.expanded());}
void S12(){SwitchRig r;r.begin("ui");const auto item=r.numbers[1];r.tap(31);CHECK(r.ime.hasWork()&&r.ime.window()->selecting()&&r.app.document.text().empty());const ImeFact pending(r.ime);r.toggle();pending.check(r.ime);r.draw();r.settle();CHECK(r.app.document.text()==std::string_view(item.text.data(),item.bytes)&&r.app.editor.undoCount()==1&&r.ime.raw().empty());const auto rev=r.app.document.textRevision();for(unsigned i=0;i<30;++i){r.toggle();r.step();}CHECK(r.app.document.textRevision()==rev&&r.app.editor.undoCount()==1);std::cout<<"PENDING_SELECTION captured=1 accepted_mutations=1 replay=0\n";}
void S13(){SwitchRig r;r.begin("ui");const auto old=r.app.presentationFrame();const auto semantic=r.ime.window()->onScreenGeneration();r.toggle();CHECK(r.app.presentationPending()&&!r.app.currentPresentation(old));CHECK(!publishActivePresentation(r.app,&r.ime,old).accepted);CHECK(r.ime.window()->onScreenGeneration()==semantic);r.draw();const auto vertical=r.app.presentationFrame();r.toggle();CHECK(!publishActivePresentation(r.app,&r.ime,vertical,r.vertical_frame).accepted&&!publishActivePresentation(r.app,&r.ime,old).accepted);r.draw();CHECK(!r.app.presentationPending());}
std::map<std::string,std::string> filesAt(const fs::path& root){std::map<std::string,std::string> result;for(const auto& f:fs::recursive_directory_iterator(root))if(f.is_regular_file()){std::ifstream in(f.path(),std::ios::binary);result.emplace(f.path().lexically_relative(root).string(),std::string(std::istreambuf_iterator<char>(in),{}));}return result;}
void S14(){SwitchRig r;r.body(longBody()+"2026 二〇二六 hello world 12.5 97% 中文标点。");r.begin("ui");const BodyFact b(r.app);const ImeFact ime(r.ime);const auto files=filesAt(r.path);const auto io=metrics.io();const auto mem=draftor::memory::allocation_stats;const auto news=phase2c_new_calls;const auto* vertical=&r.app.verticalPresentation();const auto* horizontal=&r.app.navigator;
 phase2c_forbid_new=true;draftor::memory::allocation_fail_after=0;for(unsigned i=0;i<1000;++i)r.toggle();draftor::memory::allocation_fail_after=-1;phase2c_forbid_new=false;
 CHECK(phase2c_new_calls==news&&draftor::memory::allocation_stats.calls==mem.calls&&draftor::memory::allocation_stats.live_allocations==mem.live_allocations);const auto delta=metrics.io()-io;CHECK(delta.requests==0&&delta.reads==0&&delta.bytes==0&&delta.seeks==0);CHECK(filesAt(r.path)==files);b.check(r.app);ime.check(r.ime);CHECK(vertical==&r.app.verticalPresentation()&&horizontal==&r.app.navigator&&r.app.horizontalActive());r.draw();const auto h=r.app.navigator.sync();
 for(unsigned i=0;i<128;++i){r.toggle();r.draw();}b.check(r.app);ime.check(r.ime);CHECK(r.app.navigator.sync().first_visible_line==h.first_visible_line);
 std::cout<<"TOGGLE rendered=128 count=1000 new_calls=0 editor_allocations=0 resource_reads=0 files_identical=1\n";}
void S15(){SwitchRig r;r.tap(4,2);r.toggle();r.tap(5,2);r.toggle();r.tap(6,2);CHECK(r.app.document.text()=="ABC"&&r.app.editor.undoCount()==3);for(auto expected:{"AB","A",""}){r.tap(29,1);CHECK(r.app.document.text()==expected);}for(auto expected:{"A","AB","ABC"}){r.tap(28,1);CHECK(r.app.document.text()==expected);}}
void S16(){SwitchRig r;r.begin("ui");r.toggle();r.draw();r.tap(30);r.settle();CHECK(r.app.document.text()=="是"&&r.app.editor.undoCount()==1);const auto* composition=r.ime.composition();r.type("wo");r.settle();r.toggle();r.draw();r.tap(30);r.settle();CHECK(r.app.document.text()=="是我"&&r.app.editor.undoCount()==2&&r.ime.composition()==composition);}
void S17(){SwitchRig r;r.body("initial 2026");r.tap(22,1);for(unsigned i=0;r.app.persistence.backgroundSaveActive();++i){CHECK(i<20000);r.app.service(++r.now,false);}CHECK(r.app.lastOperation);const auto id=r.app.session.currentId();CHECK(r.app.session.create("other",++r.now));CHECK(r.app.session.open(id,++r.now));r.toggle();r.body("二〇二六");r.tap(22,1);for(unsigned i=0;r.app.persistence.backgroundSaveActive();++i){CHECK(i<20000);r.app.service(++r.now,false);}CHECK(r.app.lastOperation);r.toggle();r.body("H");r.tap(29,1);CHECK(r.app.document.text()=="initial 2026二〇二六");CHECK(r.app.session.create("third",++r.now));CHECK(r.app.session.open(id,++r.now));CHECK(r.app.document.text()=="initial 2026二〇二六"&&r.app.session.currentId()==id&&r.app.horizontalActive());for(const auto& f:filesAt(r.path))CHECK(f.second.find("LayoutMode")==std::string::npos&&f.second.find("Vertical")==std::string::npos);}
void S18(){SwitchRig r;CHECK(r.app.horizontalActive());r.body("天地玄黄天地玄黄天地玄黄");r.move(0);for(unsigned i=0;i<12;++i){r.tap(79);r.draw();if(r.app.editor.caretAffinity()==Affinity::Upstream)break;}const BodyFact affinity(r.app);const auto h=r.app.navigator.sync();r.toggle();r.draw();r.toggle();r.draw();affinity.check(r.app);CHECK(r.app.navigator.sync().affinity==h.affinity);
 auto e=r.event(61);r.deliver(e);const auto token=r.app.presentationFrame();r.deliver(e);CHECK(r.app.presentationFrame().revision==token.revision);e.down=false;r.deliver(e);r.tap(61,1);CHECK(r.app.presentationFrame().revision==token.revision);
 r.begin("ui");if(r.app.horizontalActive())r.toggle();r.draw();r.tap(80);r.settle();CHECK(r.ime.view()->row==0&&r.ime.expanded());r.tap(79);r.settle();CHECK(r.ime.view()->row==0);r.tap(45);r.settle();CHECK(r.ime.view()->row==1);r.tap(46);r.settle();CHECK(r.ime.view()->row==0);
 e=r.event(81);r.deliver(e);r.toggle();e.down=false;e.ms=++r.now;r.deliver(e);r.settle();r.tap(81);r.settle();CHECK(r.ime.view()->row==1);std::cout<<"H_DEFAULT affinity preserved; F4 duplicate/modifier and direction down/up routing PASS\n";}
#ifndef PHASE2C_REUSE_RIG
int main(int argc,char** argv){try{
 CHECK(argc==2);const std::string_view name(argv[1]);
 const std::pair<const char*,void(*)()> cases[]={{"S1",S1},{"S2",S2},{"S3",S3},{"S4",S4},{"S5",S5},{"S6",S6},{"S7",S7},{"S8",S8},{"S9",S9},{"S10",S10},{"S11",S11},{"S12",S12},{"S13",S13},{"S14",S14},{"S15",S15},{"S16",S16},{"S17",S17},{"S18",S18}};
 for(const auto& test:cases)if(name==test.first){test.second();CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS "<<name<<'\n';return 0;}CHECK(false);
 }catch(const std::exception& e){phase2c_forbid_new=false;draftor::memory::allocation_fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}}

#endif
