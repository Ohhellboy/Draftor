#pragma once
#include "device_ime.hpp"
namespace s3app {
// Original HID events are never rewritten. A held physical key keeps its
// semantic action across layout changes, while DeviceIme owns freshness.
class ProductInput {
 using Action=ime_device::ImeAction;
 public:
 bool layout(App& app,const s3hid::Event& e)noexcept{return app.layoutKey(e);}
 bool ime(App& app,ime_device::DeviceIme* ime,const s3hid::Event& e){
  if(!ime)return false;
  if(epoch_!=e.epoch){epoch_=e.epoch;for(auto& k:held_)k={};}
  Action action=Action::Native;const auto slot=index(e.usage);
  if(slot<6){auto& key=held_[slot];
   if(e.down&&!key.down){key.down=true;key.action=Action::Native;
    if(!app.horizontalImeSurface()&&!e.mods&&ime->active()){
     if(ime->rawEditing()){
      key.action=e.usage==82?Action::RawPrevious:e.usage==81?Action::RawNext:
                 e.usage==80?Action::RawCandidates:Action::Unassigned;
     }else{
      key.action=e.usage==82?Action::PreviousCandidate:e.usage==81?Action::NextCandidate:
                 e.usage==80?Action::ExpandOrNextColumn:e.usage==45?Action::NextColumn:Action::PreviousColumn;
     }
    }
   }
   action=key.action;if(!e.down)key={};
  }
  return ime->event(e,action);
 }
 // Called only after BLE admission and a UI-route rejection. Enter and F2
 // can change focus before their queued physical release is drained. Retire
 // that release in the existing owner; never give a stale down UI authority.
 // The existing epoch and DeviceIme timestamp guards protect a newer press.
 void releaseAfterRouteDrop(App& app,ime_device::DeviceIme* ime,const s3hid::Event& e){
  if(e.epoch==epoch_&&!e.down&&(e.usage==40||e.usage==59))this->ime(app,ime,e);
 }
 private:
 static unsigned index(std::uint8_t u)noexcept{return u>=79&&u<=82?u-79:u==45?4:u==46?5:6;}
 struct Held{Action action=Action::Native;bool down=false;};
 std::uint32_t epoch_=0;Held held_[6]{};
};
}
