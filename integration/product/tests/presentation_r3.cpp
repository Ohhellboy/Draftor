#define PHASE2C_REUSE_RIG
#include "runtime_layout_2c.cpp"
#include "composition_oracle.hpp"
#include "ime_presentation_policy.hpp"
#include "ime_composition_font.hpp"
#include "horizontal_raw.hpp"
#include <sstream>

void py(SwitchRig& r){r.tap(57);r.settle();r.tap(44,1);r.settle();CHECK(r.resources.rules()==hanime_extension::SchemeRules::FullPinyin);}
struct Rows {
 std::vector<std::string> raw;
 std::vector<v::Rect> boxes;
 v::AbPort port(){return {[](void* p,const v::AbRun& run){auto& r=*static_cast<Rows*>(p);if(run.role==v::AbRole::Composition){r.raw.emplace_back(run.text);r.boxes.push_back(run.box);}},[](void*,v::Rect,bool){},this};}
};
Rows rows(SwitchRig& r){Rows out;const auto f=verticalIme(&r.ime,r.app,r.now,0,out.port());CHECK(f.layout.status==v::AbStatus::Ready);return out;}
bool same(v::Rect a,v::Rect b){return a.x==b.x&&a.y==b.y&&a.width==b.width&&a.height==b.height;}
void visible(SwitchRig& r){auto& p=r.app.verticalPresentation();CHECK(p.settle()==v::Status::Ready);const auto c=p.viewport().caretRect(p.caret().caret);CHECK(c.width==28&&c.x>=p.region().x&&unsigned(c.x)+c.width<=292);}
void T01(){SwitchRig r;
 r.toggle();
 r.draw();
 const auto box=r.vertical_frame.layout;
 CHECK(box.candidates.width==28&&box.composition.width==6);
 r.begin("ui");
 CHECK(r.vertical_frame.layout.composition.width==54);const auto cn_box=r.vertical_frame.layout;
 r.tap(30);
 r.settle();
 CHECK(r.ime.raw().empty()&&same(cn_box.body,r.vertical_frame.layout.body));
 visible(r);
 }
void T02(){for(bool vertical:{false,true}){SwitchRig r;r.begin("ui");if(vertical){r.toggle();r.draw();}CHECK(!r.ime.window()->browsingExpanded());r.tap(vertical?80:81);CHECK(!vertical||!r.ime.window()->browsingExpanded());r.draw();r.settle();CHECK(r.ime.expanded());r.tap(vertical?79:82);r.settle();CHECK(r.ime.view()->row==0&&r.ime.window()->browsingExpanded());r.tap(41);r.settle();CHECK(r.ime.rawEditing()&&r.ime.window()->browsingExpanded()==vertical);r.tap(41);r.settle();CHECK(!r.ime.rawEditing()&&r.ime.raw().empty()&&!r.ime.window()->browsingExpanded()&&r.app.document.text().empty());r.type("ui");r.settle();r.tap(vertical?45:46);r.settle();CHECK(r.ime.expanded());r.tap(42);r.type("i");r.settle();CHECK(r.ime.window()->browsingExpanded()==vertical);std::cout<<"B_ROUTING vertical="<<vertical<<" return_first_keeps_expanded=1 Esc_and_edit_reset=1\n";}}
void T03(){for(bool longer:{false,true}){SwitchRig r;py(r);r.type(longer?"woshangwuchifanle":"wochifanle");r.settle();r.toggle();r.draw();std::ofstream image(longer?"r3_syllables_long.pixels":"r3_syllables_short.pixels",std::ios::binary);image.write(reinterpret_cast<const char*>(r.pixels.data()),r.pixels.size());
 const auto actual=rows(r);const std::vector<std::string> expected=longer?std::vector<std::string>{"wo","shang","wu","chi","fan","le"}:std::vector<std::string>{"wo","chi","fan","le"};for(const auto& row:actual.raw)std::cout<<"SYLLABLE "<<row<<'\n';CHECK(actual.raw==expected);CHECK(r.app.compositionWidth()==compositionOraclePeak(longer?"shang":"fan"));}}
