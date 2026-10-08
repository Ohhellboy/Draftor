// Real P23 resources, App, DeviceIme and CandidateWindow. No replacement IME.
#define main legacy_query_rows_main
#include "query_rows.cpp"
#undef main
#include "vertical_view.hpp"
#include "support/app_test_access.hpp"
#include "utf8.hpp"
#include <optional>
#include <vector>
#include <type_traits>

namespace v=draftor::vertical;
namespace be=editor::body_edit;
using Affinity=hanime::renderer::CaretAffinity;
struct VerticalSlot {
  const s3host::FontPort& font;
  std::optional<s3app::VerticalPresentation> value;
  static editor::presentation::PresentationPort& construct(void* opaque,editor::SharedEditorView shared){
    auto& self=*static_cast<VerticalSlot*>(opaque);return self.value.emplace(shared,self.font);
  }
};
struct VRig {
  fs::path path; s3fs::Fs files;hanime::documents::ArduinoSdStoragePort storage;
  ChineseFont font;VerticalSlot slot;s3app::App app;Resources resources;DeviceIme ime;
  uint32_t now=0,frame=0;
  VRig():path(Rig::temp()),files(path.string()),storage(files),slot{font,{}},
      app(storage,{},7511,font,{VerticalSlot::construct,&slot}),
      resources(RESOURCE_ROOT,std::make_shared<Mount>(),productionPins(),INDEX_ROOT),ime(app,resources,font){
    CHECK(font.begin());storage.noteMounted();CHECK(app.startup(++now));
  }
  ~VRig(){std::error_code ec;fs::remove_all(path,ec);}
  s3app::VerticalPresentation& vertical(){return *slot.value;}
  void key(uint8_t usage,uint8_t mods=0){app.key({1,++now,usage,mods,true});}
  void tap(uint8_t usage,uint8_t mods=0){for(bool down:{true,false}){
    s3hid::Event e{1,++now,usage,mods,down,0,frame,ime.attemptSerial()};e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();
    if(!ime.event(e))app.key(e);
  }}
  void type(std::string_view text){for(char ch:text)tap(uint8_t(ch-'a'+4));}
  void settleIme(){unsigned n=0;do{ime.service(false);app.service(++now,false);CHECK(++n<2000000);}
    while(!resources.ready()||!ime.composition()||ime.hasWork());CHECK(resources.state()!=ResourceState::Error);CHECK(prepareImePresentation(&ime,app));}
  void begin(std::string_view raw){tap(0x39);settleIme();type(raw);settleIme();}
  VerticalDispatch number(const VerticalChoiceTarget& target){
    VerticalDispatch result=VerticalDispatch::RejectedTarget;
    for(bool down:{true,false}){s3hid::Event e{1,++now,uint8_t(29+target.number),0,down,0,target.frame.generation,ime.attemptSerial()};
      e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();const auto sent=dispatchVerticalNumber(ime,app,target,e);if(down)result=sent;}
    return result;
  }
  void body(std::string_view text){CHECK(app.commitText(text,app.editStamp()));ready();}
  void move(size_t byte,bool extend=false){auto out=app.moveToByte(byte,extend);CHECK(out.code==be::ResultCode::Applied||out.code==be::ResultCode::NoAction);ready();}
  void ready(){
    // Navigation now completes through the same cooperative App service as S3.
    unsigned turns=0;while(app.bodyNavigationPending()){app.service(++now,true);CHECK(++turns<10000);}
    CHECK(vertical().settle()==v::Status::Ready);CHECK(vertical().caret().status==v::Status::Ready);
  }
  void geometry(uint16_t height=56){CHECK(vertical().setRegion({4,6,164,height})==v::Status::Ready);}
};
struct CapturedRun {std::string text;v::Rect box;v::AbRole role;unsigned number;bool focused;const void* identity;size_t raw_offset;};
struct Capture {
  std::vector<CapturedRun> runs;std::vector<v::Rect> lines;
  std::string_view raw;
  v::AbPort port(){return {
    [](void* p,const v::AbRun& run){auto& c=*static_cast<Capture*>(p);size_t offset=0;
      if(run.role==v::AbRole::Composition){CHECK(run.text.data()>=c.raw.data());offset=size_t(run.text.data()-c.raw.data());CHECK(offset+run.text.size()<=c.raw.size());}
      c.runs.push_back({std::string(run.text),run.box,run.role,run.number,run.focused,run.identity,offset});
    },[](void* p,v::Rect r,bool){static_cast<Capture*>(p)->lines.push_back(r);},this};}
};
VerticalImeFrame ab(VRig& r,Capture& capture){
  r.ready();capture.raw=r.ime.raw();
  const auto before=metrics.io();const auto query=r.ime.window()?r.ime.window()->querySerial():0;
  auto result=verticalIme(&r.ime,r.app,r.now,r.vertical().caret().caret.column,capture.port());
  const auto io=metrics.io()-before;CHECK(io.reads==0&&io.bytes==0&&io.seeks==0);
  CHECK(!r.ime.window()||r.ime.window()->querySerial()==query);
  CHECK(result.layout.status==v::AbStatus::Ready);
  return result;
}
void dump(VRig& r,const char* label){
  r.ready();const auto& view=r.vertical().viewport();
  for(size_t i=0;i<view.eventCount();++i){const auto e=view.eventAt(i);const auto box=view.glyphRect(e);
    std::cout<<"GEOMETRY "<<label<<" byte="<<e.begin_byte<<" end="<<e.end_byte<<" cp="<<e.codepoint
      <<" column="<<e.column<<" y="<<e.y_px<<" x="<<box.x<<" screen_y="<<box.y<<" kind="<<int(e.kind)<<'\n';}
}
std::vector<v::Event> glyphs(VRig& r){r.ready();std::vector<v::Event> out;
  for(size_t i=0;i<r.vertical().viewport().eventCount();++i){auto e=r.vertical().viewport().eventAt(i);if(e.glyph())out.push_back(e);}return out;}
