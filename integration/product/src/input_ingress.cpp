#include "device_ime.hpp"
namespace ime_device {
bool DeviceIme::ingressCurrent()const{
 if(!ingress_known_||ingress_.owner!=composition_.get()||ingress_.chinese!=chinese_||ingress_.raw_edit!=raw_edit_||ingress_.mount!=resources_.mount()->epoch||!resources_.mount()->available)return false;
 if(ingress_.layout!=(window_?window_->layoutSerial():0)||ingress_.query!=(window_?window_->querySerial():0))return false;
 if(composition_&&!composition_->input_is_current(ingress_.stamp))return false;
 if(!composition_&&ingress_.waiting!=waiting_size_)return false;
 EditorSink check(app_);const bool bound=check.bind(0,0);
 return ingress_.bound==bound&&(!bound||sink_.matches(ingress_.target));
}
void DeviceIme::rememberIngress(){
 // Keep one frame fence only while this input/query/layout checkpoint lasts.
 // This records an obsolete physical picture after an already-applied edit;
 // it never records a newly displayed candidate as an early-choice target.
 const bool same=ingress_known_&&ingress_.owner==composition_.get()&&
     ingress_.query==(window_?window_->querySerial():0)&&
     ingress_.layout==(window_?window_->layoutSerial():0)&&
     (!composition_||composition_->input_is_current(ingress_.stamp));
 if(!same)superseded_frame_=0;
 if(composition_&&window_){
  const auto* shown=window_->presented(window_->onScreenGeneration());
  const auto now=composition_->snapshot();
  if(shown&&shown->count&&shown->rows[0].count){
   const auto& old=shown->rows[0].items[0];
   if(old.scheme==now.scheme_epoch&&old.activation.mount_epoch()==now.activation.mount_epoch()&&
      old.activation.shared_bundle_id()==now.activation.shared_bundle_id()&&
      (old.input!=now.input_id||old.revision!=now.revision||!targetMatches(old)))superseded_frame_=shown->generation;
  }
 }

 ingress_.owner=composition_.get();ingress_.stamp=composition_?composition_->raw_input().stamp:hanime_extension::InputStamp{};
 ingress_.query=window_?window_->querySerial():0;ingress_.layout=window_?window_->layoutSerial():0;ingress_.mount=resources_.mount()->epoch;ingress_.waiting=waiting_size_;
 ingress_.chinese=chinese_;ingress_.raw_edit=raw_edit_;EditorSink check(app_);ingress_.bound=check.bind(0,0);if(ingress_.bound)ingress_.target=check.target();ingress_known_=true;
}
void DeviceIme::invalidateIngress(){
 if(ingress_serial_<UINT32_MAX)++ingress_serial_;// Saturation permanently rejects new early authority; never reuse a token.
 ingress_causal_=0;superseded_frame_=0;ingress_known_=false;
}
uint32_t DeviceIme::ingressContext(){
 if(!ingressCurrent()){invalidateIngress();rememberIngress();}
 return ingress_serial_==UINT32_MAX?0:ingress_serial_;
}
bool DeviceIme::earlyContext(const s3hid::Event& e)const{
 return e.ime_context&&ingress_serial_&&ingress_serial_!=UINT32_MAX&&ingressCurrent()&&(e.ime_context==ingress_serial_||e.ime_context==ingress_causal_);
}
}
