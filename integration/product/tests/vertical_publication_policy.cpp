// Host-only receipt regression through real App/DeviceIme/Window and V paint.
#define main inherited_query_rows_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"
#include "vertical_view.hpp"
#include "presentation_frame.hpp"
namespace v=draftor::vertical;
struct PublicationRig:Rig {
 s3app::ProductInput input;
 std::array<std::uint8_t,300*400> pixels{};
 unsigned numbered=0,focused=0;
 void drain(){unsigned n=0;do{step();CHECK(++n<2000000);}while(!resources.ready()||!ime.composition()||ime.hasWork());}
 void vertical(){for(bool down:{true,false})CHECK(input.layout(app,{1,++now,61,0,down}));}
 VerticalImeFrame paint(){
  CHECK(prepareImePresentation(&ime,app));pixels.fill(0);numbered=focused=0;
  const s3app::VerticalPaintPort port{
   [](void* p,v::Rect b,s3app::VerticalColor c){auto& r=*static_cast<PublicationRig*>(p);for(unsigned y=b.y;y<unsigned(b.y)+b.height;++y)for(unsigned x=b.x;x<unsigned(b.x)+b.width;++x)if(x<300&&y<400)r.pixels[y*300+x]=std::uint8_t(c);},
   [](void* p,std::uint16_t x,std::uint16_t y,s3app::VerticalColor c){if(x<300&&y<400)static_cast<PublicationRig*>(p)->pixels[y*300+x]=std::uint8_t(c);},this,
   [](void* p,const v::AbRun& run){auto& r=*static_cast<PublicationRig*>(p);if(run.role==v::AbRole::CurrentCandidate){++r.numbered;if(run.focused)++r.focused;}}
  };
  const auto out=s3app::paintVerticalView(app,app.verticalPresentation(),&ime,font,now,port);
  CHECK(out.status==v::Status::Ready&&out.ime.layout.status==v::AbStatus::Ready);
  return out.ime;
 }
 void begin(){
  tap(57);drain();tap(44,1);drain();vertical();
  CHECK(prepareImePresentation(&ime,app));type("ru");drain();
  auto f=paint();CHECK(f.candidate_frame&&f.generation!=0&&numbered>0);
  CHECK(publishVerticalFrame(&ime,app,f)==f.generation);frame=f.generation;
 }
 VerticalImeFrame empty(){
  begin();type("g");
  CHECK(ime.view()->generation==0&&ime.view()->count==0);
  CHECK(ime.window()->onScreenGeneration()!=0&&candidatePublicationPending(&ime,&app));
  auto f=paint();CHECK(!f.candidate_frame&&f.generation==0&&numbered==0&&focused==0);return f;
 }
 void reject(const VerticalImeFrame& f){
  const auto before=ime.window()->onScreenGeneration();
  CHECK(publishVerticalFrame(&ime,app,f)==0);
  CHECK(ime.window()->onScreenGeneration()==before);
 }
 void noSelection(const VerticalImeFrame& f,bool digit=false){
  const auto revision=app.document.textRevision();
  VerticalChoiceTarget target{v::AbRole::CurrentCandidate,1,f};
  CHECK(chooseVertical(*ime.window(),target)==ChoiceResult::NoCandidate);
  s3hid::Event e{1,++now,30,0,true,0,f.generation,ime.attemptSerial()};e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();
  CHECK(dispatchVerticalNumber(ime,app,target,e)==VerticalDispatch::RejectedTarget);
  if(digit)tap(30);
  CHECK(app.document.textRevision()==revision&&!ime.window()->selecting());
 }
};
void emptyStorm(){
 PublicationRig r;const auto f=r.empty();r.noSelection(f,true);
 const auto old=r.ime.window()->onScreenGeneration();
 // Exact old eligibility gate: the same successfully painted surface never
 // acknowledges its receipt. The framebuffer remains identical on repeats.
 for(unsigned i=0;i<8;++i){const auto pixels=r.pixels;auto repeat=r.paint();
  if(repeat.candidate_frame)(void)publishVerticalFrame(&r.ime,r.app,repeat);
  CHECK(r.pixels==pixels&&candidatePublicationPending(&r.ime,&r.app));
  CHECK(r.ime.window()->onScreenGeneration()==old);
 }
 const auto result=publishActivePresentation(r.app,&r.ime,r.app.presentationFrame(),f);
 CHECK(result.accepted&&result.candidates==0);
 CHECK(r.ime.window()->onScreenGeneration()==0&&!candidatePublicationPending(&r.ime,&r.app));
 CHECK(!r.ime.window()->presented(0));r.noSelection(f,true);
 std::cout<<"PASS empty_storm old_gate_repeats=8 unchanged_pixels=8 fixed_publications=1 pending=0 selection=0\n";
}
void nonemptyUnauthorized(){
 PublicationRig r;r.begin();r.type("u");r.drain();CHECK(r.ime.view()->count>0&&candidatePublicationPending(&r.ime,&r.app));
 auto f=r.paint();CHECK(f.candidate_frame);f.candidate_frame=false;r.reject(f);r.noSelection(f);
 // A gen0 view with nonzero count is not a legal empty surface. Deliberate
 // Host-only malformed-state injection, restored before further IME work.
 PublicationRig empty;auto e=empty.empty();auto& view=const_cast<CandidateView&>(*empty.ime.view());view.count=1;
 empty.reject(e);view.count=0;
 std::cout<<"PASS nonempty_unauthorized and malformed_gen0_nonempty rejected\n";
}
void selectable(){
 PublicationRig r;r.begin();r.type("u");r.drain();auto f=r.paint();CHECK(f.candidate_frame&&r.numbered>0);
 CHECK(publishVerticalFrame(&r.ime,r.app,f)==f.generation&&!candidatePublicationPending(&r.ime,&r.app));
 VerticalChoiceTarget target{v::AbRole::CurrentCandidate,1,f};
 CHECK(chooseVertical(*r.ime.window(),target)==ChoiceResult::Captured);
 std::cout<<"PASS selectable_nonempty publication_and_choice\n";
}
void stale(){
 PublicationRig r;r.begin();const auto old=r.paint();r.type("u");r.drain();
 CHECK(r.ime.view()->generation!=old.generation);r.reject(old);
 const auto fresh=r.paint();CHECK(publishVerticalFrame(&r.ime,r.app,fresh)==fresh.generation);r.reject(old);
 CHECK(chooseVertical(*r.ime.window(),{v::AbRole::CurrentCandidate,1,old})==ChoiceResult::ExpiredFrame);
 std::cout<<"PASS retired_stale_generation rejected\n";
}
void mismatches(){
 PublicationRig r;const auto f=r.empty();auto bad=f;++bad.input;r.reject(bad);bad=f;++bad.revision;r.reject(bad);
 std::cout<<"PASS input_and_revision_mismatch rejected\n";
}
void generationAfterPaint(){
 PublicationRig r;const auto f=r.empty();const auto old=r.ime.window()->onScreenGeneration();r.drain();
 CHECK(r.ime.view()->generation!=0&&r.ime.view()->generation!=old&&r.ime.view()->count>0);
 r.reject(f);CHECK(candidatePublicationPending(&r.ime,&r.app));
 const auto fresh=r.paint();CHECK(publishVerticalFrame(&r.ime,r.app,fresh)==fresh.generation);
 r.reject(f);CHECK(r.ime.window()->onScreenGeneration()==fresh.generation);
 std::cout<<"PASS generation_changed_after_paint new_candidates_not_retired\n";
}
void invalidLayout(){
 PublicationRig r;auto f=r.empty();auto geometry=r.app.imeGeometry();geometry.body_cell=0;
 f.layout=v::reservedAbRegions(geometry);CHECK(f.layout.status==v::AbStatus::InvalidInput);r.reject(f);
 f.layout.status=v::AbStatus::Stale;r.reject(f);
 std::cout<<"PASS invalid_geometry_and_layout_not_ready rejected\n";
}
void barrier(){
 PublicationRig r;const auto f=r.empty();r.app.deferFileCommands(true);r.app.key({1,++r.now,18,8,true});
 CHECK(r.app.commandBarrier()&&candidateSurfaceVisible(&r.ime,&r.app));r.reject(f);
 std::cout<<"PASS command_barrier rejected\n";
}
void hidden(){
 PublicationRig r;const auto f=r.empty();r.app.key({1,++r.now,9,1,true});
 CHECK(!candidateSurfaceVisible(&r.ime,&r.app));r.reject(f);
 CHECK(publishVerticalFrame(nullptr,r.app,f)==0);
 std::cout<<"PASS hidden_surface_and_null_ime rejected\n";
}
int main(int argc,char** argv){try{
 const std::pair<const char*,void(*)()> tests[]={{"empty",emptyStorm},{"unauthorized",nonemptyUnauthorized},{"selectable",selectable},{"stale",stale},{"mismatch",mismatches},{"race",generationAfterPaint},{"layout",invalidLayout},{"barrier",barrier},{"hidden",hidden}};
 unsigned ran=0;for(const auto& t:tests)if(argc==1||std::string_view(argv[1])==t.first){t.second();++ran;CHECK(metrics.resource_handles==0&&metrics.document_handles==0);}
 CHECK(ran>0);std::cout<<"PASS vertical_publication_policy cases="<<ran<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
