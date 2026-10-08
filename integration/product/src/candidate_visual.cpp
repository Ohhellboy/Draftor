#include "candidate_visual.hpp"
#include "ime_presentation.hpp"
#include <cstring>
namespace s3app {
namespace v=draftor::vertical;
namespace {
bool same(v::Rect a,v::Rect b){return a.x==b.x&&a.y==b.y&&a.width==b.width&&a.height==b.height;}
void save(CandidateVisualList& list,std::string_view text,v::Rect box,unsigned limit,v::TextFlow flow){
 if(list.count==list.items.size()||text.size()>list.items[0].text.size()){list.overflow=true;return;}
 auto& item=list.items[list.count++];item.bytes=std::uint16_t(text.size());
 std::memcpy(item.text.data(),text.data(),text.size());item.box=box;item.limit=std::uint16_t(limit);item.flow=flow;
}
}
void CandidateVisualContinuity::capture(const v::AbRun& run) noexcept {
 if(state!=State::Ready||!vertical||(run.role!=v::AbRole::CurrentCandidate&&run.role!=v::AbRole::Preview&&run.role!=v::AbRole::Peek))return;
 save(text,run.text,run.box,0,run.flow);
}
void CandidateVisualContinuity::captureHorizontal(std::string_view value,int x,int y,unsigned width) noexcept {
 if(state!=State::Ready||vertical)return;
 save(text,value,{std::uint16_t(x),std::uint16_t(y),std::uint16_t(width),28},std::min(294,x+int(width)-4),v::TextFlow::HorizontalSmall);
}
void prepareCandidateVisual(App& app,ime_device::DeviceIme* ime) noexcept {
 auto& p=app.candidateVisual;const bool vertical=!app.horizontalImeSurface();
 if(!ime_device::candidateSurfaceVisible(ime,&app)||app.commandBarrier()||app.progressDrawing()||ime->raw().empty()){p.reset();return;}
 const auto owner=ime->resourceOwner();
 const auto layout=v::reservedAbRegions(app.imeGeometry());const int cy=ime->expanded()?210:278;
 const auto binding=vertical?layout.candidates:v::Rect{8,std::uint16_t(cy+48),284,std::uint16_t(400-cy-48)};
 if(p.surface!=app.focusToken()||p.owner!=owner||p.vertical!=vertical||!same(p.binding,binding))p.reset();
 p.surface=app.focusToken();p.owner=owner;p.vertical=vertical;p.binding=binding;
 const auto* view=ime->view();const bool available=ime->candidateAvailable();
 if(view&&view->count&&available){
  // One bounded list is sufficient: a failed/incomplete presentation invalidates
  // it, and only complete(true) promotes freshly captured text to reusable visual.
  p.text.clear();p.valid=false;p.state=CandidateVisualContinuity::State::Ready;
 }else if(view&&!view->count&&!available&&ime->hasWork()){
  // Publication mismatch alone cannot express this: the empty generation may
  // already be on screen while the query is still pending. Read live work state.
  p.state=CandidateVisualContinuity::State::Pending;
 }else p.reset();
}
[[gnu::noinline]] void paintPendingCandidateVisual(const CandidateVisualContinuity& p,const v::AbPort& port) noexcept {
 if(!p.pending()||!port.run)return;
 for(std::size_t i=0;i<p.text.count;++i){const auto& item=p.text.items[i];
  port.run(port.context,{{item.text.data(),item.bytes},item.box,item.flow,v::AbRole::Preview,0,false,nullptr});
 }
}
} // namespace s3app
