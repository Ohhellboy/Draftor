#pragma once
#include "candidate_window.hpp"
#include "member_storage.hpp"
#include "editor_sink.hpp"
#include "raw_viewport.hpp"
#include "selected_viewport.hpp"
#include "raw_edit_admission.hpp"
#include "ui_trace.hpp"
#include "hanime_extension/submission.hpp"
#include "hanime_extension/navigation.hpp"
namespace ime_device {
// Spatial intent retains the original physical event and its freshness fences.
enum class ImeAction:uint8_t { Native, PreviousCandidate, NextCandidate, ExpandOrNextColumn,
 NextColumn, PreviousColumn, RawPrevious, RawNext, RawCandidates, Unassigned };
enum class EditTargetKind:uint8_t {Unavailable,Boundary,Prefix,Member};
struct EditTarget {EditTargetKind kind=EditTargetKind::Unavailable;size_t begin=0,end=0,member=0;
 bool valid()const{return kind==EditTargetKind::Prefix||kind==EditTargetKind::Member;}
};
class DeviceIme {
 s3app::App& app_;Resources& resources_;const s3host::FontPort& font_;
 EditorSink sink_;
 // Declared before consumers and destroyed after every owned array.
 MemberStorage member_storage_;
 hanime_extension::StorageBackend member_backend_;
 std::unique_ptr<hanime_extension::Composition> composition_;
 std::unique_ptr<hanime_extension::SyllableNavigator> navigator_;
 hanime_extension::NavigationHandle navigation_;
 std::unique_ptr<CandidateWindow> window_;
 std::unique_ptr<hanime_extension::Submission> submission_;
 hanime_extension::SubmissionHandle attempt_;
 hanime::CompositionResult receipt_;
 struct AttemptSlot{uint32_t serial=0;hanime_extension::SubmissionHandle handle;};
 std::array<AttemptSlot,2> attempts_{};uint32_t attempt_serial_=0,next_attempt_serial_=0;
 hanime_extension::SubmissionHandle requested_attempt_;bool retry_requested_=false;
 bool confirm_requested_=false;void(*publish_attempt_)(uint32_t)=nullptr;
 // One owned intent after explicit choice or a verified scheme completion request
 // captures a formal Selection. No borrowed snapshot pointers survive the event.
 struct SelectionCommitIntent {
  VisibleItem choice;
  hanime_extension::InputStamp stamp;
  Target target;
  s3hid::Event event;
  uint32_t action=0;
  bool active=false,rechecking=false;
 } selection_commit_;
 void clearSelectionCommit();
 void captureSelectionCommit(const VisibleItem&,const s3hid::Event&,bool automatic=false);
 // One bounded completion intent; this is not another candidate owner.
 struct AutoRequest {
  s3hid::Event event;
  hanime_extension::InputStamp stamp;
  uint32_t action = 0;
  bool pending = false;
  bool bound = false;
 };
 std::unique_ptr<AutoRequest> auto_request_;
 bool wubi() const {
  return resources_.policy().rules == hanime_extension::SchemeRules::DirectExact &&
         resources_.policy().resource_scheme == "wubi86";
 }
 void armAuto(const s3hid::Event&);
 void serviceAuto();
 bool selectionCommitCurrent()const;
 bool advanceSelectionCommit();
 void registerAttempt();bool cancelAttempt(uint32_t);
 struct Held{uint32_t ms=0;bool down=false,seen=false,ime=false;uint32_t action=0;};std::array<Held,256> held_{};uint32_t epoch_=0;
 uint32_t current_event_ms_=0,raw_event_ms_=0;
 uint8_t event_result_=0;
 uint16_t letters_since_choice_=0,edit_actions_=0;
 uint32_t next_action_=0,current_action_=0,confirm_action_=0,accepted_action_=0;
 Target accepted_target_;
 enum class FlowStage:uint8_t {Idle,Admission,Query,Selecting,Commit};
 struct Flow {
  s3hid::Event event;uint32_t action=0,parent=0,query=0,layout=0;FlowStage stage=FlowStage::Idle;
  hanime_extension::InputStamp stamp;Target target;VisibleItem choice;
  bool captured=false,guarded=false,numbered=false,body_successor=false;uint8_t column=0;
 } flow_;
 struct Following {s3hid::Event event;uint32_t action=0,parent=0,query=0;hanime_extension::InputStamp arrival;size_t focus=0;};
 std::array<Following,32> following_{};size_t following_head_=0,following_count_=0;
 uint32_t following_peak_=0,following_overflow_=0,following_cancelled_=0;
 ImeAction spatial_action_=ImeAction::Native;
 bool dispatch(const s3hid::Event&,bool successor=false);
 void spatialNavigate(ImeAction,uint32_t);
 void applySpatial(ImeAction,uint32_t);
 bool initial_numbered_=true;
 bool selectIntent(const s3hid::Event&,bool successor);
 void startFlow(const s3hid::Event&,FlowStage);
 void stopFlow(uint8_t reason);bool flowCurrent()const;
 void flowCaptured(bool automatic=false);bool flowSelected();void flowAccepted();void serviceFollowing();
 bool enqueue(const s3hid::Event&);void noteIntent(uint8_t,const s3hid::Event&,uint32_t,uint32_t parent=0);
 // Main-loop checkpoint. BLE copies only its bounded serial, never core objects.
 struct Ingress {
  hanime_extension::InputStamp stamp;Target target;
  const hanime_extension::Composition* owner=nullptr;
  uint32_t query=0,layout=0;uint64_t mount=0;size_t waiting=0;
  bool bound=false,chinese=false,raw_edit=false;
 } ingress_;
 struct Reselection {hanime_extension::InputStamp stamp;uint32_t query=0;uint16_t begin=0,end=0;bool active=false,pending=false;uint16_t origin=0;uint32_t action=0;} reselection_;
 WindowState manual_failure_=WindowState::Empty;
 bool recoverManual();
 bool reselectionCurrent()const;
 uint32_t superseded_frame_=0;
 uint32_t ingress_serial_=0,ingress_causal_=0;bool ingress_known_=false;
 uint32_t resource_owner_=1;bool resource_owner_retired_=false;
 bool ingressCurrent()const;void rememberIngress();void invalidateIngress();
 struct IngressScope {DeviceIme& d;explicit IngressScope(DeviceIme& v):d(v){d.ingressContext();}~IngressScope(){d.rememberIngress();}};
 bool earlyContext(const s3hid::Event&)const;
 uint64_t accepted_body_revision_=0;
 std::array<char,DeviceCompositionConfig.raw_bytes+1> waiting_raw_{};size_t waiting_size_=0;
 bool initialization_failed_=false;
 bool chinese_=false,raw_edit_=false,initial_=true,entering_=false,quarantined_=false,preparing_=false,has_target_=false;
 SelectedViewport selected_viewport_;
 size_t cursor_=0;uint32_t boundary_until_=0;bool boundary_active_=false;
 BoundaryEdge boundary_edge_=BoundaryEdge::CandidateTop,pending_boundary_=BoundaryEdge::CandidateTop;
 const char* notice_="EN";
 // Relative navigation is ordered separately from selection. The captured
 // frame is checked at receipt; queued arrows never authorize future text.
 struct Arrow {uint32_t action=0;uint8_t key=0;ImeAction spatial=ImeAction::Native;};
 std::array<Arrow,32> arrows_{};
 size_t arrow_head_=0,arrow_count_=0;
 hanime_extension::InputStamp arrow_stamp_;
 uint32_t arrow_peak_=0,arrow_overflow_=0,arrow_discarded_=0;
 bool arrow_batch_=false;size_t desired_row_=0,desired_col_=0;uint32_t desired_action_=0;
 uint32_t nav_request_action_=0,nav_applied_action_=0,arrow_query_=0;
 void noteNavigation(uint8_t,uint32_t,uint32_t parent=0);
 void inheritNavigation(uint32_t);void appliedNavigation(uint32_t,size_t,size_t,bool);
 void settleNavigation();
 void clearArrows(uint8_t reason=CancelledByUser);void applyArrow(uint8_t,uint32_t);void serviceArrows();
 bool fresh(const s3hid::Event&);
 void choiceFeedback(ChoiceResult);
 uint8_t windowFailure()const;
 void discard();void resourceFailed();void switchScheme();void beginConfirm(uint32_t,uint32_t);uint8_t append(char,uint32_t);
 uint8_t edit(size_t,size_t,std::string_view,uint32_t);void mark(const char*);void boundary(BoundaryEdge);
 void observeCompositionPresentation();
 bool navigationReady();bool moveBoundary(size_t,hanime_extension::NavigationDirection);
 void escapeComposition(const s3hid::Event&);void enterRaw();void rawDown();void rawKey(const s3hid::Event&);void navigate(uint8_t,uint32_t);
 public:
 DeviceIme(s3app::App& app,Resources& resources,const s3host::FontPort& font,void(*publish)(uint32_t)=nullptr,hanime_extension::StorageBackend storage={}):app_(app),resources_(resources),font_(font),sink_(app),member_backend_(storage.allocate||storage.deallocate||storage.id?storage:member_storage_.backend()){publish_attempt_=publish;}
 const MemberStorage& memberStorage()const{return member_storage_;}
 ~DeviceIme();
 uint32_t ingressContext(); // Main loop only; publish before/after service and before LCD transfer.
 uint32_t resourceOwner()const{return resource_owner_;}
 bool event(const s3hid::Event&,ImeAction=ImeAction::Native);void service(bool allow_prefetch=false);void interrupt();
 // Called even while keyboard/font work prevents an IME work slice.
 void serviceVisuals(uint32_t now);
 // Borrow existing analyzed facts; rendering must never analyze or query.
 hanime_extension::NavigationView presentationNavigation()const {
  return composition_&&navigator_?navigator_->view(navigation_,*composition_):hanime_extension::NavigationView{};
 }
 const hanime_extension::SyllableNavigator* navigator()const{return navigator_.get();}
 uint32_t attemptSerial()const{return attempt_serial_;}
 uint32_t actionId()const{return current_action_;}
 uint32_t acceptedAction()const{return accepted_action_;}
 uint64_t acceptedUs()const{return sink_.acceptedUs();}
 bool acceptedDocumentCurrent()const{return !accepted_target_.rename&&accepted_action_&&app_.session.hasCurrent()&&accepted_target_.document==app_.session.currentId()&&accepted_target_.session==app_.session.sessionSerial()&&accepted_target_.stamp.focus==app_.editStamp().focus&&accepted_body_revision_==app_.document.revision();}
 bool acceptedTargetCurrent()const {
  if(!accepted_target_.rename)return acceptedDocumentCurrent();
  return accepted_action_&&app_.renameOpen()&&accepted_target_.document==app_.renameId()&&
    accepted_target_.session==app_.session.sessionSerial()&&accepted_target_.stamp.focus==app_.focusToken()&&
    accepted_body_revision_==app_.renameRevision();
 }
 bool hasWork()const{return following_count_||flow_.stage!=FlowStage::Idle||arrow_count_||preparing_||(window_&&window_->busy())||(chinese_&&resources_.ready()&&!composition_&&!initialization_failed_)||(resources_.state()!=ResourceState::Ready&&resources_.state()!=ResourceState::Dormant&&resources_.state()!=ResourceState::Error);}
 size_t queuedInput()const{return following_count_;}
 uint32_t inputPeak()const{return following_peak_;}uint32_t inputOverflow()const{return following_overflow_;}
 uint32_t inputCancelled()const{return following_cancelled_;}
 size_t queuedArrows()const{return arrow_count_;}
 uint32_t arrowPeak()const{return arrow_peak_;}
 uint32_t arrowOverflow()const{return arrow_overflow_;}
 uint32_t arrowDiscarded()const{return arrow_discarded_;}
 hanime_extension::SchemeRules schemeRules()const{return resources_.rules();}
 bool chinese()const{return chinese_;}bool active()const;
 EditTarget editTarget()const;
 bool reselecting()const{return reselectionCurrent();}
 bool targetMatches(const VisibleItem&)const;
 void editCaption(char*,size_t)const;
 RawViewport editViewport(const s3host::FontPort&)const;
 SelectedView selectedViewport(const s3host::FontPort& font,unsigned pixels=284){
  if(!composition_||!active()){selected_viewport_.reset();return {};}
  return selected_viewport_.update(composition_->snapshot(),cursor_,raw_edit_||reselectionCurrent(),font,pixels);
 }
 bool rawEditing()const{return raw_edit_;}size_t rawCursor()const{return cursor_;}
 bool expanded()const{return window_&&window_->view().expanded;}
 const char* notice()const{return notice_;}
 BoundaryEdge boundaryEdge()const{return boundary_edge_;}
 bool candidateAvailable()const;
 bool boundaryVisible(uint32_t now)const{return boundary_active_&&int32_t(boundary_until_-now)>0;}
 uint32_t framePresented(){return window_?window_->framePresented():0;}
 const CandidateView* view()const{return window_?&window_->view():nullptr;}
 hanime_extension::Composition* composition(){return composition_.get();}
 CandidateWindow* window(){return window_.get();}
 const CandidateWindow* window()const{return window_.get();}
 hanime_extension::Submission* submission(){return submission_.get();}
 std::string_view raw()const{return composition_?composition_->snapshot().raw:std::string_view(waiting_raw_.data(),waiting_size_);}
 std::string_view selected()const{return composition_?composition_->snapshot().selected_text:std::string_view{};}
 const hanime::CompositionResult& receipt()const{return receipt_;}
};
}
