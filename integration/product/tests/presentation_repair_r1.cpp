#define PHASE2C_REUSE_RIG
#include "runtime_layout_2c.cpp"
#include "composition_oracle.hpp"
#include "ime_composition_font.hpp"
#include "ime_presentation_policy.hpp"
#include "vertical_hud.hpp"
void fullPinyin(SwitchRig& r){r.tap(57);r.settle();r.tap(44,1);r.settle();}
const VisibleItem& focused(const DeviceIme& ime){const auto& v=*ime.view();return v.rows[v.row-v.rows[0].index].items[v.col];}
struct RepairRuns{
 std::vector<std::string> a,s;std::vector<v::Rect> boxes;std::vector<s3app::CompositionTypography> types;unsigned columns=0;
 v::AbPort port(){return {[](void* p,const v::AbRun& run){auto& c=*static_cast<RepairRuns*>(p);if(run.role==v::AbRole::Composition){c.a.emplace_back(run.text);c.boxes.push_back(run.box);c.types.push_back(run.typography);}if(run.role==v::AbRole::Selected)c.s.emplace_back(run.text);if(run.role==v::AbRole::CurrentCandidate||run.role==v::AbRole::Preview)++c.columns;},[](void*,v::Rect,bool){},this};}
};
RepairRuns capture(SwitchRig& r){RepairRuns c;auto f=verticalIme(&r.ime,r.app,r.now,0,c.port());CHECK(f.layout.status==v::AbStatus::Ready);return c;}
void R01(){
 // The old unconditional S column is replaced by both lifecycle geometries.
 const unsigned widths[]={6,21,42,74};
 for(unsigned a=1;a<=4;++a)for(unsigned b:{1U,2U,3U})for(bool selected:{false,true}){
  v::AbGeometry g;g.area=v::editor_area;g.composition_width=widths[a-1];g.candidate_columns=b;g.selected_visible=selected;
  const auto z=v::reservedAbRegions(g);
  CHECK(z.status==v::AbStatus::Ready&&z.selected.width==(selected?28:0));
  CHECK(z.candidates.x+z.candidates.width+2==z.composition.x);
  CHECK(z.composition.x+z.composition.width+2==z.selected.x);
  CHECK(z.body.x==z.selected.x+(selected?30:0));
  CHECK(z.composition.width==widths[a-1]&&z.candidates.width==30*b-2);
  CHECK(z.body.y==22&&z.body.height==356&&(z.body.width+6)/34>=2);
 }
 SwitchRig r;r.toggle();r.draw();CHECK(capture(r).s.empty()&&r.vertical_frame.layout.selected.width==0);
}
void R02(){
 SwitchRig r;fullPinyin(r);r.toggle();r.draw();const auto id=r.app.document.cursorByte();
 r.type("b");
 for(unsigned length:{1U,2U,3U,4U,3U,2U,1U,5U,6U,4U,3U,1U}){
  while(r.ime.raw().size()>length)r.tap(42);
  while(r.ime.raw().size()<length)r.type("b");
  r.settle();CHECK(r.app.compositionWidth()<=compositionOraclePeak("bbbbbb"));
  const auto c=capture(r);CHECK(c.a.size()==1&&c.a[0].size()==length);
  const auto& box=c.boxes[0];CHECK(box.x==r.vertical_frame.layout.composition.x+2);
  CHECK(box.x+box.width<=r.vertical_frame.layout.composition.x+r.vertical_frame.layout.composition.width-2);
  CHECK(c.types[0]==(length>=6?s3app::CompositionTypography::Compact6:length==5?s3app::CompositionTypography::Compact5:s3app::CompositionTypography::Normal));
  CHECK(r.app.document.cursorByte()==id);
 }
 CHECK(r.app.compositionWidth()==compositionOraclePeak("bbbbbb"));r.ime.interrupt();CHECK(r.app.compositionWidth()==6);
}
void R03(){SwitchRig r;r.begin("ui");CHECK(r.ime.view()->rows[0].count==5);const auto before=focused(r.ime);const auto query=r.ime.window()->querySerial();const auto io=metrics.io();r.toggle();r.draw();CHECK(r.ime.view()->rows[0].count==6);CHECK(focused(r.ime).key==before.key&&focused(r.ime).ordinal==before.ordinal);CHECK(r.ime.window()->querySerial()==query);std::cout<<"REPACK_IO reads="<<(metrics.io()-io).reads<<'\n';for(unsigned i=0;i<6;++i)CHECK(r.numbers[i].ordinal==i);
 r.tap(81);r.settle();CHECK(focused(r.ime).ordinal==1);const auto key=focused(r.ime).key;
 for(unsigned n=0;n<10;++n){r.toggle();r.draw();CHECK(focused(r.ime).key==key);CHECK(r.ime.window()->querySerial()==query);}
}
void R04(){SwitchRig r;r.toggle();r.begin("ui");const auto focus=focused(r.ime).key;r.tap(80);CHECK(!r.ime.window()->browsingExpanded());r.settle();CHECK(r.ime.view()->count==3&&r.vertical_frame.layout.candidate_content_rows==3&&r.ime.window()->browsingExpanded());CHECK(focused(r.ime).key==focus);for(unsigned n=0;n<8;++n){r.tap(81);r.settle();}CHECK(r.ime.view()->row==0&&r.ime.view()->col==5);r.tap(45);r.settle();CHECK(r.ime.view()->row==1);r.tap(46);r.settle();CHECK(r.ime.view()->row==0);r.tap(79);r.settle();CHECK(r.ime.view()->row==0);r.tap(41);r.settle();CHECK(r.ime.rawEditing()&&r.ime.window()->browsingExpanded());r.tap(42);r.type("i");r.settle();CHECK(r.ime.window()->browsingExpanded());r.ime.interrupt();CHECK(!r.ime.window()->browsingExpanded());}
void R05(){SwitchRig r;r.toggle();r.begin("ui");for(unsigned n=0;n<5;++n){r.tap(81);r.settle();}CHECK(r.ime.view()->col==5&&!r.ime.expanded());const auto item=focused(r.ime);r.tap(81);r.settle();CHECK(r.ime.expanded()&&r.ime.view()->count==3&&focused(r.ime).key==item.key);}
void R06(){SwitchRig r;r.body("我今天吃饭了");r.move(9);fullPinyin(r);r.type("wochifanle");r.settle();r.toggle();r.draw();const BodyFact body(r.app);r.tap(41);r.settle();CHECK(r.ime.rawEditing());const auto start=r.ime.rawCursor();r.tap(82);CHECK(r.ime.rawCursor()<start);const auto back=r.ime.rawCursor();r.tap(79);CHECK(r.ime.rawCursor()==back);r.tap(81);CHECK(r.ime.rawCursor()==start);r.toggle();r.draw();r.toggle();r.draw();CHECK(r.ime.rawCursor()==start);r.tap(80);r.settle();body.check(r.app);CHECK(!r.ime.rawEditing());}
void R07(){
 // Low-level prefix fixture retained for identity/F4 coverage; real HID
 // prefix navigation and atomic partial transition are R2 R-B1 and R-C1.
 SwitchRig r;fullPinyin(r);r.type("woshangwu");r.settle();r.toggle();r.draw();
 r.tap(80);r.settle();r.ime.window()->query(2);r.settle();r.tap(30);r.settle();
 CHECK(!r.ime.selected().empty()&&r.app.document.text().empty());
 const auto c=capture(r);CHECK(c.s.size()==1&&c.s[0]==r.ime.selected());
 CHECK(r.app.compositionWidth()==compositionOraclePeak("shang")&&!r.ime.window()->browsingExpanded());
 const auto selected=std::string(r.ime.selected());const auto item=focused(r.ime);
 r.toggle();r.draw();r.toggle();r.draw();
 CHECK(r.ime.selected()==selected&&focused(r.ime).key==item.key);
}
void R08(){SwitchRig r;r.body("我今天吃饭了");r.move(9);fullPinyin(r);r.type("hao");r.settle();r.toggle();r.draw();const BodyFact before(r.app);r.tap(80);r.settle();before.check(r.app);size_t good=0;for(;good<r.numbers.size();++good)if(std::string_view(r.numbers[good].text.data(),r.numbers[good].bytes)=="好")break;CHECK(good<r.numbers.size());r.tap(30+good);r.settle();CHECK(r.app.document.text()=="我今天好吃饭了"&&r.app.document.cursorByte()==12);CHECK(r.app.compositionWidth()==6&&!r.ime.window()->browsingExpanded());}

