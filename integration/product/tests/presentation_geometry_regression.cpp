#define main inherited_query_rows_main
#include "query_rows.cpp"
#undef main
#include "vertical_view.hpp"
#include "presentation_frame.hpp"
#include "product_input.hpp"
namespace v=draftor::vertical;
struct PendingRig:Rig {
 s3app::ProductInput input;unsigned previews=0,numbers=0,red=0;
 void tap(uint8_t key,uint8_t mods=0){for(bool down:{true,false}){s3hid::Event e{1,++now,key,mods,down,0,frame,ime.attemptSerial()};e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();if(input.layout(app,e))continue;if(!input.ime(app,&ime,e))app.key(e);}}
 void vertical(){for(bool down:{true,false})input.layout(app,{1,++now,61,0,down});}
 VerticalImeFrame paint(){
  CHECK(prepareImePresentation(&ime,app));previews=numbers=red=0;
  const s3app::VerticalPaintPort port{[](void*,v::Rect,s3app::VerticalColor){},[](void*,std::uint16_t,std::uint16_t,s3app::VerticalColor){},this,
   [](void* p,const v::AbRun& run){auto& r=*static_cast<PendingRig*>(p);if(run.role==v::AbRole::Preview){++r.previews;CHECK(!run.identity);r.numbers+=run.number!=0;r.red+=run.focused;}}};
  auto f=s3app::paintVerticalView(app,app.verticalPresentation(),&ime,font,now,port);CHECK(f.status==v::Status::Ready);return f.ime;
 }
 void complete(const VerticalImeFrame& f){auto p=publishActivePresentation(app,&ime,app.presentationFrame(),f);CHECK(p.accepted);frame=p.candidates;app.candidateVisual.complete(true);}
};
#include <sstream>
using GS=s3app::VerticalImeGeometryState;
struct GeometryRig:PendingRig {
 std::string focus(){auto& p=app.verticalPresentation();editor::presentation::ViewportState a;CHECK(p.captureViewport(a));auto c=p.viewport().caretRect(p.caret().caret);std::ostringstream s;s<<a.byte_offset<<','<<int(a.affinity)<<','<<p.viewport().firstColumn()<<','<<c.x<<','<<c.y;return s.str();}
 void base(){complete(paint());const auto r=v::reservedAbRegions(app.imeGeometry());CHECK(app.verticalImeGeometryState()==GS::ActiveBase);CHECK(r.composition.width==54&&r.candidates.width==28&&r.selected.width==28);CHECK(r.body.x==124&&r.body.width==168);}
 void idle(){complete(paint());const auto r=v::reservedAbRegions(app.imeGeometry());CHECK(app.verticalImeGeometryState()==GS::Idle);if(app.verticalPresentationState()==s3app::VerticalPresentationState::ChineseBase){CHECK(r.composition.width==54&&r.candidates.width==28&&r.selected.width==28&&r.body.x==124&&r.body.width==168);}else{CHECK(r.composition.width==6&&r.candidates.width==28&&r.selected.width==0&&r.body.x==46&&r.body.width==246);}}
};
int main(){try{
 // The first admitted letter owns space before resources/composition exist.
 {GeometryRig r;r.vertical();r.idle();r.tap(57);r.type("u");CHECK(!r.ime.composition());r.base();r.settle();r.base();r.ime.interrupt();r.idle();}
 for(bool py:{false,true}){
  GeometryRig r;CHECK(r.app.commitText("天地玄黄宇宙洪荒日月盈昃",r.app.editStamp()));r.app.moveToByte(9,false);r.tap(57);r.settle();if(py){r.tap(44,1);r.settle();}r.vertical();r.idle();
  r.type(py?"s":"u");r.base();auto anchor=r.focus();
  for(char c:std::string(py?"huang":"i")){r.type(std::string(1,c));r.base();CHECK(r.focus()==anchor);r.settle();r.base();CHECK(r.focus()==anchor);}
  CHECK(r.app.candidateVisual.valid);r.type(py?"h":"w");r.base();CHECK(r.app.candidateVisual.pending());CHECK(r.focus()==anchor);r.settle();r.base();CHECK(r.focus()==anchor);
  r.tap(42);r.base();CHECK(r.focus()==anchor);r.settle();r.base();CHECK(r.focus()==anchor);
  const auto oldOwner=r.ime.resourceOwner();const auto context=r.ime.ingressContext();s3hid::Event delayed{1,++r.now,5,0,true,0,r.frame,r.ime.attemptSerial(),0,0,context,oldOwner};auto up=delayed;up.down=false;up.ms=++r.now;const auto document=std::string(r.app.document.text());const auto revision=r.app.document.textRevision();r.tap(44,1);r.settle();CHECK(r.resources.rules()==(py?hanime_extension::SchemeRules::DirectExact:hanime_extension::SchemeRules::FullPinyin)&&r.ime.resourceOwner()!=oldOwner&&r.resources.ready());r.idle();CHECK(r.ime.event(delayed)&&r.ime.event(up));CHECK(delayed.resource_owner==oldOwner&&up.resource_owner==oldOwner&&r.ime.raw().empty()&&!r.ime.hasWork()&&r.app.document.text()==document.c_str()&&r.app.document.textRevision()==revision);std::cout<<"MIGRATION geometry from_py="<<py<<" next="<<unsigned(r.resources.rules())<<" old_owner="<<oldOwner<<" new_owner="<<r.ime.resourceOwner()<<" stale_rejected=1\n"; // Scheme switch is a real terminal.
 }
 // A fully selected composition is still active until editor acceptance.
 {GeometryRig r;r.begin(true,"wohao",false);r.vertical();r.settle();r.base();r.ime.window()->query(2);r.settle();r.base();auto anchor=r.focus();
  CHECK(r.ime.window()->choose(0,r.frame)==ChoiceResult::Captured);r.settle();r.base();CHECK(!r.ime.selected().empty()&&r.app.selectedAppeared());CHECK(r.focus()==anchor);
  CHECK(r.ime.window()->choose(0,r.frame)==ChoiceResult::Captured);r.settle();r.base();CHECK(r.ime.composition()->snapshot().state==hanime_extension::CompositionState::AllSelected);CHECK(r.focus()==anchor);
  r.tap(40);r.settle();r.idle();CHECK(!r.app.document.text().empty());
 }
 {GeometryRig r;r.begin(false,"ui",false);r.vertical();r.settle();r.base();r.tap(42);r.base();r.tap(42);r.idle();}
 // Semantic latching is independent of presentation facts, and H keeps its peak.
 {GeometryRig r;r.app.beginCompositionPresentation(true);r.app.observeComposition(true,31,false);r.app.candidateColumns(3);r.app.observeComposition(true,0,false);CHECK(r.app.verticalImeGeometryState()==GS::Expanded);CHECK(r.app.imeGeometry().composition_width==31);r.app.candidateColumns(1);CHECK(r.app.verticalImeGeometryState()==GS::ActiveBase);r.app.endCompositionPresentation();CHECK(r.app.verticalImeGeometryState()==GS::Idle);}
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 std::cout<<"PASS presentation_geometry_regression buffered_first_key=1 PY_XH=1 pending_ready_focus=1 first_S=1 all_selected_not_terminal=1 clear_scheme_commit=1 H_peak_preserved=1\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
