#pragma once
#include "candidate_window.hpp"
#include "hid_keyboard.hpp"
#include "idle_log.hpp"
#include "hanime/composition_commit.hpp"
namespace ime_device {
enum class UiTraceKind:uint8_t {KeyDown,KeyUp,Duplicate,Choice,Query,WindowReady,SelectionDone,Lcd,RouteDrop,RowRequest,RangeReady,ItemTextReady,CommitQueued,BodyAccepted,BodyLcd,CommitStopped,Intent,Text,NavigationApplied,SelectionResult,SubmissionResult,FeedbackLcd};
enum IntentResult:uint8_t {Received=0,QueuedQuery,QueuedCommit,Executing,CapturedDisplayed,ResolvedDefault,Completed,NoCandidate,ExpiredInput,ExpiredTarget,QueueFull,CancelledByUser,UnassignedRawEnter,UnknownNumberedRow,DuplicateSuppressed,CommitFailed,InvalidContext,Applied,QueuedSuccessor,UnassignedIdleChoice,BodyRouted,AppBlocked,ResourceFailure,QueuedNavigation,NavigationInherited,BeforeRawEdit,QueryContextChanged,DefaultContextExpired,DisplayedChoiceExpired,RawNavigation,NumberAbsent,RawEdited,CapacityRejected,EditNoChange,EditBusy,EditInvalid,EditFailed,Consumed,ResourceOwnerRetired,SelectionCapacity,SelectionBusy,SelectionOutputError,SelectionInvalid,SelectionCancelled,QueryLimited,QueryUnresolved,SelectionAllocationFailure};
// Main-loop-only diagnostic ring. No allocation, disk I/O, or BLE callback log.
// Full identity is diagnostic; it is never used to authorize or replay a choice.
struct UiTraceRecord {
 uint64_t us=0,input=0,revision=0,scheme=0,mount=0,bundle=0,page_generation=0;
 uint64_t item_input=0,item_revision=0,item_scheme=0,item_mount=0,item_bundle=0;
 hanime::StableLexemeKey key;hanime::RuntimeLexemeRef ref=0;
 uint32_t sequence=0,event_ms=0,epoch=0,route=0,event_frame=0,screen_frame=0,view_frame=0,attempt=0;
 uint64_t received_us=0;
 uint32_t action=0,target_row=0,first_row=0,row_count=0;
 uint32_t target_col=0,view_row=0,view_col=0,nav_pending=0;
 uint16_t begin=0,end=0,page=0,slot=0,raw=0,focus=0;
 uint8_t usage=0,detail=0;UiTraceKind kind=UiTraceKind::KeyDown;
 uint64_t target_session=0,target_focus=0;
 uint32_t event_id=0,parent=0,query=0,ingress=0;
 uint32_t resource_owner=0,current_resource_owner=0;
 uint16_t members=0,cursor=0,queued=0,letters=0,edits=0,pending_letters=0;
 uint8_t key_class=0,scheme_kind=0,text_bytes=0,text_part=0,text_parts=0;
 std::array<char,64> text{};
};
struct IntentContext {uint32_t parent=0,query=0;uint16_t cursor=0,queued=0,letters=0,edits=0;uint8_t scheme=0;uint64_t target_session=0,target_focus=0;uint16_t raw=0,pending_letters=0;uint32_t resource_owner=0;};
class UiTrace {
 UiTraceRecord* records_=nullptr;size_t head_=0,size_=0;uint32_t sequence_=0,lost_=0;
 bool detailed_=false;uint32_t aggregated_keyups_=0,aggregated_duplicates_=0;
 uint8_t mode_=1;uint32_t text_lost_=0,peak_=0;
 public:
 static constexpr size_t Capacity=32;
 explicit UiTrace(UiTraceRecord* storage=nullptr):records_(storage){}
 // Bind once at boot before any producer. Storage lives as long as the ring.
 void bind(UiTraceRecord* storage){if(!records_&&!size_)records_=storage;}
 void push(UiTraceRecord record){
  if(!mode_)return;
  // Normal mode aggregates release/repeat diagnostics, never their HID handling.
  if(!detailed_&&record.kind==UiTraceKind::KeyUp&&record.key_class!=5){++aggregated_keyups_;return;}
  if(!detailed_&&record.kind==UiTraceKind::Duplicate){++aggregated_duplicates_;return;}
  record.sequence=++sequence_;if(!records_||size_==Capacity){if(record.kind==UiTraceKind::Text)++text_lost_;else ++lost_;return;}records_[(head_+size_)%Capacity]=record;++size_;peak_=uint32_t(std::max(size_t(peak_),size_));}
 bool pop(UiTraceRecord& out){if(!size_)return false;out=records_[head_];head_=(head_+1)%Capacity;--size_;return true;}
 uint32_t aggregatedKeyups()const{return aggregated_keyups_;}
 uint32_t aggregatedDuplicates()const{return aggregated_duplicates_;}
 uint32_t lost()const{return lost_;}
 size_t size()const{return size_;}
 void setDetailed(bool detailed){setMode(detailed?2:1);}
 void setMode(uint8_t mode){mode_=std::min(uint8_t(2),mode);detailed_=mode_==2;}
 uint8_t mode()const{return mode_;}uint32_t textLost()const{return text_lost_;}uint32_t peak()const{return peak_;}
 bool detailed()const{return detailed_;}
};
extern UiTrace uiTrace;
void traceUi(UiTraceKind,const hanime_extension::Composition*,const CandidateWindow*,const s3hid::Event* event=nullptr,const VisibleItem* item=nullptr,uint8_t detail=0,uint32_t action=0,uint64_t occurred_us=0,size_t target_row=SIZE_MAX,const IntentContext* context=nullptr,size_t target_col=SIZE_MAX,size_t nav_pending=SIZE_MAX);
void traceSubmission(const hanime::CompositionResult&,uint32_t action);
void traceText(uint32_t action,uint8_t role,std::string_view text);
const char* intentName(uint8_t);
constexpr unsigned UiTraceParts=6;
// Six independently framed parts, each formatted within 384 bytes. IdleLog
// retains a partly sent line; following records cannot splice into it.
size_t formatUiTrace(const UiTraceRecord&,unsigned part,char* out,size_t capacity);
// At most one bounded line. On backpressure keep the exact record and part.
bool drainUiTracePart(UiTrace&,UiTraceRecord&,unsigned& part,IdleLog&);
}
