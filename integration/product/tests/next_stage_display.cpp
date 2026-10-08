#define main historical_main
#include "query_rows.cpp"
#undef main
#include "ime_presentation.hpp"
void spacing(){
 s3host::BuiltinSmallFont font;
 const std::array<const char*,4> py_cases{{"wandanle","xian","wandanl","abc"}};
 const std::array<const char*,4> xh_cases{{"wjdjle","wjdjl","wjddjle","abcdefghijklmnopqrstuvwxyza"}};
 for(bool py:{false,true})for(const char* raw:(py?py_cases:xh_cases)){
  Rig r;r.tap(0x39);r.settle();if(py){r.tap(44,1);r.settle();}r.type(raw);r.settle();hanime_extension::SyllableNavigator nav;auto h=nav.analyze(*r.ime.composition());auto n=nav.view(h.handle,*r.ime.composition());
  CHECK(n.status==hanime_extension::NavigationStatus::Ready);
  const auto io=metrics.io();const auto stamp=r.ime.composition()->raw_input().stamp;const auto page=r.ime.composition()->page();
  const auto display=r.ime.editViewport(font);unsigned expected=0;
  for(size_t i=0;i<r.ime.raw().size();++i){if(i&&std::find(n.boundaries,n.boundaries+n.boundary_count,i)!=n.boundaries+n.boundary_count)expected+=3;CHECK(display.x[i]==expected);expected+=font.glyph(raw[i]).advance;}
  CHECK(display.x[r.ime.raw().size()]==expected);
  for(unsigned pixels:{1u,5u,16u,32u,80u,284u})for(size_t c=0;c<=r.ime.raw().size();++c){
   const auto v=rawViewport(r.ime.raw(),c,true,font,pixels,n.boundaries,n.boundary_count);
   CHECK(v.begin<=c&&v.end>=c&&v.cursor_x<pixels&&v.width<pixels);
   CHECK(v.cursor_x==v.at(c)&&v.width==v.at(v.end));
  }
  CHECK((metrics.io()-io).reads==0&&r.ime.composition()->input_is_current(stamp)&&r.ime.composition()->page().generation==page.generation);
  CHECK(r.ime.raw()==raw);
 }
 // 64-byte tail and every caret position, including single-letter edit positions.
 std::string raw(64,'w');uint16_t bounds[33];for(unsigned i=0;i<33;++i)bounds[i]=2*i;
 for(size_t c=0;c<=64;++c){const auto v=rawViewport(raw,c,true,font,284,bounds,33);CHECK(v.begin<=c&&v.end>=c&&v.cursor_x<284);}
 std::cout<<"PASS A1 gap=3 font_space=5 raw_offsets_unchanged bounds/public nav, ambiguous/incomplete/narrow\n";
}
void firstRowYield(){
 Rig r;r.tap(0x39);r.settle();r.type("ui");r.tap(0x51);
 unsigned n=0;while(!candidateWorkShouldYield(&r.ime,&r.app)){r.step();CHECK(++n<2000000);}
 CHECK(r.ime.view()->count==1&&r.ime.window()->busy());
 const auto io=metrics.io();const auto first=*r.ime.view();
 r.frame=publishVisibleCandidate(&r.ime,&r.app);
 CHECK(r.frame==first.generation&&!candidateWorkShouldYield(&r.ime,&r.app)&&(metrics.io()-io).reads==0);
 r.settle();CHECK(r.ime.view()->count==3&&r.ime.view()->row==1);
 std::cout<<"PASS A4 collapsed first row published before pending expansion I/O; identities retained\n";
}
void cancelled(bool repaint,uint8_t key){
 Rig r;r.begin(false,"wjdjle",false);r.tap(0x50);r.tap(0x50);r.tap(0x51);r.settle();
 const auto raw=std::string(r.ime.raw());r.tap(0x29);CHECK(r.ime.rawEditing());
 if(repaint)r.settle(); // Also test old LCD frame still visible immediately after Esc.
 const auto query=r.ime.window()->querySerial();r.tap(0x50);r.tap(0x4f);r.tap(key);
 CHECK(r.ime.rawEditing()&&r.ime.raw()==raw&&r.ime.selected().empty()&&r.app.document.text().empty());
 CHECK(r.ime.window()->querySerial()==query&&!r.ime.hasWork()&&!r.ime.candidateAvailable());
 CHECK(std::string(r.ime.notice()).find("Down")!=std::string::npos);
 char caption[64];r.ime.editCaption(caption,sizeof(caption));CHECK(std::string(caption).find("Down")!=std::string::npos);
 r.tap(0x51);r.settle();CHECK(r.ime.candidateAvailable());r.tap(key);r.settle();CHECK(!r.ime.selected().empty());
 std::cout<<"PASS A2 Esc cancelled authority repaint="<<repaint<<" key="<<unsigned(key)<<'\n';
}
int main(){try{spacing();firstRowYield();for(bool p:{false,true})for(uint8_t k:{uint8_t(30),uint8_t(44)})cancelled(p,k);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