void T04(){SwitchRig r;py(r);r.type("woshangwu");r.settle();CHECK(r.app.compositionWidth()==compositionOraclePeak("shang"));for(unsigned i=0;i<5;++i)r.tap(42);CHECK(r.app.compositionWidth()==compositionOraclePeak("shang"));r.settle();CHECK(r.app.compositionWidth()==compositionOraclePeak("shang"));r.ime.interrupt();CHECK(r.app.compositionWidth()==6);r.type("hao");r.settle();CHECK(r.app.compositionWidth()==compositionOraclePeak("hao"));r.tap(30);CHECK(r.app.compositionWidth()==compositionOraclePeak("hao"));r.settle();CHECK(r.ime.raw().empty()&&r.app.compositionWidth()==6);}
void partialAndRetry(){
 {SwitchRig r;py(r);r.type("woshang");r.settle();r.ime.window()->query(2);r.settle();r.tap(30);r.settle();CHECK(!r.ime.selected().empty()&&!r.ime.raw().empty()&&r.app.compositionWidth()==compositionOraclePeak("shang")&&!r.ime.window()->browsingExpanded());}
 {SwitchRig r;std::string full(65535,'x');for(size_t at=0;at<full.size();at+=8192)r.body(std::string_view(full).substr(at,8192));py(r);r.type("shang");r.settle();r.tap(30);CHECK(r.app.compositionWidth()==compositionOraclePeak("shang"));r.settle();CHECK(r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect&&r.app.compositionWidth()==compositionOraclePeak("shang")&&!r.ime.raw().empty());}
}
void T05(){for(unsigned variant=0;variant<3;++variant){SwitchRig r;py(r);r.type("woshang");r.settle();r.toggle();r.draw();CHECK(r.app.compositionWidth()==compositionOraclePeak("shang"));r.toggle();r.draw();for(unsigned i=0;i<5;++i)r.tap(42);r.type(variant?"zhuang":"chi");if(variant==2){for(unsigned i=0;i<6;++i)r.tap(42);r.type("chi");}CHECK(r.app.compositionWidth()==compositionOraclePeak("shang"));r.toggle();r.draw();CHECK(r.vertical_frame.layout.composition.width==54);}}
void T06(){SwitchRig r;py(r);r.type(std::string(32,'b'));r.draw();CHECK(r.ime.raw().size()==32&&r.app.compositionWidth()==compositionOraclePeak("bbbbbb"));auto shown=rows(r);CHECK(shown.raw.size()==1&&shown.raw[0].size()<=6);r.ime.interrupt();r.type("woshangwuchifanlewoshangwuchifanlewoshangwuchifanle");r.draw();shown=rows(r);CHECK(shown.raw.size()<=13);CHECK(!shown.raw.empty()&&shown.raw.back()=="le");CHECK(r.ime.raw().size()>shown.raw.size());}
void T07(){SwitchRig r;py(r);r.type("shang");r.settle();const auto stale=r.ime.presentationNavigation();CHECK(stale.status==hanime_extension::NavigationStatus::Ready);r.ime.interrupt();r.type("a");r.settle();CHECK(r.app.compositionWidth()==compositionOraclePeak("a"));const auto pyOwner=r.ime.resourceOwner();const auto delayed=r.event(5);auto up=delayed;up.down=false;up.ms=++r.now;r.tap(44,1);r.settle();const auto wbOwner=r.ime.resourceOwner();CHECK(r.resources.rules()==hanime_extension::SchemeRules::DirectExact&&wbOwner!=pyOwner&&r.resources.ready());r.deliver(delayed);r.deliver(up);CHECK(r.ime.raw().empty()&&!r.ime.hasWork()&&r.app.document.text().empty());r.tap(44,1);r.settle();CHECK(r.resources.rules()==hanime_extension::SchemeRules::R1Xiaohe&&r.ime.resourceOwner()!=pyOwner&&r.ime.resourceOwner()!=wbOwner&&r.resources.ready());r.deliver(delayed);r.deliver(up);CHECK(delayed.resource_owner==pyOwner&&up.resource_owner==pyOwner&&r.ime.raw().empty()&&!r.ime.hasWork()&&r.app.document.text().empty());std::cout<<"MIGRATION T07 PY_WB_XH owners="<<pyOwner<<','<<wbOwner<<','<<r.ime.resourceOwner()<<" stale_rejected=1\n";r.type("ui");r.draw();CHECK(r.resources.rules()==hanime_extension::SchemeRules::R1Xiaohe&&r.app.compositionWidth()==compositionOraclePeak("ui"));}
void pendingSession(){SwitchRig r;py(r);r.type("shang");CHECK(r.ime.hasWork()&&r.app.compositionWidth()==compositionOraclePeak("shang"));r.ime.interrupt();r.type("a");r.settle();r.toggle();r.draw();CHECK(r.app.compositionWidth()==compositionOraclePeak("a")&&r.ime.raw()=="a"&&r.vertical_frame.layout.composition.width==54);}
void T08(){const s3app::ImeCompositionFont font;CHECK(font.lineHeight()==24);for(unsigned cp=32;cp<=126;++cp){const auto g=font.glyph(cp);CHECK(!g.missing&&g.bearing>=0&&unsigned(g.bearing)+g.width<=g.advance&&g.advance<=23);CHECK(g.height==24);}SwitchRig r;r.begin("gjpqy");r.toggle();r.draw();for(const auto b:rows(r).boxes)CHECK(b.height==24);}
void T09(){for(unsigned a:{6U,21U,42U,74U})for(unsigned b:{1U,2U,3U}){v::AbGeometry g;
 g.area=v::editor_area;
 g.composition_width=a;
 g.candidate_columns=b;
 const auto r=v::reservedAbRegions(g);
 CHECK(r.status==v::AbStatus::Ready);
 const unsigned cols=(r.body.width+6)/34;
 CHECK(cols>=1);
 CHECK(r.composition.width==a&&r.candidates.width==30*b-2);
 CHECK(r.separator.x+1<=r.body.x&&r.composition.x+r.composition.width<r.separator.x);
 std::cout<<"GEOMETRY A="<<a<<" B="<<b<<" a_rect="<<r.composition.x<<','<<r.composition.width<<" b_rect="<<r.candidates.x<<','<<r.candidates.width<<" separator="<<r.separator.x<<" body="<<r.body.x<<','<<r.body.width<<" columns="<<cols<<'\n';
 }v::AbGeometry bad;
 bad.area.width=50;
 CHECK(v::reservedAbRegions(bad).status==v::AbStatus::CapacityExceeded);
 }
