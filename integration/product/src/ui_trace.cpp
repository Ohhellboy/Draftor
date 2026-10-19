#include "ui_trace.hpp"
#include "behavior_observer.hpp"
#include "service_endpoints.hpp"
#include <cstdio>
namespace ime_device {
#ifdef ARDUINO
UiTrace uiTrace; // Payload is bound to the platform's boot-owned PSRAM storage.
#else
namespace { std::array<UiTraceRecord,UiTrace::Capacity> hostTraceStorage{}; }
UiTrace uiTrace(hostTraceStorage.data());
#endif
void traceUi(UiTraceKind kind,const hanime_extension::Composition* c,const CandidateWindow* w,const s3hid::Event* event,const VisibleItem* item,uint8_t detail,uint32_t action,uint64_t occurred_us,size_t target_row,const IntentContext* context,size_t target_col,size_t nav_pending){
 if(c&&w){
  if(kind==UiTraceKind::RangeReady){BT_OBSERVE(range(*c,*w));}
  else if(kind==UiTraceKind::WindowReady){BT_OBSERVE(window(*c,*w));}
 }
#if DRAFTOR_AUTH_OBSERVE
 if(!serviceEndpoint(kind,detail))return;
#elif DRAFTOR_TELEMETRY_CAPTURE
 return; // Ordinary diagnostics own no UART bytes in Option A capture.
#endif
 UiTraceRecord r;r.kind=kind;r.us=occurred_us?occurred_us:clockUs();r.detail=detail;r.action=action;
 if(c){const auto s=c->snapshot();r.input=s.input_id;r.revision=s.revision;r.scheme=s.scheme_epoch;r.mount=s.activation.mount_epoch();r.bundle=s.activation.shared_bundle_id();r.raw=uint16_t(s.raw.size());r.focus=uint16_t(s.focus);r.members=uint16_t(s.member_count);r.scheme_kind=uint8_t(c->raw_input().rules);}
 if(w){r.screen_frame=w->onScreenGeneration();r.view_frame=w->view().generation;if(!r.action&&kind!=UiTraceKind::RouteDrop)r.action=w->actionId();r.query=w->querySerial();r.target_row=uint32_t(w->requestedRow());r.first_row=w->view().count?uint32_t(w->view().rows[0].index):0;r.row_count=uint32_t(w->view().count);r.target_col=uint32_t(w->requestedCol());r.view_row=uint32_t(w->view().row);r.view_col=uint32_t(w->view().col);}
 // The main loop owns view publication and LCD transfer synchronously. A
 // queued next request may change requestedRow/Col without changing this view.
 if(kind==UiTraceKind::Lcd&&w){const auto& v=w->view();r.action=v.action;r.target_row=uint32_t(v.row);r.target_col=uint32_t(v.col);}
 if(target_col!=SIZE_MAX)r.target_col=uint32_t(target_col);
 if(nav_pending!=SIZE_MAX)r.nav_pending=uint32_t(nav_pending);
 if(target_row!=SIZE_MAX)r.target_row=uint32_t(std::min(target_row,size_t(UINT32_MAX)));
 if(event){r.event_ms=event->ms;r.epoch=event->epoch;r.route=event->route_epoch;r.event_frame=event->ime_frame;r.attempt=event->ime_attempt;r.usage=event->usage;r.event_id=event->event_id;r.ingress=event->ime_context;r.resource_owner=event->resource_owner;
  r.key_class=event->usage>=4&&event->usage<=29?1:event->usage==0x2c?2:event->usage==0x28?3:event->usage>=30&&event->usage<=35?4:event->usage>=0x4f&&event->usage<=0x52?5:event->usage==0x2a||event->usage==0x4c?6:event->usage==0x29?7:8;
  if(r.key_class==1)r.usage=0; // No letter value, even in individual HID diagnostics.
  const auto now=clockUs();const auto age=uint32_t(now/1000)-event->ms;
  r.received_us=event->received_us?event->received_us:uint64_t(age)*1000<=now?now-uint64_t(age)*1000-now%1000:0;
 }
 if(item){r.item_input=item->input;r.item_revision=item->revision;r.item_scheme=item->scheme;r.item_mount=item->activation.mount_epoch();r.item_bundle=item->activation.shared_bundle_id();r.key=item->key;r.ref=item->ref;r.begin=uint16_t(item->begin);r.end=uint16_t(item->end);r.page=uint16_t(item->page);r.slot=uint16_t(item->slot);r.page_generation=item->page_generation;}
 if(context){r.parent=context->parent;r.query=context->query;r.cursor=context->cursor;r.queued=context->queued;r.letters=context->letters;r.edits=context->edits;r.scheme_kind=context->scheme;r.target_session=context->target_session;r.target_focus=context->target_focus;r.raw=context->raw;r.pending_letters=context->pending_letters;r.current_resource_owner=context->resource_owner;}
 if(kind==UiTraceKind::SelectionResult&&w)r.usage=uint8_t(w->lastOutput());
#if DRAFTOR_AUTH_OBSERVE
 snapshotServiceEndpoint(r,c,w);
#endif
 uiTrace.push(r);
}
size_t formatUiTrace(const UiTraceRecord& r,unsigned part,char* out,size_t cap){
 int n=0;using U=unsigned long long;
 if(part==0)n=std::snprintf(out,cap,"[UI] n=%u k=%u us=%llu ms=%u hid=%u route=%u key=%u ef=%u lcd=%u view=%u attempt=%u result=%u raw=%u focus=%u\n",unsigned(r.sequence),unsigned(r.kind),U(r.us),unsigned(r.event_ms),unsigned(r.epoch),unsigned(r.route),unsigned(r.kind==UiTraceKind::SelectionResult||r.kind==UiTraceKind::SubmissionResult?0:r.usage),unsigned(r.event_frame),unsigned(r.screen_frame),unsigned(r.view_frame),unsigned(r.attempt),unsigned(r.detail),unsigned(r.raw),unsigned(r.focus));
 if(part==0&&r.ingress&&n>0&&size_t(n)<cap){const auto extra=std::snprintf(out+n-1,cap-size_t(n)+1," ingress=%u\n",unsigned(r.ingress));if(extra<0)return 0;n=n-1+extra;}
 else if(part==1)n=std::snprintf(out,cap,"[UI_CTX] n=%u input=%llu revision=%llu scheme=%llu mount=%llu bundle=%llu\n",unsigned(r.sequence),U(r.input),U(r.revision),U(r.scheme),U(r.mount),U(r.bundle));
 else if(part==2){char key[65]{};constexpr char hex[]="0123456789abcdef";for(size_t i=0;i<32;++i){key[i*2]=hex[r.key.bytes[i]>>4];key[i*2+1]=hex[r.key.bytes[i]&15];}
  n=std::snprintf(out,cap,"[UI_ITEM] n=%u key=%s ref=%llu begin=%u end=%u pg=%llu page=%u slot=%u\n",unsigned(r.sequence),key,U(r.ref),unsigned(r.begin),unsigned(r.end),U(r.page_generation),unsigned(r.page),unsigned(r.slot));}
 else if(part==3)n=std::snprintf(out,cap,"[UI_AUTH] n=%u input=%llu revision=%llu scheme=%llu mount=%llu bundle=%llu\n",unsigned(r.sequence),U(r.item_input),U(r.item_revision),U(r.item_scheme),U(r.item_mount),U(r.item_bundle));
 else if(part==4){
  constexpr const char* stages[]={"key_down","key_up","duplicate","choice_capture","query_start","visible_text_ready","selection_done","candidate_lcd","route_drop","row_request","range_ready","item_text_ready","commit_queued","body_accepted","body_lcd","commit_stopped","intent","text","navigation_applied","selection_result","submission_result"};
  const auto kind=unsigned(r.kind);
  if(r.kind==UiTraceKind::NavigationApplied)n=std::snprintf(out,cap,"[UI_ACTION] n=%u action=%u stage=%s occurred_us=%llu received_us=%llu target_row=%u first_row=%u rows=%u target_col=%u view_row=%u view_col=%u nav_pending=%u\n",unsigned(r.sequence),unsigned(r.action),kind<sizeof(stages)/sizeof(*stages)?stages[kind]:"unknown",U(r.us),U(r.received_us),unsigned(r.target_row),unsigned(r.first_row),unsigned(r.row_count),unsigned(r.target_col),unsigned(r.view_row),unsigned(r.view_col),unsigned(r.nav_pending));
  else if(r.kind==UiTraceKind::Lcd)n=std::snprintf(out,cap,"[UI_ACTION] n=%u action=%u stage=candidate_lcd occurred_us=%llu target_col=%u target_row=%u first_row=%u rows=%u\n",unsigned(r.sequence),unsigned(r.action),U(r.us),unsigned(r.target_col),unsigned(r.target_row),unsigned(r.first_row),unsigned(r.row_count));
  else n=std::snprintf(out,cap,"[UI_ACTION] n=%u action=%u stage=%s occurred_us=%llu received_us=%llu target_row=%u first_row=%u rows=%u\n",unsigned(r.sequence),unsigned(r.action),kind<sizeof(stages)/sizeof(*stages)?stages[kind]:"unknown",U(r.us),U(r.received_us),unsigned(r.target_row),unsigned(r.first_row),unsigned(r.row_count));
 }
 else if(part==5){
  if(r.kind==UiTraceKind::Text){
   char hex[129]{};constexpr char digits[]="0123456789abcdef";
   for(size_t i=0;i<r.text_bytes;++i){hex[i*2]=digits[uint8_t(r.text[i])>>4];hex[i*2+1]=digits[uint8_t(r.text[i])&15];}
   n=std::snprintf(out,cap,"[UI_TEXT] n=%u action=%u role=%u part=%u parts=%u bytes=%u truncated=%u hex=%s\n",unsigned(r.sequence),unsigned(r.action),unsigned(r.detail&127),r.text_part,r.text_parts,r.text_bytes,unsigned(r.detail>>7),hex);
  }else if(r.kind==UiTraceKind::SubmissionResult){
   n=std::snprintf(out,cap,"[UI_SUBMISSION] n=%u action=%u status=%u capacity=%u required=%u member_index=%u output=%u\n",unsigned(r.sequence),unsigned(r.action),unsigned(r.detail),unsigned(r.target_row),unsigned(r.target_col),unsigned(r.nav_pending),unsigned(r.usage));
  }else if(r.kind==UiTraceKind::SelectionResult){
   n=std::snprintf(out,cap,"[UI_SELECTION] n=%u action=%u status=%u output=%u detail=unknown_detail members=%u focus=%u\n",unsigned(r.sequence),unsigned(r.action),unsigned(r.detail),unsigned(r.usage),unsigned(r.members),unsigned(r.focus));
  }else if(r.kind==UiTraceKind::Intent&&(r.detail==ResourceOwnerRetired||r.detail==ResourceFailure)){
   n=std::snprintf(out,cap,"[UI_FLOW] n=%u event=%u parent=%u query=%u input=%llu rev=%llu reason=%s owner_arrival=%u owner_current=%u doc=%llu target_focus=%llu\n",unsigned(r.sequence),unsigned(r.event_id),unsigned(r.parent),unsigned(r.query),U(r.input),U(r.revision),intentName(r.detail),unsigned(r.resource_owner),unsigned(r.current_resource_owner),U(r.target_session),U(r.target_focus));
  }else n=std::snprintf(out,cap,"[UI_FLOW] n=%u event=%u parent=%u query=%u input=%llu rev=%llu scheme=%u class=%u reason=%s members=%u cursor=%u queue=%u added=%u edits=%u pending_letters=%u doc=%llu target_focus=%llu\n",unsigned(r.sequence),unsigned(r.event_id),unsigned(r.parent),unsigned(r.query),U(r.input),U(r.revision),r.scheme_kind,r.key_class,r.kind==UiTraceKind::Intent?intentName(r.detail):"observed",r.members,r.cursor,r.queued,r.letters,r.edits,r.pending_letters,U(r.target_session),U(r.target_focus));
 }

 return n>0&&size_t(n)<cap?size_t(n):0;
}
bool drainUiTracePart(UiTrace& trace,UiTraceRecord& record,unsigned& part,IdleLog& output){
 if(output.full())return false;
 if(part==UiTraceParts){if(!trace.pop(record))return false;part=record.kind==UiTraceKind::Text?5:0;}
#if DRAFTOR_AUTH_OBSERVE
 char line[384];auto bytes=formatServiceEndpoint(record,part,line,sizeof(line));
 // A bounded formatter failure is an explicit missing part, never a stalled
 // head record or a silently clipped endpoint. Preserve the original queues.
 if(!bytes){const int n=std::snprintf(line,sizeof(line),"[SW_TRUNCATED] n=%u k=%u part=%u capacity=%u\n",unsigned(record.sequence),unsigned(record.kind),part,unsigned(sizeof(line)));if(n<=0||size_t(n)>=sizeof(line))return false;bytes=size_t(n);}
 if(!output.append({line,bytes}))return false;
 ++part;if(part==(serviceCompletion(record.kind)?3u:2u))part=UiTraceParts;return true;
#else
 char line[384];const auto bytes=formatUiTrace(record,part,line,sizeof(line));
 if(!bytes||!output.append({line,bytes}))return false;
 ++part;
 using K=UiTraceKind;
 if(part==5&&!trace.detailed()&&record.kind!=K::Intent&&record.kind!=K::KeyDown&&record.kind!=K::KeyUp&&record.kind!=K::SelectionDone&&record.kind!=K::RouteDrop&&record.kind!=K::SelectionResult&&record.kind!=K::SubmissionResult)part=UiTraceParts;
 if(part==1&&!trace.detailed()&&record.kind!=K::Choice&&record.kind!=K::SelectionDone&&record.kind!=K::Lcd)part=4;
 return true;
#endif
}

const char* intentName(uint8_t n){
 constexpr const char* names[]={"received","queued_query","queued_commit","executing","captured_displayed","resolved_default","completed","no_candidate","expired_input","expired_target","queue_full","cancelled_by_user","unassigned_raw_enter","unknown_numbered_row","duplicate_suppressed","commit_failed","invalid_context","applied","queued_successor","unassigned_idle_choice","body_routed","app_blocked","resource_failure","queued_navigation","navigation_inherited","before_raw_edit","query_context_changed","default_context_expired","displayed_choice_expired","raw_navigation","number_absent","raw_edited","capacity_rejected","edit_no_change","edit_busy","edit_invalid","edit_failed","consumed","resource_owner_retired","selection_capacity_unknown_detail","selection_busy","selection_output_error","selection_invalid","selection_cancelled","query_limited","query_unresolved","selection_allocation_failure"};
 return n<sizeof(names)/sizeof(*names)?names[n]:"unknown";
}
void traceSubmission(const hanime::CompositionResult& result,uint32_t action){
 // Kind-specific receipt payload in existing generic integer slots. No ring growth,
 // borrowed identity, text or synchronous serial output is introduced.
 UiTraceRecord r;r.kind=UiTraceKind::SubmissionResult;r.us=clockUs();r.action=action;
 r.detail=uint8_t(result.status);r.usage=uint8_t(result.output);
 r.target_row=uint32_t(result.capacity);r.target_col=uint32_t(std::min(result.required_capacity,size_t(UINT32_MAX)));
 r.nav_pending=uint32_t(std::min(result.member_index,size_t(UINT32_MAX)));uiTrace.push(r);
}
void traceText(uint32_t action,uint8_t role,std::string_view text){
 if(!uiTrace.detailed())return;
 const auto size=std::min(text.size(),size_t(512));const auto count=(size+63)/64;
 for(size_t i=0;i<count;++i){UiTraceRecord r;r.kind=UiTraceKind::Text;r.us=clockUs();r.action=action;r.detail=role|(text.size()>512?128:0);r.text_part=uint8_t(i);r.text_parts=uint8_t(count);r.text_bytes=uint8_t(std::min(size-i*64,size_t(64)));std::copy_n(text.data()+i*64,r.text_bytes,r.text.begin());uiTrace.push(r);}
}

}
