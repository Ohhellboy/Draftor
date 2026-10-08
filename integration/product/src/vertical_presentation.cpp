#include "vertical_presentation.hpp"
#include "reader_display.hpp"
#include "utf8.hpp"
#include "draftor/vertical_ab.hpp"
#include <algorithm>

namespace s3app {
namespace v=draftor::vertical;
namespace ep=editor::presentation;
VerticalPresentation::VerticalPresentation(editor::SharedEditorView shared,
    const s3host::FontPort& font) noexcept : shared_(shared) {
  v::AbGeometry geometry;geometry.area=v::editor_area;
  config_.body=v::reservedAbRegions(geometry).body;
  config_.literal_advance=&s3host::FontPort::advance;
  config_.metrics_context=&font;
}
v::Stamp VerticalPresentation::stamp() const noexcept {
  v::Stamp value;value.text_revision=shared_.document.textRevision();
  value.geometry_revision=geometry_revision_;return value;
}
v::Input VerticalPresentation::input() const noexcept {
  // Legacy geometry calls these "literals"; they are display-only ASCII word
  // spans, inferred from current bytes. No semantic input session is involved.
  return {shared_.document.text(),nullptr,0,stamp(),true,
      v::bodyPolicyRules(v::BodyPolicy::LatinWordsUprightAtHash)};
}
v::Status VerticalPresentation::refresh() noexcept {
  if(preferred_valid_&&(preferred_revision_!=shared_.document.revision()||
      preferred_selection_!=shared_.editor.selectionVersion()||
      preferred_affinity_!=shared_.editor.caretAffinity()))preferred_valid_=false;
  const auto current=input();
  const v::Position focus{shared_.document.cursorByte(),shared_.editor.caretAffinity()};
  if(requested_&&requested_stamp_==current.stamp&&requested_focus_.byte==focus.byte&&
      requested_focus_.affinity==focus.affinity)return viewport_.status(current.stamp);
  const auto edit=shared_.document.lastTextEdit();
  const bool prefix=requested_&&edit.incremental&&edit.text_revision==requested_stamp_.text_revision+1U;
  requested_stamp_=current.stamp;requested_focus_=focus;requested_=true;
  return viewport_.start(current,config_,focus,prefix,edit.begin);
}
v::Step VerticalPresentation::service(std::size_t work_budget) noexcept {
  // Renewal progresses only in the App service turn, never a paint/key settle.
  if(navigationPending())return {v::Status::NeedsLayout,0,0,0};
  const auto status=refresh();
  return status==v::Status::MoreWork?viewport_.service(input(),work_budget):v::Step{status,0,0,0};
}
v::Status VerticalPresentation::settle() noexcept {
  auto status=service(256).status;
  while(status==v::Status::MoreWork)status=service(256).status;
  return status;
}
v::Status VerticalPresentation::setRegion(v::Rect region,bool focus_at_left,std::size_t work_budget) noexcept {
  auto next=config_;next.body=region;next.focus_at_left=focus_at_left;
  if(!v::validGeometry(next))return v::Status::InvalidGeometry;
  full_height_=region.height;
  if(find_open_)next.body.height=std::min<std::uint16_t>(next.body.height,display::bodyHeight);
  if(next.body.x!=config_.body.x||next.body.y!=config_.body.y||next.body.width!=config_.body.width||
      next.body.height!=config_.body.height||next.focus_at_left!=config_.focus_at_left){
    cancelNavigation();config_=next;++geometry_revision_;requested_=false;preferred_valid_=false;
  }
  return work_budget?service(work_budget).status:settle();
}
void VerticalPresentation::findSurface(bool open) noexcept {
  if(find_open_==open)return;
  find_open_=open;auto area=config_.body;area.height=full_height_;(void)setRegion(area,config_.focus_at_left);
}
void VerticalPresentation::followCaret(v::Affinity,bool reset_preferred) noexcept {
  if(reset_preferred){cancelNavigation();preferred_valid_=false;}
  preferred_revision_=shared_.document.revision();preferred_selection_=shared_.editor.selectionVersion();
  preferred_affinity_=shared_.editor.caretAffinity();
  if(reset_preferred)(void)settle();else (void)refresh();
}
v::Mapping VerticalPresentation::caret() const noexcept {
  return viewport_.locate(stamp(),{shared_.document.cursorByte(),shared_.editor.caretAffinity()});
}
bool VerticalPresentation::placeCaretSlot(std::uint16_t slot) noexcept {
  if(settle()!=v::Status::Ready||slot>=viewport_.visibleColumns())return false;
  const auto column=caret().caret.column;
  const auto first=column>slot?column-slot:0;
  if(first==viewport_.firstColumn())return true;
  viewport_.firstColumn(first);requested_=false;
  return settle()==v::Status::Ready;
}
bool VerticalPresentation::captureViewport(ep::ViewportState& out) noexcept {
  if(settle()!=v::Status::Ready)return false;
  for(std::size_t i=0;i<viewport_.eventCount();++i){
    const auto event=viewport_.eventAt(i);
    if(event.column==viewport_.firstColumn()){
      out={event.begin_byte,v::Affinity::Downstream,1};return true;
    }
  }
  return false;
}
void VerticalPresentation::restoreViewport(const ep::ViewportState& anchor) noexcept {
  cancelNavigation();preferred_valid_=false;requested_=false;
  if(anchor.layout_revision==1&&editor::isUtf8Boundary(shared_.document.text(),anchor.byte_offset)){
    const v::Position focus{anchor.byte_offset,anchor.affinity};
    auto status=viewport_.start(input(),config_,focus);
    while(status==v::Status::MoreWork)status=viewport_.service(input(),256).status;
    if(status==v::Status::Ready)(void)viewport_.scrollTo(focus);
  }
  (void)settle(); // Validate the logical cursor and keep it visible after restore.
}
ep::SpatialResult VerticalPresentation::lookupNavigation(ep::SpatialDirection direction,bool extend,ep::ByteTarget& out) noexcept {
  out={};const auto origin=caret();
  if(origin.status!=v::Status::Ready)return origin.status==v::Status::MoreWork?ep::SpatialResult::NeedsLayout:ep::SpatialResult::Invalid;
  const bool across=direction==ep::SpatialDirection::Left||direction==ep::SpatialDirection::Right;
  if(!across||!preferred_valid_)preferred_y_=origin.caret.y_px;
  const auto selection=shared_.editor.selection();v::Mapping target;
  if(!extend&&selection.has_selection&&!across){
    target.status=v::Status::Ready;
    target.caret.position={direction==ep::SpatialDirection::Up?selection.begin:selection.end,v::Affinity::Downstream};
  }else{
    v::Direction spatial=v::Direction::Up;
    switch(direction){
      case ep::SpatialDirection::Up:spatial=v::Direction::Up;break;
      case ep::SpatialDirection::Down:spatial=v::Direction::Down;break;
      case ep::SpatialDirection::Left:spatial=v::Direction::Left;break;
      case ep::SpatialDirection::Right:spatial=v::Direction::Right;break;
    }
    target=viewport_.move(stamp(),origin.caret.position,spatial,preferred_y_);
  }
  if(target.status==v::Status::Ready){
    out={target.caret.position.byte,target.caret.position.affinity,true};preferred_valid_=across;
  }
  switch(target.status){
    case v::Status::Ready:return ep::SpatialResult::Ready;
    case v::Status::NeedsLayout:return ep::SpatialResult::NeedsLayout;
    case v::Status::Boundary:return ep::SpatialResult::Boundary;
    case v::Status::Stale:return ep::SpatialResult::Stale;
    default:return ep::SpatialResult::Invalid;
  }
}
void VerticalPresentation::cancelNavigation() noexcept {
  if(pending_navigation_.active){pending_navigation_.active=false;requested_=false;}
}
bool VerticalPresentation::navigationCurrent(ep::NavigationContext context) const noexcept {
  const auto& n=pending_navigation_;
  return n.active&&n.geometry==geometry_revision_&&n.presentation==context.presentation_revision&&
      n.focus==context.focus&&n.revision==shared_.document.revision()&&n.text_revision==shared_.document.textRevision()&&
      n.selection==shared_.editor.selectionVersion()&&n.cursor==shared_.document.cursorByte()&&n.affinity==shared_.editor.caretAffinity();
}
ep::SpatialResult VerticalPresentation::spatialTarget(ep::SpatialDirection direction,bool extend,
    ep::ByteTarget& out,ep::NavigationContext context) noexcept {
  out={};
  if(navigationPending()){
    if(navigationCurrent(context)&&pending_navigation_.direction==direction&&pending_navigation_.extend==extend)
      return ep::SpatialResult::NeedsLayout; // Coalesce held-key repeats; depth remains one.
    cancelNavigation();
  }
  const auto status=refresh();
  const auto result=status==v::Status::Ready?lookupNavigation(direction,extend,out):
      status==v::Status::MoreWork?ep::SpatialResult::NeedsLayout:
      status==v::Status::Stale?ep::SpatialResult::Stale:ep::SpatialResult::Invalid;
  if(result!=ep::SpatialResult::NeedsLayout)return result;
  pending_navigation_={geometry_revision_,context.presentation_revision,context.focus,
      shared_.document.cursorByte(),shared_.document.revision(),shared_.document.textRevision(),
      shared_.editor.selectionVersion(),preferred_y_,shared_.editor.caretAffinity(),direction,
      extend,preferred_valid_,true,false};
  return result;
}
ep::SpatialResult VerticalPresentation::serviceNavigation(ep::NavigationContext context,
    std::size_t budget,ep::ByteTarget& out,bool& extend) noexcept {
  out={};
  if(!navigationCurrent(context)){cancelNavigation();return ep::SpatialResult::Stale;}
  auto& n=pending_navigation_;extend=n.extend;
  if(viewport_.status(stamp())==v::Status::MoreWork){
    const auto step=viewport_.service(input(),std::min(budget,navigation_work_budget));
    if(step.status==v::Status::MoreWork)return ep::SpatialResult::NeedsLayout;
    if(step.status!=v::Status::Ready){cancelNavigation();return ep::SpatialResult::Invalid;}
  }
  if(!navigationCurrent(context)){cancelNavigation();return ep::SpatialResult::Stale;}
  preferred_y_=n.preferred_y;preferred_valid_=n.preferred_valid;
  const auto result=lookupNavigation(n.direction,n.extend,out);
  if(result==ep::SpatialResult::NeedsLayout){
    if(n.renewed){cancelNavigation();return ep::SpatialResult::Invalid;}
    const auto direction=n.direction==ep::SpatialDirection::Right?v::Direction::Right:
        n.direction==ep::SpatialDirection::Left?v::Direction::Left:
        n.direction==ep::SpatialDirection::Up?v::Direction::Up:v::Direction::Down;
    n.preferred_y=preferred_y_;n.preferred_valid=true;n.renewed=true;
    if(viewport_.renewNavigation(input(),{n.cursor,n.affinity},direction)!=v::Status::MoreWork){
      cancelNavigation();return ep::SpatialResult::Invalid;
    }
    return ep::SpatialResult::NeedsLayout;
  }
  // Clear BEFORE publishing Ready. Only the App may apply the byte target once.
  n.active=false;
  return result;
}
v::Rectangles VerticalPresentation::rangeRectangles(v::ByteRange range,v::Rect* out,std::size_t capacity) const noexcept {
  return viewport_.rectangles(input(),range,out,capacity);
}
v::Rectangles VerticalPresentation::selectionRectangles(v::Rect* out,std::size_t capacity) const noexcept {
  const auto selected=shared_.editor.selection();return rangeRectangles({selected.begin,selected.end},out,capacity);
}
v::PaintResult VerticalPresentation::paint(const v::RasterPort& port) const noexcept {
  auto raster=port;raster.punctuation_policy=v::PunctuationPolicy::CommonVertical;
  return v::paint(viewport_,stamp(),raster);
}
} // namespace s3app
