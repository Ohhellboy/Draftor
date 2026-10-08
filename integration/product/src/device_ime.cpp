#include "behavior_bench.hpp"
#include "device_ime.hpp"
#include "wubi_product.hpp"
#include "behavior_observer.hpp"
#include "ui_trace.hpp"
#include "ime_presentation_policy.hpp"
#include <cstring>
#include <stdexcept>
namespace ime_device {
using S=hanime_extension::Status;using CS=hanime::CompositionStatus;
namespace {
const char* resultName(CS status) {
 switch(status) {
 case CS::Accepted:return "Accepted by editor";
 case CS::RetryableNoEffect:return "Rejected: Space retries, Esc edits";
 case CS::UnknownOutcome:return "Unknown: no replay";
 case CS::Cancelled:return "Cancelled";
 case CS::CapacityExceeded:return "Submission capacity reached; input retained";
 case CS::AllocationFailure:return "Submission allocation failed; input retained";
 case CS::OutputRestricted:return "Submission output unavailable; input retained";
 case CS::ReadError:return "Submission read error; input retained";
 case CS::Stale:return "Submission expired; input retained";
 case CS::Busy:return "Submission busy; input retained";
 default:return "Submission stopped: see log";
 }
}
}
DeviceIme::~DeviceIme(){submission_.reset();window_.reset();navigator_.reset();composition_.reset();}
void DeviceIme::mark(const char* t){notice_=t;app_.dirtyDisplay=true;}
bool DeviceIme::active()const{return following_count_||flow_.stage!=FlowStage::Idle||!raw().empty()||preparing_||quarantined_;}
bool DeviceIme::fresh(const s3hid::Event& e){
 if(epoch_!=e.epoch){held_={};epoch_=e.epoch;}
 auto& h=held_[e.usage];
 if(!e.down){
  // A late release must not unlatch a newer physical press. Remember the
  // release watermark so replaying a held-repeat after it cannot reach body.
  if(h.seen&&int32_t(e.ms-h.ms)<0)return false;
  if(h.seen)h.ms=e.ms;
  h.down=false;
  return false;
 }
 if(h.down){
  if(int32_t(e.ms-h.ms)>0)h.ms=e.ms;
  ++metrics.duplicates;
  return false;
 }
 if(h.seen&&int32_t(e.ms-h.ms)<=0){++metrics.duplicates;return false;}
 h={e.ms,true,true,h.ime};return true;
}
void DeviceIme::boundary(BoundaryEdge edge){boundary_edge_=edge;boundary_until_=uint32_t(clockUs()/1000)+250;boundary_active_=true;mark("Boundary");}
void DeviceIme::serviceVisuals(uint32_t now){
 if(boundary_active_&&int32_t(now-boundary_until_)>=0){
  boundary_active_=false;app_.dirtyDisplay=true;
 }
}
void DeviceIme::registerAttempt(){
 if(next_attempt_serial_==UINT32_MAX){submission_->cancel(attempt_);preparing_=false;mark("Attempt queue exhausted");return;}
 attempt_serial_=++next_attempt_serial_;attempts_[attempt_serial_%2]={attempt_serial_,attempt_};
 if(publish_attempt_)publish_attempt_(attempt_serial_);
}
bool DeviceIme::cancelAttempt(uint32_t serial){
 if(!submission_||!serial){mark("Cancel event expired");return false;}
 const auto& slot=attempts_[serial%2];
 if(slot.serial!=serial){mark("Cancel event expired");return false;}
 const auto result=submission_->cancel(slot.handle); // ORIGINAL captured handle.
 if(serial!=attempt_serial_||result.core.status!=CS::Cancelled){mark("Cancel event expired or quarantined");return false;}
 receipt_=result.core;preparing_=false;return true;
}
void DeviceIme::discard(){
 if(auto_request_)auto_request_->pending=false;
 initialization_failed_=false;
 selected_viewport_.reset();manual_failure_=WindowState::Empty;reselection_={};
 invalidateIngress();
 stopFlow(CancelledByUser);
 boundary_active_=false;
 clearArrows();
 clearSelectionCommit();
 waiting_size_=0;waiting_raw_={};confirm_requested_=false;
 if(submission_&&receipt_.status!=CS::UnknownOutcome&&receipt_.status!=CS::Accepted){receipt_=submission_->cancel(attempt_).core;preparing_=false;}
 if(quarantined_){mark("Unknown: input quarantined");return;}
 if(window_)window_->cancel();
 if(composition_){auto s=composition_->cancel_input(composition_->snapshot().input_id);if(s!=S::Ready&&s!=S::Cancelled){mark("Cancel refused");return;}}
 BT_OBSERVE(ended(current_action_,2));
 app_.endCompositionPresentation();if(window_)window_->endSession();
 preparing_=false;attempt_serial_=0;if(publish_attempt_)publish_attempt_(0);
 sink_.invalidate();has_target_=false;raw_edit_=false;initial_=true;entering_=false;cursor_=0;mark(chinese_?"CN":"EN");
}
void DeviceIme::interrupt(){
 stopFlow(InvalidContext);clearArrows(InvalidContext);
 if(quarantined_){chinese_=false;mark("Unknown retained; EN available");return;}
 discard();
 // Keep old press/release receipts across target interruption. A new HID
 // connection epoch resets them in fresh(); late old keys cannot reach body.
}
void DeviceIme::resourceFailed(){
 if(chinese_&&!resource_owner_retired_){
  // Retire before any cancellation/destruction. The BLE publication stays old
  // until service returns, so arrivals throughout cleanup cannot reach EN.
  // A new retirement requires a new CN-entry key to have passed the physical
  // FIFO. Live events cannot span an entire generation cycle; zero is invalid.
  resource_owner_=resource_owner_==UINT32_MAX?1:resource_owner_+1;
  resource_owner_retired_=true;
 }
 stopFlow(ResourceFailure);clearArrows(ResourceFailure);
 if(quarantined_||receipt_.status==CS::UnknownOutcome){
  quarantined_=true;chinese_=false;
  if(std::strcmp(notice_,"Unknown retained; resource failed; EN available")!=0)mark("Unknown retained; resource failed; EN available");
  return;
 }
 if(composition_||chinese_||active()||has_target_){
  // Use the original attempt. Accepted facts remain; destruction is not an
  // assertion that a stale/refused cancellation succeeded. No replacement session.
  discard();submission_.reset();window_.reset();navigator_.reset();composition_.reset();
  waiting_size_=0;waiting_raw_={};preparing_=confirm_requested_=false;
  sink_.invalidate();has_target_=false;raw_edit_=false;initial_=true;entering_=false;cursor_=0;
  attempt_serial_=0;if(publish_attempt_)publish_attempt_(0);
  chinese_=false;mark(resources_.error().c_str());
 }
 resources_.releaseFailed();
}
void DeviceIme::switchScheme(){
 if(quarantined_){mark("Unknown: scheme switch unavailable");return;}
 discard();submission_.reset();window_.reset();navigator_.reset();composition_.reset();
 // Retire queued arrivals before activating the next scheme. Old raw letters,
 // choices and shortcuts cannot be reinterpreted by the replacement engine.
 resource_owner_=resource_owner_==UINT32_MAX?1:resource_owner_+1;
 using Rules=hanime_extension::SchemeRules;
 const auto current=resources_.rules();
 const auto r=current==Rules::R1Xiaohe?Rules::FullPinyin:
              current==Rules::FullPinyin?Rules::DirectExact:Rules::R1Xiaohe;
 app_.compositionScheme(r==hanime_extension::SchemeRules::FullPinyin);
 resources_.start(r);mark("Scheme loading; input discarded");
}
// Keep the borrowed snapshot's stack frame separate from navigation analysis.
// This is called after semantic updates even when Vertical is hidden.
[[gnu::noinline]] void DeviceIme::observeCompositionPresentation(){
 const auto snapshot=composition_->snapshot();
 app_.observeComposition(resources_.rules()==hanime_extension::SchemeRules::FullPinyin,
     s3app::requiredCompositionWidth(snapshot.raw,presentationNavigation()),snapshot.member_count!=0);
}
bool DeviceIme::navigationReady(){
 using N=hanime_extension::NavigationStatus;
 if(!navigator_||!composition_){mark("CN resources pending");return false;}
 // A valid handle means this revision was already observed, in H or V.
 // The navigator invalidates it on identity/revision/scheme changes.
 if(navigator_->view(navigation_,*composition_).status==N::Ready)return true;
 const auto start=clockUs();const auto io=metrics.io();const auto result=navigator_->analyze(*composition_);
 const auto cost=navigator_->cost();
 metrics.step(Phase::Navigation,start,0,0,cost.table_entries+cost.character_comparisons,io);
 if(result.status!=N::Ready){mark(result.status==N::CapacityExceeded?"Navigation capacity; raw retained":"Navigation stale; raw retained");return false;}
 navigation_=result.handle;
 observeCompositionPresentation();return true;
}
bool DeviceIme::moveBoundary(size_t pos,hanime_extension::NavigationDirection direction){
 if(!navigationReady())return false;
 auto move=navigator_->neighbor(navigation_,*composition_,pos,direction);
 // Keep the established member-reopen path before focus. In the unselected
 // suffix use public complete spelling endpoints, never a private parser.
 // This borrowed view is consumed now and is not retained across a tick.
 const auto endpoints=navigator_->prefix_endpoints(navigation_,*composition_);
 if(endpoints.status!=hanime_extension::NavigationStatus::Ready){
  mark("Navigation stale; raw retained");return false;
 }
 if(resources_.rules()!=hanime_extension::SchemeRules::DirectExact&&pos>=endpoints.begin){
  if(direction==hanime_extension::NavigationDirection::Right){
   for(size_t i=0;i<endpoints.count;++i){
    if(endpoints.ends[i]>pos){move={hanime_extension::NavigationStatus::Ready,endpoints.ends[i]};break;}
   }
  }else if(pos>endpoints.begin){
   size_t previous=endpoints.begin;
   for(size_t i=0;i<endpoints.count&&endpoints.ends[i]<pos;++i)previous=endpoints.ends[i];
   move={hanime_extension::NavigationStatus::Ready,previous};
  }
 }
 using N=hanime_extension::NavigationStatus;
 if(move.status!=N::Ready&&move.status!=N::AtEdge){mark("Navigation stale; raw retained");return false;}
 cursor_=move.offset;
 if(move.status==N::AtEdge){boundary(direction==hanime_extension::NavigationDirection::Left?BoundaryEdge::RawLeft:BoundaryEdge::RawRight);return true;}
 const auto view=navigator_->view(navigation_,*composition_);
 mark(resources_.rules()==hanime_extension::SchemeRules::DirectExact?"Raw code editor":view.ambiguous?"Raw syllables (ambiguous)":view.has_unknown?"Raw syllables (unknown)":view.has_incomplete?"Raw syllables (incomplete)":"Raw syllable editor");
 return true;
}
bool DeviceIme::candidateAvailable()const{
 if(!window_||!composition_||preparing_||reselection_.pending)return false;
 const auto& v=window_->view();if(!v.count)return false;
 const auto& i=v.rows[0].items[0];const auto s=composition_->snapshot();
 return targetMatches(i)&&i.input==s.input_id&&i.revision==s.revision&&i.scheme==s.scheme_epoch&&i.activation.mount_epoch()==s.activation.mount_epoch()&&i.activation.shared_bundle_id()==s.activation.shared_bundle_id();
}
void DeviceIme::escapeComposition(const s3hid::Event& event){
 // Resource-owner, quarantine and ingress fences run before this entry.
 // Never turn a stale flow/target into authority to cancel its replacement.
 if(reselection_.active&&!raw_edit_&&!preparing_&&(!earlyContext(event)||!has_target_)){
  noteIntent(QueryContextChanged,event,current_action_);mark("Reselection target expired");return;
 }
 if((has_target_&&!sink_.current())||
    ((flow_.stage!=FlowStage::Idle||following_count_)&&
     (!flowCurrent()||event.epoch!=flow_.event.epoch||event.route_epoch!=flow_.event.route_epoch))){
  noteIntent(InvalidContext,event,current_action_);mark("Return context expired");return;
 }
 if(preparing_){
  if(event.ime_attempt!=attempt_serial_){noteIntent(ExpiredInput,event,current_action_);mark("Return attempt expired");return;}
  // Before prepare there is no new transaction to cancel. A queued retry
  // still owns the previous no-effect attempt and must retire that handle.
  if((!confirm_requested_||retry_requested_)&&!cancelAttempt(event.ime_attempt))return;
 }else if(receipt_.status==CS::RetryableNoEffect&&!cancelAttempt(event.ime_attempt))return;
 // Shared Esc ladder: candidate -> raw -> cancel uncommitted composition.
 // Existing discard retires this input without undoing accepted body text.
 if (raw_edit_) {
  discard();
  noteIntent(CancelledByUser, event, current_action_);
  return;
 }
 if(auto_request_)auto_request_->pending=false;
 const bool manual=reselectionCurrent();
 size_t endpoint=manual?reselection_.origin:0;
 if(!manual&&composition_&&window_){
  const auto snap=composition_->snapshot();
  endpoint=snap.focus;
  const auto* visible=window_->presented(window_->onScreenGeneration());
  bool found=false;
  if(visible&&visible->count&&visible->row>=visible->rows[0].index&&visible->row<visible->rows[0].index+visible->count){
   const auto& row=visible->rows[visible->row-visible->rows[0].index];
   if(visible->col<row.count){const auto& item=row.items[visible->col];
    found=targetMatches(item)&&item.input==snap.input_id&&item.revision==snap.revision&&
     item.scheme==snap.scheme_epoch&&item.activation.mount_epoch()==snap.activation.mount_epoch()&&
     item.activation.shared_bundle_id()==snap.activation.shared_bundle_id();
    if(found)endpoint=item.end;
   }
  }
  if(!found&&navigationReady()){
   const auto ends=navigator_->prefix_endpoints(navigation_,*composition_);
   if(ends.status==hanime_extension::NavigationStatus::Ready&&ends.count)endpoint=ends.ends[0];
  }
 }
 stopFlow(CancelledByUser);clearArrows(CancelledByUser);clearSelectionCommit();
 preparing_=confirm_requested_=retry_requested_=false;attempt_serial_=0;
 if(publish_attempt_)publish_attempt_(0);
 if(window_){window_->cancel();window_->retirePresentedChoices();}
 reselection_={};manual_failure_=WindowState::Empty;boundary_active_=false;
 raw_edit_=true;initial_=false;entering_=false;
 cursor_=endpoint;
 invalidateIngress();
 if(composition_)navigationReady();
 noteIntent(CancelledByUser,event,current_action_);
 mark("Raw edit: Down loads marked range");
}
void DeviceIme::enterRaw(){
 const auto snap=composition_?composition_->snapshot():hanime_extension::Snapshot{};
 const bool continuation=snap.focus>0&&snap.focus<snap.raw.size();
 if(moveBoundary(continuation?snap.focus:raw().size(),continuation?
    hanime_extension::NavigationDirection::Right:hanime_extension::NavigationDirection::Left)){
  raw_edit_=true;
  if(auto_request_)auto_request_->pending=false;
 }
}
uint8_t DeviceIme::edit(size_t begin, size_t end, std::string_view text, uint32_t original_attempt) {
 // Capacity only preserves a still-valid owner. Health/target checks precede
 // admission so a full buffer cannot keep stale resource or editor authority.
 if (has_target_ && !sink_.current()) {
  interrupt();
  mark("Input target expired");
  return ExpiredTarget;
 }
 if (composition_ && (!resources_.ready() || !composition_->valid())) {
  resources_.service();
  if (resources_.state() == ResourceState::Error) resourceFailed();
  else interrupt();
  mark("Input resources expired");
  return ResourceFailure;
 }
 const auto admission = wubi()?admitWubiEdit(raw(),begin,end,text):admitRawEdit(raw().size(), begin, end, text);
 if(resources_.rules()==hanime_extension::SchemeRules::DirectExact&&!wubi())return EditInvalid;
 if (admission == RawAdmission::Invalid) {
  mark("Invalid raw edit");
  return EditInvalid;
 }
 if (admission == RawAdmission::Capacity) {
  mark("Raw capacity reached");
  return CapacityRejected;
 }
 if (admission == RawAdmission::NoChange) return EditNoChange;
 if (composition_) {
  const auto snapshot = composition_->snapshot();
  if (snapshot.revision == UINT64_MAX || (snapshot.raw.empty() && snapshot.input_id == UINT64_MAX)) {
   mark("Input identity capacity reached");
   return CapacityRejected;
  }
 }
 // A new exact-code edit retires only the no-effect attempt captured by
 // this physical event. Rejected edits above retain its original scene.
 if(wubi()&&receipt_.status==CS::RetryableNoEffect&&!cancelAttempt(original_attempt))return ExpiredInput;
 if (!has_target_) {
  if (!sink_.bind(0, 0)) {
   mark("No editable target");
   return ExpiredTarget;
  }
  has_target_ = true;
 }
 if(auto_request_)auto_request_->pending=false;
 // Only an admitted edit may retire this adapter's original query tickets.
 // No old Selection is extended or recreated to make a rejected edit work.
 reselection_ = {};
 manual_failure_ = WindowState::Empty;
 clearArrows();
 clearSelectionCommit();
 if (!composition_) {
  std::memmove(waiting_raw_.data() + begin + text.size(), waiting_raw_.data() + end, waiting_size_ - end);
  std::copy(text.begin(), text.end(), waiting_raw_.begin() + begin);
  waiting_size_ = waiting_size_ - (end - begin) + text.size();
  waiting_raw_[waiting_size_] = 0;
 } else {
  window_->cancel();
  const auto status = composition_->edit_raw(begin, end, text);
  if (status != S::Ready) {
   // Admission is only a size/range check. A submission lock or a later core
   // failure still refuses the real edit. Retired work stays retired; do not
   // reopen a query against Busy/Unknown or pretend the revision advanced.
   const auto result = status == S::Busy ? EditBusy : status == S::CapacityExceeded ? CapacityRejected :
                       status == S::InvalidRequest ? EditInvalid : EditFailed;
   stopFlow(result);
   mark(hanime_extension::name(status));
   return result;
  }
 }
 BT_OBSERVE(raw(*this,current_action_,text.empty()?3:1));
 initial_numbered_ = true;
 raw_event_ms_ = current_event_ms_;
 if (begin != end && edit_actions_ < UINT16_MAX) ++edit_actions_;
 cursor_ = begin + text.size();
 initial_ = true;
 if (composition_) navigationReady();
 if (raw().empty()) {
  discard();
  return RawEdited;
 }
 app_.beginCompositionPresentation(resources_.rules()==hanime_extension::SchemeRules::FullPinyin);
 if (composition_) {
  window_->query(0, current_action_);
  mark("Query pending");
 } else {
  mark("Raw buffered");
 }
 return RawEdited;
}
uint8_t DeviceIme::append(char c,uint32_t attempt) {
 const auto pos = raw_edit_ ? cursor_ : raw().size();
 return edit(pos, pos, std::string_view(&c, 1),attempt);
}
void DeviceIme::rawDown(){
 clearArrows();clearSelectionCommit();manual_failure_=WindowState::Empty;
 if(!composition_||!window_){mark("CN resources pending");return;}
 // DirectExact has no phonetic endpoints: the caret never defines its range.
 if(resources_.rules()==hanime_extension::SchemeRules::DirectExact){
  reselection_={};
  const auto* current=window_->focusedCurrent();
  const bool ready=window_->state()==WindowState::Ready&&candidateAvailable()&&
   current&&current->begin==0&&current->end==raw().size();
  // query(0) delegates to query_next(), whose DirectExact branch chooses the
  // complete current input before the planner path. Edits already issue it.
  if(window_->state()==WindowState::Empty||window_->state()==WindowState::Stale||
     (window_->state()==WindowState::Ready&&!ready))window_->query(0,current_action_);
  if(window_->busy()){
   reselection_={composition_->raw_input().stamp,window_->querySerial(),0,uint16_t(raw().size()),false,true,uint16_t(cursor_),current_action_};
   mark("Full code candidates pending");return;
  }
  if(ready){
   raw_edit_=false;initial_=true;entering_=false;initial_numbered_=true;
   window_->retirePresentedChoices();invalidateIngress();
   mark("Full code candidates ready");
  }else mark(window_->state()==WindowState::NoResults?"No candidates; raw retained":"Candidates unavailable; raw retained");
  return;
 }
 const auto target=editTarget();
 if(!target.valid()){
  boundary(BoundaryEdge::RawLeft);mark("No Down range here; move Left/Right");return;
 }
 // Do not cancel a valid automatic query to discover that a manual endpoint
 // is rejected. Wait on this exact query/stamp, then ask the public API.
 if(window_->busy()){
  reselection_={composition_->raw_input().stamp,window_->querySerial(),uint16_t(target.begin),uint16_t(target.end),false,true,uint16_t(cursor_),current_action_};
  mark("Down range waiting for current query");return;
 }
 if(target.kind==EditTargetKind::Member){
  window_->cancel();
  const auto status=composition_->reopen_segment(target.member);
  if(status!=S::Ready){mark(hanime_extension::name(status));return;}
  BT_OBSERVE(raw(*this,current_action_,4));
  // Reopening a confirmed member is an explicit new target, not a requery.
  window_->beginCandidateStage();
 }
 const auto status=window_->tryPrefix(target.end,current_action_);
 if(status==S::InvalidRequest){
  if(target.kind==EditTargetKind::Member){
   window_->query(0,current_action_);mark("Reopened range rejected; auto pending");
  }else mark("Manual range rejected; auto kept");
  return;
 }
 reselection_={composition_->raw_input().stamp,window_->querySerial(),uint16_t(target.begin),uint16_t(target.end),true,false,uint16_t(cursor_),current_action_};
 raw_edit_=false;initial_=false;entering_=true;initial_numbered_=true;
 mark("Manual range pending");
}
bool DeviceIme::recoverManual(){
 if(!reselectionCurrent()||window_->busy()||flow_.stage==FlowStage::Selecting)return false;
 const auto failed=window_->state();
 if(failed!=WindowState::NoResults&&failed!=WindowState::Unresolved&&failed!=WindowState::Limited&&failed!=WindowState::Error&&failed!=WindowState::Stale)return false;
 const auto origin=reselection_.origin;const auto action=reselection_.action;
 stopFlow(windowFailure());clearArrows();clearSelectionCommit();reselection_={};
 raw_edit_=true;cursor_=origin;initial_=true;entering_=false;
 // One recovery only. The replacement has no active manual ticket, so its
 // failure is reported normally rather than retried on every service slice.
 window_->query(0,action);manual_failure_=failed;
 mark(failed==WindowState::NoResults?"Manual no match; auto pending":
      failed==WindowState::Limited?"Manual budget limit; auto pending":
      failed==WindowState::Unresolved?"Manual unresolved; auto pending":
      failed==WindowState::Stale?"Manual expired; auto pending":"Manual read error; auto pending");
 return true;
}
void DeviceIme::rawKey(const s3hid::Event& event){
 const auto key = event.usage;
 if (key != 0x2a && key != 0x4c) reselection_.pending=false;
 if(key==0x50){moveBoundary(cursor_,hanime_extension::NavigationDirection::Left);}
 else if(key==0x4f){moveBoundary(cursor_,hanime_extension::NavigationDirection::Right);}
 else if(key==0x2a){if(cursor_)noteIntent(edit(cursor_-1,cursor_,{},event.ime_attempt),event,current_action_);else {boundary(BoundaryEdge::RawLeft);noteIntent(EditNoChange,event,current_action_);}}
 else if(key==0x4c){if(cursor_<raw().size())noteIntent(edit(cursor_,cursor_+1,{},event.ime_attempt),event,current_action_);else {boundary(BoundaryEdge::RawRight);noteIntent(EditNoChange,event,current_action_);}}
 else if(key==0x51){rawDown();}
 else mark("Raw command not assigned");
 if((key==0x50||key==0x4f)&&window_&&window_->state()==WindowState::Empty&&!boundary_active_)mark("Raw edit: Down loads marked range");
}
void DeviceIme::navigate(uint8_t key,uint32_t frame){
 invalidateIngress();initial_numbered_=false;
 clearSelectionCommit();
 if(initial_&&!entering_){
  if(key==0x50){enterRaw();return;}
  if(key==0x52){boundary(BoundaryEdge::CandidateTop);appliedNavigation(current_action_,0,0,true);return;}
  if(window_){noteNavigation(QueuedNavigation,current_action_);inheritNavigation(current_action_);nav_request_action_=current_action_;arrow_stamp_=composition_->raw_input().stamp;arrow_query_=window_->querySerial();pending_boundary_=key==0x51?BoundaryEdge::CandidateBottom:BoundaryEdge::CandidateRight;window_->request(key==0x51?1:0,key==0x4f?1:0,key==0x51,true,current_action_);entering_=true;mark("Candidate target pending");}return;
 }
 if(!window_||!composition_){noteNavigation(InvalidContext,current_action_);return;}
 const auto* v=window_->presented(frame);
 if((!v||!v->count)&&!entering_){noteNavigation(ExpiredInput,current_action_);mark("Candidate frame expired");return;}
 if(v&&v->count){
  const auto s=composition_->snapshot();const auto& item=v->rows[0].items[0];
  if(item.input!=s.input_id||item.revision!=s.revision||item.scheme!=s.scheme_epoch||item.activation.mount_epoch()!=s.activation.mount_epoch()||item.activation.shared_bundle_id()!=s.activation.shared_bundle_id()){
   noteNavigation(ExpiredInput,current_action_);mark("Candidate frame expired");return;
  }
 }
 if(arrow_count_==arrows_.size()){++arrow_overflow_;++metrics.blocked;noteNavigation(QueueFull,current_action_);mark("Navigation queue full; key not applied");return;}
 if(!arrow_count_&&!nav_request_action_){arrow_stamp_=composition_->raw_input().stamp;arrow_query_=window_->querySerial();}
 arrows_[(arrow_head_+arrow_count_)%arrows_.size()]={current_action_,key};
 ++arrow_count_;arrow_peak_=std::max(arrow_peak_,uint32_t(arrow_count_));
 noteNavigation(QueuedNavigation,current_action_);
 serviceArrows();
}

void DeviceIme::spatialNavigate(ImeAction action,uint32_t frame){
 invalidateIngress();clearSelectionCommit();initial_numbered_=false;
 if(!window_||!composition_)return;
 const auto* shown=window_->presented(frame);
 if(!shown||!shown->count){noteNavigation(ExpiredInput,current_action_);mark("Candidate frame expired");return;}
 const auto snap=composition_->snapshot();const auto& item=shown->rows[0].items[0];
 if(item.input!=snap.input_id||item.revision!=snap.revision||item.scheme!=snap.scheme_epoch||
    item.activation.mount_epoch()!=snap.activation.mount_epoch()||item.activation.shared_bundle_id()!=snap.activation.shared_bundle_id()){
  noteNavigation(ExpiredInput,current_action_);mark("Candidate frame expired");return;
 }
 // Vertical Up enters the common raw editor only before candidate browsing.
 // Preserve the current-frame checks and the existing initial/entering owner.
 if(action==ImeAction::PreviousCandidate&&initial_&&!entering_){enterRaw();return;}
 if(arrow_count_==arrows_.size()){++arrow_overflow_;noteNavigation(QueueFull,current_action_);return;}
 if(!arrow_count_&&!nav_request_action_){arrow_stamp_=composition_->raw_input().stamp;arrow_query_=window_->querySerial();}
 initial_=false;
 arrows_[(arrow_head_+arrow_count_)%arrows_.size()]={current_action_,0,action};
 ++arrow_count_;arrow_peak_=std::max(arrow_peak_,uint32_t(arrow_count_));
 noteNavigation(QueuedNavigation,current_action_);serviceArrows();
}
void DeviceIme::applySpatial(ImeAction action,uint32_t serial){
 inheritNavigation(serial);boundary_active_=false;
 const auto& view=window_->view();
 if(!view.count||view.row<view.rows[0].index||view.row>=view.rows[0].index+view.count)return;
 auto row=view.row,col=view.col;bool expand=view.expanded,edge=false;
 const auto count=view.rows[row-view.rows[0].index].count;
 switch(action){
  case ImeAction::PreviousCandidate:if(col)--col;else edge=true;break;
  case ImeAction::NextCandidate:if(col+1<count)++col;else if(!expand)expand=true;else edge=true;break;
  case ImeAction::ExpandOrNextColumn:if(!expand)expand=true;else ++row;break;
  case ImeAction::NextColumn:++row;expand=true;break;
  case ImeAction::PreviousColumn:if(expand&&row)--row;else edge=true;break;
  default:edge=true;break;
 }
 if(window_->rowBeyondEnd(row))edge=true;
 pending_boundary_=action==ImeAction::PreviousCandidate||action==ImeAction::PreviousColumn?BoundaryEdge::CandidateTop:BoundaryEdge::CandidateBottom;
 if(edge){boundary(pending_boundary_);appliedNavigation(serial,view.row,view.col,true);return;}
 nav_request_action_=serial;window_->request(row,col,expand,false,serial);settleNavigation();mark("Candidate target pending");
}

void DeviceIme::noteNavigation(uint8_t reason,uint32_t action,uint32_t parent){
 if(action==current_action_)event_result_=reason;
 const IntentContext context{parent,window_?window_->querySerial():0,uint16_t(cursor_),uint16_t(arrow_count_),letters_since_choice_,edit_actions_,uint8_t(resources_.rules()),sink_.target().session,sink_.target().stamp.focus,uint16_t(raw().size()),0};
 traceUi(UiTraceKind::Intent,composition_.get(),window_.get(),nullptr,nullptr,reason,action,0,SIZE_MAX,&context);
}
void DeviceIme::inheritNavigation(uint32_t next){
 if(nav_applied_action_&&nav_applied_action_!=next)noteNavigation(NavigationInherited,nav_applied_action_,next);
 nav_applied_action_=0;
}
void DeviceIme::appliedNavigation(uint32_t action,size_t row,size_t col,bool edge){
 BT_OBSERVE(navigation(*window_,action,row,col,edge));
 if(action==current_action_)event_result_=QueuedNavigation; // Suppress generic Applied fallback.
 traceUi(UiTraceKind::NavigationApplied,composition_.get(),window_.get(),nullptr,nullptr,uint8_t(edge),action,0,row,nullptr,col,arrow_count_);
 nav_applied_action_=action;
}
void DeviceIme::settleNavigation(){
 if((nav_request_action_||arrow_count_)&&(!window_||!composition_||!composition_->input_is_current(arrow_stamp_)||window_->querySerial()!=arrow_query_)){clearArrows(InvalidContext);return;}
 if(!nav_request_action_||!window_||window_->busy())return;
 const auto action=nav_request_action_;nav_request_action_=0;
 if(window_->state()==WindowState::Ready||window_->state()==WindowState::Boundary){
  const auto& v=window_->view();appliedNavigation(action,v.row,v.col,window_->state()==WindowState::Boundary);
 }else noteNavigation(InvalidContext,action);
}
void DeviceIme::clearArrows(uint8_t reason){
 if(nav_request_action_)noteNavigation(reason,nav_request_action_);
 for(size_t i=0;i<arrow_count_;++i)noteNavigation(reason,arrows_[(arrow_head_+i)%arrows_.size()].action);
 nav_request_action_=nav_applied_action_=0;arrow_batch_=false;
 arrow_discarded_+=uint32_t(arrow_count_);arrow_head_=arrow_count_=0;
}
void DeviceIme::applyArrow(uint8_t key,uint32_t action){
 inheritNavigation(action);
 boundary_active_=false;
 pending_boundary_=key==0x51?BoundaryEdge::CandidateBottom:key==0x52?BoundaryEdge::CandidateTop:key==0x50?BoundaryEdge::CandidateLeft:BoundaryEdge::CandidateRight;
 const auto& v=window_->view();
 if(!v.count||v.row<v.rows[0].index||v.row>=v.rows[0].index+v.count){boundary(pending_boundary_);appliedNavigation(action,v.row,v.col,true);return;}
 size_t row=v.row,col=v.col;const auto& current=v.rows[row-v.rows[0].index];
 if(key==0x4f){if(col+1<current.count)++col;else{++row;col=0;}}
 else if(key==0x50){if(col)--col;else if(row){--row;col=5;}else{boundary(pending_boundary_);appliedNavigation(action,v.row,v.col,true);return;}}
 else if(key==0x51)++row;
 else if(key==0x52){if(row)--row;else{boundary(pending_boundary_);appliedNavigation(action,v.row,v.col,true);return;}}
 nav_request_action_=action;window_->request(row,col,true,false,action);settleNavigation();mark("Candidate target pending");
}
void DeviceIme::serviceArrows(){
 settleNavigation();
 if(!arrow_count_)return;
 if(!composition_||!composition_->input_is_current(arrow_stamp_)||raw_edit_||preparing_){clearArrows(InvalidContext);return;}
 if(entering_||window_->busy())return;
 if(window_->state()!=WindowState::Ready&&window_->state()!=WindowState::Boundary){clearArrows(InvalidContext);return;}
 if(arrows_[arrow_head_].spatial!=ImeAction::Native){
  const auto arrow=arrows_[arrow_head_];arrow_head_=(arrow_head_+1)%arrows_.size();--arrow_count_;
  applySpatial(arrow.spatial,arrow.action);return;
 }
 if(!arrow_batch_&&arrow_count_==1){
  const auto arrow=arrows_[arrow_head_];arrow_head_=(arrow_head_+1)%arrows_.size();--arrow_count_;
  applyArrow(arrow.key,arrow.action);return;
 }
 if(!arrow_batch_){desired_row_=window_->view().row;desired_col_=window_->view().col;arrow_batch_=true;}
 for(size_t work=0;work<arrows_.size()&&arrow_count_;++work){
  const auto arrow=arrows_[arrow_head_];if(arrow.spatial!=ImeAction::Native)break;const auto key=arrow.key;
  inheritNavigation(arrow.action);
  size_t currentCount=0;
  if(!window_->rowCount(desired_row_,currentCount)){window_->probeRow(desired_row_,arrow.action);return;}
  size_t row=desired_row_,col=desired_col_;bool edge=false;
  if(key==0x4f){if(col+1<currentCount)++col;else {++row;col=0;}}
  else if(key==0x50){if(col)--col;else if(row){--row;col=5;}else edge=true;}
  else if(key==0x51)++row;
  else if(key==0x52){if(row)--row;else edge=true;}
  if(window_->rowBeyondEnd(row))edge=true;
  size_t count=0;
  if(!edge&&!window_->rowCount(row,count)){window_->probeRow(row,arrow.action);return;}
  pending_boundary_=key==0x51?BoundaryEdge::CandidateBottom:key==0x52?BoundaryEdge::CandidateTop:key==0x50?BoundaryEdge::CandidateLeft:BoundaryEdge::CandidateRight;
  if(edge)boundary(pending_boundary_);
  else {boundary_active_=false;desired_row_=row;desired_col_=std::min(col,count-1);}
  desired_action_=arrow.action;
  appliedNavigation(arrow.action,desired_row_,desired_col_,edge);
  arrow_head_=(arrow_head_+1)%arrows_.size();--arrow_count_;
 }
 if(!arrow_count_||arrows_[arrow_head_].spatial!=ImeAction::Native){arrow_batch_=false;nav_request_action_=desired_action_;window_->request(desired_row_,desired_col_,true,false,desired_action_);mark("Final navigation target pending");}

}
void DeviceIme::beginConfirm(uint32_t serial, uint32_t action) {
    if (!submission_ || !composition_ ||
        composition_->snapshot().state != hanime_extension::CompositionState::AllSelected) return;
    if (!has_target_ || !sink_.current()) {
        mark("Commit target expired; cancel input");
        return;
    }
    retry_requested_ = receipt_.status == CS::RetryableNoEffect;
    if (retry_requested_) {
        if (!serial || serial != attempt_serial_ || attempts_[serial % 2].serial != serial) {
            mark("Retry event expired");
            return;
        }
        requested_attempt_ = attempts_[serial % 2].handle;
    }
    const auto snapshot = composition_->snapshot();
    if (!sink_.bind(snapshot.input_id, snapshot.revision)) return;
    BT_OBSERVE(confirm(action));BT_MEASURE(confirm(action));
    traceText(action,3,snapshot.selected_text);
    confirm_action_ = action;
    confirm_requested_ = true;
    preparing_ = true;
    mark("Whole text confirmation queued");
    traceUi(UiTraceKind::CommitQueued, composition_.get(), window_.get(),
            nullptr, nullptr, 0, confirm_action_);
}

void DeviceIme::clearSelectionCommit() {
    selection_commit_ = {};
}

void DeviceIme::captureSelectionCommit(const VisibleItem& choice, const s3hid::Event& event, bool automatic) {
    clearSelectionCommit();
    if ((!automatic && ((event.usage != 0x2c && !(event.usage >= 30 && event.usage <= 35)) || event.mods)) || !composition_ || !has_target_ || !sink_.current()) return;
    selection_commit_.choice = choice;
    selection_commit_.stamp = composition_->raw_input().stamp;
    selection_commit_.target = sink_.target();
    selection_commit_.event = event; // Original event/attempt, never retagged on completion.
    selection_commit_.action = current_action_;
    selection_commit_.active = true;
}

bool DeviceIme::selectionCommitCurrent() const {
    if (!selection_commit_.active || !composition_ || !has_target_ || !sink_.current()) return false;
    if (!composition_->input_is_current(selection_commit_.stamp)) return false;
    const auto& original = selection_commit_.target;
    const auto& bound = sink_.target();
    return original.rename == bound.rename && original.document == bound.document && original.session == bound.session &&
           original.stamp.focus == bound.stamp.focus && original.stamp.revision == bound.stamp.revision &&
           original.stamp.selection == bound.stamp.selection && original.cursor == bound.cursor &&
           original.selected == bound.selected && original.begin == bound.begin && original.end == bound.end;
}

bool DeviceIme::advanceSelectionCommit() {
    if (!selection_commit_.active) return false;
    const auto expected = selection_commit_.rechecking ? SelectionCompletion::Recheck : SelectionCompletion::Choice;
    if (window_->completion() != expected) {
        clearSelectionCommit();
        return false;
    }
    const auto& original = selection_commit_.choice;
    const auto snapshot = composition_->snapshot();
    if (snapshot.input_id != original.input || snapshot.scheme_epoch != original.scheme ||
        snapshot.activation.mount_epoch() != original.activation.mount_epoch() ||
        snapshot.activation.shared_bundle_id() != original.activation.shared_bundle_id()) {
        clearSelectionCommit();
        return false;
    }
    if (!selection_commit_.rechecking) {
        const auto* completed = window_->completedChoice();
        if (!completed || completed->input != original.input || completed->revision != original.revision ||
            completed->key != original.key || completed->ref != original.ref ||
            completed->begin != original.begin || completed->end != original.end) {
            clearSelectionCommit();
            return false;
        }
    }
    bool member_verified = false;
    for (size_t i = 0; i < snapshot.member_count; ++i) {
        const auto& member = snapshot.members[i];
        if (member.valid && member.key == original.key && member.ref == original.ref &&
            member.begin == original.begin && member.end == original.end) {
            member_verified = true;
            break;
        }
    }
    if (!member_verified) {
        clearSelectionCommit();
        return false;
    }
    // Only a matched window-owned completion may advance this revision receipt.
    // Editing/external mutation is rejected before service; see space_commit tests.
    selection_commit_.stamp = composition_->raw_input().stamp;
    if (!selectionCommitCurrent()) {
        clearSelectionCommit();
        return false;
    }
    return true;
}
uint8_t DeviceIme::windowFailure()const {
 if(!window_)return NoCandidate;
 switch(window_->state()){
 case WindowState::Capacity:return SelectionCapacity;
 case WindowState::Busy:return SelectionBusy;
 case WindowState::Stale:return DisplayedChoiceExpired;
 case WindowState::OutputError:return SelectionOutputError;
 case WindowState::Unresolved:return QueryUnresolved;
 case WindowState::Limited:return QueryLimited;
 case WindowState::Error:
  switch(window_->lastStatus()){
  case S::ResourceError:return ResourceFailure;
  case S::Cancelled:return SelectionCancelled;
  default:return SelectionInvalid;
  }
 default:return NoCandidate;
 }
}
void DeviceIme::choiceFeedback(ChoiceResult result){
 switch(result){
 case ChoiceResult::Captured:boundary_active_=false;raw_edit_=false;mark("Selecting displayed candidate");break;
 case ChoiceResult::Waiting:mark("Candidate pending; press when shown");break;
 case ChoiceResult::Busy:mark("Selection pending");break;
 case ChoiceResult::NoCandidate:mark("No displayed candidate");break;
 case ChoiceResult::Boundary:pending_boundary_=BoundaryEdge::CandidateRight;boundary(pending_boundary_);break;
 case ChoiceResult::ExpiredFrame:case ChoiceResult::ExpiredInput:
  // Restore current work only. This failed event never authorizes a future frame.
  if(raw_edit_&&window_&&!window_->busy())window_->query(0,current_action_);
  mark("Candidate expired; wait, then choose shown");break;
 }
}
bool DeviceIme::event(const s3hid::Event& e,ImeAction action){
 struct Restore{ImeAction& field;~Restore(){field=ImeAction::Native;}} restore{spatial_action_};
 spatial_action_=action;
 IngressScope ingress_scope(*this);
 const bool owns=chinese_&&active();const bool first=fresh(e);
 if(first){current_action_=next_action_<UINT32_MAX?++next_action_:0;held_[e.usage].action=current_action_;BT_OBSERVE(input(e,current_action_));BT_MEASURE(input(e,current_action_));}
 if(e.usage<4||e.usage>29)traceUi(!e.down?UiTraceKind::KeyUp:first?UiTraceKind::KeyDown:UiTraceKind::Duplicate,composition_.get(),window_.get(),&e,nullptr,0,held_[e.usage].action);
 if(!e.resource_owner||e.resource_owner!=resource_owner_){
  // fresh() has already processed releases/watermarks. Consume stale presses
  // before every shortcut/EN branch; the main-loop handled path clears repeat.
  held_[e.usage].ime=true;
  if(e.down&&first)noteIntent(ResourceOwnerRetired,e,current_action_);
  return true;
 }
 if(!e.down)return owns||held_[e.usage].ime;
 if(!first)return true;
 noteIntent(Received,e,current_action_);
 if(e.usage==0x2c||e.usage==0x28||(e.usage>=30&&e.usage<=35))traceText(current_action_,1,raw());
 const bool raw_navigation=raw_edit_||(initial_&&!entering_&&e.usage==0x50);
 const auto before_query=window_?window_->querySerial():0;
 const auto handled=dispatch(e);
 BT_OBSERVE(state());
 // Ordered physical editing is an explicit causal predecessor, not an external
 // query replacement. Retain its arrival token for later keys in that HID batch.
 if(e.ime_context&&((e.usage>=4&&e.usage<=29)||e.usage==0x2a||e.usage==0x4c)&&
    ((window_&&window_->querySerial()!=before_query)||(!composition_&&waiting_size_)))ingress_causal_=e.ime_context;
 // A handled key alone is not evidence of a successful raw modification.
 if(event_result_==Received)noteIntent(handled?(raw_navigation&&e.usage>=0x4f&&e.usage<=0x52?RawNavigation:Consumed):BodyRouted,e,current_action_);
 return handled;
}
bool DeviceIme::dispatch(const s3hid::Event& e,bool successor){
 current_event_ms_=e.ms;
 if(!following_count_&&flow_.stage==FlowStage::Idle)flow_.guarded=false;
 const bool body=app_.imeTextTargetAvailable();
 const bool chord=(e.mods&0x99)&&!(e.mods&0x66)&&e.usage==0x2c;
 if(!successor)held_[e.usage].ime=(chinese_&&active())||(body&&(e.usage==0x39||chord||(chinese_&&e.usage>=4&&e.usage<=29)));
 if(chord&&body){if(preparing_&&confirm_requested_&&e.ime_attempt!=attempt_serial_){mark("Switch event expired");return true;}if(((preparing_&&!confirm_requested_)||receipt_.status==CS::RetryableNoEffect)&&!cancelAttempt(e.ime_attempt))return true;switchScheme();return true;}
 if(e.usage==0x39&&body){
  if(active()){++metrics.blocked;noteIntent(AppBlocked,e,current_action_);mark("Finish or cancel CN first");return true;}
  chinese_=!chinese_;
  if(chinese_){
   resource_owner_retired_=false;
   if(resources_.state()==ResourceState::Dormant||resources_.state()==ResourceState::Error)resources_.start(resources_.rules());
  }
  mark(chinese_?"CN":"EN");return true;
 }
 if(!body&&active()){interrupt();noteIntent(InvalidContext,e,current_action_);return true;}
 if(!chinese_||!body)return false;
 if(quarantined_){noteIntent(CommitFailed,e,current_action_);if(e.usage==0x29){chinese_=false;mark("Unknown retained; EN available");}else mark("Unknown: input quarantined");return true;}
 // A delayed Esc must not acquire authority over a replacement query. This
 // check precedes generic flow cancellation, which would otherwise discard it.
 if(!successor&&e.usage==0x29&&active()&&
    ((e.ime_context&&!earlyContext(e))||int32_t(e.ms-raw_event_ms_)<0)){
  noteIntent(QueryContextChanged,e,current_action_);mark("Cancel input/query expired");return true;
 }
 if(!successor&&e.usage==0x29&&!e.mods&&app_.renameOpen()){
  // A current Esc cancels the entire temporary target, not the body Esc ladder.
  // Retire owned queries/selections/submissions before App cancels the modal.
  interrupt();return false;
 }
 if(!successor&&e.usage==0x29&&!e.mods&&active()){
  escapeComposition(e);return true;
 }

 if(!successor&&(flow_.stage!=FlowStage::Idle||following_count_)&&e.ime_context&&(e.usage==0x2c||e.usage==0x28||(e.usage>=30&&e.usage<=35))&&!earlyContext(e)){
  noteIntent(QueryContextChanged,e,current_action_);mark("Selection input/query expired");return true;
 }
 // Movement/new Down cancels a provisional manual choice, not the input.
 // Its successors retire together. A fresh command then uses the existing raw
 // editor; old Esc/context checks above still run before any cancellation.
 if(!successor&&!preparing_&&(flow_.stage==FlowStage::Admission||
    (reselectionCurrent()&&flow_.stage!=FlowStage::Idle))&&
    (e.usage==0x29||e.usage==0x2a||e.usage==0x4c||(e.usage>=0x4f&&e.usage<=0x52))){
  if(!earlyContext(e)||!flowCurrent()||e.epoch!=flow_.event.epoch||e.route_epoch!=flow_.event.route_epoch){noteIntent(QueryContextChanged,e,current_action_);return true;}
  const bool admitted=reselection_.active;
  stopFlow(CancelledByUser);clearSelectionCommit();reselection_={};
  if(admitted){window_->cancel();window_->query(0,current_action_);raw_edit_=true;initial_=false;entering_=false;}
 }
 if(!successor&&(flow_.stage!=FlowStage::Idle||following_count_)){
  if(!flowCurrent()||e.epoch!=flow_.event.epoch||e.route_epoch!=flow_.event.route_epoch){stopFlow(InvalidContext);noteIntent(InvalidContext,e,current_action_);mark("Input chain expired");return true;}
  if(e.usage==0x29){if(preparing_&&e.ime_attempt!=attempt_serial_){noteIntent(ExpiredInput,e,current_action_);mark("Cancel event expired");return true;}stopFlow(CancelledByUser);}
  else if(!preparing_&&(e.usage==0x2a||(e.usage==0x50&&spatial_action_==ImeAction::Native)||spatial_action_==ImeAction::RawPrevious||spatial_action_==ImeAction::RawNext||e.usage==0x4c)){
   stopFlow(CancelledByUser);clearSelectionCommit();if(window_){window_->cancel();window_->query(0,current_action_);}
  }else if((e.usage>=4&&e.usage<=35)||e.usage==0x2c||e.usage==0x28){return enqueue(e);}
  else if((e.usage==54||e.usage==55)&&!e.mods){
   if(!earlyContext(e)){noteIntent(InvalidContext,e,current_action_);mark("Punctuation input expired");return true;}
   return enqueue(e);
  }
  else {noteIntent(InvalidContext,e,current_action_);mark("Action unavailable during input chain");return true;}
 }
 if(preparing_){++metrics.blocked;noteIntent(AppBlocked,e,current_action_);mark("Whole-text submission pending");return true;}
 if(e.mods&0xdd){if(active()){++metrics.blocked;noteIntent(AppBlocked,e,current_action_);mark("App action blocked by CN");return true;}return false;}
 if (e.usage >= 4 && e.usage <= 29) {
  const bool empty = raw().empty();
  if(wubi()&&!auto_request_){
   try {
    auto_request_ = std::make_unique<AutoRequest>();
   } catch (const std::bad_alloc&) {
    noteIntent(SelectionAllocationFailure, e, current_action_);
    mark("Wubi intent allocation failed; input retained");
    return true;
   }
  }
  const auto result = append(char('a' + e.usage - 4),e.ime_attempt);
  if (result == RawEdited) {
   if(wubi())armAuto(e);
   if (empty) letters_since_choice_ = edit_actions_ = 0;
   if (letters_since_choice_ < UINT16_MAX) ++letters_since_choice_;
  }
  noteIntent(result, e, current_action_);
  return true;
 }
 if(!active()){if(successor){noteIntent(UnassignedIdleChoice,e,current_action_);return true;}return false;}
 // A key may arrive after F4 but before its first framebuffer. Queue its
 // spatial intent behind regrouping, rather than applying V motion to H rows.
 if(!raw_edit_&&window_&&((e.usage>=79&&e.usage<=82)||e.usage==45||e.usage==46))
  window_->setVerticalPacking(!app_.horizontalImeSurface());
 if(spatial_action_!=ImeAction::Native){
  if(raw_edit_){
   if(spatial_action_==ImeAction::RawPrevious)moveBoundary(cursor_,hanime_extension::NavigationDirection::Left);
   else if(spatial_action_==ImeAction::RawNext)moveBoundary(cursor_,hanime_extension::NavigationDirection::Right);
   else if(spatial_action_==ImeAction::RawCandidates)rawDown();
   else mark("Raw command not assigned");
  }else spatialNavigate(spatial_action_,e.ime_frame);
  return true;
 }
 if(raw_edit_&&e.usage==0x28){noteIntent(UnassignedRawEnter,e,current_action_);mark("Raw Enter not assigned");return true;}
 if(raw_edit_&&!(e.usage==0x2c||(e.usage>=30&&e.usage<=35))){rawKey(e);return true;}
 if(e.usage>=0x4f&&e.usage<=0x52){navigate(e.usage,e.ime_frame);return true;}
 if (e.usage == 0x2a) {
  uint8_t result = EditNoChange;
  if (!raw().empty()) result = edit(raw().size() - 1, raw().size(), {},e.ime_attempt);
  noteIntent(result, e, current_action_);
  return true;
 }
 if(e.usage==0x2c||e.usage==0x28||(e.usage>=30&&e.usage<=35)){
  if(reselection_.pending){
   const auto target=editTarget();
   if(!earlyContext(e)||!has_target_||!sink_.current()||!composition_||!window_||
      int32_t(e.ms-raw_event_ms_)<0||!composition_->input_is_current(reselection_.stamp)||
      window_->querySerial()!=reselection_.query||cursor_!=reselection_.origin||
      !target.valid()||target.begin!=reselection_.begin||target.end!=reselection_.end){
    noteIntent(QueryContextChanged,e,current_action_);mark("Manual choice target expired");return true;
   }
   // Own intent only: no Selection or old automatic candidate is captured.
   startFlow(e,FlowStage::Admission);flow_.parent=reselection_.action;
   noteIntent(QueuedQuery,e,current_action_,flow_.parent);mark("Manual range choice queued");return true;
  }
  return selectIntent(e,successor);
 }
 if(e.usage==0x2d||e.usage==0x2e){navigate(e.usage==0x2d?0x52:0x51,e.ime_frame);return true;}
 ++metrics.blocked;noteIntent(AppBlocked,e,current_action_);mark("App action blocked by CN");return true;
}
void DeviceIme::service(bool allow_prefetch){
 IngressScope ingress_scope(*this);
 serviceVisuals(uint32_t(clockUs()/1000));
 if(flow_.guarded&&!flowCurrent()){
  const bool owns_query=window_&&window_->querySerial()==flow_.query&&window_->layoutSerial()==flow_.layout;
  stopFlow(sink_.matches(flow_.target)?ExpiredInput:ExpiredTarget);clearSelectionCommit();if(owns_query&&!preparing_)window_->cancel();
  mark("Input chain expired");
 }
 serviceFollowing();
 if(selection_commit_.active&&!selectionCommitCurrent())clearSelectionCommit();
 if(!quarantined_&&has_target_&&!preparing_&&!sink_.current()){interrupt();mark("Input target expired");return;}
 const auto before=resources_.state();resources_.service(); // Ready: no-I/O health guard, including cache hits.
 if(resources_.state()==ResourceState::Error){resourceFailed();return;}
 if(quarantined_){stopFlow(CommitFailed);return;}
 if(before!=ResourceState::Ready){
  if(before!=resources_.state())mark("CN resource initialization");
  return;
 }
 if(!composition_){
  if(!chinese_||initialization_failed_)return;
  try{
   const auto config=compositionConfig(resources_.rules());
   const auto commit_config=submissionConfig(config);
   if(!compatibleCapacity(config,commit_config)){
    initialization_failed_=true;stopFlow(InvalidContext);mark("IME configuration invalid; input retained");return;
   }
   auto composition=std::make_unique<hanime_extension::Composition>(resources_.scheme,resources_.shared,resources_.policy(),config,member_backend_);
   if(!composition->valid())throw std::bad_alloc();
   auto navigator=std::make_unique<hanime_extension::SyllableNavigator>(hanime_extension::NavigationConfig{DeviceCompositionConfig.raw_bytes,DeviceCompositionConfig.raw_bytes});
   auto window=std::make_unique<CandidateWindow>(*composition,font_);
   auto submission=std::make_unique<hanime_extension::Submission>(*composition,resources_.shared,sink_,commit_config,member_backend_);
   if(!submission->valid())throw std::bad_alloc();
   window->setVerticalPacking(!app_.horizontalImeSurface());
   composition_=std::move(composition);navigator_=std::move(navigator);window_=std::move(window);submission_=std::move(submission);
   if(waiting_size_){auto s=composition_->edit_raw(0,0,{waiting_raw_.data(),waiting_size_});if(s==S::Ready){waiting_size_=0;navigationReady();window_->query(0,current_action_);}else mark(hanime_extension::name(s));}
   if(flow_.stage==FlowStage::Query){
    flow_.guarded=true;flow_.stamp=composition_->raw_input().stamp;flow_.query=window_->querySerial();flow_.layout=window_->layoutSerial();
    const auto status=flow_.numbered?window_->chooseInitialNumber(flow_.column,flow_.action):window_->chooseDefault(flow_.action);
    if(status!=ChoiceResult::Waiting&&status!=ChoiceResult::Captured)stopFlow(windowFailure());
    else flowCaptured();
   }
   if (auto_request_ && auto_request_->pending && !auto_request_->bound) {
    auto_request_->stamp = composition_->raw_input().stamp;
    auto_request_->bound = true;
   }
   mark("CN ready");
  }catch(const std::bad_alloc&){
    initialization_failed_=true;stopFlow(SelectionAllocationFailure);
    mark("IME allocation failed; Space retries, Esc cancels");
   }catch(const std::invalid_argument&){
    initialization_failed_=true;stopFlow(InvalidContext);
    mark("IME configuration invalid; input retained");
   }
  return;
 }
 if(preparing_){
  if(confirm_requested_){
   confirm_requested_=false;
   auto result=retry_requested_?submission_->retry(requested_attempt_):submission_->prepare();
   attempt_=result.transaction;receipt_=result.core;preparing_=receipt_.status==CS::Preparing||receipt_.status==CS::Ready;
   if(preparing_)registerAttempt();else {stopFlow(CommitFailed);traceSubmission(receipt_,confirm_action_);mark(resultName(receipt_.status));}
   return;
  }
  const auto start=clockUs();const auto io=metrics.io();auto r=submission_->advance(attempt_,Resources::Budget);receipt_=r.result.core;
  metrics.step(Phase::Submission,start,r.cost.read_calls,r.cost.read_bytes,r.cost.work_items,io);
  if(receipt_.status==CS::Ready)receipt_=submission_->confirm(attempt_).core;
  if(receipt_.status!=CS::Preparing&&receipt_.status!=CS::Ready){
   preparing_=false;quarantined_=receipt_.status==CS::UnknownOutcome;
   if(receipt_.status==CS::Accepted){
    app_.endCompositionPresentation();if(window_)window_->endSession();
    accepted_action_=confirm_action_;accepted_target_=sink_.target();
    accepted_body_revision_=accepted_target_.rename?app_.renameRevision():app_.document.revision(); // A later edit can supersede the pending LCD frame.
    traceUi(UiTraceKind::BodyAccepted,composition_.get(),window_.get(),nullptr,nullptr,uint8_t(receipt_.status),accepted_action_,sink_.acceptedUs());
    BT_OBSERVE(ended(confirm_action_,1));
    has_target_=false;raw_edit_=false;initial_=true;window_->cancel();flowAccepted();
   }else {stopFlow(CommitFailed);traceSubmission(receipt_,confirm_action_);traceUi(UiTraceKind::CommitStopped,composition_.get(),window_.get(),nullptr,nullptr,uint8_t(receipt_.status),confirm_action_);}
   mark(resultName(receipt_.status));
  }return;
 }
 if(flow_.stage==FlowStage::Idle&&!preparing_)window_->setVerticalPacking(!app_.horizontalImeSurface());
 window_->service(allow_prefetch&&!raw_edit_);
 serviceAuto();
 if(reselection_.pending&&!window_->busy()){
  const auto pending=reselection_;
  const bool direct=resources_.rules()==hanime_extension::SchemeRules::DirectExact;
  const auto target=direct?EditTarget{}:editTarget();
  const bool matching=raw_edit_&&cursor_==pending.origin&&window_->querySerial()==pending.query&&
   composition_->input_is_current(pending.stamp)&&
   (direct?(pending.begin==0&&pending.end==raw().size()):
           (target.valid()&&target.begin==pending.begin&&target.end==pending.end));
  const bool choice=flow_.stage==FlowStage::Admission&&flowCurrent();
  reselection_={};
  if(matching){
   const auto action=current_action_;current_action_=pending.action;
   // A completed failed whole-code query stays in raw, with no retry loop.
   if(direct&&window_->state()!=WindowState::Ready)
    mark(window_->state()==WindowState::NoResults?"No candidates; raw retained":"Candidates unavailable; raw retained");
   else rawDown();
   current_action_=action;
   if(choice){
    // Only this successful transition may advance the intent's expected query
    // and stamp (a member reopen can publish a new revision). Do not redispatch
    // the old physical frame through selectIntent: it could name the auto page.
    if(reselectionCurrent()&&reselection_.action==pending.action&&
       reselection_.begin==pending.begin&&reselection_.end==pending.end&&window_->busy()&&sink_.matches(flow_.target)){
     flow_.stamp=composition_->raw_input().stamp;flow_.query=window_->querySerial();flow_.layout=window_->layoutSerial();flow_.stage=FlowStage::Query;
     const auto result=flow_.numbered?window_->chooseInitialNumber(flow_.column,flow_.action):window_->chooseDefault(flow_.action);
     if(result==ChoiceResult::Captured)flowCaptured();
     else if(result!=ChoiceResult::Waiting)stopFlow(windowFailure());
    }else stopFlow(windowFailure()); // Rejected range never targets automatic recovery.
   }
  }else if(flow_.stage==FlowStage::Admission){stopFlow(QueryContextChanged);mark("Manual choice target expired");}
 }
 if(recoverManual())return;
 if(flow_.stage==FlowStage::Query&&flow_.numbered&&!window_->capturedChoice()){
  const auto number=window_->chooseInitialNumber(flow_.column,flow_.action);
  if(number!=ChoiceResult::Waiting&&number!=ChoiceResult::Captured){stopFlow(window_->state()==WindowState::Ready?uint8_t(NumberAbsent):windowFailure());mark("Number missing from initial row");}
 }
 settleNavigation();
 if(flow_.stage==FlowStage::Query&&window_->capturedChoice()){
  flowCaptured();noteIntent(ResolvedDefault,flow_.event,flow_.action,flow_.parent);
 }
 if(entering_&&!window_->busy()){initial_=window_->state()!=WindowState::Ready;entering_=false;if(initial_)clearArrows();}
 if(window_->state()==WindowState::Selected){
  if(!flowSelected()){window_->cancel();mark("Selection chain expired");return;}
  const bool continue_selection=advanceSelectionCommit();
  initial_=true;raw_edit_=false;initial_numbered_=true;reselection_={};
  const auto snap=composition_->snapshot();
  // A successful partial choice advances the semantic target in either layout.
  // An AllSelected input keeps its presentation until BODY accepts; Unknown
  // and no-effect submission results must retain this last stage.
  if(window_->completion()==SelectionCompletion::Choice&&
      snap.state!=hanime_extension::CompositionState::AllSelected)
    window_->beginCandidateStage();
  bool retained_at_focus=false;
  for(size_t i=0;i<snap.member_count;++i){
   if(!snap.members[i].valid&&snap.members[i].begin==snap.focus){retained_at_focus=true;break;}
  }
  if(retained_at_focus){
   if(continue_selection)selection_commit_.rechecking=true;
   window_->recheck();flow_.layout=window_->layoutSerial();mark("Selected suffix recheck pending");
  }else if(snap.state!=hanime_extension::CompositionState::AllSelected){
   clearSelectionCommit();window_->query(0,window_->actionId());
   // Only the matched predecessor may advance the expected suffix query.
   flow_.query=window_->querySerial();flow_.layout=window_->layoutSerial();
   if(flow_.stage!=FlowStage::Idle){noteIntent(Completed,flow_.event,flow_.action,flow_.parent);flow_.stage=FlowStage::Idle;}
  }else{
   const auto original_attempt=selection_commit_.event.ime_attempt;
   const auto original_action=selection_commit_.action;
   clearSelectionCommit();window_->cancel();flow_.layout=window_->layoutSerial();
   if(continue_selection){beginConfirm(original_attempt,original_action);flow_.stage=FlowStage::Commit;}
   else {if(flow_.stage!=FlowStage::Idle)noteIntent(Completed,flow_.event,flow_.action,flow_.parent);flow_.stage=FlowStage::Idle;mark("All selected; Space/Enter to commit");}
  }
 }
 if((flow_.stage==FlowStage::Query||flow_.stage==FlowStage::Selecting)&&
    (window_->state()==WindowState::NoResults||window_->state()==WindowState::Unresolved||
     window_->state()==WindowState::Limited||window_->state()==WindowState::Error||
     window_->state()==WindowState::Stale||window_->state()==WindowState::Capacity||
     window_->state()==WindowState::Busy||window_->state()==WindowState::OutputError)){
  clearSelectionCommit();stopFlow(windowFailure());
 }
 navigationReady(); // Prepare borrowed navigation facts before any drawing.
 if(window_->changed()){
  switch(window_->state()){
  case WindowState::Boundary:boundary(pending_boundary_);break;
  case WindowState::NoResults:mark("No candidates; input retained");break;
  case WindowState::Unresolved:mark("Input unresolved; continue editing");break;
  case WindowState::Limited:mark("Query limit reached; input retained");break;
  case WindowState::Ready:mark(manual_failure_!=WindowState::Empty?"Auto restored; edit or Space/digit":raw_edit_?(wubi()?"Raw code; Esc candidates; Space/digit choose":"Raw edit; Space/digit choose, Down prefix"):window_->planningLimited()?"Query limited; candidates available":"Candidates ready");break;
  case WindowState::Capacity:clearSelectionCommit();mark("Selection capacity reached; input retained");break;
  case WindowState::Busy:clearSelectionCommit();mark("Selection busy; input retained");break;
  case WindowState::OutputError:clearSelectionCommit();mark("Candidate output unavailable; input retained");break;
  case WindowState::Error:clearSelectionCommit();mark("Candidate operation failed; input retained");break;
  case WindowState::Stale:clearSelectionCommit();mark("Candidate expired; choose current frame");break;
  default:app_.dirtyDisplay=true;break;
  }
 }
 serviceArrows();
}
}