void V1(){
  VRig r;r.geometry();r.body("天地玄黄");const auto before=r.app.document.textRevision();
  const auto e=glyphs(r);CHECK(e.size()==4);
  CHECK(e[0].begin_byte==0&&e[0].column==0&&e[0].y_px==0);
  CHECK(e[1].begin_byte==3&&e[1].column==0&&e[1].y_px==28);
  CHECK(e[2].begin_byte==6&&e[2].column==1&&e[2].y_px==0);
  CHECK(r.vertical().viewport().glyphRect(e[0]).x==140&&r.vertical().viewport().glyphRect(e[2]).x==106);
  CHECK(r.app.document.text()=="天地玄黄"&&r.app.document.textRevision()==before);dump(r,"R1_columns");
  r.key(29,1);r.body("中\n\n𨿍\n");const auto wide=glyphs(r);CHECK(wide[1].end_byte-wide[1].begin_byte==4);dump(r,"R1_newlines_utf8");
}
void V2(){
  VRig r;r.geometry();r.body("天地玄黄");const auto rev=r.app.document.textRevision();const auto history=r.app.editor.undoCount();
  for(int i=0;i<3;++i)r.ready();CHECK(r.app.document.textRevision()==rev&&r.app.editor.undoCount()==history);
  r.move(6);r.key(40);r.ready();CHECK(r.app.document.text()=="天地\n玄黄");
  CHECK(r.app.document.textRevision()==rev+1&&r.app.editor.undoCount()==history+1);
  CHECK(r.vertical().caret().caret.column==1&&r.vertical().caret().caret.y_px==0);
  r.key(29,1);CHECK(r.app.document.text()=="天地玄黄");r.key(28,1);CHECK(r.app.document.text()=="天地\n玄黄");dump(r,"Enter");
}
void V3(){
  VRig r;r.geometry(112);r.body("天地玄黄\n中\n甲乙丙丁");r.move(9);
  r.key(80);CHECK(r.app.document.cursorByte()==16&&r.vertical().caret().caret.y_px==28);
  r.key(80);CHECK(r.app.document.cursorByte()==26&&r.vertical().caret().caret.y_px==84);
  r.key(79);CHECK(r.app.document.cursorByte()==16);r.key(79);CHECK(r.app.document.cursorByte()==9);
  r.move(0);r.key(82);CHECK(r.app.document.cursorByte()==0);r.key(81);CHECK(r.app.document.cursorByte()==3);
  r.geometry();r.move(3);r.key(81);CHECK(r.app.document.cursorByte()==6&&r.app.editor.caretAffinity()==Affinity::Upstream);
  r.key(81);CHECK(r.app.document.cursorByte()==6&&r.app.editor.caretAffinity()==Affinity::Downstream);
  r.key(82);CHECK(r.app.document.cursorByte()==6&&r.app.editor.caretAffinity()==Affinity::Upstream);
  r.key(82);CHECK(r.app.document.cursorByte()==3);
  CHECK(r.app.moveToByte(2).code==be::ResultCode::InvalidPosition);dump(r,"cursor");
  std::string large;for(unsigned i=0;i<100;++i)large+="天地玄黄\n";
  r.key(4,1);r.body(large);CHECK(r.vertical().viewport().firstColumn()>0);
  for(unsigned i=0;i<20;++i){const auto before=r.app.document.cursorByte();r.key(79);r.ready();CHECK(r.app.document.cursorByte()<before);CHECK(r.vertical().viewport().visible(r.vertical().caret().caret.column));}
}
void V4(){
  VRig r;r.geometry();r.body("天地玄\n黄宇");r.move(3);r.move(10,true);
  v::Rect rects[16];auto selected=r.vertical().selectionRectangles(rects,16);
  CHECK(selected.status==v::Status::Ready&&selected.written==3);
  CHECK(rects[0].x==140&&rects[0].y==34);CHECK(rects[1].x==106&&rects[2].x==106);
  for(size_t i=0;i<selected.written;++i)std::cout<<"SELECTION x="<<rects[i].x<<" y="<<rects[i].y<<" h="<<rects[i].height<<'\n';
  r.move(10);r.move(3,true);CHECK(r.app.editor.selection().anchor==10&&r.app.editor.selection().active==3);
  r.key(81);CHECK(!r.app.editor.selection().has_selection&&r.app.document.cursorByte()==10);
  r.move(0);r.key(81,2);CHECK(r.app.editor.selection().begin==0&&r.app.editor.selection().end==3);
  r.key(82);CHECK(!r.app.editor.selection().has_selection&&r.app.document.cursorByte()==0);
  r.move(0);r.move(3,true);CHECK(r.vertical().selectionRectangles(rects,16).written==1);
  CHECK(r.app.editor.undoCount()==1);
}
void V5(){
  VRig r;r.geometry();std::string body;for(unsigned i=0;i<70;++i)body+="天地玄黄\n";
  const auto hit=body.size();body+="命中测试";r.body(body);r.move(0);r.move(3,true);
  const auto history=r.app.editor.undoCount();const auto* allocation=r.app.editor.historyAllocation();
  r.key(9,1);CHECK(r.app.findOpen());CHECK(r.app.find.setQuery("命中")==hanime::find::QueryEditResult::Applied);
  r.key(40);for(unsigned i=0;r.app.find.view().status==hanime::find::Status::Searching;++i){CHECK(i<10000);r.app.service(++r.now,false);}
  CHECK(r.app.find.view().match.begin==hit&&r.app.document.cursorByte()==hit);r.ready();
  CHECK(r.vertical().viewport().visible(r.vertical().caret().caret.column)&&r.vertical().viewport().firstColumn()>0);
  CHECK(!r.app.editor.selection().has_selection&&r.app.editor.undoCount()==history);
  r.key(41);CHECK(r.app.document.cursorByte()==hit+6&&r.app.editor.historyAllocation()==allocation);
  CHECK(r.app.findDocument.placeCaretAtByte(6));r.ready();CHECK(r.vertical().caret().caret.column==1&&r.app.editor.caretAffinity()==Affinity::Downstream);
  r.key(82);CHECK(r.app.document.cursorByte()==6&&r.app.editor.caretAffinity()==Affinity::Upstream);
  CHECK(r.app.findDocument.placeCaretAtByte(6));CHECK(r.vertical().caret().caret.column==1);
  CHECK(r.app.document.text()==body.c_str());
}
void V6(){
  VRig r;r.body("A中文B");const auto* history=r.app.editor.historyAllocation();
  r.move(1);r.move(7,true);r.body("X");CHECK(r.app.document.text()=="AXB");
  r.key(40);CHECK(r.app.document.text()=="AX\nB");r.key(42);CHECK(r.app.document.text()=="AXB");
  r.key(76);CHECK(r.app.document.text()=="AX");const auto n=r.app.editor.undoCount();CHECK(n==5);
  for(size_t i=0;i<n;++i)r.key(29,1);CHECK(r.app.document.text().empty());
  for(size_t i=0;i<n;++i)r.key(28,1);CHECK(r.app.document.text()=="AX"&&r.app.editor.historyAllocation()==history);
  // Persistence uses the same shared editor port and a Vertical byte anchor.
  r.key(22,1);for(unsigned i=0;r.app.persistence.backgroundSaveActive();++i){CHECK(i<20000);r.app.service(++r.now,false);}
  CHECK(r.app.lastOperation);const auto id=r.app.session.currentId();
  CHECK(r.app.session.create("other",++r.now));CHECK(r.app.session.open(id,++r.now));
  CHECK(r.app.document.text()=="AX"&&r.app.document.cursorByte()==2);r.ready();
  std::string longbody;for(unsigned i=0;i<60;++i)longbody+="天地玄黄\n";
  r.key(4,1);r.body(longbody);r.geometry();r.move(longbody.size()-10);
  editor::presentation::ViewportState saved,restored;
  CHECK(r.vertical().captureViewport(saved)&&saved.byte_offset>0);const auto cursor=r.app.document.cursorByte();
  r.key(22,1);for(unsigned i=0;r.app.persistence.backgroundSaveActive();++i){CHECK(i<20000);r.app.service(++r.now,false);}
  CHECK(r.app.lastOperation);CHECK(r.app.session.create("again",++r.now));CHECK(r.app.session.open(id,++r.now));
  CHECK(r.vertical().captureViewport(restored));CHECK(saved.byte_offset==restored.byte_offset&&saved.affinity==restored.affinity);
  CHECK(r.app.document.text()==longbody.c_str()&&r.app.document.cursorByte()==cursor);
}
void V7(){
  VRig r;std::string raw;for(unsigned i=0;i<32;++i)raw+="wo";r.begin(raw);
  Capture c;const auto frame=ab(r,c);CHECK(frame.candidate_frame&&!c.runs.empty());
  size_t first=raw.size(),last=0;for(const auto& run:c.runs)if(run.role==v::AbRole::Composition){
    first=std::min(first,run.raw_offset);last=std::max(last,run.raw_offset+run.text.size());
    CHECK(run.text.size()<=6&&run.text==raw.substr(run.raw_offset,run.text.size()));
    CHECK(run.box.x==frame.layout.composition.x+s3app::ImeCompositionFontMetrics::padding);
  }
  // Candidate coverage keeps the target visible without an inactive raw caret.
  CHECK(first==0&&last<raw.size()&&!c.lines.empty());CHECK(r.ime.raw()==raw);
  for(const auto& line:c.lines)
    CHECK(!(line.x>=frame.layout.composition.x&&line.x<frame.layout.composition.x+frame.layout.composition.width&&line.width==1&&line.height==24));
  r.tap(42);r.settleIme();CHECK(r.ime.raw()==std::string_view(raw).substr(0,raw.size()-1));Capture after;ab(r,after);
  // The valid focused candidate range determines the covered syllable.
  VRig short_raw;short_raw.begin("wowo");Capture focused;ab(short_raw,focused);
  CHECK(focused.runs[0].role==v::AbRole::Composition&&focused.runs[0].focused);
  std::cout<<"A_VIEWPORT first_raw="<<first<<" last_raw="<<last<<" slots="<<c.runs.size()<<'\n';
}
void V8(){
  VRig r;r.begin("ui");r.ime.window()->request(0,0,true);r.settleIme();Capture c;auto frame=ab(r,c);
  unsigned numbers=0;for(const auto& run:c.runs)if(run.role==v::AbRole::CurrentCandidate){
    CHECK(run.number==++numbers&&run.number<=6&&run.identity);}
  CHECK(numbers>0);CHECK(publishVerticalFrame(&r.ime,r.app,frame)==frame.generation);r.frame=frame.generation;
  // Every available digit is represented; unavailable 1..6 slots have no target.
  for(unsigned number=numbers+1;number<=6;++number)CHECK(chooseVertical(*r.ime.window(),{v::AbRole::CurrentCandidate,number,frame})==ChoiceResult::NoCandidate);
  const auto item=r.ime.view()->rows[0].items[0];const std::string expected(item.text.data(),item.bytes);
  CHECK(r.number({v::AbRole::CurrentCandidate,1,frame})==VerticalDispatch::Delivered);r.settleIme();if(r.app.document.text().empty()){r.tap(40);r.settleIme();}
  CHECK(r.app.document.text()==expected.c_str()&&r.app.editor.undoCount()==1);
  r.key(29,1);CHECK(r.app.document.text().empty());r.key(28,1);CHECK(r.app.document.text()==expected.c_str());
  for(unsigned number=2;number<=numbers;++number){
    VRig numbered;numbered.begin("ui");numbered.ime.window()->request(number==2?1:0,0,true);numbered.settleIme();
    Capture shown;const auto receipt=ab(numbered,shown);CHECK(publishVerticalFrame(&numbered.ime,numbered.app,receipt));numbered.frame=receipt.generation;
    const auto& shown_view=*numbered.ime.view();const auto choice=shown_view.rows[shown_view.row-shown_view.rows[0].index].items[number-1];const std::string text(choice.text.data(),choice.bytes);
    CHECK(numbered.number({v::AbRole::CurrentCandidate,number,receipt})==VerticalDispatch::Delivered);numbered.settleIme();
    if(numbered.app.document.text().empty()){numbered.tap(40);numbered.settleIme();}
    CHECK(numbered.app.document.text()==text.c_str()&&numbered.app.editor.undoCount()==1);
  }
  std::cout<<"ACTIVE numbered="<<numbers<<" allowed_digits=1..6 shared_commit=1\n";
}
void V9(){
  VRig r;r.begin("ui");
  for(size_t row:{0u,1u,2u}){
    if(row==0){
      CHECK(verticalCandidateKey(*r.ime.window(),'=')==v::EdgeSide::StartRight);
      r.ime.window()->request(0,0,true);
    }else CHECK(verticalCandidateKey(*r.ime.window(),'-')==v::EdgeSide::None);
    r.settleIme();CHECK(r.ime.view()->row==row);Capture c;auto frame=ab(r,c);
    CHECK(frame.layout.candidate_content_rows==3);CHECK(publishVerticalFrame(&r.ime,r.app,frame)==frame.generation);
    unsigned active=0,preview=0;for(const auto& run:c.runs){
      if(run.role==v::AbRole::Preview){++preview;CHECK(run.number==0&&!run.identity&&!run.focused);
        CHECK(chooseVertical(*r.ime.window(),{run.role,1,frame})==ChoiceResult::NoCandidate);
        CHECK(r.number({run.role,1,frame})==VerticalDispatch::RejectedTarget);}
      if(run.role==v::AbRole::CurrentCandidate){++active;CHECK(run.number&&run.identity);}
      std::cout<<"CANDIDATE row="<<row<<" role="<<int(run.role)<<" number="<<run.number<<" x="<<run.box.x<<" y="<<run.box.y<<'\n';
    }
    CHECK(active&&preview);CHECK(frame.layout.candidates.x+frame.layout.candidates.width+2==frame.layout.composition.x);
    CHECK(frame.layout.composition.x+frame.layout.composition.width+2==frame.layout.selected.x&&frame.layout.selected.width==28&&frame.layout.selected.x+frame.layout.selected.width+2==frame.layout.body.x);
  }
  size_t last=2;
  for(;last<300;++last){r.ime.window()->request(last,0,true);r.settleIme();const auto& view=*r.ime.view();
    if(view.at_end&&view.row==view.rows[view.count-1].index)break;}
  CHECK(last<300);Capture end;auto tail=ab(r,end);CHECK(tail.layout.candidate_content_rows<=2);
  CHECK(verticalCandidateKey(*r.ime.window(),'-')==v::EdgeSide::EndLeft);
  CHECK(verticalCandidateKey(*r.ime.window(),'=')==v::EdgeSide::None);r.settleIme();CHECK(r.ime.view()->row==last-1);
  std::cout<<"CANDIDATE_TAIL local_row="<<last<<" columns="<<tail.layout.candidate_content_rows<<'\n';
}
void V10(){
  VRig r;r.begin("ui");r.ime.window()->request(0,0,true);r.settleIme();Capture c;const auto old=ab(r,c);
  CHECK(publishVerticalFrame(&r.ime,r.app,old)==old.generation);
  r.ime.window()->request(1,0,true);r.settleIme();CHECK(r.ime.view()->generation!=old.generation);
  CHECK(chooseVertical(*r.ime.window(),{v::AbRole::CurrentCandidate,1,old})==ChoiceResult::ExpiredFrame);
  CHECK(r.number({v::AbRole::CurrentCandidate,1,old})==VerticalDispatch::ExpiredFrame);
  CHECK(publishVerticalFrame(&r.ime,r.app,old)==0);
  Capture fresh;const auto current=ab(r,fresh);CHECK(publishVerticalFrame(&r.ime,r.app,current));
  r.type("wo");r.settleIme();CHECK(publishVerticalFrame(&r.ime,r.app,current)==0);
  CHECK(chooseVertical(*r.ime.window(),{v::AbRole::CurrentCandidate,1,current})!=ChoiceResult::Captured);
  CHECK(r.app.document.text().empty()&&r.app.editor.undoCount()==0);
}
void V11(){
  VRig r;r.geometry(112);const std::string text="中文 hello world 中文";r.body(text);const auto e=glyphs(r);
  unsigned rotated=0,spaces=0;for(const auto& glyph:e){
    if(glyph.codepoint==' ')++spaces;
    if(glyph.codepoint>='a'&&glyph.codepoint<='z'){CHECK(glyph.kind==v::EventKind::RotateCW90);++rotated;}
    if(glyph.codepoint>127)CHECK(glyph.kind==v::EventKind::Upright);
  }
  CHECK(rotated==10&&spaces==3&&r.app.document.text()==text.c_str());dump(r,"mixed");
  r.key(4,1);r.body("中ChatGPT文");auto word=glyphs(r);CHECK(word[1].codepoint=='C'&&word[1].column==1&&word[1].y_px==0);
  r.key(4,1);r.geometry();r.body("中ABCDEFGHIJ");auto longword=glyphs(r);
  CHECK(longword[1].column==0&&longword[1].y_px==28);
  for(size_t i=1;i<longword.size();++i){CHECK(longword[i].kind==v::EventKind::RotateCW90);
    CHECK(longword[i].advance_px==r.font.glyph(longword[i].codepoint).advance);
    CHECK(longword[i].y_px+longword[i].advance_px<=56);}
  // Original R1 fixture uses fixed 14px Latin, unlike P23's proportional font.
  v::Config fixed;fixed.body={4,6,164,56};const v::ByteRange span{3,13};
  const v::Input original{"中ABCDEFGHIJ",&span,1};v::Flow flow;
  CHECK(flow.start(original,fixed)==v::Status::MoreWork);std::vector<v::Event> fixture;
  for(;;){v::Event event;const auto step=flow.step(original,&event,1,3);
    if(step.written)fixture.push_back(event);if(step.status==v::Status::DocumentEnd)break;
    CHECK(step.status==v::Status::MoreWork||step.status==v::Status::OutputFull);}
  CHECK(fixture[1].column==0&&fixture[1].y_px==28&&fixture[2].y_px==42);
  CHECK(fixture[3].kind==v::EventKind::SoftBreak&&fixture[4].column==1&&fixture[4].codepoint=='C');
  CHECK(r.app.document.text()=="中ABCDEFGHIJ");dump(r,"long_Latin");
}
struct Pixels {std::vector<uint8_t> bytes=std::vector<uint8_t>(300*400);size_t ink=0;
  s3app::VerticalPaintPort port(){return {
    [](void* p,v::Rect r,s3app::VerticalColor color){auto& c=*static_cast<Pixels*>(p);for(unsigned y=r.y;y<unsigned(r.y)+r.height;++y)for(unsigned x=r.x;x<unsigned(r.x)+r.width;++x){CHECK(x<300&&y<400);c.bytes[y*300+x]=uint8_t(color);}},
    [](void* p,uint16_t x,uint16_t y,s3app::VerticalColor color){auto& c=*static_cast<Pixels*>(p);CHECK(x<300&&y<400);c.bytes[y*300+x]=uint8_t(color);++c.ink;},this,nullptr};}
};
void V12(){
  VRig r;const std::string text="，。、！？：「」『』（）【】《》……—";r.body(text);
  CHECK(v::punctuationForm(0xff0c,v::PunctuationPolicy::CommonVertical).native==0xfe10);
  CHECK(v::punctuationForm(0x3001,v::PunctuationPolicy::CommonVertical).native==0xfe11);
  CHECK(v::punctuationForm(0x300c,v::PunctuationPolicy::CommonVertical).rotate);
  CHECK(v::punctuationForm(0x2014,v::PunctuationPolicy::CommonVertical).native==0xfe31);
  Pixels a,b;const auto history=r.app.editor.undoCount();const auto revision=r.app.document.textRevision();
  auto first=s3app::paintVerticalView(r.app,r.vertical(),&r.ime,r.font,r.now,a.port());
  auto second=s3app::paintVerticalView(r.app,r.vertical(),&r.ime,r.font,r.now,b.port());
  CHECK(first.status==v::Status::Ready&&second.status==v::Status::Ready&&a.ink>0&&a.bytes==b.bytes);
  CHECK(r.app.document.text()==text.c_str()&&r.app.document.textRevision()==revision&&r.app.editor.undoCount()==history);
  s3app::VerticalPresentation recreated(r.app.sharedState(),r.font);CHECK(recreated.settle()==v::Status::Ready);
  CHECK(r.app.document.text()==text.c_str());dump(r,"punctuation");
}
void V13(){
  VRig r;const std::string text="2026 12.5 97% 二〇二六";r.body(text);Pixels pixels;
  const auto before=r.app.document.textRevision();const auto result=s3app::paintVerticalView(r.app,r.vertical(),&r.ime,r.font,r.now,pixels.port());
  CHECK(result.status==v::Status::Ready&&r.app.document.text()==text.c_str()&&r.app.document.textRevision()==before);
  dump(r,"digits_no_conversion");
}
void V14(){
  VRig r;const auto shared=r.app.sharedState();
  static_assert(std::is_const_v<std::remove_reference_t<decltype(shared.document)>>);
  CHECK(&r.vertical().shared().document==&shared.document&&&r.vertical().shared().editor==&shared.editor);
  CHECK(&s3app::AppTestAccess::activePresentation(r.app)==&r.vertical());r.body("同一正文");r.move(3);r.move(6,true);
  const auto* history=shared.editor.historyAllocation();const auto selection=shared.editor.selection();const auto cursor=shared.document.cursorByte();
  {s3app::VerticalPresentation recreated(shared,r.font);CHECK(recreated.settle()==v::Status::Ready);}
  CHECK(shared.document.cursorByte()==cursor&&shared.editor.selection().version==selection.version&&shared.editor.historyAllocation()==history);
  const auto h_anchor=r.app.navigator.firstVisibleAnchor();const auto h_affinity=r.app.navigator.caretAffinity();
  r.key(74);r.key(77);r.key(29,1);r.key(28,1);
  CHECK(r.app.navigator.firstVisibleAnchor().byte_offset==h_anchor.byte_offset&&r.app.navigator.caretAffinity()==h_affinity);
  r.move(0);r.begin("ui");auto* ime=&r.ime;auto* window=r.ime.window();auto* composition=r.ime.composition();
  r.ime.window()->request(1,0,true);r.settleIme();Pixels pixels;const auto query=window->querySerial();const auto rev=composition->snapshot().revision;
  draftor::memory::allocation_fail_after=0;
  auto painted=s3app::paintVerticalView(r.app,r.vertical(),ime,r.font,r.now,pixels.port());
  draftor::memory::allocation_fail_after=-1;
  CHECK(painted.status==v::Status::Ready&&painted.ime.candidate_frame&&pixels.ink);
  size_t a_red=0,b_red=0;const auto a=painted.ime.layout.composition,b=painted.ime.layout.candidates;
  for(unsigned y=34;y<340;++y)for(unsigned x=8;x<292;++x)if(pixels.bytes[y*300+x]==uint8_t(s3app::VerticalColor::Red)){
    if(x>=a.x&&x<unsigned(a.x)+a.width)++a_red;
    if(x>=b.x&&x<unsigned(b.x)+b.width){++b_red;CHECK(x>=unsigned(b.x)+30&&x<unsigned(b.x)+58);}
  }
  CHECK(a_red>0&&b_red>0);std::cout<<"FOCUS_PIXELS A_red="<<a_red<<" B_active_red="<<b_red<<" preview_red=0\n";
  CHECK(window==r.ime.window()&&composition==r.ime.composition()&&window->querySerial()==query&&composition->snapshot().revision==rev);
  CHECK(shared.editor.historyAllocation()==history&&shared.document.text()=="同一正文");
  CHECK(publishVerticalFrame(ime,r.app,painted.ime)==painted.ime.generation);
  const auto active=r.vertical().viewport().caretRect(r.vertical().caret().caret);
  // R2 keeps the full reserved body even before its first column is filled.
  CHECK(active.x>=painted.ime.layout.body.x&&active.x+active.width<=painted.ime.layout.body.x+painted.ime.layout.body.width);
  std::cout<<"SIZES App="<<sizeof(s3app::App)<<" Vertical="<<sizeof(s3app::VerticalPresentation)
    <<" Viewport="<<sizeof(v::Viewport)<<" IME="<<sizeof(DeviceIme)<<" Window="<<sizeof(CandidateWindow)
    <<" BorrowedAB="<<sizeof(v::AbCandidateView)<<" Frame="<<sizeof(VerticalImeFrame)<<'\n';
}
int main(int argc,char** argv){try{
  CHECK(argc==2);const std::string name=argv[1];
  void(*tests[])()={V1,V2,V3,V4,V5,V6,V7,V8,V9,V10,V11,V12,V13,V14};
  CHECK(name.size()>1&&name[0]=='V');const auto n=std::stoul(name.substr(1));CHECK(n>=1&&n<=14);
  tests[n-1]();std::cout<<"PHASE2B "<<name<<" OK\n";return 0;
}catch(const std::exception& e){std::cerr<<"PHASE2B FAIL "<<e.what()<<'\n';return 1;}}