void metricsChange(){v::Viewport view;v::Input input;input.text="甲乙丙丁";input.stamp.geometry_revision=1;v::Config config;config.body={8,34,142,84};
 CHECK(view.start(input,config,{6,Affinity::Downstream})==v::Status::MoreWork);while(view.service(input,64).status==v::Status::MoreWork){}CHECK(view.visibleColumns()==4);const auto old=input.stamp;
 ++input.stamp.geometry_revision;config.column_width_px=24;config.column_gap_px=4;config.upright_advance_px=24;
 CHECK(view.start(input,config,{6,Affinity::Downstream})==v::Status::MoreWork);while(view.service(input,64).status==v::Status::MoreWork){}CHECK(view.status(old)==v::Status::Stale&&view.visibleColumns()==5);const auto caret=view.locate(input.stamp,{6,Affinity::Downstream});CHECK(caret.status==v::Status::Ready&&view.caretRect(caret.caret).width==24);
}
void T10(){SwitchRig r;r.body(longBody());r.move(900);py(r);r.toggle();r.draw();auto& p=r.app.verticalPresentation();CHECK(p.placeCaretSlot(p.viewport().visibleColumns()-1));r.draw();const auto first=p.viewport().firstColumn(),column=p.caret().caret.column;const BodyFact fact(r.app);r.type("zhuang");r.draw();fact.check(r.app);const auto n=p.viewport().visibleColumns();CHECK(p.viewport().firstColumn()==(column>=first+n?column+1-n:first));visible(r);const auto avoided=p.viewport().firstColumn();r.ime.interrupt();r.draw();CHECK(p.viewport().firstColumn()==avoided);}
void T11(){SwitchRig r;r.body(longBody());r.move(1200);r.begin("ui");r.toggle();r.draw();auto& p=r.app.verticalPresentation();const auto requested=p.viewport().visibleColumns()-1;p.placeCaretSlot(requested);r.draw();const auto column=p.caret().caret.column;const auto cursor=r.app.document.cursorByte();auto anchored=[&](){CHECK(p.viewport().firstColumn()==column-std::min<unsigned>(requested,p.viewport().visibleColumns()-1));CHECK(r.app.document.cursorByte()==cursor);};r.tap(80);r.settle();anchored();r.tap(41);r.settle();anchored();r.ime.interrupt();r.draw();anchored();}
void T12(){SwitchRig r;r.body("我今天吃饭了");r.move(9);py(r);r.type("shang");r.settle();const BodyFact b(r.app);r.toggle();r.draw();b.check(r.app);for(unsigned i=0;i<5;++i)r.tap(42);r.type("hao");r.settle();for(unsigned i=0;i<4;++i){r.toggle();r.draw();b.check(r.app);}std::size_t chosen=0;for(;chosen<r.numbers.size();++chosen)if(std::string_view(r.numbers[chosen].text.data(),r.numbers[chosen].bytes)=="好")break;CHECK(chosen<r.numbers.size());r.tap(30+chosen);r.settle();CHECK(r.app.document.text()=="我今天好吃饭了"&&r.app.document.cursorByte()==12&&r.app.editor.undoCount()==2);}
void T13(){for(const auto* text:{"","甲\n","甲乙 hello,@#2026。\n"}){SwitchRig r;r.body(text);const BodyFact b(r.app);for(unsigned i=0;i<4;++i){r.toggle();r.draw();b.check(r.app);if(!r.app.horizontalActive())visible(r);}}S5();S7();S18();}
void T14(){SwitchRig r;
 std::string text;
 for(unsigned i=0;i<80;++i)text+="甲\n";
 r.body(text);
 r.move(60);
 r.app.observeComposition(true,74,true);
 r.draw();
 CHECK(r.app.navigator.sync().visible_lines==9);
 CHECK(r.app.navigator.placeCaretSlot(3));
 const auto first=r.app.navigator.sync().first_visible_line;
 const BodyFact b(r.app);
 for(unsigned i=0;i<100;++i){r.toggle();
 r.draw();
 auto& p=r.app.verticalPresentation();
 CHECK(p.viewport().visibleColumns()==4&&p.caret().caret.column-p.viewport().firstColumn()==2);
 r.toggle();
 r.draw();
 CHECK(r.app.navigator.sync().first_visible_line==first&&r.app.navigator.sync().visual_line-first==3);
 b.check(r.app);
 }std::cout<<"F4_ROUNDTRIP target_bookmarks H_slot=3 V_slot=2 rounds=100 drift=0\n";
 }
