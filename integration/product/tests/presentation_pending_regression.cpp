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
int main(){try{
 for(bool vertical:{false,true})for(unsigned key:{30U,44U,40U}){
  PendingRig r;r.begin(true,"ru",false);if(vertical){r.vertical();r.settle();}CHECK(prepareImePresentation(&r.ime,r.app));
  if(vertical)r.complete(r.paint());else{
   const auto& view=*r.ime.view();const auto& row=view.rows[0];unsigned x=8;
   for(std::size_t i=0;i<row.count;++i){const auto& item=row.items[i];r.app.candidateVisual.captureHorizontal({item.text.data(),item.bytes},x,326,item.width);x+=item.width;}
   r.frame=publishVisibleCandidate(&r.ime,&r.app);r.app.candidateVisual.complete(true);
  }
  CHECK(r.app.candidateVisual.valid);const auto old=r.frame;r.type("g");CHECK(prepareImePresentation(&r.ime,r.app));
  CHECK(r.ime.view()->generation==0&&r.app.candidateVisual.pending());const auto raw=r.ime.raw();const auto rev=r.app.document.textRevision();
  if(vertical){auto f=r.paint();CHECK(f.generation==0&&!f.candidate_frame&&r.previews);
   CHECK(r.numbers==0&&r.red==0);r.complete(f);
  }else{r.frame=publishVisibleCandidate(&r.ime,&r.app);r.app.candidateVisual.complete(true);}
  CHECK(r.ime.window()->onScreenGeneration()==0&&!r.ime.window()->presented(0));
  CHECK(r.ime.window()->choose(0,0)!=ChoiceResult::Captured);
  CHECK(r.ime.window()->choose(0,old)!=ChoiceResult::Captured);
  const auto count=r.app.candidateVisual.text.count;
  CHECK(prepareImePresentation(&r.ime,r.app));
  CHECK(!candidatePublicationPending(&r.ime,&r.app)&&r.app.candidateVisual.pending()&&r.app.candidateVisual.text.count==count);
  r.tap(std::uint8_t(key));CHECK(r.app.document.textRevision()==rev&&!r.ime.window()->selecting());
  CHECK(r.ime.raw()==raw); // No visual-cache text can be committed by these keys.
  r.app.candidateVisual.complete(false);CHECK(!r.app.candidateVisual.valid);
 }
 {PendingRig r;r.begin(true,"ru",false);r.vertical();r.settle();r.complete(r.paint());CHECK(r.app.candidateVisual.valid);
  r.ime.interrupt();r.paint();CHECK(!r.app.candidateVisual.pending()&&!r.app.candidateVisual.valid);
  r.type("ru");r.settle();r.complete(r.paint());r.app.key({1,++r.now,9,1,true});CHECK(prepareImePresentation(&r.ime,r.app));CHECK(!r.app.candidateVisual.valid);
 }
 {PendingRig r;r.begin(true,"ru",false);r.vertical();r.settle();r.complete(r.paint());
  r.type("g");r.complete(r.paint());CHECK(r.app.candidateVisual.pending());
  r.vertical();CHECK(prepareImePresentation(&r.ime,r.app));CHECK(!r.app.candidateVisual.valid&&!r.app.candidateVisual.pending());
  r.vertical();r.settle();auto f=r.paint();CHECK(!r.app.candidateVisual.valid);r.complete(f);CHECK(r.app.candidateVisual.valid);
  r.type("u");CHECK(prepareImePresentation(&r.ime,r.app));CHECK(r.app.candidateVisual.pending());
  ++r.app.candidateVisual.binding.width;CHECK(prepareImePresentation(&r.ime,r.app));CHECK(!r.app.candidateVisual.valid);
  r.settle();f=r.paint();r.complete(f);CHECK(r.app.candidateVisual.valid);
  CHECK(r.app.candidateVisual.text.items[0].bytes==r.ime.view()->rows[0].items[0].bytes);
  CHECK(std::string_view(r.app.candidateVisual.text.items[0].text.data(),r.app.candidateVisual.text.items[0].bytes)==std::string_view(r.ime.view()->rows[0].items[0].text.data(),r.ime.view()->rows[0].items[0].bytes));
  r.app.candidateVisual.complete(false);r.type("o");CHECK(prepareImePresentation(&r.ime,r.app));CHECK(!r.app.candidateVisual.pending());
 }
 {s3app::CandidateVisualContinuity p;p.state=s3app::CandidateVisualContinuity::State::Ready;
  for(unsigned i=0;i<19;++i)p.captureHorizontal("visual",8,326,50);
  p.complete(true);CHECK(!p.valid);p.state=s3app::CandidateVisualContinuity::State::Pending;CHECK(!p.pending());
  p.reset();p.state=s3app::CandidateVisualContinuity::State::Ready;p.captureHorizontal(std::string(66,'x'),8,326,50);p.complete(true);CHECK(!p.valid);
 }
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 std::cout<<"PASS presentation_pending_regression H_V_keys=6 gen0_nonselectable=1 stale_rejected=1 no_identity=1 terminal_hidden_failed_clear=1 mode_surface_invalidation=1 ready_recovery=1 unpublished_capture_rejected=1 overflow_rejected=1 pending_after_empty_publication=1\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
