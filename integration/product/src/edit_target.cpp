#include "device_ime.hpp"
#include <cstdio>
namespace ime_device {
bool DeviceIme::reselectionCurrent()const{
 return reselection_.active&&composition_&&window_&&window_->querySerial()==reselection_.query&&
        composition_->input_is_current(reselection_.stamp);
}
bool DeviceIme::targetMatches(const VisibleItem& item)const{
 return !reselectionCurrent()||(item.begin==reselection_.begin&&item.end==reselection_.end);
}
EditTarget DeviceIme::editTarget()const{
 if(!composition_||!window_)return {};
 const auto snap=composition_->snapshot();
 if(!raw_edit_){
  if(reselectionCurrent())return {EditTargetKind::Prefix,reselection_.begin,reselection_.end,0};
  if(candidateAvailable()) {const auto& item=window_->view().rows[0].items[0];return {EditTargetKind::Prefix,item.begin,item.end,0};}
  return {};
 }
 // The caret is a right endpoint. A complete left member owns a shared
 // boundary, regardless of the validity of the member on its right.
 if(!cursor_)return {EditTargetKind::Boundary,0,0,0};
 for(size_t i=0;i<snap.member_count;++i){const auto& m=snap.members[i];
  if(m.valid&&m.end==cursor_)return {EditTargetKind::Member,m.begin,m.end,i};
 }
 const auto nav=navigator_?navigator_->view(navigation_,*composition_):hanime_extension::NavigationView{};
 if(nav.status==hanime_extension::NavigationStatus::Ready){
  for(size_t i=0;i<snap.member_count;++i){const auto& m=snap.members[i];
   if(!m.valid||cursor_<=m.begin||cursor_>=m.end)continue;
   // Selected members use the public confirmed-member navigation geometry.
   // Unknown/incomplete editing boundaries never grant query admission.
   size_t end=m.begin;
   for(size_t j=0;j<nav.span_count&&end<cursor_;++j){const auto& span=nav.spans[j];
    if(span.begin!=end)continue;
    if(span.kind!=hanime_extension::NavigationKind::CompleteSyllable&&
       span.kind!=hanime_extension::NavigationKind::CodePair)break;
    end=span.end;
   }
   if(end==cursor_)return {EditTargetKind::Member,m.begin,cursor_,i};
   return {EditTargetKind::Boundary,cursor_,cursor_,0};
  }
  const auto ends=navigator_->prefix_endpoints(navigation_,*composition_);
  if(cursor_>snap.focus&&ends.status==hanime_extension::NavigationStatus::Ready&&ends.begin==snap.focus){
   for(size_t i=0;i<ends.count;++i)
    if(ends.ends[i]==cursor_)return {EditTargetKind::Prefix,snap.focus,cursor_,0};
  }
 }
 return {EditTargetKind::Boundary,cursor_,cursor_,0};
}
void DeviceIme::editCaption(char* out,size_t capacity)const{
 if(!capacity)return;
 if(resources_.rules()==hanime_extension::SchemeRules::DirectExact){
  std::snprintf(out, capacity, raw_edit_ ? "CODE: arrows | Esc candidates" : "CAND arrows | Esc cancel");
  return;
 }
 const auto target=editTarget();
 if(raw_edit_){
  if(reselection_.pending)std::snprintf(out,capacity,"EDIT: Down waiting; choice stays queued");
  else if(target.valid())std::snprintf(out,capacity,candidateAvailable()?"EDIT: Down marked | Space/digit shown":"EDIT: Down loads underlined range");
  else std::snprintf(out,capacity,"EDIT: move L/R to mark text");
 }else if(reselectionCurrent())std::snprintf(out,capacity,window_->busy()?"RESELECT loading | Esc edit":"RESELECT ready | Space/digit | Esc edit");
 else std::snprintf(out,capacity,"CAND arrows | Esc edit");
}
RawViewport DeviceIme::editViewport(const s3host::FontPort& font)const{
 // Keep the active short prefix visible after Down, even with a long tail.
 // The same glyph advances serve raw text, target underline and edit caret.
 const bool selecting=reselectionCurrent();
 const auto nav=navigator_&&composition_?navigator_->view(navigation_,*composition_):hanime_extension::NavigationView{};
 const bool ready=nav.status==hanime_extension::NavigationStatus::Ready;
 return rawViewport(raw(),selecting?editTarget().end:cursor_,raw_edit_||selecting,font,284,
                    ready?nav.boundaries:nullptr,ready?nav.boundary_count:0);
}
}
