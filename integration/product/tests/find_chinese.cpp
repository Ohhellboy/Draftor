#include <filesystem>
#include <string>
// Use the explicit, authenticated Alpha 2 Host inputs selected by CMake.
// Do not silently replace them with an older source-relative staged bundle.
#if !defined(RESOURCE_ROOT) || !defined(INDEX_ROOT)
#error "Explicit Host resource and index roots are required"
#endif
#define main unused_find_query_rows_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"
#include "presentation_frame.hpp"
#include "horizontal_body.hpp"
#include "vertical_view.hpp"
#include "support/app_test_access.hpp"
#include <fstream>
struct FindRig:Rig {
 s3app::ProductInput product; s3hid::Repeat repeat;
 std::array<uint8_t,300*400> pixels{};s3app::find_ui::Receipt painted{};
 static void dot(void* ptr,int x,int y,bool ink){auto& r=*static_cast<FindRig*>(ptr);CHECK(x>=0&&x<300&&y>=0&&y<400);r.pixels[y*300+x]=ink?0:255;}
 void rect(int x,int y,int w,int h,bool ink){for(int j=0;j<h;++j)for(int i=0;i<w;++i)if(x+i>=0&&x+i<300&&y+j>=0&&y+j<400)dot(this,x+i,y+j,ink);}
 void bodyGlyph(uint32_t cp,int x,int y,bool ink){const auto g=font.glyph(cp);for(unsigned j=0;j<g.height;++j)for(unsigned i=0;i<g.width;++i)if(g.bits[j*g.stride+i/8]&(0x80>>(i%8)))if(x+int(i)>=0&&x+int(i)<300&&y+int(j)>=0&&y+int(j)<400)dot(this,x+i,y+j,ink);}
 bool display(){
  if(!ime_device::prepareImePresentation(&ime,app))return false;
  pixels.fill(255);s3app::VerticalPaintResult vertical;
  if(app.horizontalActive())s3app::paintHorizontalBody(app,font,[&](int x,int y,int w,int h,bool ink){rect(x,y,w,h,ink);},[&](uint32_t cp,int x,int y,bool ink){bodyGlyph(cp,x,y,ink);});
  else{
   s3app::VerticalPaintPort port{
    [](void* ptr,draftor::vertical::Rect box,s3app::VerticalColor color){static_cast<FindRig*>(ptr)->rect(box.x,box.y,box.width,box.height,color!=s3app::VerticalColor::White);},
    [](void* ptr,uint16_t x,uint16_t y,s3app::VerticalColor color){dot(ptr,x,y,color!=s3app::VerticalColor::White);},this,nullptr};
   vertical=s3app::paintVerticalView(app,app.verticalPresentation(),app.findOpen()?nullptr:&ime,font,now,port);CHECK(vertical.status==draftor::vertical::Status::Ready);
  }
  painted=app.findOpen()?s3app::find_ui::paint(app,&ime,{dot,this}):s3app::find_ui::Receipt{};
  const auto result=ime_device::publishActivePresentation(app,&ime,app.presentationFrame(),vertical.ime,painted);CHECK(result.accepted);frame=result.candidates;
  if(app.findOpen()&&painted.candidates)CHECK(frame==ime.view()->generation);return true;
 }
 void settle(){unsigned n=0;for(;;){step();CHECK(++n<2000000);if(!ime.hasWork()&&display())return;}}
 void serviceSearch(){for(unsigned n=0;n<200000;++n){step();if(n>100&&!app.find.hasWork()&&app.find.view().status!=hanime::find::Status::Editing){CHECK(display());return;}}CHECK(false);}
 void context(){repeat.queryAxis(app.findOpen(),!app.horizontalActive());repeat.context(app.focusToken(),!ime.active()&&app.imeTextTargetAvailable());}
 void event(s3hid::Event e){e.ime_frame=frame;e.ime_attempt=ime.attemptSerial();e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();context();if(product.layout(app,e)){repeat.clear();return;}const bool chinese=ime.chinese();if(!product.ime(app,&ime,e)){repeat.event(e);app.key(e,chinese);}else repeat.clear();context();}
 void tap(uint8_t usage,uint8_t mods=0){event({1,++now,usage,mods,true});event({1,++now,usage,mods,false});}
 void text(std::string_view s){for(char c:s)tap(c==' '?44:uint8_t(c-'a'+4));}
 void enter(bool vertical){CHECK(app.commitText("清风吹过山岗。今天写下新的故事。\n这是清风，也是清风。\naaaa Abc ABC abc\n输入内容只属于查询。",app.editStamp()));CHECK(app.session.manualSave(++now));app.service(++now,false);if(vertical)tap(61);tap(9,1);CHECK(app.findOpen()&&app.find.view().query.empty());CHECK(display());}
 void chinese(unsigned scheme){tap(57);settle();for(unsigned n=0;n<scheme;++n){tap(44,1);settle();}CHECK(ime.chinese());}
 void save(const fs::path& dir,const std::string& name){if(dir.empty())return;CHECK(display());fs::create_directories(dir);std::ofstream f(dir/(name+".pgm"),std::ios::binary);f<<"P5\n300 400\n255\n";f.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());}
};
// Document::revision also counts Find caret navigation; textRevision is the text fence.
struct BodyGuard {std::string text;uint32_t revision,text_revision;size_t undo,redo;bool dirty;
 explicit BodyGuard(const FindRig& r):text(r.app.document.text()),revision(r.app.document.revision()),text_revision(r.app.document.textRevision()),undo(r.app.editor.undoCount()),redo(r.app.editor.redoCount()),dirty(r.app.document.dirty()){}
 void check(const FindRig& r)const{CHECK(std::string_view(r.app.document.text())==std::string_view(text)&&r.app.document.textRevision()==text_revision&&r.app.editor.undoCount()==undo&&r.app.editor.redoCount()==redo&&r.app.document.dirty()==dirty);}
};
struct HintInk {int left=300,top=400,right=-1,bottom=-1;unsigned count=0;};
HintInk hintInk(const FindRig& r,int left,int top,int right,int bottom){
 HintInk ink;
 for(int y=top;y<bottom;++y)for(int x=left;x<right;++x)if(!r.pixels[y*300+x]){
  ink.left=std::min(ink.left,x);ink.top=std::min(ink.top,y);
  ink.right=std::max(ink.right,x);ink.bottom=std::max(ink.bottom,y);++ink.count;
 }
 return ink;
}
unsigned hintGlyphInk(std::string_view text){
 const s3app::ui16::Font face;unsigned count=0;
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at)){
  const auto glyph=face.glyph(s3app::display::scalar(text,at));CHECK(!glyph.missing);
  for(unsigned y=0;y<glyph.height;++y)for(unsigned x=0;x<glyph.width;++x)
   if(glyph.bits[y*glyph.stride+x/8]&(0x80>>(x%8))){CHECK(x<glyph.advance);++count;}
 }
 return count;
}
void hintLayout(const fs::path& out){
 for(bool vertical:{false,true}){
  FindRig r;r.enter(vertical);CHECK(r.app.commitQuery("清风",r.app.queryStamp()));r.serviceSearch();const BodyGuard body(r);
  r.save(out,vertical?"fix2-find-vertical-hints":"fix2-find-horizontal-hints");
  if(vertical){
   const auto previous=hintInk(r,16,100,32,330),next=hintInk(r,44,100,60,330);
   std::cout<<"J05_INK previous="<<previous.left<<','<<previous.top<<','<<previous.right<<','<<previous.bottom
            <<" next="<<next.left<<','<<next.top<<','<<next.right<<','<<next.bottom
            <<" counts="<<previous.count<<','<<next.count<<std::endl;
   CHECK(previous.count==hintGlyphInk("Shift+回车上一个")&&next.count==hintGlyphInk("回车下一个"));
   CHECK(previous.top==next.top);
   CHECK(previous.top>=100&&previous.bottom<330&&next.bottom<330);
   CHECK(previous.right<next.left&&next.right<96); // Query rail begins at x=96.
   CHECK(!hintInk(r,32,100,44,330).count); // Separate complete hint columns.
  }else{
   CHECK(hintInk(r,8,318,292,334).count==hintGlyphInk("回车 下一个"));
   CHECK(hintInk(r,8,342,292,358).count==hintGlyphInk("Shift+回车 上一个"));
  }
  body.check(r);
 }
 std::cout<<"PASS native 300x400 Find hint ink tops, complete glyph counts, disjoint columns/query rail, unchanged body\n";
}
void direct(bool vertical,const fs::path& out){
 FindRig r;r.enter(vertical);const BodyGuard body(r);const std::string tag=vertical?"vertical-":"horizontal-";r.save(out,tag+"empty");
 EditorSink q(r.app);CHECK(q.bind(1,1)&&q.target().kind==TextTarget::Find);CHECK(q.commit("清风"));r.serviceSearch();CHECK(r.app.find.view().total_matches==3&&r.app.find.view().current_index>0);r.save(out,tag+"match");
 const auto first=r.app.find.view().current_index;r.tap(40);r.serviceSearch();CHECK(r.app.find.view().current_index==first%3+1);r.tap(40,2);r.serviceSearch();CHECK(r.app.find.view().current_index==first);
 const auto version=r.app.find.view().query_version;r.tap(vertical?82:80);CHECK(r.app.find.view().query_cursor==3&&r.app.find.view().query_version==version);CHECK(r.app.commitQuery("😀",r.app.queryStamp()));CHECK(r.app.find.view().query=="清😀风");r.tap(42);CHECK(r.app.find.view().query=="清风");
 EditorSink stale(r.app);CHECK(stale.bind(2,2));r.tap(vertical?82:80);r.tap(vertical?81:79);CHECK(!stale.commit("漏"));
 r.tap(4,1);CHECK(r.app.find.view().query_selected);CHECK(r.app.commitQuery("星河 abc123",r.app.queryStamp()));r.serviceSearch();CHECK(r.app.find.view().status==hanime::find::Status::NotFound);r.save(out,tag+"not-found");
 r.tap(4,8);r.tap(76);CHECK(r.app.find.view().query.empty());std::string cap;for(unsigned n=0;n<85;++n)cap+="山";cap+='a';CHECK(r.app.commitQuery(cap,r.app.queryStamp()));const auto stamp=r.app.queryStamp();CHECK(!r.app.commitQuery("超",stamp)&&r.app.find.view().query==cap);r.save(out,tag+"limit");
 r.tap(4,1);r.tap(42);CHECK(r.app.find.view().query.empty());r.text("abcdefghijklmnopqrstuvwx");r.tap(74);r.context();const uint8_t direction=vertical?81:79;r.event({1,++r.now,direction,0,true});size_t emitted=0;for(unsigned held=1;held<=2400;++held){++r.now;s3hid::Event repeat;if(r.repeat.poll(r.now,1,repeat)){const auto before=r.app.queryStamp();r.app.key(repeat);const auto after=r.app.queryStamp();r.repeat.actionResult(before.cursor!=after.cursor);++emitted;}}
 CHECK(emitted>15&&r.app.find.view().query_cursor==24);r.event({1,++r.now,direction,0,false});s3hid::Event none;CHECK(!r.repeat.poll(r.now+500,1,none));
 r.tap(74);r.event({1,++r.now,direction,0,true});r.tap(61);CHECK(!r.repeat.poll(r.now+500,1,none));r.tap(61);
 EditorSink old(r.app);CHECK(old.bind(3,3));r.tap(41);CHECK(!r.app.findOpen()&&!old.commit("漏"));r.tap(9,1);CHECK(r.app.findOpen()&&r.app.find.view().query.empty());body.check(r);
 std::cout<<"PASS Find direct/edit/selection/count/wrap/UTF8/capacity/repeat/stale/body isolation layout="<<vertical<<" repeats="<<emitted<<'\n';
}
void shiftSelection(bool vertical){
 FindRig r;r.enter(vertical);const BodyGuard body(r);const uint8_t back=vertical?82:80,forward=vertical?81:79;
 CHECK(r.app.commitQuery("A清风Z",r.app.queryStamp()));r.serviceSearch();const auto version=r.app.find.view().query_version;
 r.tap(back);r.tap(back,2);r.tap(back,32); // Both physical Shift modifier bits.
 auto q=r.app.find.view();CHECK(q.selectionBegin()==1&&q.selectionEnd()==7&&q.query_version==version);
 if(const char* dir=std::getenv("R2_UI_PREVIEW_DIR"))r.save(dir,vertical?"fix1-find-vertical-selection":"fix1-find-horizontal-selection");
 EditorSink stale(r.app);CHECK(stale.bind(1,1));r.tap(forward,2);r.tap(back,2);CHECK(!stale.commit("漏"));
 r.chinese(0);r.text("ui");r.settle();CHECK(r.ime.candidateAvailable());r.tap(44);r.settle();if(r.ime.active()){r.tap(40);r.settle();}
 CHECK(r.app.find.view().query=="A是Z");r.tap(57);CHECK(!r.ime.chinese());
 r.tap(back,2);r.tap(76);CHECK(r.app.find.view().query=="AZ");
 r.tap(74);r.tap(77,2);r.tap(42);CHECK(r.app.find.view().query.empty());
 r.text("abcdefghijklmnopqrstuvwx");r.tap(74);r.context();r.event({1,++r.now,forward,2,true});
 size_t emitted=0;for(unsigned held=1;held<=2400;++held){++r.now;s3hid::Event repeat;if(r.repeat.poll(r.now,1,repeat)){const auto before=r.app.queryStamp();r.app.key(repeat);const auto after=r.app.queryStamp();r.repeat.actionResult(before.cursor!=after.cursor);++emitted;}}
 CHECK(emitted>15&&r.app.find.view().selectionBegin()==0&&r.app.find.view().selectionEnd()==24);
 r.event({1,++r.now,forward,2,false});s3hid::Event none;CHECK(!r.repeat.poll(r.now+500,1,none));
 r.tap(4,1);CHECK(r.app.commitQuery("清风",r.app.queryStamp()));r.serviceSearch();const auto index=r.app.find.view().current_index;
 r.tap(40);r.serviceSearch();r.tap(40,2);r.serviceSearch();CHECK(r.app.find.view().current_index==index);
 if(vertical)if(const char* dir=std::getenv("R2_UI_PREVIEW_DIR"))r.save(dir,"fix1-find-vertical-previous-hint");
 body.check(r);std::cout<<"PASS Shift selection actual H/V route, held repeat, partial Chinese commit, deletion, reverse search layout="<<vertical<<" repeats="<<emitted<<'\n';
}
void chinese(bool vertical,unsigned scheme,uint8_t confirm,const fs::path& out){
 FindRig r;r.enter(vertical);const BodyGuard body(r);r.chinese(scheme);CHECK(r.app.commitQuery("abc ",r.app.queryStamp()));r.tap(4,1);CHECK(r.app.find.view().query_selected);
 r.text(scheme==0?"ui":scheme==1?"shi":"jg");r.settle();CHECK(r.ime.candidateAvailable()&&r.frame&&r.ime.window()->findProfile());
 const auto& view=*r.ime.view();const auto& word=view.rows[view.row-view.rows[0].index].items[view.col];const std::string expected(word.text.data(),word.bytes);CHECK(!expected.empty());
 const std::string tag=(vertical?"vertical-":"horizontal-")+std::to_string(scheme);
 if(confirm==44){r.save(out,tag+"candidates");r.tap(vertical?80:81);r.settle();r.save(out,tag+"expanded");r.tap(vertical?79:82);r.settle();}
 r.tap(confirm);r.settle();if(r.ime.active()){r.tap(40);r.settle();}CHECK(!r.ime.active()&&r.app.findOpen()&&r.app.find.view().query==expected);CHECK(!r.ime.acceptedDocumentCurrent());r.serviceSearch();body.check(r);
 r.text(scheme==0?"ui":scheme==1?"shi":"jg");r.settle();const auto query=std::string(r.app.find.view().query);r.tap(41);r.settle();CHECK(r.app.findOpen()&&!r.ime.active()&&r.app.find.view().query==query);r.tap(41);CHECK(!r.app.findOpen());body.check(r);
 std::cout<<"PASS real-resource IME -> Find only/selection/whole Esc layout="<<vertical<<" scheme="<<scheme<<" confirm="<<unsigned(confirm)<<'\n';
}