void R09(){for(unsigned digit=1;digit<=6;++digit){SwitchRig r;r.toggle();r.begin("ui");CHECK(r.numbers.size()==6);const auto chosen=r.numbers[digit-1];r.tap(29+digit);r.settle();CHECK(r.app.document.text()==std::string_view(chosen.text.data(),chosen.bytes));CHECK(r.app.editor.undoCount()==1&&r.ime.raw().empty());}}
void R10(){SwitchRig r;fullPinyin(r);r.type("woshangwuchifanle");r.settle();r.toggle();r.draw();CHECK((capture(r).a==std::vector<std::string>{"wo","shang","wu","chi","fan","le"}));r.tap(80);r.settle();r.ime.window()->query(2);r.settle();r.tap(30);r.settle();const auto one=std::string(r.ime.selected());CHECK(!one.empty());const auto snapshot=r.ime.composition()->snapshot();r.ime.window()->query(7);r.settle();r.tap(30);r.settle();CHECK(r.ime.selected().size()>one.size()&&r.app.document.text().empty());const auto runs=capture(r);std::string assembled;for(const auto& member:runs.s)assembled+=member;CHECK(assembled==r.ime.selected());CHECK(r.ime.composition()->snapshot().member_count>snapshot.member_count);}
void R11(){
 SelectedViewport viewport;class Axis final:public s3host::FontPort{public:s3host::GlyphView glyph(uint32_t cp)const noexcept override{return {nullptr,0,0,0,uint16_t(cp<128?14:28),0,false};}uint16_t lineHeight()const noexcept override{return 28;}uint16_t defaultAdvance()const noexcept override{return 28;}} font;
 std::string text;for(unsigned i=0;i<24;++i)text+="甲";
 hanime_extension::Snapshot s;s.selected_text=text;auto view=viewport.update(s,0,false,font,356);CHECK(view.begin>0&&view.end==text.size()&&view.left&&!view.right);CHECK((view.end-view.begin)/3==12);
}
void R12(){SwitchRig r;
 fullPinyin(r);
 r.type("shuang");
 r.settle();
 CHECK(r.app.compositionWidth()==compositionOraclePeak("shuang"));
 r.toggle();
 r.draw();
 CHECK(capture(r).types[0]==s3app::CompositionTypography::Compact6);
 r.toggle();
 r.draw();
 r.tap(42);
 r.tap(42);
 r.settle();
 CHECK(r.app.compositionWidth()==compositionOraclePeak("shuang"));
 r.toggle();
 r.draw();
 for(auto type:capture(r).types)CHECK(type==s3app::CompositionTypography::Normal);
 CHECK(r.vertical_frame.layout.composition.width==54);
 r.app.observeCompositionWidth(0);
 CHECK(r.app.compositionWidth()==compositionOraclePeak("shuang"));
 }
