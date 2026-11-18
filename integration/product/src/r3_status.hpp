#pragma once
#include "device_ime.hpp"
#include "idle_log.hpp"
#include "stability_diagnostics.hpp"
namespace r3 {
// Explicit, low-frequency reads of existing state only. No query, allocation,
// Count lookup, filesystem operation, scheduler drain or product mutation.
inline void keyText(const hanime::StableLexemeKey& key,char (&out)[65]){
 static constexpr char h[]="0123456789abcdef";
 for(unsigned n=0;n<32;++n){out[2*n]=h[key.bytes[n]>>4];out[2*n+1]=h[key.bytes[n]&15];}out[64]=0;
}
inline bool status(const char* command,ime_device::DeviceIme& ime,ime_device::LearningTrial& l,ime_device::IdleLog& log){
 using U=unsigned long long;
 if(std::strcmp(command,"R3PAGE")==0){
  auto* c=ime.composition();if(!c){log.printf("[R3_PAGE] unavailable=1\n");return true;}
  const auto p=c->page();const auto raw=ime.raw();
  log.printf("[R3_PAGE] raw=%.*s mode=%u size=%u more=%u scheme=%u\n",int(raw.size()),raw.data(),unsigned(p.mode),unsigned(p.size),unsigned(p.has_more),unsigned(ime.schemeRules()));
  for(size_t n=0;n<p.size&&n<6;++n){const auto& x=p.items[n];char k[65];keyText(x.key,k);log.printf("[R3_RANK] slot=%u key=%s ref=%u count=%u score=%lld ordinal=%u\n",unsigned(n),k,unsigned(x.ref),unsigned(x.effective_count),(long long)x.score,unsigned(x.representative_ordinal));}
  return true;
 }
 if(std::strcmp(command,"R3RAM")==0){
  hanime::StateEntry entries[4];auto* owner=l.counts();const auto n=owner?owner->snapshot_ram(entries,4):0;
  log.printf("[R3_RAM] slots=%u\n",unsigned(n));
  for(size_t i=0;i<n;++i){char k[65];keyText(entries[i].key,k);log.printf("[R3_COUNT] key=%s count=%u kind=%u update=%llu\n",k,unsigned(entries[i].count),unsigned(entries[i].kind),U(entries[i].update));}
  return true;
 }
 if(std::strcmp(command,"R3STATUS")!=0)return false;
 const auto h=l.health();const auto s=l.fileInfo();const auto& st=l.stats;const auto& io=l.files().io;
 log.printf("[R3_STATE] phase=%u status=%u record=%u apply=%u work=%u dirty=%u frozen=%u batches=%u generation=%llu keys=%llu tail=%llu sequence=%llu\n",unsigned(l.phase()),unsigned(l.status()),l.recording(),l.applying(),l.hasWork(),unsigned(h.dirty),h.frozen_write,unsigned(h.batches_pending),U(s.generation),U(s.snapshot_keys),U(s.tail_frames),U(s.sequence));
 log.printf("[R3_LEARNING] accepted=%llu body=%llu requested=%llu ram=%llu saved=%llu paused=%llu overflow=%llu max_step_us=%llu\n",U(st.accepted),U(st.body_accepted),U(st.requested_members),U(st.ram_members),U(st.saved_members),U(st.paused_members),U(st.observation_overflow),U(st.max_step_us));
 log.printf("[R3_IO] reads=%llu writes=%llu syncs=%llu controls=%llu errors=%llu handles=%u peak_handles=%u max_read_us=%llu max_write_us=%llu max_sync_us=%llu max_control_us=%llu\n",U(io.reads),U(io.writes),U(io.syncs),U(io.controls),U(io.errors),io.handles,io.peak_handles,U(io.max_read_us),U(io.max_write_us),U(io.max_sync_us),U(io.max_control_us));
 const auto batch=l.stateDiagnostics();
 if(auto* owner=l.counts()){
  const auto q=owner->counts_progress();
  log.printf("[R31_PROGRESS] phase=%s cursor=%llu members=%u stop=%s work=%llu deadline_yields=%llu zero_returns=%llu sort_steps=%llu\n",q.phase,U(q.cursor),unsigned(q.members),q.stop,U(q.work),U(q.deadline_yields),U(q.zero_progress_returns),U(batch.batch_sort_steps));
 }
 log.printf("[R31_COUNT_IO] reads=%llu bytes=%llu read_us=%llu batches=%llu members=%llu snapshot_decodes=%llu pivots=%llu stream=%llu tail=%llu restarts=%llu batch_peak=%u\n",U(io.reads),U(io.read_bytes),U(io.read_us),U(batch.batch_begins),U(batch.batch_members),U(batch.snapshot_decodes),U(batch.batch_pivots),U(batch.batch_stream_records),U(batch.batch_tail_frames),U(batch.batch_restarts),unsigned(batch.batch_peak_bytes));
 if(auto* p=l.personal()){
  const auto x=p->cost();const auto v=p->persistence();
  log.printf("[R3_INDEX] healthy=%u bitmap_words=%llu members=%llu count_queries=%llu fallback=%llu coverage_waits=%llu during_save=%llu new=%llu existing=%llu added=%llu memory=%u peak=%u max_step_us=%llu\n",p->healthy(),U(x.bitmap_words),U(x.positive_members),U(x.count_queries),U(x.local_fallbacks),U(x.coverage_waits),U(x.query_during_save),U(x.new_identities),U(x.existing_updates),U(x.relations_added),unsigned(x.memory),unsigned(x.peak),U(x.max_step_us));
  log.printf("[R3_PERSIST] loaded=%u covered=%u saving=%u checkpoint=%u phase=%u dirty=%u journal=%u saved=%llu frozen=%llu checkpoints=%llu copy_bytes=%llu certificates=%llu error=%s\n",v.loaded,v.coverage_complete,v.saving,v.checkpoint,v.phase,unsigned(v.dirty_words),unsigned(v.journal_bytes),U(v.saved_sequence),U(v.frozen_sequence),U(x.checkpoints),U(x.snapshot_copy_bytes),U(x.certificate_rewrites),p->error());
 }
 const auto q=log.costs();
 log.printf("[R3_QUEUE] dropped=%llu oversized=%llu input_overflow=%u arrow_overflow=%u\n",U(q.dropped),U(q.oversized),unsigned(ime.inputOverflow()),unsigned(ime.arrowOverflow()));
 s3app::stability::capture(s3app::stability::Kind::Memory,0x5233);
 return true;
}
}