void lifecycle(bool vertical,const fs::path& out){
 FindRig r;r.enter(vertical);const BodyGuard body(r);const std::string tag=vertical?"vertical-":"horizontal-";
 r.text("abc");CHECK(r.app.find.view().query=="abc");for(unsigned i=0;i<50;++i)r.step();CHECK(r.app.find.view().status==hanime::find::Status::Editing);
 r.text("d");for(unsigned i=0;i<50;++i)r.step();CHECK(r.app.find.view().status==hanime::find::Status::Editing);r.serviceSearch();CHECK(r.app.find.view().status==hanime::find::Status::NotFound);
 r.tap(4,1);r.tap(42);CHECK(r.app.commitQuery(std::string(255,'a'),r.app.queryStamp()));r.chinese(0);r.text("ui");r.settle();const auto q=r.app.find.view().query_version;r.tap(44);r.settle();CHECK(r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect&&r.ime.active());CHECK(r.app.find.view().query.size()==255&&r.app.find.view().query_version==q&&r.app.find.view().capacity_exceeded);r.save(out,tag+"ime-capacity");r.tap(41);r.settle();CHECK(r.app.findOpen()&&!r.ime.active());
 r.tap(4,1);r.tap(42);CHECK(r.app.commitQuery("ac",r.app.queryStamp()));r.tap(vertical?82:80);r.text("ui");r.settle();
 const auto oldFrame=r.app.presentationFrame();const auto oldPaint=r.painted;const auto oldGeneration=r.frame;const auto oldContext=r.ime.ingressContext();
 r.tap(61);CHECK(!ime_device::publishActivePresentation(r.app,&r.ime,oldFrame,{},oldPaint).accepted);r.settle();CHECK(r.app.find.view().query=="ac");r.tap(61);r.settle();
 r.tap(44);r.settle();CHECK(r.app.find.view().query=="a是c");r.serviceSearch();body.check(r);
 r.text("ui");r.settle();r.tap(vertical?82:80);r.settle();CHECK(r.ime.rawEditing());r.save(out,tag+"raw-edit");r.tap(41);r.settle();CHECK(!r.ime.active()&&r.app.findOpen());
 r.text("ui");r.settle();r.tap(42);r.settle();const auto before=std::string(r.app.find.view().query);
 s3hid::Event stale{1,++r.now,30,0,true,0,oldGeneration,r.ime.attemptSerial()};stale.ime_context=oldContext;stale.resource_owner=r.ime.resourceOwner();CHECK(r.product.ime(r.app,&r.ime,stale));stale.down=false;stale.ms=++r.now;r.product.ime(r.app,&r.ime,stale);r.settle();CHECK(r.app.find.view().query==before);
 r.tap(9,1);r.settle();CHECK(r.app.findOpen()&&!r.ime.active());r.tap(9,1);CHECK(!r.app.findOpen());body.check(r);
 // Current document text changes are an explicit external-model path; old IME
 // authority expires and the query is retained/recomputed for the new revision.
 r.tap(9,1);CHECK(r.app.commitQuery("清风",r.app.queryStamp()));r.serviceSearch();CHECK(r.app.find.view().total_matches==3);EditorSink old(r.app);CHECK(old.bind(1,1));
 CHECK(s3app::AppTestAccess::document(r.app).insertUtf8("清风")==editor::EditResult::Changed);
 r.app.session.onBodyMutation(++r.now);r.app.service(++r.now,false);CHECK(!old.commit("漏")&&r.app.find.view().query=="清风"&&!r.app.find.view().match.valid);r.serviceSearch();CHECK(r.app.find.view().total_matches==4);
 const auto original=r.app.session.currentId();const auto created=r.app.session.create("other",++r.now);if(!created)std::cerr<<"external_create status="<<int(created.status)<<" stage="<<created.stage<<" bg="<<r.app.persistence.backgroundSaveActive()<<"\n";CHECK(created);CHECK(!r.app.findOpen());CHECK(r.app.session.open(original,++r.now));r.tap(9,1);CHECK(r.app.find.view().query.empty());
 std::cout<<"PASS Find coalescing/overflow retry/raw Escape/F4/stale LCD/external revision/document switch layout="<<vertical<<'\n';
}
void fullWords(const fs::path& out){
 for(bool vertical:{false,true})for(unsigned kind=0;kind<3;++kind){FindRig r;r.enter(vertical);std::string word;
  if(kind==0)for(unsigned n=0;n<21;++n)word+="汉";
  else word=std::string(64,kind==1?'a':'W');
  CHECK(s3app::find_ui::wholeCandidate(word,vertical,{FindRig::dot,&r}));
  CHECK(!s3app::find_ui::wholeCandidate(word+"xx",vertical,{FindRig::dot,&r}));
  if(!out.empty()){const auto name=(vertical?"vertical-full-":"horizontal-full-")+std::to_string(kind);std::ofstream f(out/(name+".pgm"),std::ios::binary);f<<"P5\n300 400\n255\n";f.write(reinterpret_cast<const char*>(r.pixels.data()),r.pixels.size());}
 }
 const s3app::ui16::Font font;for(uint32_t cp=32;cp<127;++cp)CHECK(font.glyph(cp).advance<=14&&!font.glyph(cp).missing);
 CHECK(font.glyph(0x1f600).missing);std::cout<<"PASS complete 21 Han / 64 Latin / 64 wide W whole-candidate rendering both layouts; oversized input refused\n";
}
#ifndef DRAFTOR_FIND_TEST_FIXTURE_ONLY
int main(int argc,char** argv){try{
 const fs::path out=argc>1?fs::absolute(argv[1]):fs::path{};
 if(argc>2&&std::string_view(argv[2])=="--hints"){hintLayout(out);return 0;}
 if(argc>2){for(bool vertical:{false,true})lifecycle(vertical,out);fullWords(out);return 0;}
 hintLayout(out);
 for(bool vertical:{false,true}){shiftSelection(vertical);direct(vertical,out);for(unsigned scheme=0;scheme<3;++scheme)for(uint8_t confirm:{uint8_t(30),uint8_t(44),uint8_t(40)})chinese(vertical,scheme,confirm,out);}
 for(bool vertical:{false,true})lifecycle(vertical,out);fullWords(out);
 std::cout<<"MEM App="<<sizeof(s3app::App)<<" Target="<<sizeof(Target)<<" CandidateWindow="<<sizeof(CandidateWindow)<<" FindReceipt="<<sizeof(s3app::find_ui::Receipt)<<" UI16 Font="<<sizeof(s3app::ui16::Font)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
