#include "behavior_observer.hpp"
#if DRAFTOR_TELEMETRY_PROFILE
#include "device_ime.hpp"
#include "candidate_window.hpp"
#include "device_metrics.hpp"
#include <cstring>
#include <new>
namespace ime_device::behavior {
namespace t = draftor::telemetry;
namespace {
struct Frame { std::uint32_t generation{}, snapshot{}, revision{}, action{}, start{}, count{}; };
struct Ticket { std::uint32_t epoch{}, ms{}, event{}, action{}; std::uint8_t usage{}; };
struct Observer {
 t::TelemetryEmitter emitter;
 s3app::App& app;
 DeviceIme* ime;
 t::BehaviorEvent event{}; // Never put a rich event on a product call stack.
 t::EventEnvelope chosen{}, committed{}, display{};
 Frame frames[3]{};
 Ticket tickets[96]{};
 std::uint32_t ticket_cursor{}, external{}, compositions{}, composition{}, revision{}, snapshots{}, work{}, selections{}, pending_selection{}, displays{}, layout{}, action{};
 std::uint32_t previous_index{UINT32_MAX}, selection_begin{}, selection_end{}, last_text_revision{}, last_navigation_action{}, file_operation{};
 std::uint64_t last_session{}, last_control{};
 t::StateSnapshotEvent last_state{};
 unsigned control{};
 bool pending{}, display_pending{}, have_state{}, exhausted{}, stopped{};
 Observer(std::uint64_t boot,s3app::App& a,DeviceIme* i):emitter(boot,t::PrivacyLevel(DRAFTOR_TELEMETRY_PROFILE)),app(a),ime(i),last_text_revision(a.document.textRevision()),last_session(a.session.sessionSerial()){}
};
Observer* o=nullptr;
std::uint32_t next(std::uint32_t& n) noexcept {
 if(n==UINT32_MAX){o->emitter.record_transport_loss();o->exhausted=true;return 0;}
 return ++n;
}
std::uint32_t imeAction(std::uint32_t a) noexcept {
 if(!a||a>UINT32_MAX/2){o->emitter.record_transport_loss();if(a>UINT32_MAX/2)o->exhausted=true;return 0;}
 return a*2;
}
std::uint32_t external() noexcept {
 if(o->external>=UINT32_MAX/2){o->emitter.record_transport_loss();o->exhausted=true;return 0;}
 return (++o->external)*2-1;
}
std::uint8_t modeValue() noexcept {return o->app.horizontalActive()?1:2;}
t::EventEnvelope context(std::uint32_t action) noexcept {
 return {0,0,clockUs(),action,o->composition,o->revision,0,o->app.document.textRevision(),o->layout};
}
t::BehaviorEvent& begin(t::EventType type,std::uint32_t action) noexcept {
 o->event={};o->event.type=type;o->event.envelope=context(action);return o->event;
}
void emit() noexcept {if(!o->exhausted&&!o->stopped)o->emitter.try_emit(o->event);}
void detail(std::string_view text) noexcept {
#if DRAFTOR_TELEMETRY_PROFILE == 2
 // Never turn a truncated prefix into a claimed complete string.
 if(text.size()<=t::kTextCapacity){o->event.detail.text_length=std::uint8_t(text.size());std::memcpy(o->event.detail.text,text.data(),text.size());}
#else
 (void)text;
#endif
}
void route(std::uint32_t a,unsigned target,unsigned semantic) noexcept {
 if(!a)return;
 begin(t::EventType::RouteDecision,a).payload.route_decision={std::uint8_t(target),std::uint8_t(semantic)};emit();
}
Frame* frame(std::uint32_t generation) noexcept {
 if(generation)for(auto& f:o->frames)if(f.generation==generation&&f.revision==o->revision)return &f;
 return nullptr;
}
void rawFact(std::string_view raw,std::size_t focus,std::size_t members,std::uint32_t action,unsigned cause) noexcept {
 if(!action||(!o->composition&&raw.empty()))return;
 const bool first=!o->composition;
 if(first){o->composition=next(o->compositions);o->revision=0;}
 next(o->revision);o->action=action;o->previous_index=UINT32_MAX;
 for(auto& f:o->frames)f={};
 auto& e=begin(first?t::EventType::CompositionBegin:t::EventType::CompositionRevision,action);
 if(first)e.payload.composition_begin={std::uint32_t(raw.size()),std::uint32_t(focus),std::uint32_t(members),std::uint8_t(cause)};
 else e.payload.composition_revision={std::uint32_t(raw.size()),std::uint32_t(focus),std::uint32_t(members),std::uint8_t(cause)};
 detail(raw);emit();
}
void runtime() noexcept {
 // Opaque IDs resolved by this delivery's IDENTITY.json, never content hashes.
 auto& e=begin(t::EventType::RuntimeIdentity,0);
 #ifdef ARDUINO
 constexpr unsigned scope=2;
#else
 constexpr unsigned scope=1;
#endif
 e.payload.runtime_identity={20261002,234u+2u*DRAFTOR_TELEMETRY_PROFILE,130,841,1,1,23,scope,2,2,8,10000000,2,8,1,1};emit();
}
void snapshot(t::EventType type) noexcept {
 if(!o)return;
 auto& e=begin(type,o->action);t::StateSnapshotEvent s{};
 s.writing_mode=modeValue();s.base_input_mode=o->ime&&o->ime->chinese()?1:2;
 s.input_scheme=3;
 if(o->ime&&o->ime->chinese()){
  // Preserve the accepted pending-Pinyin snapshot; unencoded direct schemes
  // use r2 Unknown. Runtime build identity does not encode the active family.
  s.input_scheme=2;
  if(o->ime->schemeRules()==hanime_extension::SchemeRules::DirectExact)s.input_scheme=0;
  else if(o->ime->composition())switch(o->ime->composition()->raw_input().rules){
   case hanime_extension::SchemeRules::FullPinyin:s.input_scheme=1;break;
   case hanime_extension::SchemeRules::R1Xiaohe:s.input_scheme=2;break;
   default:s.input_scheme=0;break;
  }
 }
 s.edit_context=o->app.findOpen()?3:o->composition?2:1;
 if(o->ime){s.raw_length=std::uint32_t(o->ime->raw().size());if(o->ime->composition()){const auto c=o->ime->composition()->snapshot();s.focus=std::uint32_t(c.focus);s.selected_member_count=std::uint32_t(c.member_count);}
  if(o->ime->window()&&o->ime->candidateAvailable()){auto* f=frame(o->ime->window()->view().generation);if(f){s.candidate_valid=1;e.envelope.candidate_snapshot_id=f->snapshot;}}
 }
 if(type==t::EventType::StateChange&&o->have_state&&s.writing_mode==o->last_state.writing_mode&&s.base_input_mode==o->last_state.base_input_mode&&s.input_scheme==o->last_state.input_scheme&&s.edit_context==o->last_state.edit_context)return;
 if(type==t::EventType::SessionStart)e.payload.session_start={s.writing_mode,s.base_input_mode,s.input_scheme,s.edit_context,s.raw_length,s.focus,s.selected_member_count,s.candidate_valid};
 else if(type==t::EventType::SyncSnapshot)e.payload.sync_snapshot={s.writing_mode,s.base_input_mode,s.input_scheme,s.edit_context,s.raw_length,s.focus,s.selected_member_count,s.candidate_valid};
 else e.payload.state_change={s.writing_mode,s.base_input_mode,s.input_scheme,s.edit_context,s.raw_length,s.focus,s.selected_member_count,s.candidate_valid};
 o->last_state=s;o->have_state=true;emit();
}
void inputFact(const s3hid::Event& e,std::uint32_t a) noexcept {
 auto& r=begin(t::EventType::InputReceived,a);
 unsigned key=e.usage==80?2:e.usage==79?3:e.usage==82?4:e.usage==81?5:e.usage==42?6:e.usage==76?7:e.usage==40?8:e.usage==41?9:e.usage==43?10:e.usage>=4&&e.usage<=56?1:0;
 r.payload.input_received={std::uint8_t(key),e.mods};
 // Envelope time is the ordered observer observation from begin()/context().
 // Original HID received_us remains exclusively in TEST_ONLY bench latency.
 emit();o->action=a;
}
std::uint32_t editorAction(const s3hid::Event& e) noexcept {
 for(const auto& t:o->tickets)if(t.action&&t.epoch==e.epoch&&t.ms==e.ms&&t.event==e.event_id&&t.usage==e.usage)return t.action;
 const auto a=external();inputFact(e,a);return a;
}
void document(const s3app::App& app,std::uint32_t before,std::uint32_t cursor,t::EventEnvelope envelope) noexcept {
 const auto after=app.document.textRevision();if(before==after)return;
 const auto edit=app.document.lastTextEdit();
 if(!edit.incremental||edit.text_revision!=after||after!=before+1){o->emitter.record_transport_loss();o->last_text_revision=after;return;}
 const auto inserted=edit.new_end-edit.begin,removed=edit.old_end-edit.begin;
 auto& e=begin(t::EventType::DocumentMutation,envelope.action_id);e.envelope=envelope;e.envelope.device_monotonic_us=clockUs();e.envelope.document_revision=after;
 e.payload.document_mutation={before,after,std::uint8_t(inserted&&removed?3:inserted?1:2),std::uint32_t(edit.begin),std::uint32_t(edit.old_end),std::uint32_t(inserted),std::uint32_t(removed),cursor,std::uint32_t(app.document.cursorByte())};
 detail(std::string_view(app.document.text()).substr(edit.begin,inserted));emit();o->last_text_revision=after;o->action=envelope.action_id;
}
void fileInstalled(std::uint32_t action) noexcept {
 if(!o->file_operation||o->last_session==o->app.session.sessionSerial())return;
 auto& e=begin(t::EventType::FileOpEnd,action);e.payload.file_op_end={o->file_operation,7,1,std::uint32_t(o->app.document.text().size())};emit();o->file_operation=0;o->last_session=o->app.session.sessionSerial();o->last_text_revision=o->app.document.textRevision();
}
}
std::size_t storageBytes() noexcept {return sizeof(Observer);}
void bind(void* storage,std::uint64_t boot,s3app::App& app,DeviceIme* ime) noexcept {
 if(!storage||o)return;
 o=new(storage)Observer(boot,app,ime);snapshot(t::EventType::SessionStart);runtime();snapshot(t::EventType::SyncSnapshot);o->last_control=clockUs();
}
void stop() noexcept {if(o)o->stopped=true;}
bool drained() noexcept {return !o||o->emitter.queued()==0;}
const t::TelemetryHealth* health() noexcept{return o?&o->emitter.health():nullptr;}
void service(t::TelemetrySink& sink,bool input_pending) noexcept {
 if(!o||input_pending)return;
 fileInstalled(0); // Deferred App operations have returned before loop-tail service.
 // One small control event and at most one sink poll/offer per existing loop.
 const auto now=clockUs();
 if(!o->stopped&&!o->control&&(now-o->last_control>=5000000||o->emitter.snapshot_required()))o->control=1;
 if(!o->stopped&&o->control&&o->emitter.queued()<t::kQueueCapacity-4){
  if(o->control==1)snapshot(t::EventType::SyncSnapshot);
  else if(o->control==2)runtime();
  else {o->event=o->emitter.health_event(now);emit();}
  if(++o->control>3){o->control=0;o->last_control=now;}
 }
 o->emitter.service(sink);
}
void input(const s3hid::Event& e,std::uint32_t action) noexcept {
 if(!o||!e.down)return;const auto a=imeAction(action);if(!a)return;
 o->tickets[o->ticket_cursor++%96]={e.epoch,e.ms,e.event_id,a,e.usage};inputFact(e,a);
}
void raw(DeviceIme& ime,std::uint32_t action,unsigned cause) noexcept {
 if(!o)return;const auto a=imeAction(action);route(a,1,cause==3?5:cause==4?8:1);
 std::size_t focus=0,members=0;if(ime.composition()){const auto c=ime.composition()->snapshot();focus=c.focus;members=c.member_count;}
 rawFact(ime.raw(),focus,members,a,cause);
}
void ended(std::uint32_t action,unsigned outcome) noexcept {
 if(!o||!o->composition)return;
 const auto a=action?imeAction(action):external();
 begin(t::EventType::CompositionEnd,a).payload.composition_end={std::uint8_t(outcome)};emit();o->composition=o->revision=0;o->pending=false;o->committed={};for(auto& f:o->frames)f={};
}
void query(const hanime_extension::Composition& c,const CandidateWindow& w,std::size_t prefix) noexcept {
 if(!o||!o->composition)return;const auto s=c.snapshot();next(o->work);o->previous_index=UINT32_MAX;
 auto& e=begin(t::EventType::ImePlanBegin,imeAction(w.actionId()));e.payload.ime_plan_begin={o->work,std::uint32_t(s.focus),std::uint32_t(prefix?prefix:s.raw.size())};emit();
}
void range(const hanime_extension::Composition& c,const CandidateWindow& w) noexcept {
 if(!o||!o->composition)return;const auto p=c.page();
 begin(t::EventType::ImeRangeReady,imeAction(w.actionId())).payload.ime_range_ready={o->work,std::uint32_t(p.begin),std::uint32_t(p.end)};emit();
}
void window(const hanime_extension::Composition& c,const CandidateWindow& w) noexcept {
 if(!o||!o->composition||!w.view().count)return;const auto& v=w.view();
 std::uint32_t count=0;const auto start=v.rows[0].items[0].ordinal;
 for(std::size_t r=0;r<v.count;++r)for(std::size_t j=0;j<v.rows[r].count;++j){if(v.rows[r].items[j].ordinal!=start+count){o->emitter.record_transport_loss();return;}++count;}
 const auto& first=v.rows[0].items[0];const auto snap=next(o->snapshots);const auto a=imeAction(v.action);
 o->frames[2]=o->frames[1];o->frames[1]=o->frames[0];o->frames[0]={v.generation,snap,o->revision,a,std::uint32_t(start),count};
 auto& e=begin(t::EventType::CandidateDataReady,a);e.envelope.candidate_snapshot_id=snap;
 e.payload.candidate_data_ready={o->work,std::uint32_t(first.begin),std::uint32_t(first.end),t::kUnknownCount,std::uint32_t(start),count,1};emit();
#if DRAFTOR_TELEMETRY_PROFILE == 2
 for(std::size_t r=0;r<v.count;++r)for(std::size_t j=0;j<v.rows[r].count;++j){const auto& item=v.rows[r].items[j];auto& d=begin(t::EventType::CandidateItemDetail,a);d.envelope.candidate_snapshot_id=snap;d.payload.candidate_item_detail={std::uint32_t(item.ordinal),std::uint32_t(item.ordinal+1),0};detail({item.text.data(),item.bytes});emit();}
#endif
 (void)c;
}
void selection(const CandidateWindow& w,const VisibleItem& item,std::uint32_t generation,std::uint32_t action) noexcept {
 if(!o||!o->composition)return;const auto a=imeAction(action);const auto* f=frame(generation);
 std::uint32_t snap=f?f->snapshot:next(o->snapshots);
 if(!f){auto& e=begin(t::EventType::CandidateDataReady,a);e.envelope.candidate_snapshot_id=snap;
  e.payload.candidate_data_ready={o->work,std::uint32_t(item.begin),std::uint32_t(item.end),t::kUnknownCount,t::kUnknownCount,0,1};emit();}
 route(a,2,2);o->pending_selection=next(o->selections);o->chosen=context(a);o->chosen.candidate_snapshot_id=snap;o->pending=true;o->selection_begin=std::uint32_t(item.begin);o->selection_end=std::uint32_t(item.end);
 auto& e=begin(t::EventType::SelectionRequest,a);e.envelope=o->chosen;e.payload.selection_request={o->pending_selection,std::uint32_t(item.ordinal),std::uint32_t(item.ordinal+1)};emit();(void)w;
}
void selectionResult(const hanime_extension::Composition& c,unsigned outcome) noexcept {
 if(!o||!o->pending)return;const auto s=c.snapshot();const auto a=o->chosen.action_id;
 auto& e=begin(t::EventType::SelectionResult,a);e.envelope=o->chosen;e.envelope.device_monotonic_us=clockUs();e.payload.selection_result={o->pending_selection,std::uint8_t(outcome),o->selection_begin,o->selection_end,std::uint32_t(s.focus),outcome==1?o->revision+1:o->revision,t::kUnknownCount,0};emit();o->pending=false;
 if(outcome==1)rawFact(s.raw,s.focus,s.member_count,a,2);
}
void navigation(const CandidateWindow& w,std::uint32_t action,std::size_t row,std::size_t col,bool edge) noexcept {
 if(!o||!o->composition)return;auto* f=frame(w.view().generation);if(!f)return;const auto& v=w.view();
 // Batched arrow intents precede publication; observe only the settled focus.
 // The same action may be reported again by settleNavigation().
 if(row!=v.row||col!=v.col||action==o->last_navigation_action)return;
 if(row<v.rows[0].index||row-v.rows[0].index>=v.count||col>=v.rows[row-v.rows[0].index].count)return;
 o->last_navigation_action=action;
 const auto index=std::uint32_t(v.rows[row-v.rows[0].index].items[col].ordinal);const auto a=imeAction(action);route(a,2,3);
 auto& e=begin(t::EventType::CandidateNavigation,a);e.envelope.candidate_snapshot_id=f->snapshot;e.payload.candidate_navigation={o->previous_index,index,f->start,f->count,0,std::uint8_t(edge?2:1)};emit();o->previous_index=index;
}
void revalidated(const hanime_extension::Composition& c,std::uint32_t action) noexcept {if(o&&o->composition){const auto s=c.snapshot();rawFact(s.raw,s.focus,s.member_count,imeAction(action),2);}}
void boundary() noexcept {if(o){o->file_operation=next(o->work);auto& e=begin(t::EventType::FileOpBegin,0);e.payload.file_op_begin={o->file_operation,7};emit();}}
void confirm(std::uint32_t action) noexcept {if(o)o->committed=context(imeAction(action));}
void commit(const s3app::App& app,std::uint32_t before,std::size_t cursor) noexcept {
 if(o&&o->committed.action_id)document(app,before,std::uint32_t(cursor),o->committed);
}
void mode(const s3app::App&,const s3hid::Event& e) noexcept {if(o){inputFact(e,external());snapshot(t::EventType::StateChange);}}
void state() noexcept {snapshot(t::EventType::StateChange);}
EditorScope::EditorScope(const s3app::App& a,const s3hid::Event& e) noexcept:app_(o&&e.down?&a:nullptr),before_(a.document.textRevision()),cursor_(std::uint32_t(a.document.cursorByte())),action_(app_?editorAction(e):0),session_(a.session.sessionSerial()),usage_(e.usage),mods_(e.mods){}
EditorScope::~EditorScope(){
 if(!app_||!o)return;const auto& a=*app_;
 if(session_!=a.session.sessionSerial()){
  fileInstalled(action_);return;
 }
 if(before_==a.document.textRevision()&&cursor_==a.document.cursorByte())return;
 const auto sem=usage_==42?6:usage_==76?7:usage_>=79&&usage_<=82?4:usage_==40?9:(!(mods_&0xdd)&&usage_>=4&&usage_<=56)?10:0;
 route(action_,3,sem);auto& e=begin(t::EventType::EditorAction,action_);e.envelope.composition_id=e.envelope.composition_revision=0;e.payload.editor_action={std::uint8_t(sem),0,cursor_,std::uint32_t(a.document.cursorByte())};emit();auto envelope=context(action_);envelope.composition_id=envelope.composition_revision=0;document(a,before_,cursor_,envelope);
}
void displayBegin(const s3app::App&,const DeviceIme* ime,bool candidates) noexcept {
 if(!o||!o->action)return;const auto* f=candidates&&ime&&ime->window()?frame(ime->window()->view().generation):nullptr;
 next(o->layout);auto& e=begin(t::EventType::ViewReady,f?f->action:o->action);if(f)e.envelope.candidate_snapshot_id=f->snapshot;e.payload.view_ready={modeValue(),std::uint8_t(f?2:o->composition?1:8)};o->display=e.envelope;emit();
 begin(t::EventType::DisplaySubmit,o->display.action_id).envelope=o->display;o->event.payload.display_submit={next(o->displays)};emit();o->display_pending=true;
}
void displayEnd(bool success) noexcept {
 if(!o||!o->display_pending)return;auto& e=begin(t::EventType::DisplayAck,o->display.action_id);e.envelope=o->display;e.envelope.device_monotonic_us=clockUs();e.payload.display_ack={o->displays,std::uint8_t(success?1:2)};emit();o->display_pending=false;
}
}
#endif
