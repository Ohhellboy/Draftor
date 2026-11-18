#include "device_ime.hpp"
#include <cstring>

namespace ime_device {
#if BGIME_A_REV1_DAILY
void DeviceIme::dailyFeedback(const char* message){mark(message);app_.notice=message;app_.dirtyDisplay=true;}
void DeviceIme::dailyFault(){
 if(learning_)learning_->configure(learning_->lambda(),false,false);
 daily_phase_=DailyLearningPhase::Fault;daily_detach_=true;
 dailyFeedback("Learning ERROR; not safe off");
}
bool DeviceIme::dailyDocumentCurrent()const{
 return app_.session.hasCurrent()&&daily_document_==app_.session.currentId()&&
  daily_session_==app_.session.sessionSerial()&&daily_revision_epoch_==app_.document.textRevisionEpoch();
}
bool DeviceIme::dailyOpen(){
 // Caller has verified the idle boundary. Never discard an unfinished input.
 retireLearningConsumers();daily_saved_=daily_save_failed_=daily_revision_changed_=false;
 if(!learning_||!learning_->open()){dailyFault();return false;}
 daily_phase_=DailyLearningPhase::Opening;dailyFeedback("Learning starting");return true;
}
bool DeviceIme::dailySave(uint32_t now){
 if(!learning_||active()||reset_active_||app_.commandBarrier()||app_.transferActive()||
    !app_.session.hasCurrent()||app_.filesOpen()||app_.findOpen()||app_.renameOpen()||app_.statsOpen()){
  dailyFeedback("Finish input before Ctrl+F6");return false;
 }
 if(daily_phase_!=DailyLearningPhase::Enabled&&daily_phase_!=DailyLearningPhase::Paused)return false;
 if(daily_phase_==DailyLearningPhase::Enabled){
  retireLearningConsumers();
  if(!learning_->beginClose()){dailyFault();return false;}
 }
 daily_document_=app_.session.currentId();daily_session_=app_.session.sessionSerial();
 daily_revision_epoch_=app_.document.textRevisionEpoch();daily_revision_=app_.document.textRevision();
 daily_revision_changed_=false;daily_saved_=false;daily_save_failed_=false;
 const auto result=app_.requestSave(now);daily_operation_=app_.operationSerial;
 daily_saved_=result.status==hanime::documents::Status::Ok;
 daily_save_failed_=!daily_saved_&&!(result.status==hanime::documents::Status::Busy&&
  result.stage&&std::strcmp(result.stage,"manual_save_accepted")==0&&app_.manualSavePending());
 daily_phase_=DailyLearningPhase::Saving;
 if(!daily_save_failed_)dailyFeedback("Saving body + learning");
 return true;
}
bool DeviceIme::dailyKey(const s3hid::Event& event){
 if(daily_phase_==DailyLearningPhase::Disabled)return false;
 const bool toggle=event.usage==63&&(event.mods==1||event.mods==16||event.mods==17);
 const bool primary=((event.mods&0x11)!=0)!=((event.mods&0x88)!=0);
 const bool save=(event.usage==63&&!event.mods)||
  (event.usage==22&&primary&&!(event.mods&0x66));
 // DeviceIme::fresh owns press/release and held-key deduplication for this key.
 if(!toggle){
  if(save&&daily_phase_==DailyLearningPhase::Paused&&!active())return dailySave(event.ms);
  return false;
 }
 if(active()||reset_active_){dailyFeedback("Finish input before Ctrl+F6");return true;}
 if(daily_phase_==DailyLearningPhase::Enabled){dailySave(event.ms);return true;}
 if(daily_phase_==DailyLearningPhase::Paused){
  if(daily_save_failed_){dailyFeedback("Save failed; F6 retries");return true;}
  if(app_.commandBarrier()||app_.persistence.backgroundSaveActive()||app_.manualSavePending()||
     app_.transferActive()||app_.filesOpen()||app_.findOpen()||app_.renameOpen()||app_.statsOpen()){
   dailyFeedback("Finish save before Ctrl+F6");return true;
  }
  dailyOpen();return true;
 }
 dailyFeedback(daily_phase_==DailyLearningPhase::Fault?"Learning ERROR; not safe off":"Learning operation pending");return true;
}
#endif
bool DeviceIme::startDailyLearning(bool root_ready){
#if BGIME_A_REV1_DAILY
 if(daily_phase_!=DailyLearningPhase::Disabled||active())return false;
 if(!root_ready||!learning_){dailyFault();return false;}
 return dailyOpen();
#else
 (void)root_ready;return false;
#endif
}
void DeviceIme::serviceDailyLearning(){
#if BGIME_A_REV1_DAILY
 if(daily_phase_==DailyLearningPhase::Disabled)return;
 if(learning_&&learning_->phase()==LearningPhase::Error&&daily_phase_!=DailyLearningPhase::Fault)dailyFault();
 if(daily_phase_==DailyLearningPhase::Fault){
  // A failed owner is never reopened underneath a live composition. Retain any
  // current raw input; detach only after its explicit completion/cancellation.
  if(daily_detach_&&!active()){retireLearningConsumers();daily_detach_=false;}
  return;
 }
 if(daily_phase_==DailyLearningPhase::Opening&&learning_->ready()){
  // No consumer exists before this point, including boot-time buffered raw.
  if(!learning_->configure(learning_->lambda(),true,true)){dailyFault();return;}
  daily_phase_=DailyLearningPhase::Enabled;dailyFeedback("Learning ON");
 }
 if(daily_phase_==DailyLearningPhase::Saving){
  if(!dailyDocumentCurrent()||app_.document.textRevision()!=daily_revision_)daily_revision_changed_=true;
  if(app_.operationSerial!=daily_operation_){
   daily_operation_=app_.operationSerial;
   if(!app_.manualSavePending()&&!app_.persistence.backgroundSaveActive()){
    daily_saved_=app_.lastOperation.status==hanime::documents::Status::Ok;
    daily_save_failed_=!daily_saved_;
   }
  }
  if(learning_->phase()==LearningPhase::Closed&&learning_->status()==hanime::StateStatus::ClosedSaved&&
     !app_.manualSavePending()&&!app_.persistence.backgroundSaveActive()){
   daily_phase_=DailyLearningPhase::Paused;app_.dirtyDisplay=true;
  }
 }
#endif
}
DailyLearningPhase DeviceIme::dailyLearningPhase()const{
#if BGIME_A_REV1_DAILY
 return daily_phase_;
#else
 return DailyLearningPhase::Disabled;
#endif
}
bool DeviceIme::dailySafeToPowerOff()const{
#if BGIME_A_REV1_DAILY
 return daily_phase_==DailyLearningPhase::Paused&&daily_saved_&&!daily_save_failed_&&!daily_revision_changed_&&
  dailyDocumentCurrent()&&app_.document.textRevision()==daily_revision_&&!app_.document.dirty()&&
  !app_.manualSavePending()&&!app_.persistence.backgroundSaveActive()&&!app_.persistence.bodyDirty()&&
  !app_.persistence.positionDirty()&&!app_.commandBarrier()&&!app_.transferActive()&&!active()&&
  app_.lastOperation.status==hanime::documents::Status::Ok&&
  learning_&&learning_->phase()==LearningPhase::Closed&&learning_->status()==hanime::StateStatus::ClosedSaved;
#else
 return false;
#endif
}
const char* DeviceIme::dailyLearningNotice()const{
#if BGIME_A_REV1_DAILY
 switch(daily_phase_){
 case DailyLearningPhase::Opening:return "Learning starting";
 case DailyLearningPhase::Saving:return daily_save_failed_?"Save failed; not safe off":"Saving body + learning";
 case DailyLearningPhase::Paused:return daily_save_failed_?"Save failed; F6 retries":
  dailySafeToPowerOff()?"Saved; safe power off":"Learning OFF; F6 saves";
 case DailyLearningPhase::Fault:return "Learning ERROR; not safe off";
 default:break;
 }
#endif
 return "";
}
}