void R13(){SwitchRig r;r.toggle();r.begin("ui");r.tap(80);r.settle();for(unsigned i=0;i<8;++i){r.tap(45);r.settle();}const auto identity=focused(r.ime);const auto query=r.ime.window()->querySerial();for(unsigned n=0;n<6;++n){r.toggle();r.draw();CHECK(focused(r.ime).key==identity.key&&focused(r.ime).ordinal==identity.ordinal&&r.ime.window()->querySerial()==query&&r.ime.expanded());}CHECK(r.ime.window()->costs().prefetch_starts==0);}
void R14(){const s3host::BuiltinSmallFont font;for(const char* mode:{"EN | SAVED","CN XH | UNSAVED","CN PY | UNSAVED"}){const auto line=s3app::verticalTop(mode,"Untitled",font);CHECK(!line.shortened&&line.width<=284);CHECK(292-line.width>=8);}for(const char* status:{"CAND up/down | -/= cols | Esc","EDIT up/down | Left load | Esc","Unknown: retained, no replay","Rejected: Space retry / Esc edit","Pair: type 123456 + Enter"}){s3app::VerticalHudLine line;s3app::hudAppend(line,status,font);CHECK(!line.shortened&&line.width<=284);std::cout<<"HUD "<<line.width<<' '<<line.text<<'\n';}const auto longname=s3app::verticalTop("CN PY | UNSAVED",std::string(96,'w'),font);CHECK(longname.shortened&&longname.width<=284);CHECK(v::editor_area.y>=18&&v::editor_area.y+v::editor_area.height<=383);}
void R15(){SwitchRig r;r.toggle();r.begin("ui");r.tap(80);r.settle();unsigned last=0;for(;last<100;++last){r.ime.window()->request(last,0,true);r.settle();const auto& v=*r.ime.view();if(v.at_end&&v.row==v.rows[v.count-1].index)break;}CHECK(last<100&&r.ime.window()->browsingExpanded());CHECK(r.vertical_frame.layout.candidates.width==30*r.ime.view()->count-2&&r.vertical_frame.layout.candidate_content_rows<=2);const auto identity=focused(r.ime).key;r.tap(45);r.settle();CHECK(focused(r.ime).key==identity);
 const auto query=r.ime.window()->querySerial();
 for(unsigned i=0;i<6;++i){r.toggle();r.draw();CHECK(focused(r.ime).key==identity&&r.ime.window()->querySerial()==query);}
}

