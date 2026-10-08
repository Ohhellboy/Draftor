#define PHASE2C_REUSE_RIG
#include "runtime_layout_2c.cpp"
#include "composition_oracle.hpp"
#include "horizontal_raw.hpp"
#include <sstream>

bool sameRect(v::Rect a,v::Rect b){return a.x==b.x&&a.y==b.y&&a.width==b.width&&a.height==b.height;}
bool contains(v::Rect outer,v::Rect inner){return inner.x>=outer.x&&inner.y>=outer.y&&
  unsigned(inner.x)+inner.width<=unsigned(outer.x)+outer.width&&unsigned(inner.y)+inner.height<=unsigned(outer.y)+outer.height;}
v::AbResult reservation(){v::AbGeometry g;g.area=v::editor_area;return v::reservedAbRegions(g);}
void checkReservation(SwitchRig& r){
  const auto expected=v::reservedAbRegions(r.app.imeGeometry());const auto& actual=r.vertical_frame.layout;
  CHECK(actual.status==v::AbStatus::Ready&&sameRect(actual.body,expected.body));
  CHECK(sameRect(actual.composition,expected.composition)&&sameRect(actual.candidates,expected.candidates));
  CHECK(sameRect(actual.separator,expected.separator)&&actual.candidate_content_rows<=v::reserved_ime_capacity);
  for(unsigned y=expected.separator.y;y<unsigned(expected.separator.y)+expected.separator.height;++y)
    CHECK(r.pixels[y*300+expected.separator.x]==std::uint8_t(s3app::VerticalColor::Black));
}
std::string bodyGeometry(SwitchRig& r){
  auto& p=r.app.verticalPresentation();CHECK(p.settle()==v::Status::Ready);
  const auto body=p.region();const auto caret=p.caret().caret;std::ostringstream s;
  s<<body.x<<','<<body.y<<','<<body.width<<','<<body.height<<'/'<<p.viewport().firstColumn()<<'/'
    <<caret.position.byte<<','<<int(caret.position.affinity)<<','<<caret.column<<','<<caret.y_px;
  for(size_t i=0;i<p.viewport().eventCount();++i){const auto e=p.viewport().eventAt(i);const auto b=p.viewport().glyphRect(e);
    s<<'/'<<e.begin_byte<<','<<e.end_byte<<','<<e.column<<','<<e.codepoint<<','<<e.y_px<<','<<e.advance_px<<','<<int(e.kind)
      <<','<<b.x<<','<<b.y<<','<<b.width<<','<<b.height;}
  std::array<v::Rect,v::Viewport::event_capacity> selected{};
  const auto selection=p.selectionRectangles(selected.data(),selected.size());CHECK(selection.status==v::Status::Ready);
  for(size_t i=0;i<selection.written;++i){const auto b=selected[i];s<<"/s"<<b.x<<','<<b.y<<','<<b.width<<','<<b.height;}
  editor::presentation::ViewportState anchor;CHECK(p.captureViewport(anchor));
  s<<"/anchor="<<anchor.byte_offset<<','<<int(anchor.affinity);return s.str();
}
void startVertical(SwitchRig& r,bool selected=false){
  r.body(longBody());r.move(840);if(selected)r.move(858,true);
  const BodyFact before(r.app);r.toggle();r.app.preparePresentation();
  CHECK(sameRect(r.app.verticalPresentation().region(),reservation().body));
  before.check(r.app);r.draw();checkReservation(r);
}
void unchanged(SwitchRig& r,const std::string& before){r.draw();checkReservation(r);CHECK(bodyGeometry(r)==before);}
void P1(){SwitchRig r;startVertical(r);r.tap(57);r.settle();const auto first=bodyGeometry(r);unchanged(r,first);
  CHECK(r.vertical_frame.layout.candidate_content_rows==0);
  const auto g=reservation();std::cout<<"FIXED body="<<g.body.x<<','<<g.body.width<<" A="<<g.composition.x<<','<<g.composition.width
    <<" B="<<g.candidates.x<<','<<g.candidates.width<<" separator="<<g.separator.x<<" reserved_ime_capacity="<<v::reserved_ime_capacity<<'\n';}
