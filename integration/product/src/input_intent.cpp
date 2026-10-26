#include "device_ime.hpp"
#include "ui_trace.hpp"
namespace ime_device {
using CS=hanime::CompositionStatus;
void DeviceIme::noteIntent(uint8_t reason,const s3hid::Event& e,uint32_t action,uint32_t parent){
 if(action==current_action_)event_result_=reason;
 // Raw edits carry separate receipt and disposition records, without letter values.
 if(reason==Received&&!((e.usage>=4&&e.usage<=29)||e.usage==0x2a||e.usage==0x4c))return;
 IntentContext context{parent,window_?window_->querySerial():0,uint16_t(cursor_),uint16_t(following_count_),letters_since_choice_,edit_actions_,uint8_t(resources_.rules()),sink_.target().session,sink_.target().stamp.focus,uint16_t(raw().size()),0};
 context.resource_owner=resource_owner_;
 for(size_t i=following_count_;i>0;--i){const auto& q=following_[(following_head_+i-1)%following_.size()];const auto key=q.event.usage;if(i==following_count_&&q.action==action&&(key<4||key>29))continue;if(key<4||key>29)break;++context.pending_letters;}
 traceUi(UiTraceKind::Intent,composition_.get(),window_.get(),&e,nullptr,reason,action,0,SIZE_MAX,&context);
}
bool DeviceIme::flowCurrent()const{
 if(flow_.stage==FlowStage::Admission){
  const auto target=editTarget();
  if(!raw_edit_||!reselection_.pending||flow_.parent!=reselection_.action||cursor_!=reselection_.origin||
     !target.valid()||target.begin!=reselection_.begin||target.end!=reselection_.end)return false;
 }
 return !flow_.guarded||(sink_.matches(flow_.target)&&composition_&&composition_->input_is_current(flow_.stamp)&&(window_&&window_->querySerial()==flow_.query&&window_->layoutSerial()==flow_.layout));
}
void DeviceIme::startFlow(const s3hid::Event& e,FlowStage stage){
 flow_.body_successor=false; // A selection intent is not an Accepted receipt.
 flow_.event=e;flow_.action=current_action_;flow_.stage=stage;flow_.captured=false;flow_.numbered=e.usage>=30&&e.usage<=35;flow_.column=flow_.numbered?uint8_t(e.usage-30):0;flow_.query=window_?window_->querySerial():0;
 flow_.layout=window_?window_->layoutSerial():0;flow_.target=sink_.target();flow_.guarded=bool(composition_);
 if(composition_)flow_.stamp=composition_->raw_input().stamp;
}
void DeviceIme::stopFlow(uint8_t reason){
 if(flow_.stage!=FlowStage::Idle)noteIntent(reason,flow_.event,flow_.action,flow_.parent);
 for(size_t i=0;i<following_count_;++i){const auto& q=following_[(following_head_+i)%following_.size()];noteIntent(reason,q.event,q.action,q.parent);following_[(following_head_+i)%following_.size()]={};}
 following_cancelled_+=uint32_t(following_count_);following_count_=following_head_=0;flow_={};
}
bool DeviceIme::enqueue(const s3hid::Event& e){
 if(following_count_==following_.size()){++following_overflow_;noteIntent(QueueFull,e,current_action_);mark("Input queue full; key rejected");return true;}
 if(e.usage>=30&&e.usage<=35&&!e.ime_context){noteIntent(UnknownNumberedRow,e,current_action_);return true;}
 // A queued successor is confined to the next initial row of this verified
 // predecessor chain. It never means an unknown navigation destination.
 following_[(following_head_+following_count_)%following_.size()]={e,current_action_,flow_.action,window_?window_->querySerial():0,composition_?composition_->raw_input().stamp:hanime_extension::InputStamp{},composition_?composition_->snapshot().focus:0};
 ++following_count_;following_peak_=std::max(following_peak_,uint32_t(following_count_));
 noteIntent(preparing_?QueuedCommit:QueuedSuccessor,e,current_action_,flow_.action);mark("Input queued in order");return true;
}
bool DeviceIme::selectIntent(const s3hid::Event& e,bool successor){
 if(!successor&&int32_t(e.ms-raw_event_ms_)<0){noteIntent(BeforeRawEdit,e,current_action_);mark("Choice predates raw edit");return true;}
 if(app_.findOpen()&&raw_edit_&&window_&&window_->focusedNeedsWholeText()){
  noteIntent(NoCandidate,e,current_action_);mark("Long candidate: load range to inspect");return true;
 }
 if(raw_edit_&&e.usage==0x28){noteIntent(UnassignedRawEnter,e,current_action_);mark("Raw Enter not assigned");return true;}
 if((!composition_||!window_)&&!successor&&!earlyContext(e)){noteIntent(DefaultContextExpired,e,current_action_);return true;}
 if(!composition_||!window_){
  // A fresh explicit choice may retry construction; service never spins retries.
  initialization_failed_=false;
  if(!raw().empty()&&(e.usage==0x2c||e.usage==0x28||((e.usage>=30&&e.usage<=35)&&earlyContext(e)))){startFlow(e,FlowStage::Query);noteIntent(QueuedQuery,e,current_action_);mark("Default queued until CN ready");}
  else {noteIntent(NoCandidate,e,current_action_);mark("Resources pending; selection unavailable");}
  return true;
 }
 if(composition_->snapshot().state==hanime_extension::CompositionState::AllSelected){
  if(e.ime_context&&!successor&&!earlyContext(e)){noteIntent(DefaultContextExpired,e,current_action_);return true;}
  startFlow(e,FlowStage::Commit);beginConfirm(e.ime_attempt,current_action_);
  if(!preparing_)stopFlow(CommitFailed);
  return true;
 }
 // Esc cancelled this query. Even a still-presented old frame cannot revive
 // its authority before the clearing frame reaches the LCD. Only Down or a raw
 // edit starts new work; a Space/digit here is neither a query nor a future intent.
 if(raw_edit_&&window_->state()==WindowState::Empty){
  noteIntent(NoCandidate,e,current_action_);mark("Raw edit: Down loads marked range");return true;
 }
 // An incomplete editing endpoint has no manual query. While its automatic
 // replacement is still pending, do not turn a raw-mode choice into a future
 // selection. A new key may select once the real automatic candidate is ready.
 if(raw_edit_&&window_->busy()&&!editTarget().valid()){
  noteIntent(NoCandidate,e,current_action_);mark("No complete raw range; wait for candidate");return true;
 }
 clearArrows();
 const auto* v=window_->presented(e.ime_frame);const VisibleItem* item=nullptr;
 if(!successor&&v&&v->count&&v->row>=v->rows[0].index&&v->row<v->rows[0].index+v->count){
  const auto& row=v->rows[v->row-v->rows[0].index];const auto col=e.usage>=30&&e.usage<=35?size_t(e.usage-30):v->col;
  if(col<row.count)item=&row.items[col];
 }
 const auto snapshot=composition_->snapshot();
 if(reselection_.active&&!successor&&!earlyContext(e)){noteIntent(QueryContextChanged,e,current_action_);mark("Reselection target expired");return true;}
 const bool valid=item&&targetMatches(*item)&&item->input==snapshot.input_id&&item->revision==snapshot.revision&&item->scheme==snapshot.scheme_epoch&&item->activation.mount_epoch()==snapshot.activation.mount_epoch()&&item->activation.shared_bundle_id()==snapshot.activation.shared_bundle_id();
 if(valid){
  const auto result=e.usage>=30&&e.usage<=35?window_->choose(e.usage-30,e.ime_frame,current_action_):window_->chooseHighlighted(e.ime_frame,current_action_);
  traceUi(UiTraceKind::Choice,composition_.get(),window_.get(),&e,item,uint8_t(result),current_action_);
  choiceFeedback(result);
  if(result==ChoiceResult::Captured){startFlow(e,FlowStage::Selecting);flowCaptured();noteIntent(CapturedDisplayed,e,current_action_);}
  else noteIntent(DisplayedChoiceExpired,e,current_action_);
  return true;
 }
 // An early demand may carry the physical nonzero frame that was already
 // superseded by the input edit at its ingress checkpoint. Other missing old
 // frames stay expired; a current displayed identity was handled above.
 const bool early_frame=earlyContext(e)&&(!e.ime_frame||e.ime_frame==superseded_frame_);
 if(!successor&&e.ime_frame&&e.ime_frame!=window_->onScreenGeneration()&&!early_frame){noteIntent(DisplayedChoiceExpired,e,current_action_);mark("Original displayed frame expired");return true;}
 const bool numbered=e.usage>=30&&e.usage<=35;
 if(!successor&&!earlyContext(e)){noteIntent(QueryContextChanged,e,current_action_);mark("Selection input/query expired");return true;}
 if(numbered){
  if((!successor&&!earlyContext(e))||!initial_numbered_||arrow_count_||(entering_&&!raw_edit_&&!initial_numbered_)||window_->requestedRow()!=0){noteIntent(UnknownNumberedRow,e,current_action_);mark("Numbered row not fixed; key rejected");return true;}
  startFlow(e,FlowStage::Query);const auto result=window_->chooseInitialNumber(e.usage-30,current_action_);
  if(result==ChoiceResult::Waiting){noteIntent(QueuedQuery,e,current_action_);mark("Initial numbered choice queued");}
  else if(result==ChoiceResult::Captured){flowCaptured();noteIntent(ResolvedDefault,e,current_action_);}
  else {stopFlow(window_->state()==WindowState::Ready?uint8_t(NumberAbsent):windowFailure());mark("Number missing from initial row");}
  return true;
 }
 // A new press on the current screen after editing creates a CURRENT query
 // demand. A genuinely older external frame must never be revived this way.
 if(!successor&&e.ime_frame!=window_->onScreenGeneration()&&!early_frame){noteIntent(DefaultContextExpired,e,current_action_);mark("Choice context expired");return true;}
 if(!initial_&&!raw_edit_&&!successor&&!(reselectionCurrent()&&initial_numbered_)){noteIntent(ExpiredInput,e,current_action_);mark("Navigation frame unavailable");return true;}
 startFlow(e,FlowStage::Query);
 const auto result=window_->chooseDefault(current_action_);
 if(result==ChoiceResult::Waiting){noteIntent(QueuedQuery,e,current_action_);mark("Default selection queued");}
 else if(result==ChoiceResult::Captured){flowCaptured();noteIntent(ResolvedDefault,e,current_action_);}
 else {stopFlow(windowFailure());mark("No candidate for this input");}
 return true;
}
void DeviceIme::flowCaptured(bool automatic){
 if(!composition_||!window_||flow_.captured)return;
 const auto* choice=window_->capturedChoice();if(!choice)return;
 flow_.choice=*choice;flow_.captured=true;flow_.stage=FlowStage::Selecting;
 const auto saved=current_action_;current_action_=flow_.action;
 captureSelectionCommit(*choice,flow_.event,automatic);current_action_=saved;
 raw_edit_=false; // Keep the Down origin until selection publishes or is cancelled.
}
bool DeviceIme::flowSelected(){
 if(flow_.stage!=FlowStage::Selecting)return true;
 const auto s=composition_->snapshot();const auto* choice=window_->completedChoice();
 if(window_->completion()==SelectionCompletion::Choice&&(!choice||choice->key!=flow_.choice.key||choice->ref!=flow_.choice.ref||choice->revision!=flow_.choice.revision||choice->begin!=flow_.choice.begin||choice->end!=flow_.choice.end)){stopFlow(ExpiredInput);return false;}
 bool member=false;for(size_t i=0;i<s.member_count;++i){const auto& m=s.members[i];if(m.valid&&m.key==flow_.choice.key&&m.ref==flow_.choice.ref&&m.begin==flow_.choice.begin&&m.end==flow_.choice.end)member=true;}
 if(!member||s.input_id!=flow_.stamp.input_id()||s.scheme_epoch!=flow_.stamp.scheme_epoch()||!sink_.matches(flow_.target)){stopFlow(ExpiredInput);return false;}
 flow_.stamp=composition_->raw_input().stamp;letters_since_choice_=edit_actions_=0;traceText(flow_.action,2,s.selected_text);return true;
}
void DeviceIme::flowAccepted(){
 if(flow_.stage==FlowStage::Idle)return;
 // Only this exact Accepted callback authorizes the successor body target.
 // No external focus or later document edit can be silently rebound.
 if(!acceptedTargetCurrent()){stopFlow(ExpiredTarget);return;}
 EditorSink guard(app_);if(!guard.bind(0,0)){stopFlow(ExpiredTarget);return;}
 flow_.target=guard.target();flow_.stamp=composition_->raw_input().stamp;flow_.query=window_->querySerial();flow_.layout=window_->layoutSerial();
 if(accepted_target_.kind==TextTarget::Body)traceText(flow_.action,4,app_.document.text());
 flow_.body_successor=true;
 noteIntent(Completed,flow_.event,flow_.action,flow_.parent);flow_.stage=FlowStage::Idle;
}
void DeviceIme::serviceFollowing(){
 if(flow_.stage!=FlowStage::Idle||preparing_||!following_count_)return;
 if(!flowCurrent()){stopFlow(sink_.matches(flow_.target)?ExpiredInput:ExpiredTarget);return;}
 const auto q=following_[following_head_];following_[following_head_]={};following_head_=(following_head_+1)%following_.size();--following_count_;
 current_action_=q.action;flow_.parent=q.parent;
 noteIntent(Executing,q.event,q.action,q.parent);
 if(q.event.usage==54||q.event.usage==55){
  // The queue is a provisional intent. Only the exact successful predecessor
  // may hand it to the existing body key mapping, never a partial selection.
  if(!flow_.body_successor||!raw().empty()||has_target_||quarantined_){
   ++metrics.blocked;noteIntent(AppBlocked,q.event,q.action,q.parent);
   mark("Finish CN before punctuation");
  }else{
   // flowCurrent() checked the post-Accepted document, caret and selection.
   // Consume this queue entry before calling the editor; an uncertain throw
   // must retire later events, never replay this possibly applied body key.
   // This queue is entered only by Chinese dispatch. Accepted may have reset
   // composition; preserve the originating body's mode as an operation arg.
   try {app_.key(q.event,true);}catch(...){stopFlow(CommitFailed);mark("Body key uncertain; no replay");return;}
   EditorSink guard(app_);
   if(!guard.bind(0,0)){stopFlow(ExpiredTarget);return;}
   flow_.target=guard.target();
   noteIntent(BodyRouted,q.event,q.action,q.parent);
  }
  if(!following_count_)flow_.guarded=false;
  return;
 }
 // Dispatch an operation, not a synthetic physical event. Original ingress
 // frame/attempt/time/epoch remain untouched in the owned receipt.
 if(!dispatch(q.event,true)){noteIntent(UnassignedIdleChoice,q.event,q.action,q.parent);}
 if(flow_.stage==FlowStage::Idle&&composition_){
  // This synchronous dispatch alone may have edited raw and started its query.
  flow_.stamp=composition_->raw_input().stamp;flow_.query=window_->querySerial();flow_.layout=window_->layoutSerial();
  if(has_target_)flow_.target=sink_.target();
 }
 if(!following_count_&&flow_.stage==FlowStage::Idle)flow_.guarded=false;
}
}