void R16(){SwitchRig r;
 fullPinyin(r);
 std::string raw;
 for(unsigned i=0;i<14;++i)raw+="wo";
 raw+="shang";
 r.type(raw);
 r.settle();
 r.toggle();
 r.draw();
 r.tap(80);
 r.settle();
 for(unsigned i=0;i<14;++i){r.ime.window()->query(2*(i+1));
 r.settle();
 r.tap(30);
 r.settle();
 CHECK(r.app.document.text().empty());
 }const auto all=std::string(r.ime.selected());
 CHECK(all.size()>=42);
 auto c=capture(r);
 std::string visible;
 for(const auto& text:c.s)visible+=text;
 CHECK(visible.size()==36&&visible==all.substr(all.size()-36));
 CHECK(r.vertical_frame.layout.selected.width==28&&!r.ime.window()->browsingExpanded());
 r.tap(41);
 r.settle();
 for(unsigned i=0;i<13;++i)r.tap(82);
 r.draw();
 c=capture(r);
 visible.clear();
 for(const auto& text:c.s)visible+=text;
 CHECK(!visible.empty()&&all.find(visible)!=std::string::npos);
 r.tap(80);
 r.settle();
 CHECK(r.app.document.text().empty());
 c=capture(r);
 visible.clear();
 for(const auto& text:c.s)visible+=text;
 CHECK(r.ime.selected().find(visible)!=std::string_view::npos);
 }
void R17(){SwitchRig r;fullPinyin(r);r.type("mingtianhao");r.settle();r.ime.window()->query(8);r.settle();r.toggle();r.draw();std::size_t match=0;for(;match<r.numbers.size();++match)if(std::string_view(r.numbers[match].text.data(),r.numbers[match].bytes)=="明天")break;CHECK(match<r.numbers.size());r.tap(30+match);r.settle();CHECK(r.ime.selected()=="明天"&&r.app.document.text().empty());CHECK((capture(r).s==std::vector<std::string>{"明天"}));}
void R18(){SwitchRig r;r.begin("ui");r.toggle();for(unsigned i=0;i<5;++i)r.tap(81);r.settle();CHECK(r.ime.view()->col==5&&focused(r.ime).ordinal==5&&!r.ime.expanded());r.toggle();r.tap(79);r.settle();CHECK(focused(r.ime).ordinal==6);}
int main(int argc,char** argv){try{CHECK(argc==2);const std::pair<const char*,void(*)()> tests[]={{"R01",R01},{"R02",R02},{"R03",R03},{"R04",R04},{"R05",R05},{"R06",R06},{"R07",R07},{"R08",R08},{"R09",R09},{"R10",R10},{"R11",R11},{"R12",R12},{"R13",R13},{"R14",R14},{"R15",R15},{"R16",R16},{"R17",R17},{"R18",R18}};for(const auto& test:tests)if(std::string_view(argv[1])==test.first){test.second();std::cout<<"PASS "<<test.first<<'\n';return 0;}CHECK(false);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