void P2(){SwitchRig r;startVertical(r,true);r.tap(57);r.settle();const auto before=bodyGeometry(r);const BodyFact body(r.app);
  r.type("ui");CHECK(!r.ime.raw().empty()&&r.ime.hasWork());unchanged(r,before);body.check(r.app);}
void P3(){SwitchRig r;startVertical(r,true);r.tap(57);r.settle();const auto before=bodyGeometry(r);r.type("ui");r.settle();unchanged(r,before);
  CHECK(r.vertical_frame.layout.candidate_content_rows==1&&r.ime.view()->count==1);}
void P4(){SwitchRig r;startVertical(r,true);r.tap(57);r.settle();const auto before=bodyGeometry(r);r.type("ui");r.settle();
  for(unsigned i=0;i<3;++i){r.ime.window()->request(1,0,true);r.settle();CHECK(r.vertical_frame.layout.candidate_content_rows==3);checkReservation(r);
    // R3-R2: same-target editing/requery retains this candidate stage.
    r.tap(42);r.type("i");r.settle();CHECK(r.vertical_frame.layout.candidate_content_rows==3);checkReservation(r);}
  r.ime.interrupt();r.settle();CHECK(r.ime.raw().empty());checkReservation(r);}
void commitCycle(SwitchRig& r,SwitchRig& control,unsigned index){
  const BodyFact logical(r.app);r.type("ui");r.draw();const auto before=bodyGeometry(r);r.settle();unchanged(r,before);logical.check(r.app);
  CHECK(r.vertical_frame.layout.candidate_content_rows==1);
  if(index%2){r.ime.window()->request(1,0,true);r.settle();CHECK(r.vertical_frame.layout.candidate_content_rows==3);checkReservation(r);}
  CHECK(!r.numbers.empty());const auto chosen=r.numbers[0];const std::string text(chosen.text.data(),chosen.bytes);
  r.tap(30);r.settle();CHECK(r.ime.raw().empty());checkReservation(r);
  control.body(text);checkReservation(control);CHECK(r.app.document.text()==control.app.document.text());
  // Compare to the same edit with no IME: legitimate cursor/scroll advancement
  // from committed text is allowed; extra reflow caused by hiding IME is not.
  CHECK(r.app.document.cursorByte()==control.app.document.cursorByte());
  CHECK(r.app.verticalPresentation().viewport().visible(r.app.verticalPresentation().caret().caret.column));
}
void cycles(unsigned n){SwitchRig r,control;startVertical(r);startVertical(control);r.tap(57);r.settle();
  for(unsigned i=0;i<n;++i)commitCycle(r,control,i);
  std::cout<<"COMMIT_CYCLES count="<<n<<" geometry_matches_direct_editor=1\n";}
void P5(){cycles(1);}void P6(){cycles(12);}

