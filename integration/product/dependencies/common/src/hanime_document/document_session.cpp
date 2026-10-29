#include "../../core_memory.hpp"
#include "document_session.hpp"

namespace hanime::documents {
namespace {
class BusyGuard {
 public:
  explicit BusyGuard(bool& busy) : busy_(busy) { busy_ = true; }
  ~BusyGuard() { busy_ = false; }
 private: bool& busy_;
};
bool success(const Status s) { return s == Status::Ok || s == Status::OkDegraded; }
}  // namespace

void retireRestoreToken(RestoreToken* const token) noexcept {
  if (!token) return;
  draftor::memory::String retired_text;
  retired_text.swap(token->target.text);
  token->valid = false;
  token->token_serial = 0U;
  token->document_id = {};
  token->editor_revision = 0U;
  token->session_serial = 0U;
  token->mount_epoch = 0U;
  token->latest_body_generation = 0U;
  token->command = RestoreCommand::Latest;
  token->target.text.clear();
  token->target.cursor_byte = 0U;
  token->target.viewport = {};
  token->target.generation = 0U;
  token->target.text_crc32 = 0U;
  token->target_source = RestoreSource::None;
}

OperationResult DocumentSession::startupRestore() {
  if (busy_) return {Status::Busy, "start"};
  BusyGuard guard(busy_);
  const Status initialized = catalog_.initialize(); if (!success(initialized)) return {initialized, "collection"};
  CurrentRecord current{}; const Status loaded = catalog_.loadCurrent(&current);
  if (loaded == Status::NotFound || (success(loaded) && !current.has_current)) return {Status::NoCurrentDocument, "current_record"};
  if (!success(loaded)) return {loaded, "current_record"};
  DocumentMetadata meta{}; const Status got = catalog_.get(current.document_id, &meta);
  if (!success(got)) return {got, "current_target"};
  if (meta.area != DocumentArea::Normal) return {Status::NoCurrentDocument, "current_target"};
  LoadResult body = persistence_.loadLatest(current.document_id); if (!success(body.status)) return {body.status, "body_load"};
  const Status position = persistence_.applyPosition(current.document_id, &body.snapshot);
  // Position is optional: an imported snapshot already owns a valid default
  // cursor/viewport. Missing POSITION is not a failed body save. Other read,
  // validation and degraded-record outcomes must remain visible.
  const std::uint64_t body_generation = body.snapshot.generation;
  const std::uint32_t body_crc32 = body.snapshot.text_crc32;
  const Status installed = editor_.install(&body.snapshot); if (installed != Status::Ok) return {Status::InstallFailed, "editor_install"};
  current_id_ = current.document_id; ++session_serial_;
  editor_.onDocumentActivated(current_id_);
  persistence_.bindCurrent(current_id_, session_serial_, body_generation,
                           body_crc32, editor_.bodyRevision(),
                           body.latest_body_generation, body.source);
  if (position != Status::Ok && position != Status::NotFound) {
    return {Status::OkDegraded, position == Status::OkDegraded
        ? "position_degraded" : "position_fallback", false, true};
  }
  if (body.status == Status::OkDegraded)
    return {Status::OkDegraded, "body_recovered", false, true};
  if (initialized == Status::OkDegraded || loaded == Status::OkDegraded ||
      got == Status::OkDegraded)
    return {Status::OkDegraded, "metadata_recovered", false, true};
  return {Status::Ok, position == Status::NotFound ? "position_default" : "complete",
          false, true};
}

OperationResult DocumentSession::saveOutgoingIfDirty(const std::uint32_t now_ms) {
  if (persistence_.backgroundSaveActive()) return {Status::Busy, "background_save"};
  if (!hasCurrent()) return {Status::Ok, "clean"};
  if (persistence_.bodyDirty()) {
    return persistence_.saveCurrentAuto(now_ms, editor_);
  }
  if (persistence_.positionDirty()) {
    return persistence_.flushCurrentPosition(editor_);
  }
  return {Status::Ok, "clean"};
}

OperationResult DocumentSession::installAndCommitCurrent(
    const DocumentId& id, Snapshot prepared, const bool touch_order,
    const std::uint64_t latest_body_generation,
    const RestoreSource body_source) {
  const std::uint64_t body_generation = prepared.generation;
  const std::uint32_t body_crc32 = prepared.text_crc32;
  const Status installed = editor_.install(&prepared); if (installed != Status::Ok) return {Status::InstallFailed, "editor_install"};
  current_id_ = id; ++session_serial_;
  editor_.onDocumentActivated(current_id_);
  persistence_.bindCurrent(current_id_, session_serial_, body_generation,
                           body_crc32, editor_.bodyRevision(),
                           latest_body_generation, body_source);
  Status touch = Status::Ok; if (touch_order) touch = catalog_.touchOpened(id);
  CurrentRecord record{}; record.has_current = true; record.document_id = id;
  const Status current_saved = catalog_.commitCurrent(record);
  if (current_saved != Status::Ok) return {Status::Partial, "current_record", false, false};
  if (touch != Status::Ok) return {Status::Partial, "opened_order", false, true};
  return {Status::Ok, "complete", false, true};
}

OperationResult DocumentSession::open(const DocumentId& id, const std::uint32_t now_ms) {
  if (busy_) return {Status::Busy, "start"};
  BusyGuard guard(busy_);
  if (id.empty()) return {Status::NotFound, "target"};
  if (id == current_id_) return {Status::Ok, "same_document"};
  if (editor_.compositionActive()) return {Status::CompositionActive, "composition"};
  if (editor_.inputQueuePending()) return {Status::Busy, "input_queue"};
  const std::uint64_t outgoing_revision = editor_.revision();
  const OperationResult saved = saveOutgoingIfDirty(now_ms); if (!saved) return {saved.status, "outgoing_save"};
  if (editor_.revision() != outgoing_revision && persistence_.bodyDirty()) return {Status::Conflict, "outgoing_revision"};
  DocumentMetadata target{}; const Status got = catalog_.get(id, &target);
  if (!success(got) || target.area != DocumentArea::Normal) return {success(got) ? Status::NotFound : got, "target_metadata"};
  LoadResult loaded = persistence_.loadLatest(id); if (!success(loaded.status)) return {loaded.status, "target_load"};
  (void)persistence_.applyPosition(id, &loaded.snapshot);
  if (editor_.revision() != outgoing_revision) return {Status::Conflict, "preinstall_revision"};
  return installAndCommitCurrent(id, std::move(loaded.snapshot), true,
                                 loaded.latest_body_generation, loaded.source);
}

const char* DocumentSession::createStage() const noexcept {
  static const char* names[]={"idle","reserve_plan","collection","intent","initial_auto","publish","clear_intent","install","current","finish"};
  return names[static_cast<unsigned>(create_phase_)];
}
void DocumentSession::abortCreate() noexcept {
  if(external_create_)return;
  if(creating())catalog_.abortCreateRecord();
  create_writer_={};create_intent_={};draftor::memory::String{}.swap(create_name_);draftor::memory::String{}.swap(create_payload_);
  create_phase_=CreatePhase::Idle;create_writing_=false;busy_=false;
}
OperationResult DocumentSession::startCreate(std::string_view name,std::uint32_t now_ms) {
  if(busy_)return {Status::Busy,"start"};
  if(editor_.compositionActive())return {Status::CompositionActive,"composition"};
  if(editor_.inputQueuePending())return {Status::Busy,"input_queue"};
  if(name.size()>kMaxNameBytes)return {Status::InvalidName,"create_name"};
  const auto outgoing=saveOutgoingIfDirty(now_ms);if(!outgoing)return outgoing;
  if(!catalog_.cooperativeCreateSupported())return create(name,now_ms);
  create_name_.assign(name);create_revision_=editor_.revision();create_intent_={};
  create_generation_=0;create_resumed_=create_writing_=create_published_=create_restart_=false;
  create_phase_=CreatePhase::Plan;busy_=true;
  return {Status::Busy,"create_started"};
}
OperationResult DocumentSession::advanceCreate() {
  if(!creating())return {Status::Conflict,"no_create"};
  const char* stage=createStage();
  auto fail=[&](Status status){const bool published=create_published_;abortCreate();return OperationResult{status,stage,published,false};};
  auto valid=catalog_.validateSession();if(valid!=Status::Ok)return fail(valid);
  if(create_writing_){
    const auto step=catalog_.serviceCreateRecord(create_writer_);
    if(create_writer_.active())return {Status::Busy,stage};
    if(step.status!=Status::Ok){
      if(create_phase_==CreatePhase::Publish){DocumentMetadata actual;auto got=catalog_.get(create_intent_.metadata.id,&actual);
        if((got==Status::Ok||got==Status::OkDegraded)&&actual.name==create_intent_.metadata.name){create_published_=true;return fail(Status::Partial);}}
      return fail(create_phase_==CreatePhase::Current||create_published_?Status::Partial:step.status);
    }
    if(create_phase_==CreatePhase::Publish){create_published_=true;catalog_.publishedCreate(create_intent_,create_writer_.generation());}
    create_writer_={};create_writing_=false;create_payload_.clear();
    if(create_phase_==CreatePhase::Clear&&create_restart_){create_restart_=false;create_published_=false;create_phase_=CreatePhase::Plan;}
    else create_phase_=static_cast<CreatePhase>(static_cast<unsigned>(create_phase_)+1);
    return {Status::Busy,stage};
  }
  if(create_phase_==CreatePhase::Plan){
    const auto planned=catalog_.planCreate(create_name_,&create_intent_,&create_generation_,&create_resumed_);
    if(!success(planned))return fail(planned);
    if(create_resumed_&&create_intent_.metadata.area==DocumentArea::Trash){
      create_restart_=true;create_phase_=CreatePhase::Clear; // finish old intent, never reopen a trashed document
    }else create_phase_=create_resumed_?CreatePhase::Initial:CreatePhase::Collection;
    return {Status::Busy,stage};
  }
  if(create_phase_==CreatePhase::Install){
    if(editor_.revision()!=create_revision_)return fail(Status::Conflict);
    Snapshot prepared;prepared.generation=1;prepared.text_crc32=crc32(prepared.text);
    std::uint64_t latest=1;RestoreSource source=RestoreSource::Auto;
    if(create_resumed_){auto loaded=persistence_.loadLatest(create_intent_.metadata.id);if(!success(loaded.status))return fail(loaded.status);
      prepared=std::move(loaded.snapshot);latest=loaded.latest_body_generation;source=loaded.source;}
    const auto generation=prepared.generation;const auto crc=prepared.text_crc32;
    if(editor_.install(&prepared)!=Status::Ok)return fail(Status::InstallFailed);
    current_id_=create_intent_.metadata.id;++session_serial_;
    editor_.onDocumentActivated(current_id_);
    persistence_.bindCurrent(current_id_,session_serial_,generation,crc,editor_.bodyRevision(),latest,source);
    create_phase_=CreatePhase::Current;return {Status::Busy,stage};
  }
  if(create_phase_==CreatePhase::Finish){create_phase_=CreatePhase::Idle;abortCreate();return {Status::Ok,"complete",true,true};}
  Status started=Status::Conflict;
  using R=DocumentCatalog::CreateRecord;
  switch(create_phase_){
  case CreatePhase::Collection:started=catalog_.startCreateRecord(R::Collection,create_intent_,create_generation_,create_writer_,create_payload_);break;
  case CreatePhase::Intent:started=catalog_.startCreateRecord(R::Intent,create_intent_,create_generation_,create_writer_,create_payload_);break;
  case CreatePhase::Initial:
    if(create_resumed_){Snapshot blank;started=persistence_.saveInitial(create_intent_.metadata.id,blank);}
    else started=persistence_.startBlankInitial(create_intent_.metadata.id,create_writer_,create_payload_);
    break;
  case CreatePhase::Publish:started=catalog_.startCreateRecord(R::Metadata,create_intent_,create_generation_,create_writer_,create_payload_);break;
  case CreatePhase::Clear:started=catalog_.startCreateRecord(R::ClearIntent,create_intent_,create_generation_-(create_resumed_?1:0),create_writer_,create_payload_);break;
  case CreatePhase::Current:started=catalog_.startCreateRecord(R::Current,create_intent_,create_generation_,create_writer_,create_payload_);break;
  default:break;
  }
  if(started!=Status::Ok&&started!=Status::Busy)return fail(started);
  create_writing_=create_writer_.active();
  if(!create_writing_){if(create_phase_==CreatePhase::Publish)create_published_=true;create_phase_=static_cast<CreatePhase>(static_cast<unsigned>(create_phase_)+1);}
  return {Status::Busy,stage};
}

OperationResult DocumentSession::create(const std::string_view name, const std::uint32_t now_ms, DocumentId* created_id) {
  if(busy_)return {Status::Busy,"start"};
  if(catalog_.cooperativeCreateSupported()){
    auto result=startCreate(name,now_ms);while(creating())result=advanceCreate();
    if(created_id&&result)*created_id=current_id_;
    return result;
  }
  if (busy_) return {Status::Busy, "start"};
  BusyGuard guard(busy_);
  if (editor_.compositionActive()) return {Status::CompositionActive, "composition"};
  if (editor_.inputQueuePending()) return {Status::Busy, "input_queue"};
  const OperationResult outgoing = saveOutgoingIfDirty(now_ms); if (!outgoing) return {outgoing.status, "outgoing_save"};
  CreateIntent intent{}; const Status reserved = catalog_.reserveCreate(name, &intent); if (!success(reserved)) return {reserved, "create_reserve"};
  Snapshot blank{}; blank.text_crc32 = crc32(blank.text);
  const Status initial = persistence_.saveInitial(intent.metadata.id, blank); if (initial != Status::Ok) return {initial, "create_blank"};
  const Status published = catalog_.publishCreate(intent); if (published != Status::Ok) return {published, "create_publish", true, false};
  if (created_id) *created_id = intent.metadata.id;
  LoadResult loaded = persistence_.loadLatest(intent.metadata.id); if (!success(loaded.status)) return {loaded.status, "create_reload", true, false};
  return installAndCommitCurrent(intent.metadata.id, std::move(loaded.snapshot), false,
                                 loaded.latest_body_generation, loaded.source);
}

OperationResult DocumentSession::moveToTrash(const DocumentId& id, const std::uint32_t now_ms) {
  if (busy_) return {Status::Busy, "start"};
  BusyGuard guard(busy_);
  DocumentMetadata meta{}; const Status got = catalog_.get(id, &meta); if (!success(got)) return {got, "metadata"};
  if (meta.area == DocumentArea::Trash) return {Status::Ok, "already_trashed"};
  const bool current = id == current_id_;
  if (current) {
    if (editor_.compositionActive()) return {Status::CompositionActive, "composition"};
    if (editor_.inputQueuePending()) return {Status::Busy, "input_queue"};
    const OperationResult saved = saveOutgoingIfDirty(now_ms); if (!saved) return {saved.status, "current_save"};
  }
  const Status moved = catalog_.setArea(id, DocumentArea::Trash); if (moved != Status::Ok && moved != Status::Partial) return {moved, "trash_metadata"};
  if (!current) return {moved, moved == Status::Ok ? "complete" : "trash_metadata_visible"};
  current_id_ = {}; ++session_serial_; persistence_.unbindCurrent();
  CurrentRecord empty{}; const Status cleared = catalog_.commitCurrent(empty);
  return {cleared == Status::Ok && moved == Status::Ok ? Status::Ok : Status::Partial,
          cleared == Status::Ok ? "complete" : "current_record", true, cleared == Status::Ok};
}

OperationResult DocumentSession::restoreFromTrash(const DocumentId& id) {
  if (busy_) return {Status::Busy, "start"};
  BusyGuard guard(busy_);
  DocumentMetadata meta{}; const Status got = catalog_.get(id, &meta); if (!success(got)) return {got, "metadata"};
  if (meta.area == DocumentArea::Normal) return {Status::Ok, "already_normal"};
  const Status restored = catalog_.setArea(id, DocumentArea::Normal);
  return {restored, restored == Status::Ok ? "complete" : "restore_metadata"};
}

LoadResult DocumentSession::loadRestoreTarget(const RestoreCommand command) {
  if (!hasCurrent()) return {Status::NoCurrentDocument};
  if (command == RestoreCommand::Latest) return persistence_.loadLatest(current_id_);
  if (command == RestoreCommand::Manual) {
    LoadResult result = persistence_.loadLane(current_id_, SaveLane::Manual);
    if (result.status == Status::NotFound) result.status = Status::NoManualVersion;
    return result;
  }
  return persistence_.loadLane(current_id_, SaveLane::Protection);
}

Status DocumentSession::prepareRestore(const RestoreCommand command, RestoreToken* out) {
  if (!out) return Status::Conflict;
  if (busy_) return Status::Busy;
  if (!hasCurrent()) return Status::NoCurrentDocument;
  if (editor_.compositionActive()) return Status::CompositionActive;
  if (editor_.inputQueuePending()) return Status::Busy;
  LoadResult target = loadRestoreTarget(command); if (!success(target.status)) return target.status;
  RestoreToken token{}; token.valid = true; token.token_serial = ++token_serial_; token.document_id = current_id_;
  token.editor_revision = editor_.revision(); token.session_serial = session_serial_;
  token.mount_epoch = catalog_.boundSession().mount_epoch; token.command = command;
  token.latest_body_generation = target.latest_body_generation;
  token.target = std::move(target.snapshot); token.target_source = target.source; *out = std::move(token);
  return Status::ConfirmationRequired;
}

OperationResult DocumentSession::confirmRestore(RestoreToken* token, const std::uint32_t now_ms) {
  (void)now_ms;
  if (busy_) return {Status::Busy, "start"};
  BusyGuard guard(busy_);
  if (!token || !token->valid || token->token_serial == 0U || token->document_id != current_id_ ||
      token->session_serial != session_serial_ || token->mount_epoch != catalog_.boundSession().mount_epoch ||
      token->editor_revision != editor_.revision()) return {Status::StaleToken, "token"};
  if (editor_.compositionActive()) return {Status::CompositionActive, "composition"};
  EditorView current{}; const Status captured = editor_.capture(&current); if (captured != Status::Ok) return {captured, "capture"};
  const Status protected_status = persistence_.saveView(current_id_, SaveLane::Protection, current);
  if (protected_status != Status::Ok) return {protected_status, "protect"};
  const std::uint64_t body_generation = token->target.generation;
  const std::uint32_t body_crc32 = token->target.text_crc32;
  const Status installed = editor_.install(&token->target); if (installed != Status::Ok) return {Status::InstallFailed, "editor_install", true, false};
  ++session_serial_;
  const bool body_lane_identity =
      token->target_source == RestoreSource::Auto ||
      token->target_source == RestoreSource::Manual;
  persistence_.bindCurrent(
      current_id_, session_serial_, body_lane_identity ? body_generation : 0U,
      body_lane_identity ? body_crc32 : 0U, editor_.bodyRevision(),
      token->latest_body_generation,
      body_lane_identity ? token->target_source : RestoreSource::None);
  if (!body_lane_identity) {
    persistence_.onBodyMutation(now_ms, editor_.bodyRevision());
  }
  return {Status::Ok, "complete", true, false};
}

void DocumentSession::onBodyMutation(const std::uint32_t now_ms) noexcept {
  persistence_.onBodyMutation(now_ms, editor_.bodyRevision());
}

SessionState DocumentSession::state() const noexcept {
  SessionState value{};
  value.has_current = hasCurrent();
  value.current_id = current_id_;
  value.body_dirty = persistence_.bodyDirty();
  value.position_recorded = !persistence_.positionDirty();
  value.medium_available = catalog_.validateSession() == Status::Ok;
  value.busy = busy_;
  value.background_save_active = persistence_.backgroundSaveActive();
  value.session_serial = session_serial_;
  return value;
}

Status DocumentSession::acquireExternalCreate() noexcept {
  if(busy_||persistence_.backgroundSaveActive())return Status::Busy;
  if(editor_.compositionActive())return Status::CompositionActive;
  if(editor_.inputQueuePending())return Status::Busy;
  external_create_=true;busy_=true;return Status::Ok;
}
void DocumentSession::releaseExternalCreate() noexcept {
  if(external_create_){external_create_=false;busy_=false;}
}

}  // namespace hanime::documents
