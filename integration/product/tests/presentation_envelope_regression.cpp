#define main inherited_query_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"
#include "vertical_ime.hpp"
namespace v=draftor::vertical;
using PS=s3app::VerticalPresentationState;
struct EnvelopeRig:Rig {
 s3app::ProductInput input;
 void tap(uint8_t key,uint8_t mods=0){for(bool down:{true,false}){s3hid::Event e{1,++now,key,mods,down,0,frame,ime.attemptSerial()};e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();if(input.layout(app,e))continue;if(!input.ime(app,&ime,e))app.key(e);}}
 void type(std::string_view s){for(char c:s)tap(uint8_t(c-'a'+4));}
 void base(){CHECK(prepareImePresentation(&ime,app));auto g=v::reservedAbRegions(app.imeGeometry());CHECK(app.verticalPresentationState()==PS::ChineseBase);CHECK(g.body.x==124&&g.body.width==168&&g.composition.width==54&&g.selected.width==28&&g.candidates.width==28);}
 void cn(){tap(57);settle();tap(44,1);settle();tap(61);base();}
};
void anchorEdges(){
 for(unsigned edge=0;edge<5;++edge)for(size_t offset:{0U,9U,36U}){
  EnvelopeRig r;std::string body;for(unsigned n=0;n<12;++n)body+="天地玄黄宇宙洪荒日月盈昃\n";
  CHECK(r.app.commitText(body,r.app.editStamp()));r.app.moveToByte(8*37+offset,false);r.cn();
  auto& p=r.app.verticalPresentation();CHECK(p.placeCaretSlot(edge));const auto origin=p.viewport().caretRect(p.caret().caret);const auto cursor=r.app.document.cursorByte();
  for(unsigned cols:{2U,3U,2U,1U}){
   r.app.candidateColumns(cols);CHECK(r.app.preparePresentation());auto c=p.caret();auto actual=p.viewport().caretRect(c.caret);unsigned max=std::min<unsigned>(p.viewport().visibleColumns()-1,c.caret.column);unsigned wanted=std::min(edge,max);
   std::cout<<"ANCHOR edge="<<edge<<" offset="<<offset<<" cols="<<cols<<" origin="<<origin.x<<","<<origin.y<<" actual="<<actual.x<<","<<actual.y<<" cap="<<p.viewport().visibleColumns()<<" first="<<p.viewport().firstColumn()<<" column="<<c.caret.column<<" expected_slot="<<wanted<<std::endl;   CHECK(actual.x==origin.x+int(edge-wanted)*34&&actual.y==origin.y);CHECK(r.app.document.cursorByte()==cursor);CHECK(c.caret.column-p.viewport().firstColumn()==wanted);
  }
  CHECK(p.viewport().caretRect(p.caret().caret).x==origin.x);
 }
}
int main(){try{
 {EnvelopeRig r;r.tap(61);r.tap(57);r.base();r.type("u");CHECK(!r.ime.composition());r.base();r.settle();r.base();}
 {EnvelopeRig r;r.cn();for(const char* word:{"ruguo","wogen","ruguo"}){
   for(char c:std::string(word)){r.type(std::string(1,c));r.base();r.settle();r.base();}
   r.tap(44);r.settle();r.base();CHECK(r.ime.raw().empty()&&!r.ime.active()&&!r.ime.candidateAvailable());
   for(unsigned n=0;n<32;++n){r.step();r.base();}
  }
  const auto before=r.app.document.cursorByte();r.tap(82);r.base();CHECK(r.app.document.cursorByte()!=before);r.tap(42);r.base();
  r.tap(61);CHECK(prepareImePresentation(&r.ime,r.app));CHECK(r.app.horizontalActive());r.tap(61);r.base();
  r.tap(57);CHECK(prepareImePresentation(&r.ime,r.app));CHECK(r.app.verticalPresentationState()==PS::NonChinese);CHECK(v::reservedAbRegions(r.app.imeGeometry()).body.x==46);
 }
 anchorEdges();
 // Context loss, not composition completion, releases the envelope.
 {EnvelopeRig r;r.cn();r.app.chinesePresentationContext(false);CHECK(r.app.preparePresentation());CHECK(r.app.verticalPresentationState()==PS::NonChinese);r.base();}
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 std::cout<<"PASS presentation_envelope_regression E1_E2_E3_E6_E7_E8_E9=1 anchor_edge_cases=15 transitions=60 cursor_unchanged=1 geometry_idle_separate=1\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