void T15(){s3app::IdealCaretAnchor anchor;
 std::uint16_t slot=9;
 CHECK(!anchor.quantize(0,slot));
 anchor.observe(3,9);
 CHECK(anchor.quantize(3,slot)&&slot==1);
 CHECK(anchor.quantize(1,slot)&&slot==0);
 anchor.observe(0,1);
 CHECK(anchor.quantize(9,slot)&&slot==3);
 anchor.observe(65534,65535);
 CHECK(anchor.quantize(65535,slot)&&slot==65534);
 anchor.observe(0,0);
 CHECK(!anchor.quantize(3,slot));
 SwitchRig r;
 r.body("甲");
 r.app.observeComposition(true,74,true);
 r.app.candidateColumns(3);
 r.toggle();
 r.app.preparePresentation();
 visible(r);
 CHECK(r.app.document.cursorByte()==3);
 }
void T16(){SwitchRig r;r.body(longBody());r.move(1500);r.toggle();r.draw();const auto old=r.app.verticalPresentation().viewport().firstColumn();r.toggle();r.draw();r.move(90);r.toggle();r.draw();CHECK(r.app.verticalPresentation().viewport().firstColumn()<old);visible(r);}
void anchorGeometry(){
 SwitchRig r;
 std::string text;
 for(unsigned i=0;i<80;++i)text+="甲\n";
 r.body(text);
 r.move(60);
 r.draw();
 r.app.navigator.placeCaretSlot(3);
 r.toggle();
 r.draw();
 auto& p=r.app.verticalPresentation();
 CHECK(p.viewport().visibleColumns()==7&&p.caret().caret.column-p.viewport().firstColumn()==5);
 r.app.observeComposition(true,74,true);
 r.draw();
 CHECK(p.viewport().visibleColumns()==4);
 r.toggle();
 r.draw();
 const auto h=r.app.navigator.sync();
 CHECK(h.visual_line-h.first_visible_line==3);
}
void T17(){S8();S10();S11();S12();S13();}
void T18(){for(bool raw:{false,true}){SwitchRig r;r.body("我今天吃饭了");r.move(9);r.begin("ui");if(raw){r.tap(41);r.settle();}const BodyFact b(r.app);for(auto key:{74,77,75,78}){r.tap(key);r.draw();b.check(r.app);r.toggle();r.draw();r.tap(key);b.check(r.app);}CHECK(r.ime.raw()=="ui");}}
void T19(){SwitchRig r;r.begin("ui");const auto io=metrics.io();const auto costs=r.ime.window()->costs();const auto query=r.ime.window()->querySerial();for(unsigned i=0;i<20;++i){r.toggle();r.draw();}const auto delta=metrics.io()-io;CHECK(delta.requests==0&&delta.reads==0&&r.ime.window()->querySerial()==query);CHECK(r.ime.window()->costs().materialized_items==costs.materialized_items);}
void T20(){SwitchRig r;
 py(r);
 r.toggle();
 r.type("zhuang");
 CHECK(r.ime.hasWork()&&r.app.compositionWidth()==compositionOraclePeak("zhuang"));
 r.draw();
 CHECK(r.vertical_frame.layout.composition.width==54&&!rows(r).raw.empty());
 const auto rect=r.vertical_frame.layout.separator;
 r.settle();
 CHECK(same(rect,r.vertical_frame.layout.separator));
 }
