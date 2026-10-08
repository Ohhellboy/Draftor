#include "vertical_ime.hpp"

namespace ime_device {
namespace v=draftor::vertical;
const char* verticalStatus(const DeviceIme& ime) noexcept {
 const std::string_view notice=ime.notice();
 if(notice=="Raw edit: Down loads marked range"||notice.substr(0,13)=="Raw syllables"||notice=="Raw syllable editor"||notice=="Raw code editor")return "EDIT up/down | Left load | Esc";
 if(notice=="No Down range here; move Left/Right")return "No raw range; use up/down";
 if(notice=="Candidates"||notice=="CN ready"||notice=="Candidate ready")return "CAND up/down | -/= cols | Esc";
 if(notice=="Candidate target pending"||notice=="Final navigation target pending"||notice=="Query pending")return "CN pending | raw retained";
 // Error, retry, pending-commit and uncertain outcome text take precedence
 // over navigation help. It is never replaced by a generic ready caption.
 return ime.notice();
}

bool prepareImePresentation(DeviceIme* ime,s3app::App& app,bool incremental) noexcept {
  app.chinesePresentationContext(candidateSurfaceVisible(ime,&app));
  if(ime&&ime->window()&&!ime->window()->setVerticalPacking(!app.horizontalImeSurface()))return false;
  app.candidateColumns(ime&&ime->window()?ime->window()->occupiedColumns():1);
  if(app.horizontalActive())app.layout.setHeightPx(candidateSurfaceVisible(ime,&app)?(ime&&ime->expanded()?210:278)-34:306);
  const bool ready=app.preparePresentation(incremental);
  if(ready)s3app::prepareCandidateVisual(app,ime);else app.candidateVisual.reset();
  return ready;
}
v::AbCandidateView verticalCandidateView(const hanime_extension::Snapshot& snapshot,
    const CandidateView& source) noexcept {
  v::AbCandidateView result;
  if(source.count>result.rows.size()){result.status=v::AbStatus::InvalidInput;return result;}
  result.count=source.count;result.row=source.row;result.col=source.col;result.expanded=source.expanded;
  for(std::size_t row=0;row<source.count;++row){
    const auto& original=source.rows[row];auto& target=result.rows[row];
    if(original.count>original.items.size()){result.status=v::AbStatus::InvalidInput;return result;}
    target.index=original.index;target.count=original.count;
    for(std::size_t i=0;i<original.count;++i){
      const auto& item=original.items[i];
      if(item.input!=snapshot.input_id||item.revision!=snapshot.revision||item.scheme!=snapshot.scheme_epoch||
          item.activation.mount_epoch()!=snapshot.activation.mount_epoch()||
          item.activation.shared_bundle_id()!=snapshot.activation.shared_bundle_id()){
        result.status=v::AbStatus::Stale;return result;
      }
      if(item.bytes>item.text.size()||item.begin>=item.end||item.end>snapshot.raw.size()||item.end>65535U){result.status=v::AbStatus::InvalidInput;return result;}
      target.items[i]={{item.text.data(),item.bytes},original.index==source.row?&item:nullptr};
      if(original.index==source.row&&i==source.col){
        result.mark.begin=std::uint16_t(item.begin);result.mark.end=std::uint16_t(item.end);
      }
    }
  }
  if(source.count&&(source.row<source.rows[0].index||
      source.row-source.rows[0].index>=source.count||
      source.col>=source.rows[source.row-source.rows[0].index].count))
    result.status=v::AbStatus::InvalidInput;
  return result;
}
// Keep S viewport/raster temporaries off the candidate projection frame.
[[gnu::noinline]] static void selectedRuns(DeviceIme& ime,const hanime_extension::Snapshot& snapshot,
    v::AbResult& result,const v::AbPort& port) noexcept {
      // Reuse the selected-member viewport over the same borrowed text. Only
      // its axis metric changes (no bitmap lookup, TF read or text copy).
      class AxisFont final:public s3host::FontPort{
       public:s3host::GlyphView glyph(std::uint32_t cp)const noexcept override{return {nullptr,0,0,0,std::uint16_t(cp<128?14:28),0,false};}
       std::uint16_t lineHeight()const noexcept override{return 28;}
       std::uint16_t defaultAdvance()const noexcept override{return 28;}
      } axis;
      const auto viewport=ime.selectedViewport(axis,result.selected.height);
      unsigned y=result.selected.y;
      for(std::size_t m=0;m<snapshot.member_count;++m){const auto& member=snapshot.members[m];
        const auto begin=std::max(viewport.begin,member.text_begin),end=std::min(viewport.end,member.text_begin+member.text_size);
        if(begin>=end)continue;
        const auto text=snapshot.selected_text.substr(begin,end-begin);unsigned height=0;
        for(std::size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))height+=s3app::display::scalar(text,at)<128?14:28;
        const v::Rect box{result.selected.x,std::uint16_t(y),28,std::uint16_t(height)};
        if(port.run){port.run(port.context,{text,box,v::TextFlow::VerticalBody,v::AbRole::Selected,0,!member.valid});++result.runs;}
        y+=height;
      }
      if(port.line&&viewport.left)port.line(port.context,{result.selected.x,result.selected.y,28,1},true);
      if(port.line&&viewport.right)port.line(port.context,{result.selected.x,std::uint16_t(result.selected.y+result.selected.height-1),28,1},true);
}
VerticalImeFrame verticalIme(DeviceIme* ime,const s3app::App& app,std::uint32_t now,
    std::uint32_t active_column,const v::AbPort& port) noexcept {
  VerticalImeFrame result;const auto geometry=app.imeGeometry();
  (void)active_column;result.layout=v::reservedAbRegions(geometry);
  if(result.layout.status!=v::AbStatus::Ready)return result;
  if(port.line)port.line(port.context,result.layout.separator,false);
  if(!candidateSurfaceVisible(ime,&app)||app.commandBarrier())return result;
  const auto* window=ime->window();
  if(window){result.generation=window->view().generation;result.row=window->view().row;}
  if(const auto* composition=ime->composition()){
    const auto snapshot=composition->snapshot();result.input=snapshot.input_id;result.revision=snapshot.revision;
    if(!snapshot.raw.empty()&&window){
      auto edge=v::EdgeSide::None;
      if(ime->boundaryVisible(now)){
        if(ime->boundaryEdge()==BoundaryEdge::CandidateTop)edge=v::EdgeSide::StartRight;
        if(ime->boundaryEdge()==BoundaryEdge::CandidateBottom)edge=v::EdgeSide::EndLeft;
      }
      const bool available=ime->candidateAvailable()&&window->focusedCurrent();
      auto candidates=available?verticalCandidateView(snapshot,window->view()):v::AbCandidateView{};
      if(available){
        // Compact's current column is numbered too; this is a drawing
        // descriptor, independent of the one/two/three-column occupancy.
        candidates.expanded=true;
        candidates.mark.candidate=!ime->rawEditing();
      }
      result.layout=v::layoutAb(snapshot,ime->presentationNavigation(),ime->rawCursor(),
          candidates,edge,geometry,port);
    }
    if(result.layout.status==v::AbStatus::Ready&&!snapshot.selected_text.empty())selectedRuns(*ime,snapshot,result.layout,port);
    result.candidate_frame=window&&ime->candidateAvailable()&&window->focusedCurrent()&&
        result.layout.status==v::AbStatus::Ready;
  }else if(!ime->raw().empty()){
    result.layout=v::layoutPreparingAb(ime->raw(),ime->rawCursor(),geometry,port);
  }
  return result;
}
std::uint32_t publishVerticalFrame(DeviceIme* ime,const s3app::App& app,
    const VerticalImeFrame& frame) noexcept {
  if(!candidateSurfaceVisible(ime,&app)||app.commandBarrier()||
      frame.layout.status!=v::AbStatus::Ready||!ime->window()||!ime->composition())return 0;
  const auto snapshot=ime->composition()->snapshot();
  if(snapshot.input_id!=frame.input||snapshot.revision!=frame.revision||
      ime->view()->generation!=frame.generation)return 0;
  // A successfully painted empty surface retires the old on-screen frame,
  // without granting selection authority to generation zero.
  const auto& current=*ime->view();
  const bool empty_surface=frame.generation==0&&current.generation==0&&current.count==0;
  if(!frame.candidate_frame&&!empty_surface)return 0;
  return publishVisibleCandidate(ime,&app);
}
ChoiceResult chooseVertical(CandidateWindow& window,const VerticalChoiceTarget& target,std::uint32_t action){
  if(target.role!=v::AbRole::CurrentCandidate||target.number<1||target.number>6||
      !target.frame.candidate_frame)return ChoiceResult::NoCandidate;
  const auto& view=window.view();
  if(view.generation!=target.frame.generation||view.row!=target.frame.row||
      window.onScreenGeneration()!=target.frame.generation)return ChoiceResult::ExpiredFrame;
  const auto* shown=window.presented(target.frame.generation);
  if(!shown||!shown->count)return ChoiceResult::ExpiredFrame;
  for(std::size_t i=0;i<shown->count;++i)if(shown->rows[i].index==target.frame.row){
    const auto& row=shown->rows[i];
    if(target.number>row.count)return ChoiceResult::NoCandidate;
    const auto& item=row.items[target.number-1];
    if(item.input!=target.frame.input||item.revision!=target.frame.revision)return ChoiceResult::ExpiredInput;
    return window.choose(target.number-1,target.frame.generation,action);
  }
  return ChoiceResult::NoCandidate;
}
v::EdgeSide browseVerticalCandidates(CandidateWindow& window,bool left,std::uint32_t action){
  const auto& view=window.view();
  if(!view.count)return v::EdgeSide::None;
  if(!left&&view.row==0)return v::EdgeSide::StartRight;
  if(left&&view.at_end&&view.row==view.rows[view.count-1].index)return v::EdgeSide::EndLeft;
  window.request(left?view.row+1:view.row-1,view.col,true,false,action);
  return v::EdgeSide::None;
}
v::EdgeSide verticalCandidateKey(CandidateWindow& window,char key,std::uint32_t action){
  return key=='-'?browseVerticalCandidates(window,true,action):
      key=='='?browseVerticalCandidates(window,false,action):v::EdgeSide::None;
}
VerticalDispatch dispatchVerticalNumber(DeviceIme& ime,const s3app::App& app,
    const VerticalChoiceTarget& target,const s3hid::Event& event){
  if(event.usage<30||event.usage>35||event.mods)return VerticalDispatch::RejectedTarget;
  if(!event.down){(void)ime.event(event);return VerticalDispatch::Delivered;}
  if(target.role!=v::AbRole::CurrentCandidate||target.number!=unsigned(event.usage-29)||
      !target.frame.candidate_frame||!candidateSurfaceVisible(&ime,&app)||app.commandBarrier())
    return VerticalDispatch::RejectedTarget;
  if(!ime.window()||!ime.composition())return VerticalDispatch::ExpiredFrame;
  const auto snapshot=ime.composition()->snapshot();const auto& view=*ime.view();
  if(snapshot.input_id!=target.frame.input||snapshot.revision!=target.frame.revision||
      view.generation!=target.frame.generation||view.row!=target.frame.row||
      event.ime_frame!=target.frame.generation||ime.window()->onScreenGeneration()!=target.frame.generation)
    return VerticalDispatch::ExpiredFrame;
  const auto* shown=ime.window()->presented(target.frame.generation);
  if(!shown||!shown->count)return VerticalDispatch::ExpiredFrame;
  for(std::size_t i=0;i<shown->count;++i)if(shown->rows[i].index==target.frame.row&&target.number<=shown->rows[i].count){
    (void)ime.event(event);return VerticalDispatch::Delivered;
  }
  return VerticalDispatch::RejectedTarget;
}
} // namespace ime_device
