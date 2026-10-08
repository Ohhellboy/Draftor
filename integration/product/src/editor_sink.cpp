#include "behavior_bench.hpp"
#include "editor_sink.hpp"
#include "device_metrics.hpp"
namespace ime_device {
bool EditorSink::bind(uint64_t input,uint64_t revision){
 if(!app_.imeTextTargetAvailable())return false;
 if(app_.renameOpen()){
  const auto n=app_.nameStamp();
  target_={app_.renameId(),app_.session.sessionSerial(),input,revision,{n.focus,n.revision,0},n.cursor,0,0,n.selected,true};
  armed_=true;return true;
 }
 const auto s=app_.editor.selection();
 target_={app_.session.currentId(),app_.session.sessionSerial(),input,revision,app_.editStamp(),app_.document.cursorByte(),s.begin,s.end,s.has_selection};armed_=true;return true;
}
bool EditorSink::current()const{
 return armed_&&matches(target_);
}
bool EditorSink::matches(const Target& target_)const{
 if(!app_.imeTextTargetAvailable())return false;
 if(target_.rename!=app_.renameOpen())return false;
 if(target_.rename){
  const auto n=app_.nameStamp();
  return target_.document==app_.renameId()&&target_.session==app_.session.sessionSerial()&&
    target_.stamp.focus==n.focus&&target_.stamp.revision==n.revision&&target_.cursor==n.cursor&&target_.selected==n.selected;
 }
 const auto stamp=app_.editStamp();const auto s=app_.editor.selection();
 return target_.document==app_.session.currentId()&&target_.session==app_.session.sessionSerial()&&target_.stamp.focus==stamp.focus&&target_.stamp.revision==stamp.revision&&target_.stamp.selection==stamp.selection&&target_.cursor==app_.document.cursorByte()&&target_.selected==s.has_selection&&target_.begin==s.begin&&target_.end==s.end;
}
bool EditorSink::commit(std::string_view text){
 if(!current())return false;
 // Consume authority BEFORE the call. An exception after model mutation remains
 // Unknown to the core; never catch it and fabricate a zero-effect rejection.
 armed_=false;
 const bool accepted=target_.rename?
   app_.commitName(text,{target_.stamp.focus,target_.stamp.revision,target_.cursor,target_.selected}):
   app_.commitText(text,target_.stamp);
 if(accepted){BT_MEASURE(committed());}
 if(accepted)accepted_us_=clockUs(); // Final editor model acceptance, before later LCD drawing.
 if(!accepted)armed_=true; // Same immutable target, only the core can issue a retry.
 return accepted;
}
}