struct InkCapture {
  std::vector<std::pair<int,int>> black;std::vector<std::array<int,4>> accents;
  s3app::HorizontalRawPort port(){return {[](void* p,int x,int y){static_cast<InkCapture*>(p)->black.emplace_back(x,y);},
    [](void* p,int x,int y,unsigned w,unsigned h){static_cast<InkCapture*>(p)->accents.push_back({x,y,int(w),int(h)});},this};}
};
std::vector<std::pair<int,int>> expectedGlyphs(std::string_view raw,const ime_device::RawViewport& viewport,int y){
  const s3app::ImeCompositionFont font;std::vector<std::pair<int,int>> out;
  for(size_t i=viewport.begin;i<viewport.end;++i){const auto g=font.glyph(static_cast<unsigned char>(raw[i]));CHECK(!g.missing);
    for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)
      if(g.bits[row*g.stride+col/8]&(128U>>(col%8)))out.emplace_back(8+int(viewport.at(i))+g.bearing+int(col),y+int(row));}
  return out;
}
void P7(){
  SwitchRig r;r.body(longBody());r.move(840);r.tap(57);r.settle();
  const s3app::ImeCompositionFont font;const auto body=r.app.navigator.sync();const auto height=r.app.layout.heightPx();
  const char* samples[]={"a","ab","abcd","abcdef","abcdefgh","abcdefghijkl","abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl"};
  for(const auto* sample:samples){const std::string_view raw(sample);r.type(raw.substr(r.ime.raw().size()));r.draw();
    const auto before=metrics.io();const auto query=r.ime.window()->querySerial();
    for(const int y:{278,210}){InkCapture capture;s3app::paintHorizontalRaw(r.ime,y,capture.port());
      CHECK(capture.black==expectedGlyphs(raw,r.ime.editViewport(font),y+s3app::ImeCompositionFontMetrics::horizontal_raw_y));
      CHECK(!capture.black.empty());for(auto p:capture.black)CHECK(p.first>=8&&p.first<292&&p.second>=y+22&&p.second<y+47);
      for(auto b:capture.accents)CHECK(b[0]>=7&&b[0]+b[2]<=292&&b[1]>=y+22&&b[1]+b[3]<=y+48);
    }
    CHECK(r.ime.window()->querySerial()==query);const auto io=metrics.io()-before;CHECK(io.reads==0&&io.bytes==0&&io.seeks==0);
    CHECK(r.app.layout.heightPx()==height&&r.app.navigator.sync().first_visible_line==body.first_visible_line);
    CHECK(r.app.navigator.sync().cursor_x==body.cursor_x&&font.lineHeight()==24);
  }
  CHECK(r.ime.editViewport(font).begin>0);
  // Exercise every caret position with the existing viewport algorithm. No
  // width or size adaptation is permitted, including leading j/f overhang.
  const std::string raw(64,'w');for(size_t cursor=0;cursor<=64;++cursor){const auto vp=rawViewport(raw,cursor,true,font);
    CHECK(vp.begin<=cursor&&vp.end>=cursor&&vp.width<284);}
  std::cout<<"H_A_FONT nominal=22 fixed=1 no_ink_clipped=1 body_separator_B_unchanged=1\n";
}
struct RawRuns {
  std::vector<v::AbRun> runs;
  v::AbPort port(){return {[](void* p,const v::AbRun& run){static_cast<RawRuns*>(p)->runs.push_back(run);},[](void*,v::Rect,bool){},this};}
};
v::Bitmap bodyBitmap(void* p,std::uint32_t cp){const auto g=static_cast<s3host::FontPort*>(p)->glyph(cp);return {g.bits,g.width,g.height,g.stride,g.bearing,size_t(g.stride)*g.height,g.missing};}
void checkRaw(std::string_view raw,size_t cursor){
  RawRuns capture;v::AbGeometry g;g.area=v::editor_area;g.composition_width=compositionOracleRow(raw);const auto result=v::layoutPreparingAb(raw,cursor,g,capture.port());
  const auto fixed=v::reservedAbRegions(g);CHECK(result.status==v::AbStatus::Ready&&sameRect(result.body,fixed.body)&&sameRect(result.composition,fixed.composition)&&sameRect(result.separator,fixed.separator));
  size_t first=raw.size(),last=0;
  for(const auto& run:capture.runs){const s3app::ImeCompositionFont font(run.typography);CHECK(run.flow==v::TextFlow::CompositionLatin&&run.box.height==font.lineHeight());
    CHECK(contains(fixed.composition,run.box)&&run.box.x==fixed.composition.x+s3app::ImeCompositionFontMetrics::padding);
    const auto at=size_t(run.text.data()-raw.data());first=std::min(first,at);last=std::max(last,at+run.text.size());
    std::vector<std::pair<int,int>> ink;v::RasterPort port;port.glyph=bodyBitmap;port.font_context=const_cast<s3app::ImeCompositionFont*>(&font);port.clip=fixed.composition;port.pixel_context=&ink;
    port.ink=[](void* p,uint16_t x,uint16_t y){static_cast<std::vector<std::pair<int,int>>*>(p)->emplace_back(x,y);};
    const auto painted=v::paintAbRun(run,port);CHECK(painted.status==v::Status::Ready&&painted.glyphs==run.text.size());
    size_t expected_ink=0;for(const auto cp:run.text){const auto b=font.glyph(static_cast<unsigned char>(cp));for(unsigned y=0;y<b.height;++y)for(unsigned x=0;x<b.width;++x)if(b.bits[y*b.stride+x/8]&(128U>>(x%8)))++expected_ink;}
    CHECK(ink.size()==expected_ink);for(auto p:ink)CHECK(contains(run.box,{uint16_t(p.first),uint16_t(p.second),1,1}));
  }
  CHECK(first<=cursor&&last>=cursor);
}
void P8(){for(const std::string raw:{"a","ab","abcd","abcdef","abcdefgh","abcdefghijkl","fjjwMW"})checkRaw(raw,raw.size());
  const std::string raw(64,'w');for(size_t cursor=0;cursor<=raw.size();++cursor)checkRaw(raw,cursor);
  std::cout<<"V_A_FONT nominal=22 start_aligned=1 bounds_fixed=1 full_ink_retained=1 caret_scroll=1\n";
}
void P9(){SwitchRig r;r.begin("wowo");const ImeFact before(r.ime);const BodyFact body(r.app);
  InkCapture h;s3app::paintHorizontalRaw(r.ime,278,h.port());CHECK(!h.black.empty());
  const auto io=metrics.io();r.toggle();r.draw();checkReservation(r);before.check(r.ime);body.check(r.app);
  RawRuns runs;const auto frame=verticalIme(&r.ime,r.app,r.now,0,runs.port());CHECK(frame.layout.status==v::AbStatus::Ready);
  const s3app::ImeCompositionFont font;unsigned glyphs=0;
  for(const auto& run:runs.runs)if(run.role==v::AbRole::Composition){CHECK(run.flow==v::TextFlow::CompositionLatin&&run.box.height==font.lineHeight());glyphs+=run.text.size();}
  CHECK(glyphs==4);r.toggle();r.draw();InkCapture back;s3app::paintHorizontalRaw(r.ime,278,back.port());CHECK(back.black==h.black);
  before.check(r.ime);body.check(r.app);const auto delta=metrics.io()-io;CHECK(delta.reads==0&&delta.bytes==0&&delta.seeks==0);
  std::cout<<"HV_FONT raw=wowo shared_nominal=22 transform=identity requery=0\n";
}
void P10(){SwitchRig r;startVertical(r);r.begin("ui");r.ime.window()->request(1,0,true);r.settle();
  const auto io=metrics.io();const auto costs=r.ime.window()->costs();const ImeFact fact(r.ime);
  const auto allocations=draftor::memory::allocation_stats;const auto new_calls=phase2c_new_calls;
  size_t pixels=0;const s3app::VerticalPaintPort port{[](void*,v::Rect,s3app::VerticalColor){},
    [](void* p,uint16_t,uint16_t,s3app::VerticalColor){++*static_cast<size_t*>(p);},&pixels,nullptr};
  const s3app::HorizontalRawPort h{[](void* p,int,int){++*static_cast<size_t*>(p);},[](void*,int,int,unsigned,unsigned){},&pixels};
  phase2c_forbid_new=true;draftor::memory::allocation_fail_after=0;
  for(unsigned i=0;i<100;++i){const auto painted=s3app::paintVerticalView(r.app,r.app.verticalPresentation(),&r.ime,r.font,r.now,port);
    if(painted.status!=v::Status::Ready)std::abort();s3app::paintHorizontalRaw(r.ime,210,h);}
  draftor::memory::allocation_fail_after=-1;phase2c_forbid_new=false;
  CHECK(pixels>0&&phase2c_new_calls==new_calls&&draftor::memory::allocation_stats.calls==allocations.calls);
  fact.check(r.ime);const auto after=r.ime.window()->costs();const auto delta=metrics.io()-io;
  CHECK(after.built_rows==costs.built_rows&&after.materialized_items==costs.materialized_items&&after.prefetch_starts==costs.prefetch_starts);
  CHECK(delta.reads==0&&delta.bytes==0&&delta.seeks==0);std::cout<<"PAINT_ONLY iterations=100 new=0 heap=0 query=0 materialized=0 TF_reads=0\n";
}
int main(int argc,char** argv){try{CHECK(argc==2);const std::string_view name(argv[1]);
  const std::pair<const char*,void(*)()> tests[]={{"P1",P1},{"P2",P2},{"P3",P3},{"P4",P4},{"P5",P5},{"P6",P6},{"P7",P7},{"P8",P8},{"P9",P9},{"P10",P10}};
  for(const auto& test:tests)if(name==test.first){test.second();CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS "<<name<<'\n';return 0;}CHECK(false);
}catch(const std::exception& e){phase2c_forbid_new=false;draftor::memory::allocation_fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}}
