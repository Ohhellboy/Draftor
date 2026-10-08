// Contract tests: product HID path + original P23 authorization and resources.
#define PHASE2C_REUSE_RIG
#include "runtime_layout_2c.cpp"
#include "composition_oracle.hpp"
#include "vertical_hud.hpp"
#include "ime_composition_font.hpp"
#include <sstream>
#include <set>
#include <iomanip>
namespace {
bool recording=false;
unsigned next_script=0;
struct RunFact {std::string text;
 v::Rect box;
 v::AbRole role;
 bool focused;
 unsigned number;
 s3app::CompositionTypography type;
 };
struct LineFact {v::Rect box;
 bool red;
 };
struct Capture {
 std::vector<RunFact> runs;
 std::vector<LineFact> lines;
 v::AbPort port(){return {
  [](void* p,const v::AbRun& r){static_cast<Capture*>(p)->runs.push_back({std::string(r.text),r.box,r.role,r.focused,r.number,r.typography});
 },
  [](void* p,v::Rect b,bool red){static_cast<Capture*>(p)->lines.push_back({b,red});
 },this};
 }
 std::string marked()const{std::string s;
 for(const auto& r:runs)if(r.role==v::AbRole::Composition&&r.focused)s+=r.text;
 return s;
 }
 std::string raw()const{std::string s;
 for(const auto& r:runs)if(r.role==v::AbRole::Composition)s+=r.text;
 return s;
 }
 unsigned carets(v::Rect a)const{unsigned n=0;
 for(const auto& l:lines)if(l.box.x>=a.x&&l.box.x<a.x+a.width&&l.box.width==1&&l.box.height==24)++n;
 return n;
 }
};
struct R2Rig:SwitchRig {
 unsigned script_id=++next_script,event_index=0,transition_frames=0;
 bool watch_partial=false;
 void tap(std::uint8_t key,std::uint8_t mods=0){
  for(bool down:{true,false}){
   const auto e=event(key,down,mods);
 ++event_index;
   if(recording)std::cout<<"EVENT "<<event_index<<" script="<<script_id<<" key="<<unsigned(key)<<" down="<<down<<" mods="<<unsigned(mods)<<" frame="<<e.ime_frame<<'\n';
   deliver(e);
  }
 }
 void toggle(){tap(61);
 }
 void type(std::string_view s){for(char c:s)tap(std::uint8_t(c-'a'+4));
 }
 void settle(bool paint=true){
  unsigned n=0;
  do{
   step();
 CHECK(++n<2000000);
   if(watch_partial&&ime.selected()=="我"){
    // Inspect every service boundary after selection, before waiting for ready.
    CHECK(!ime.window()->browsingExpanded());
    if(prepareImePresentation(&ime,app)){
     draw();
 ++transition_frames;
     CHECK(vertical_frame.layout.selected.width==28);
     CHECK(vertical_frame.layout.candidates.width==28);
     CHECK(vertical_frame.layout.composition.width==54);
    }
   }
  }while(!resources.ready()||!ime.composition()||ime.hasWork());
  if(paint)draw();
 }
 void begin(std::string_view raw){tap(57);
 settle();
 type(raw);
 settle();
 }
};
void py(R2Rig& r){r.tap(57);
 r.settle();
 r.tap(44,1);
 r.settle();
 CHECK(r.resources.rules()==hanime_extension::SchemeRules::FullPinyin);
 }
Capture capture(R2Rig& r){
 Capture c;
 const auto caret=r.ime.rawCursor();
 const auto query=r.ime.window()?r.ime.window()->querySerial():0;
 const auto f=verticalIme(&r.ime,r.app,r.now,0,c.port());
 CHECK(f.layout.status==v::AbStatus::Ready);
 CHECK(r.ime.rawCursor()==caret);
 CHECK(!r.ime.window()||r.ime.window()->querySerial()==query);
 return c;
}
void rect(const char* name,v::Rect b){std::cout<<' '<<name<<'='<<b.x<<','<<b.y<<','<<b.width<<','<<b.height;
 }
void smallText(R2Rig& r,std::string_view text,int x,int y){
 const s3host::BuiltinSmallFont font;
 for(unsigned char cp:text){const auto g=font.glyph(cp);
 for(unsigned j=0;j<g.height;++j)for(unsigned i=0;i<g.width;++i)
  if(g.bits[j*g.stride+i/8]&(0x80U>>(i%8))){const int xx=x+g.bearing+i,yy=y+j;
 if(xx>=0&&xx<300&&yy>=0&&yy<400)r.pixels[yy*300+xx]=1;
 }x+=g.advance;
 }
}
void save(R2Rig& r,const char* label){
 r.draw();
 const auto c=capture(r);
 const auto& f=r.vertical_frame.layout;
 std::cout<<"FRAME "<<label<<" script="<<r.script_id<<" event="<<r.event_index<<" mode="<<(r.ime.rawEditing()?"raw-edit":"candidate")
  <<" stage="<<(r.ime.window()?r.ime.window()->candidateStage():0)<<" raw="<<r.ime.raw()<<" selected="<<r.ime.selected()
  <<" raw_caret="<<r.ime.rawCursor()<<" affinity=upstream_endpoint body_cursor="<<r.app.document.cursorByte();
 if(r.ime.composition()){const auto s=r.ime.composition()->snapshot();
 std::cout<<" input="<<s.input_id<<" revision="<<s.revision<<" scheme="<<s.scheme_epoch<<" focus="<<s.focus;
 }
 if(r.ime.window()){const auto* item=r.ime.window()->focusedCurrent();
 if(item)std::cout<<" range="<<item->begin<<','<<item->end<<" ordinal="<<item->ordinal<<" ref="<<item->ref;
 }
 rect("A",f.composition);
 rect("S",f.selected);
 rect("B",f.candidates);
 rect("BODY",f.body);
 std::cout<<'\n';
 for(const auto& run:c.runs){std::cout<<"RUN role="<<int(run.role)<<" focused="<<run.focused<<" number="<<run.number<<" font="<<int(run.type)<<" text="<<run.text;
 rect("box",run.box);
 std::cout<<'\n';
 }
 for(const auto& line:c.lines){std::cout<<"LINE red="<<line.red;
 rect("box",line.box);
 std::cout<<'\n';
 }
 if(!recording)return;
 const s3host::BuiltinSmallFont font;
 const char* mode=!r.ime.chinese()?"EN | UNSAVED":r.resources.rules()==hanime_extension::SchemeRules::FullPinyin?"CN PY | UNSAVED":"CN XH | UNSAVED";
 const auto top=s3app::verticalTop(mode,"Untitled",font);
 smallText(r,top.text,292-top.width,4);
 s3app::VerticalHudLine bottom;
 s3app::hudAppend(bottom,r.ime.active()?verticalStatus(r.ime):"Ready",font);
 smallText(r,bottom.text,292-bottom.width,383);
 std::ofstream image(std::string(label)+".pixels",std::ios::binary);
 image.write(reinterpret_cast<const char*>(r.pixels.data()),r.pixels.size());
}
void toRaw(R2Rig& r,std::size_t position){
 if(!r.ime.rawEditing()){r.tap(41);
 r.settle();
 }
 for(unsigned n=0;r.ime.rawCursor()!=position;++n){
  CHECK(n<600);
 const auto prior=r.ime.rawCursor();
 r.tap(prior>position?82:81);
 r.settle();
 CHECK(prior!=r.ime.rawCursor());
 }
}
void clearRaw(R2Rig& r){
 toRaw(r,r.ime.raw().size());
 while(!r.ime.raw().empty())r.tap(42);
 r.settle();
}
std::size_t findNumber(R2Rig& r,std::string_view text){
 for(std::size_t i=0;i<r.numbers.size();++i)if(std::string_view(r.numbers[i].text.data(),r.numbers[i].bytes)==text)return i;
 throw std::runtime_error("Expected frozen-resource identity not in current numbered column: "+std::string(text));
}
void loadPrefix(R2Rig& r,std::size_t endpoint){toRaw(r,endpoint);
 r.tap(80);
 r.settle();
 CHECK(!r.ime.rawEditing());
 }
void woStage(R2Rig& r){
 py(r);
 r.type("woxianzai");
 r.settle();
 r.toggle();
 r.draw();
 loadPrefix(r,2);
 CHECK(r.numbers[findNumber(r,"我")].begin==0&&r.numbers[findNumber(r,"我")].end==2);
}
void chooseWo(R2Rig& r,bool expand=true){
 const BodyFact before(r.app);
 woStage(r);
 if(expand){r.tap(80);
 r.settle();
 CHECK(r.ime.window()->browsingExpanded());
 }
 r.tap(30+findNumber(r,"我"));
 r.settle();
 CHECK(r.ime.selected()=="我");
 before.check(r.app);
}
void A1(){
 R2Rig r;
 r.toggle();
 r.draw();
 CHECK(r.app.compositionWidth()==6&&r.vertical_frame.layout.composition.width==6);
 save(r,"01_xh_idle");
 r.begin("ui");
 CHECK(r.app.compositionWidth()==compositionOraclePeak("ui"));
 r.tap(30);
 r.settle();
 CHECK(r.ime.raw().empty()&&r.app.compositionWidth()==6);
 r.type("wowo");
 r.settle();
 CHECK(r.app.compositionWidth()==compositionOraclePeak("wo"));
 r.type(std::string(18,'w'));
 r.settle();
 CHECK(r.app.compositionWidth()==compositionOraclePeak("ww"));
 r.tap(44,1);
 r.settle();
 CHECK(r.ime.raw().empty()&&r.app.compositionWidth()==6);
 r.type("shuang");
 r.settle();
 CHECK(r.app.compositionWidth()==compositionOraclePeak("shuang"));
 r.tap(44,1);
 r.settle();CHECK(r.resources.rules()==hanime_extension::SchemeRules::DirectExact&&r.resources.ready());r.tap(44,1);r.settle();CHECK(r.resources.rules()==hanime_extension::SchemeRules::R1Xiaohe&&r.resources.ready()); // WB transit; preserve the following Xiaohe width oracle.
 CHECK(r.ime.raw().empty()&&r.app.compositionWidth()==6);
 r.type("u");
 r.settle();
 CHECK(r.app.compositionWidth()==compositionOraclePeak("u"));
}
void A2(){
 R2Rig r;
 py(r);
 unsigned peak=0;
 for(unsigned length:{1U,2U,4U,2U,5U,6U,3U}){
  while(r.ime.raw().size()>length)r.tap(42);
  while(r.ime.raw().size()<length)r.type("b");
  r.settle();
 peak=std::max(peak,compositionOraclePeak(std::string(length,'b')));
 CHECK(r.app.compositionWidth()==peak);
  // No Vertical paint has occurred while maintaining this entire peak.
 }
 r.toggle();
 r.draw();
 const auto c=capture(r);
 CHECK(c.raw()=="bbb"&&r.vertical_frame.layout.composition.width==54);
 for(const auto& run:c.runs)if(run.role==v::AbRole::Composition)CHECK(run.type==s3app::CompositionTypography::Normal);
 clearRaw(r);
 CHECK(r.app.compositionWidth()==6&&r.ime.raw().empty());
 R2Rig mixed;
 py(mixed);
 mixed.type("woshuanshuang");
 mixed.settle();
 mixed.toggle();
 mixed.draw();
 std::set<int> types;
 for(const auto& run:capture(mixed).runs)if(run.role==v::AbRole::Composition)types.insert(int(run.type));
 CHECK(types.size()==3);
 save(mixed,"06_shuan_shuang_same_frame");
}
void SelectedLifecycle(){
 R2Rig r;
 woStage(r);
 CHECK(!r.app.selectedAppeared()&&r.vertical_frame.layout.selected.width==28);
 CHECK(r.vertical_frame.layout.body.x==r.vertical_frame.layout.selected.x+r.vertical_frame.layout.selected.width+2);
 save(r,"02_py_no_s");
 r.tap(30+findNumber(r,"我"));
 r.settle();
 CHECK(r.app.selectedAppeared()&&r.vertical_frame.layout.selected.width==28);
 toRaw(r,2);
 r.tap(42);
 r.settle();
 CHECK(r.ime.selected().empty());
 CHECK(r.app.selectedAppeared()&&r.vertical_frame.layout.selected.width==28);
 clearRaw(r);
 CHECK(!r.app.selectedAppeared()&&r.vertical_frame.layout.selected.width==28&&r.app.compositionWidth()==6);
}
void B1(){
 R2Rig r;
 woStage(r);
 r.tap(80);
 r.settle();
 const auto stage=r.ime.window()->candidateStage();
 r.watch_partial=true;
 r.tap(30+findNumber(r,"我"));
 r.settle();
 r.watch_partial=false;
 CHECK(r.transition_frames>0&&r.ime.window()->candidateStage()==stage+1);
 CHECK(r.app.compositionWidth()==compositionOraclePeak("xian")&&r.app.selectedAppeared()&&!r.ime.window()->browsingExpanded());
 std::cout<<"TRANSITION inspected_frames="<<r.transition_frames<<" S_first_and_B_compact_atomic=1\n";
 save(r,"03_partial_s_appears_b_compact");
}
void B2(){
 R2Rig r;
 woStage(r);
 r.tap(80);
 r.settle();
 const auto stage=r.ime.window()->candidateStage();
 r.tap(41);
 r.settle();
 r.tap(42);
 r.type("o");
 CHECK(r.ime.hasWork()&&r.ime.window()->browsingExpanded());
 r.settle();
 CHECK(r.ime.window()->candidateStage()==stage);
 CHECK(r.ime.rawEditing()); // Remain in raw for the explicit loadPrefix below; no obsolete Esc cycle.
 r.settle();
 loadPrefix(r,2);
 CHECK(r.ime.window()->candidateStage()==stage&&r.ime.window()->browsingExpanded());
 r.toggle();
 r.draw();
 r.tap(30+findNumber(r,"我"));
 r.settle();
 CHECK(r.ime.selected()=="我"&&!r.ime.window()->browsingExpanded());
 r.toggle();
 r.draw();
 CHECK(r.vertical_frame.layout.selected.width==28&&r.vertical_frame.layout.candidates.width==28);
 r.tap(80);
 r.settle();
 const auto next=r.ime.window()->candidateStage();
 loadPrefix(r,2);
 CHECK(r.ime.window()->candidateStage()==next+1&&!r.ime.window()->browsingExpanded());
 CHECK(r.app.selectedAppeared());
}
void B3(){
 R2Rig r;
 r.toggle();
 r.begin("ui");
 for(unsigned i=0;i<5;++i){r.tap(81);
 r.settle();
 }
 CHECK(!r.ime.window()->browsingExpanded());
 const auto ordinal=r.ime.window()->focusedCurrent()->ordinal;
 r.tap(81);
 unsigned n=0,expanded_frames=0;
 do{r.step();
 CHECK(++n<100000);
 if(r.ime.view()->expanded){CHECK(r.ime.view()->count==3);
 ++expanded_frames;
 }}while(r.ime.hasWork());
 r.draw();
 CHECK(expanded_frames&&r.ime.window()->focusedCurrent()->ordinal==ordinal&&r.vertical_frame.layout.candidates.width==88);
 // Exercise genuine terminal one/two-column windows through physical keys.
 bool one=false,two=false;
 for(unsigned i=0;i<100;++i){
  const auto& view=*r.ime.view();
 one|=view.count==1;
 two|=view.count==2;
  if(view.at_end&&view.row==view.rows[view.count-1].index)break;
  r.tap(45);
 r.settle();
 CHECK(r.vertical_frame.layout.candidates.width==30*r.ime.view()->count-2);
 }
 CHECK(two);
 (void)one;
 R2Rig short_r;
 chooseWo(short_r);
 short_r.tap(80);
 short_r.settle();
 CHECK(short_r.ime.view()->count==1&&short_r.ime.view()->at_end&&short_r.vertical_frame.layout.candidates.width==28);
}
void C1(){
 R2Rig r;
 chooseWo(r);
 const auto cursor=r.ime.rawCursor();
 const BodyFact body(r.app);
 CHECK(r.ime.window()->focusedCurrent()&&std::string_view(r.ime.window()->focusedCurrent()->text.data(),r.ime.window()->focusedCurrent()->bytes)=="现在");
 auto c=capture(r);
 CHECK(c.marked()=="xianzai"&&c.carets(r.vertical_frame.layout.composition)==0);
 CHECK(r.ime.rawCursor()==cursor);
 body.check(r.app);
 save(r,"04_xian_zai_coverage");
 r.tap(41);
 r.settle();
 c=capture(r);
 CHECK(c.marked().empty()&&c.carets(r.vertical_frame.layout.composition)==1);
 // Existing Esc selects the genuine right endpoint of the displayed target.
 CHECK(r.ime.rawCursor()==9);
 save(r,"05_back_to_raw_edit");
 r.tap(80);
 r.settle();
 r.toggle();
 r.draw();
 r.toggle();
 r.draw();
 CHECK(capture(r).marked()=="xianzai");
 save(r,"08_f4_return_with_state");
}
void C2(){
 R2Rig r;
 chooseWo(r);
 loadPrefix(r,6);
 CHECK(r.ime.window()->focusedCurrent()->begin==2&&r.ime.window()->focusedCurrent()->end==6);
 CHECK(capture(r).marked()=="xian");
 loadPrefix(r,9);
 CHECK(capture(r).marked()=="xianzai");
 r.tap(81);
 r.settle();
 CHECK(r.ime.window()->focusedCurrent()->ordinal==1);
 CHECK(capture(r).marked()=="xianzai");
 // Synthetic projection adversaries are not product event/authorization tests.
 auto snapshot=r.ime.composition()->snapshot();
 auto source=*r.ime.view();
 source.col=1;
 source.rows[0].items[0].end=6;
 auto projection=verticalCandidateView(snapshot,source);
 CHECK(projection.status==v::AbStatus::Ready&&projection.mark.end==9);
 source.rows[0].items[1].end=65535;
 CHECK(verticalCandidateView(snapshot,source).status==v::AbStatus::InvalidInput);
 source=*r.ime.view();
 source.rows[0].items[1].begin=source.rows[0].items[1].end;
 CHECK(verticalCandidateView(snapshot,source).status==v::AbStatus::InvalidInput);
 source=*r.ime.view();
 ++snapshot.revision;
 CHECK(verticalCandidateView(snapshot,source).status==v::AbStatus::Stale);
 snapshot=r.ime.composition()->snapshot();
 ++snapshot.scheme_epoch;
 CHECK(verticalCandidateView(snapshot,source).status==v::AbStatus::Stale);
}
void C3(){
 using Mark=v::AbCompositionMark;
 const std::string raw="woxianzai";
 const hanime_extension::NavigationSpan spans[]={{0,2,hanime_extension::NavigationKind::CompleteSyllable},{2,6,hanime_extension::NavigationKind::CompleteSyllable},{6,9,hanime_extension::NavigationKind::CompleteSyllable}};
 hanime_extension::Snapshot snapshot;
 snapshot.raw=raw;
 hanime_extension::NavigationView navigation;
 navigation.status=hanime_extension::NavigationStatus::Ready;
 navigation.spans=spans;
 navigation.span_count=3;
 v::AbGeometry g;
 g.area=v::editor_area;
 g.composition_width=compositionOracleRow("xian");
 for(auto affinity:{Mark::Affinity::Upstream,Mark::Affinity::Downstream})for(std::size_t cursor:{0U,2U,6U,9U}){
  v::AbCandidateView candidates;
 candidates.mark.affinity=affinity;
 Capture c;
  const auto f=v::layoutAb(snapshot,navigation,cursor,candidates,v::EdgeSide::None,g,c.port());
 CHECK(f.status==v::AbStatus::Ready&&c.carets(f.composition)==1);
  const unsigned row=cursor==0?0:cursor==9?2:cursor==2?(affinity==Mark::Affinity::Upstream?0:1):(affinity==Mark::Affinity::Upstream?1:2);
  for(const auto& l:c.lines)if(l.box.width==1&&l.box.height==24)CHECK(l.box.y==22+26*row);
 }
 v::AbCandidateView candidates;
 candidates.mark={3,8,true};
 Capture c;
 CHECK(v::layoutAb(snapshot,navigation,2,candidates,v::EdgeSide::None,g,c.port()).status==v::AbStatus::Ready);
 CHECK(c.marked()=="ianza");
 R2Rig r;
 py(r);
 r.type(std::string(64,'b'));
 r.settle();
 r.toggle();
 r.draw();
 toRaw(r,0);
 for(unsigned i=0;i<64;++i){r.tap(81);
 r.settle();
 CHECK(r.ime.rawCursor()==i+1);
 CHECK(capture(r).carets(r.vertical_frame.layout.composition)==1);
 }
 // Every scalar remains representable by the bounded raw viewport; the
 // production navigator retains its own Unknown/IncompleteTail boundaries.
 for(unsigned i=0;i<=64;++i){Capture tail;
 CHECK(v::layoutPreparingAb(std::string(64,'b'),i,g,tail.port()).status==v::AbStatus::Ready);
 CHECK(tail.carets(v::reservedAbRegions(g).composition)==1);
 }
 clearRaw(r);
 std::string syllables;
 for(unsigned i=0;i<24;++i)syllables+="wo";
 r.type(syllables);
 r.settle();
 toRaw(r,syllables.size());
 const auto last=capture(r);
 CHECK(last.raw().size()<=26&&last.carets(r.vertical_frame.layout.composition)==1);
 clearRaw(r);
 r.type("sh");
 r.settle();
 CHECK(r.ime.presentationNavigation().has_incomplete);
 CHECK(capture(r).raw()=="sh");
}
#include "../dependencies/extension/src/navigation_syllables.inc"
void G1(){
 const unsigned expected[]={6,21,42,74};
 for(unsigned slots=1;slots<=4;++slots)for(bool selected:{false,true})for(unsigned columns:{1U,2U,3U}){
  v::AbGeometry g;
 g.area=v::editor_area;
 g.composition_width=expected[slots-1];
 g.selected_visible=selected;
 g.candidate_columns=columns;
  const auto f=v::reservedAbRegions(g);
 CHECK(f.status==v::AbStatus::Ready&&f.composition.width==expected[slots-1]);
  CHECK(f.selected.width==(selected?28:0));
 CHECK(f.body.x==f.composition.x+expected[slots-1]+(selected?32:2));
 CHECK((f.body.width+6)/34>=2);
 }
 unsigned tested=0;
 for(const auto& syllable:kSyllables){
  const std::string_view raw(syllable);
 const auto t=raw.size()==6?s3app::CompositionTypography::Compact6:raw.size()==5?s3app::CompositionTypography::Compact5:s3app::CompositionTypography::Normal;
  const s3app::ImeCompositionFont font(t);
 unsigned advance=0,ink_right=0;
  for(unsigned char cp:raw){const auto glyph=font.glyph(cp);
 CHECK(!glyph.missing&&glyph.bearing>=0);
 ink_right=std::max(ink_right,advance+glyph.bearing+glyph.width);
 advance+=glyph.advance;
 }
  CHECK(ink_right<=advance);
  const hanime_extension::NavigationSpan span{0,static_cast<uint16_t>(raw.size()),hanime_extension::NavigationKind::CompleteSyllable};
  hanime_extension::NavigationView nav;nav.status=hanime_extension::NavigationStatus::Ready;nav.spans=&span;nav.span_count=1;
  CHECK(s3app::requiredCompositionWidth(raw,nav)==compositionOracleRow(raw));
  v::AbGeometry geometry;geometry.area=v::editor_area;geometry.composition_width=compositionOracleRow(raw);
  Capture output;CHECK(v::layoutPreparingAb(raw,raw.size(),geometry,output.port()).status==v::AbStatus::Ready&&output.raw()==raw);
  CHECK(output.carets(v::reservedAbRegions(geometry).composition)==1);
 ++tested;
 }
 for(unsigned length=1;length<=6;++length){
  const std::string raw(length,'m');
 v::AbGeometry g;
 g.area=v::editor_area;
 g.composition_width=compositionOracleRow(raw);
 Capture c;
  CHECK(v::layoutPreparingAb(raw,raw.size(),g,c.port()).status==v::AbStatus::Ready);
 CHECK(c.raw()==raw);
 }
 for(const char* sample:{"wo","xian","zai","shuan","shuang","mmmm"}){
  const std::string_view raw(sample);
 const s3app::ImeCompositionFont font(raw.size()==6?s3app::CompositionTypography::Compact6:raw.size()==5?s3app::CompositionTypography::Compact5:s3app::CompositionTypography::Normal);
  unsigned advance=0,left=UINT32_MAX,right=0,ink=0;
  for(unsigned char cp:raw){const auto glyph=font.glyph(cp);
 for(unsigned y=0;y<glyph.height;++y)for(unsigned x=0;x<glyph.width;++x)if(glyph.bits[y*glyph.stride+x/8]&(128U>>(x%8))){left=std::min(left,advance+glyph.bearing+x);
 right=std::max(right,advance+glyph.bearing+x+1);
 ++ink;
 }advance+=glyph.advance;
 }
  std::cout<<"METRIC text="<<sample<<" advance="<<advance<<" ink="<<left<<':'<<right<<" ink_pixels="<<ink<<" A_old=74 A_new="<<compositionOracleRow(raw)<<" left_pad=2 right_residual="<<compositionOracleRow(raw)-2-advance<<" gap=2 old_end_to_S="<<74-advance<<" new_end_to_S="<<compositionOracleRow(raw)-advance<<'\n';
 }
 std::cout<<"MEASURED supported_syllables="<<tested<<" all_fit=1\n";
 R2Rig r;
 chooseWo(r);
 r.tap(80);
 r.settle();
 save(r,"07_max_a4_s1_b3");
}
void N1(){
 R2Rig r;
 r.body("我今天吃饭了");
 r.move(9);
 r.toggle();
 r.begin("ui");
 const BodyFact body(r.app);
 r.tap(80);
 r.settle();
 r.tap(81);
 r.settle();
 r.tap(82);
 r.settle();
 r.tap(45);
 r.settle();
 r.tap(46);
 r.settle();
 r.tap(79);
 r.settle();
 body.check(r.app);
 auto held=r.event(81);
 r.deliver(held);
 r.toggle();
 held.down=false;
 held.ms=++r.now;
 r.deliver(held);
 r.settle();
 body.check(r.app);
 for(unsigned digit=1;digit<=6;++digit){R2Rig d;
 d.toggle();
 d.begin("ui");
 CHECK(d.numbers.size()==6);
 const auto chosen=d.numbers[digit-1];
 d.tap(29+digit);
 d.settle();
 CHECK(d.app.document.text()==std::string_view(chosen.text.data(),chosen.bytes)&&d.app.editor.undoCount()==1);
 }
 R2Rig space;
 space.toggle();
 space.begin("ui");
 const auto item=space.numbers[0];
 space.tap(44);
 space.settle();
 CHECK(space.app.document.text()==std::string_view(item.text.data(),item.bytes));
}
void F1(){S8();
 S10();
 S11();
 S12();
 S13();
 S18();
 R2Rig r;
 r.toggle();
 r.begin("ui");
 r.tap(80);
 r.settle();
 for(unsigned i=0;i<8;++i){r.tap(45);
 r.settle();
 }
 const auto item=*r.ime.window()->focusedCurrent();
 const auto query=r.ime.window()->querySerial();
 for(unsigned i=0;i<12;++i){r.toggle();
 r.draw();
 CHECK(r.ime.window()->focusedCurrent()->key==item.key&&r.ime.window()->focusedCurrent()->ordinal==item.ordinal&&r.ime.window()->querySerial()==query);
 }
}
void D1(){
 R2Rig r;
 r.body("我今天吃饭了");
 r.move(9);
 py(r);
 r.type("woxianzai");
 r.settle();
 r.toggle();
 r.draw();
 loadPrefix(r,2);
 r.tap(30+findNumber(r,"我"));
 r.settle();
 const BodyFact before(r.app);
 r.tap(80);
 r.settle();
 r.toggle();
 r.draw();
 r.toggle();
 r.draw();
 before.check(r.app);
 toRaw(r,9);
 for(unsigned i=0;i<9;++i)r.tap(42);
 r.settle();
 CHECK(r.ime.raw().empty());
 r.type("hao");
 r.settle();
 r.tap(30+findNumber(r,"好"));
 r.settle();
 CHECK(r.app.document.text()=="我今天好吃饭了"&&r.app.document.cursorByte()==12&&r.app.editor.undoCount()==2);
 for(unsigned i=0;i<20;++i)r.step();
 CHECK(r.app.editor.undoCount()==2);
 r.tap(29,1);
 CHECK(r.app.document.text()=="我今天吃饭了");
 r.tap(28,1);
 CHECK(r.app.document.text()=="我今天好吃饭了");
 R2Rig sentence;
 py(sentence);
 sentence.type("woxianzaihao");
 sentence.settle();
 sentence.toggle();
 sentence.draw();
 loadPrefix(sentence,2);
 sentence.tap(80);
 sentence.settle();
 sentence.tap(30+findNumber(sentence,"我"));
 sentence.settle();
 CHECK(sentence.ime.selected()=="我"&&!sentence.ime.window()->browsingExpanded());
 loadPrefix(sentence,9);
 sentence.tap(80);
 sentence.settle();
 sentence.tap(30+findNumber(sentence,"现在"));
 sentence.settle();
 CHECK(sentence.ime.selected()=="我现在"&&!sentence.ime.window()->browsingExpanded()&&sentence.app.document.text().empty());
 sentence.tap(30+findNumber(sentence,"好"));
 sentence.settle();
 CHECK(sentence.app.document.text()=="我现在好"&&sentence.app.editor.undoCount()==1&&!sentence.app.selectedAppeared()&&sentence.app.compositionWidth()==6);
 S5();
 S7();
 S17();
}
void E1(){
 R2Rig r;
 woStage(r);
 r.tap(80);
 r.settle();
 const auto stage=r.ime.window()->candidateStage();
 auto stale=r.event(30);
 stale.ime_frame=UINT32_MAX;
 r.deliver(stale);
 stale.down=false;
 stale.ms=++r.now;
 r.deliver(stale);
 r.settle();
 CHECK(r.ime.selected().empty()&&r.ime.window()->candidateStage()==stage&&r.ime.window()->browsingExpanded());
 r.tap(30+findNumber(r,"我"));
 r.settle();
 CHECK(r.ime.selected()=="我"&&!r.ime.window()->browsingExpanded());
 // Separate Unknown fault executables prove no replay after actual sink acceptance.
}
void V1(){
 R2Rig r;
 r.body(longBody());
 r.move(900);
 chooseWo(r);
 r.tap(80);
 r.settle();
 const BodyFact fact(r.app);
 const auto column=r.app.verticalPresentation().viewport().firstColumn();
 const auto key=r.ime.window()->focusedCurrent()->key;
 for(unsigned i=0;i<100;++i){r.toggle();
 r.draw();
 r.toggle();
 r.draw();
 CHECK(r.app.verticalPresentation().viewport().firstColumn()==column&&r.ime.window()->focusedCurrent()->key==key);
 fact.check(r.app);
 }
 R2Rig cycles;
 cycles.toggle();
 cycles.tap(57);
 cycles.settle();
 for(unsigned i=0;i<10;++i){cycles.type("ui");
 cycles.settle();
 cycles.tap(80);
 cycles.settle();
 cycles.tap(30);
 cycles.settle();
 CHECK(cycles.ime.raw().empty()&&cycles.app.compositionWidth()==6&&!cycles.app.selectedAppeared()&&!cycles.ime.window()->browsingExpanded());
 }CHECK(cycles.app.editor.undoCount()==10);
}
}
#ifndef FOLLOWUP_REUSE_CONVERGENCE
int main(int argc,char** argv){try{
 CHECK(argc==2);
 const std::string name=argv[1];
 recording=name=="--frames";
 const std::pair<const char*,void(*)()> tests[]={{"R-A1",A1},{"R-A2",A2},{"R-S1",SelectedLifecycle},{"R-B1",B1},{"R-B2",B2},{"R-B3",B3},{"R-C1",C1},{"R-C2",C2},{"R-C3",C3},{"R-G1",G1},{"R-N1",N1},{"R-F1",F1},{"R-D1",D1},{"R-E1",E1},{"R-V1",V1}};
 if(recording){A1();
 A2();
 SelectedLifecycle();
 B1();
 C1();
 G1();
 return 0;
 }
 for(const auto& test:tests)if(name==test.first){test.second();
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 std::cout<<"PASS "<<name<<'\n';
 return 0;
 }CHECK(false);
 }catch(const std::exception& e){phase2c_forbid_new=false;
 draftor::memory::allocation_fail_after=-1;
 std::cerr<<e.what()<<'\n';
 return 1;
 }}

#endif // FOLLOWUP_REUSE_CONVERGENCE
