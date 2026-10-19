// TEST_ONLY endpoint format reused from the completed finite B1 experiment.
// No B1 parser, resource, cache or scheduling changes are included.
#include "service_endpoints.hpp"
#include <cstdio>
#if DRAFTOR_AUTH_OBSERVE
namespace ime_device {
bool serviceEndpoint(UiTraceKind k,uint8_t d){
 using K=UiTraceKind;
 if(k==K::Intent)return d==Received||d==Executing||d==RawEdited||d==CancelledByUser||d==QueueFull||d==ResourceFailure||d==QueryLimited||d==QueryUnresolved||d==CommitFailed||d==ExpiredInput||d==ExpiredTarget||d==QueuedQuery||d==QueuedCommit||d==QueuedSuccessor;
 return k==K::KeyDown||k==K::Query||k==K::RangeReady||k==K::WindowReady||k==K::Lcd||k==K::RowRequest||k==K::BodyAccepted||k==K::BodyLcd||k==K::CommitQueued||k==K::CommitStopped||k==K::SelectionDone||k==K::SelectionResult||k==K::SubmissionResult||k==K::FeedbackLcd||k==K::RouteDrop;
}
bool serviceCompletion(UiTraceKind k){return k==UiTraceKind::Query||k==UiTraceKind::RowRequest||k==UiTraceKind::RangeReady||k==UiTraceKind::WindowReady||k==UiTraceKind::Lcd;}
void snapshotServiceEndpoint(UiTraceRecord& r,const hanime_extension::Composition* c,const CandidateWindow* w){
 // Endpoint-only overlay of unused generic slots. Never consumed by product
 // identity, authorization, presentation or submission code. Ring ABI unchanged.
 if(c){const auto s=c->snapshot();r.members=uint16_t(s.member_count);r.text_bytes=uint8_t(std::min(s.raw.size(),size_t(63)));std::copy_n(s.raw.data(),r.text_bytes,r.text.data());r.text[r.text_bytes]=0;}
 // Reuse spare slots of the existing 352 B endpoint record. No new ring,
 // per-slice events, allocation, reads, or mutation of product state.
 if(c&&serviceCompletion(r.kind)){
  const auto plan=c->planning_report();const auto page=c->page();
  r.resource_owner=unsigned(plan.valid)|(unsigned(plan.limited)<<1)|(unsigned(plan.incomplete_tail)<<2)|(unsigned(plan.unknown_tail)<<3)|(unsigned(plan.direct_range)<<4);
  r.current_resource_owner=plan.policy;r.event_ms=uint32_t(plan.begin);r.epoch=uint32_t(plan.end);r.route=uint32_t(plan.covered_end);
  r.event_frame=uint32_t(plan.checked_ranges);r.attempt=uint32_t(plan.queries);r.ingress=uint32_t(plan.cache_hits);r.parent=uint32_t(plan.score);
  r.view_row=uint32_t(plan.syllables);r.view_col=uint32_t(plan.words);r.edits=uint16_t(page.size);r.letters=uint16_t(page.has_more);r.key_class=uint8_t(c->status());
  r.item_input=metrics.phases[unsigned(Phase::Query)].us;r.item_revision=metrics.phases[unsigned(Phase::VisibleText)].us;
  r.item_scheme=metrics.reads;r.item_mount=metrics.read_us+metrics.seek_us;
  r.item_bundle=metrics.bytes; // Unused completion overlay slot; cumulative physical bytes.
 }
 r.target_session=metrics.phases[unsigned(Phase::Query)].work;r.target_focus=0;r.pending_letters=0;
 r.text_part=w?uint8_t(w->state()):255;
 r.text_parts=w?uint8_t(w->view().expanded)|(uint8_t(w->view().at_end)<<1):0;
 if(!w||(r.kind!=UiTraceKind::WindowReady&&r.kind!=UiTraceKind::Lcd))return;
 const auto& v=w->view();uint64_t h=1469598103934665603ULL;
 auto byte=[&](uint8_t b){h^=b;h*=1099511628211ULL;};
 auto number=[&](uint64_t n){for(unsigned i=0;i<8;++i)byte(uint8_t(n>>(8*i)));};
 number(v.count);number(v.row);number(v.col);number(v.at_end);
 r.pending_letters=0;
 for(size_t i=0;i<v.count;++i){const auto& row=v.rows[i];number(row.index);number(row.count);number(row.width);
  for(size_t j=0;j<row.count;++j){const auto& item=row.items[j];++r.pending_letters;
   for(auto b:item.key.bytes)byte(b);
   number(item.ref);number(item.begin);number(item.end);number(item.ordinal);
   for(size_t k=0;k<item.bytes;++k)byte(uint8_t(item.text[k]));
  }
 }
 r.target_focus=h;
}
size_t formatServiceEndpoint(const UiTraceRecord& r,unsigned part,char* out,size_t cap){
 using U=unsigned long long;int n=0;
 if(part==0)n=std::snprintf(out,cap,"[SW] n=%u k=%u t=%llu rx=%llu a=%u q=%u i=%llu v=%llu s=%u raw=%s len=%u f=%u m=%u d=%u key=%u event=%u\n",unsigned(r.sequence),unsigned(r.kind),U(r.us),U(r.received_us),unsigned(r.action),unsigned(r.query),U(r.input),U(r.revision),unsigned(r.scheme_kind),r.text.data(),unsigned(r.raw),unsigned(r.focus),unsigned(r.members),unsigned(r.detail),unsigned(r.usage),unsigned(r.event_id));
 else if(part==1)n=std::snprintf(out,cap,"[SW_VIEW] n=%u work=%llu hash=%016llx state=%u expanded=%u end=%u first=%u rows=%u row=%u col=%u count=%u frame=%u screen=%u scheme=%llu mount=%llu bundle=%llu\n",unsigned(r.sequence),U(r.target_session),U(r.target_focus),unsigned(r.text_part),unsigned(r.text_parts&1),unsigned((r.text_parts>>1)&1),unsigned(r.first_row),unsigned(r.row_count),unsigned(r.target_row),unsigned(r.target_col),unsigned(r.pending_letters),unsigned(r.view_frame),unsigned(r.screen_frame),U(r.scheme),U(r.mount),U(r.bundle));
 else if(part==2)n=std::snprintf(out,cap,"[AUTH_DONE] n=%u flags=%u policy=%u b=%u e=%u cov=%u ranges=%u queries=%u hits=%u score=%d syll=%u words=%u size=%u more=%u status=%u qus=%llu tus=%llu reads=%llu ious=%llu bytes=%llu\n",unsigned(r.sequence),unsigned(r.resource_owner),unsigned(r.current_resource_owner),unsigned(r.event_ms),unsigned(r.epoch),unsigned(r.route),unsigned(r.event_frame),unsigned(r.attempt),unsigned(r.ingress),int(int32_t(r.parent)),unsigned(r.view_row),unsigned(r.view_col),unsigned(r.edits),unsigned(r.letters),unsigned(r.key_class),U(r.item_input),U(r.item_revision),U(r.item_scheme),U(r.item_mount),U(r.item_bundle));
 return n>0&&size_t(n)<cap?size_t(n):0;
}
void traceServiceFeedback(const hanime_extension::Composition* c,const CandidateWindow* w,uint64_t done,uint8_t detail){
#if DRAFTOR_AUTH_OBSERVE
 static uint64_t lastInput=0,lastRevision=0,lastScheme=0;
 if(!c)return;
 const auto s=c->snapshot();if(s.input_id==lastInput&&s.revision==lastRevision&&s.scheme_epoch==lastScheme)return;
 lastInput=s.input_id;lastRevision=s.revision;lastScheme=s.scheme_epoch;
 traceUi(UiTraceKind::FeedbackLcd,c,w,nullptr,nullptr,detail,0,done);
#else
 (void)c;(void)w;(void)done;(void)detail;
#endif
}
}

#endif