void T21(){for(bool latin:{false,true}){SwitchRig r;std::string text;if(latin)text.assign(65535,'a');else {for(unsigned i=0;i<16383;++i)text+="甲\n";text+="abc";}for(std::size_t at=0;at<text.size();at+=8192)r.body(std::string_view(text).substr(at,8192));r.toggle();auto& p=r.app.verticalPresentation();unsigned slices=0;
  do{const auto before=p.viewport().totalWork();const bool ready=prepareImePresentation(&r.ime,r.app,true);CHECK(p.viewport().totalWork()-before<=2048);++slices;if(ready)break;CHECK(slices<1000);}while(true);
  r.draw();const auto initial=p.viewport().totalWork();std::size_t max_work=0;for(unsigned i=0;i<20;++i){const auto before=p.viewport().totalWork();r.toggle();r.draw();r.toggle();r.draw();max_work=std::max(max_work,p.viewport().totalWork()-before);}CHECK(r.app.document.text()==text.c_str());std::cout<<"LONG_DOCUMENT bytes="<<text.size()<<" latin="<<latin<<" initial_work="<<initial<<" repeated_max_work="<<max_work<<'\n';CHECK(max_work<2000);const auto before=p.viewport().totalWork();r.tap(42);r.draw();r.body("x");const auto edit_work=p.viewport().totalWork()-before;std::cout<<" LONG_EDIT work="<<edit_work<<'\n';CHECK(edit_work<2000);}}
int main(int argc,char** argv){try{CHECK(argc==2);const std::pair<const char*,void(*)()> tests[]={
 {"T01",T01},{"T02",T02},{"T03",T03},{"T04",+[]{T04();partialAndRetry();}},{"T05",T05},{"T06",T06},{"T07",+[]{T07();pendingSession();}},{"T08",T08},{"T09",+[]{T09();metricsChange();}},{"T10",T10},{"T11",T11},{"T12",T12},{"T13",T13},{"T14",T14},{"T15",T15},{"T16",+[]{T16();anchorGeometry();}},{"T17",T17},{"T18",T18},{"T19",T19},{"T20",T20},{"T21",T21}};
 for(const auto& test:tests)if(std::string_view(argv[1])==test.first){test.second();CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS "<<test.first<<'\n';return 0;}CHECK(false);
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
