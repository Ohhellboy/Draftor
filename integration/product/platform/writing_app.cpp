#include "stability_diagnostics.hpp"
#include "behavior_bench.hpp"
#include "writing_app.hpp"
#include "body_punctuation.hpp"
#include "behavior_observer.hpp"
#include "product_config.hpp"
#include "operation_profile.hpp"
#include "hid_product_key_adapter.hpp"
#include <algorithm>
#include <cstdio>
namespace s3app {
namespace be=editor::body_edit; namespace vn=editor::visual_nav;
App::App(ds::StoragePort& storage,std::array<uint8_t,16> uid,uint64_t entropy,const s3host::FontPort& font,PresentationConstruction construction)
 : document_(draftor::product::document()),editor_(document_,draftor::product::editor()),horizontal_(document_,font),vertical_({document_,editor_},font),presentation_({document_,editor_},horizontal_,vertical_,construction.construct?&construction.construct(construction.context,{document_,editor_}):nullptr),document(document_),editor(editor_),layout(horizontal_.layout),navigator(horizontal_.navigator),ids(uid,entropy),catalog(storage,ids),persistence(storage,catalog,{},draftor::product::save()),editorPort(document_,editor_,presentation_,{nullptr,nullptr,&App::afterInstall,this}),session(catalog,persistence,editorPort),manager(catalog,session),findDocument(document_,editor_,presentation_,{},0,0),findController(find,findDocument){}
editor::body_edit::Outcome App::moveToByte(size_t byte,bool extend){
 if(transfer_active_||!ready_||!session.hasCurrent()||filesOpen()||findOpen()||statsOpen_||commandBarrier())return {be::ResultCode::InputStateDisallowed};
 const auto outcome=editor_.moveToByte(byte,extend);
 if(outcome.code==be::ResultCode::Applied||outcome.code==be::ResultCode::NoAction)presentation().followCaret(editor_.caretAffinity());
 mutation(outcome,now_);return outcome;
}

bool App::enterTransfer()noexcept{
 if(transfer_active_||!ready_||commandBarrier()||manualSavePending()||renaming_||findOpen()||statsOpen_||manager.state().pending!=ds::PendingAction::None)return false;
 transfer_active_=true;++focusEpoch_;dirtyDisplay=true;return true;
}
void App::leaveTransfer()noexcept{if(!transfer_active_)return;transfer_active_=false;++focusEpoch_;dirtyDisplay=true;}
void App::transferSaveResult(ds::OperationResult r){result(r);}
ds::OperationResult App::openImported(const ds::DocumentId& id,uint32_t now){
 if(!transfer_active_||commandBarrier())return {ds::Status::Busy,"usb_open_admission"};
 // Prepare the displayed name before installation. A later persistence OOM
 // must not leave the newly installed body carrying the original draft's name.
 ds::DocumentMetadata target;const auto got=catalog.get(id,&target);
 if(got!=ds::Status::Ok&&got!=ds::Status::OkDegraded){ds::OperationResult r{got,"usb_open_metadata"};result(r);return r;}
 ds::OperationResult r;
 try{r=session.open(id,now);}
 catch(const std::bad_alloc&){if(session.currentId()!=id)throw;r={ds::Status::Partial,"usb_open_memory"};}
 result(r);
 if(session.currentId()==id){manager.cancel();currentName.swap(target.name);syncReaders();}
 return r;
}
void App::afterInstall(void* p)noexcept{BT_OBSERVE(boundary());auto self=static_cast<App*>(p);if(self->fileTiming.start_us&&!self->fileTiming.complete_us)self->fileTiming.install_us=s3profile::clockUs();++self->focusEpoch_;self->closeReaders();self->find=hanime::find::FindSession{};self->stats.reset();self->editor_.resetForDocument();self->navigator.invalidatePreferredX();self->dirtyDisplay=true;}
void App::cacheCurrentName(){if(!session.hasCurrent())return;ds::DocumentMetadata meta;auto status=catalog.get(session.currentId(),&meta);if(status==ds::Status::Ok||status==ds::Status::OkDegraded)currentName=std::move(meta.name);}
void App::result(ds::OperationResult r){if(r.status==ds::Status::IoError||r.status==ds::Status::Corrupt||r.status==ds::Status::MediaUnavailable||r.status==ds::Status::SessionChanged||r.status==ds::Status::Partial)catalog.invalidateCache();lastOperation=r;++operationSerial;notice=ds::statusMessage(r.status);dirtyDisplay=true;}
ds::OperationResult App::startup(uint32_t now){if(!s3profile::active)s3profile::begin("startup");now_=now;ready_=editor_.ready();if(!ready_)return {ds::Status::InstallFailed,"editor_memory"};auto r=session.startupRestore();if(r.status==ds::Status::NoCurrentDocument){auto status=manager.enter(ds::FileManagerView::Normal);
 if(status==ds::Status::Ok && manager.state().page.items.empty()){
  ds::ListPage trash;auto checked=catalog.list(ds::DocumentArea::Trash,0,1,&trash);
  if(checked==ds::Status::Ok&&trash.items.empty()){r=session.create({},now);manager.cancel();}
  else r={checked,"no_current_trash"};
 }
 else r={status,"no_current_list"};}
 ready_=bool(r);result(r);if(ready_){cacheCurrentName();syncReaders();}return r;}
void App::mutation(const be::Outcome& r,uint32_t now,bool preserve_preferred){if(!horizontalActive()&&(r.code==be::ResultCode::Applied||r.code==be::ResultCode::NoAction))presentation_.followCaret(editor_.caretAffinity(),!preserve_preferred);if(r.textChanged()){session.onBodyMutation(now);syncReaders();notice="Unsaved changes";}if(r.cursorChanged()||r.selectionChanged())session.onPositionChanged(now);if(r.changes!=be::Change::None)dirtyDisplay=true;
 if(r.code!=be::ResultCode::Applied&&r.code!=be::ResultCode::NoAction){notice=be::resultMessage(r.code);dirtyDisplay=true;}}
bool App::commitText(std::string_view text,EditStamp stamp){if(transfer_active_||!ready_||!session.hasCurrent()||filesOpen()||findOpen()||statsOpen_||stamp.focus!=focusToken()||stamp.revision!=document.revision()||stamp.selection!=editor_.selectionVersion()||commandBarrier())return false;
#if DRAFTOR_TELEMETRY_PROFILE
 const auto observed_revision=document.textRevision();const auto observed_cursor=document.cursorByte();
#endif
 auto r=editor_.insertUtf8(text);BT_OBSERVE(commit(*this,observed_revision,observed_cursor));mutation(r,now_);return r.code==be::ResultCode::Applied||r.code==be::ResultCode::NoAction;}
void App::cancelFileRequest()noexcept{
 if(queuedFile_!=Command::None||(pending_!=Command::None&&pending_==fileTiming.command&&!fileTiming.complete_us)){if(pending_==fileTiming.command)pending_=Command::None;queuedFile_=Command::None;fileTiming.cancelled=true;fileTiming.complete_us=s3profile::clockUs();++focusEpoch_;dirtyDisplay=true;}
}
void App::fileFramePresented(uint64_t us,bool busy)noexcept{
 if(queuedFile_!=Command::None&&busy&&!fileTiming.feedback_us)fileTiming.feedback_us=us;
 if(!busy&&fileTiming.complete_us&&!fileTiming.cancelled&&!fileTiming.body_lcd_us&&
    fileTiming.command==Command::OpenSelected&&!filesOpen())fileTiming.body_lcd_us=us;
}
bool App::manualSaveIdentityValid()const noexcept{
 return session.hasCurrent()&&manualSave_.id==session.currentId()&&
     manualSave_.session==session.sessionSerial()&&
     manualSave_.mount==catalog.boundSession().mount_epoch&&
     manualSave_.revision_epoch==document.textRevisionEpoch()&&
     catalog.validateSession()==ds::Status::Ok;
}
void App::failRequestedSave(ds::OperationResult r){
 if(manualSave_.pending)stability::event(stability::Kind::Save,(5u<<24)|unsigned(r.status),uint32_t(manualSave_.session),manualSave_.requested_revision);
 const bool cancelledCommand=pending_!=Command::None;
 manualSave_={};cancelFileRequest();pending_=Command::None;
 if(cancelledCommand)++focusEpoch_;
 result(r); // No acknowledgement: the existing persistence owner retains dirty.
}
ds::OperationResult App::startRequestedSave(uint32_t now){
 if(!manualSaveIdentityValid()){
  ds::OperationResult r{ds::Status::SessionChanged,"manual_save_identity"};failRequestedSave(r);return r;
 }
 if(persistence.backgroundSaveActive())return {ds::Status::Busy,"manual_save_accepted"};
 // Same-task capture: this is the revision of the original immutable snapshot.
 // Waiting edits can be included; edits after capture still use the writer's
 // existing revision-sensitive acknowledgement and remain dirty.
 manualSave_.started_revision=document.textRevision();
 const auto r=persistence.startManualSave(now,editorPort);
 if(r.status==ds::Status::Busy&&persistence.backgroundSaveActive()){
  stability::event(stability::Kind::Save,3u<<24,uint32_t(manualSave_.session),manualSave_.started_revision);
  manualSave_.running=true;notice="Saving snapshot...";dirtyDisplay=true;
  return {ds::Status::Busy,"manual_save_accepted"};
 }
 failRequestedSave(r);return r;
}
ds::OperationResult App::requestSave(uint32_t now){
 if(!ready_||transfer_active_||commandBarrier()){
  ds::OperationResult r{ds::Status::Busy,"manual_save_not_accepted"};result(r);
  notice="Command pending; save not accepted";return r;
 }
 if(!session.hasCurrent()){
  ds::OperationResult r{ds::Status::NoCurrentDocument,"manual_save_no_document"};result(r);return r;
 }
 if(manualSave_.pending&&!manualSaveIdentityValid()){
  ds::OperationResult r{ds::Status::SessionChanged,"manual_save_identity"};failRequestedSave(r);return r;
 }
 if(!manualSave_.pending){
  const auto valid=catalog.validateSession();
  if(valid!=ds::Status::Ok){ds::OperationResult r{valid,"manual_save_media"};result(r);return r;}
  manualSave_.id=session.currentId();manualSave_.session=session.sessionSerial();
  manualSave_.mount=catalog.boundSession().mount_epoch;
  manualSave_.revision_epoch=document.textRevisionEpoch();manualSave_.pending=true;
 }
 // Acceptance boundary: one bounded token, coalesced to the newest requested
 // body revision. Never retain a second body snapshot or an input event.
 manualSave_.requested_revision=document.textRevision();
 stability::event(stability::Kind::Save,1u<<24,uint32_t(manualSave_.session),manualSave_.requested_revision);
 if(persistence.backgroundSaveActive()){
  if(!manualSave_.running||manualSave_.requested_revision>manualSave_.started_revision)stability::event(stability::Kind::Save,2u<<24,uint32_t(manualSave_.session),manualSave_.requested_revision);
  notice=manualSave_.running?"Saving snapshot...":"Waiting for save";dirtyDisplay=true;
  return {ds::Status::Busy,"manual_save_accepted"};
 }
 return startRequestedSave(now);
}
void App::issue(Command c,uint32_t now,bool resumed){
 if(c==Command::Save){(void)requestSave(now);return;}
 if(commandBarrier()&&!fileExecuting_)return;
 if(deferFiles_&&!fileExecuting_&&!resumed&&(c==Command::Files||c==Command::OpenSelected)){
  if(c==Command::OpenSelected){const auto& state=manager.state();if(state.selected>=state.page.items.size())return;queuedFileId_=state.page.items[state.selected].id;}
  closeReaders();++focusEpoch_;queuedFocus_=focusEpoch_;queuedSession_=session.sessionSerial();queuedRevision_=document.revision();
  queuedFile_=c;const auto next=fileTiming.serial+1;fileTiming={};fileTiming.serial=next;fileTiming.key_ms=now;fileTiming.command=c;fileTiming.request_us=s3profile::clockUs();
  if(!s3profile::active)s3profile::begin(c==Command::Files?"files":"open_selected");
  progress(c==Command::Files?"Reading document list...":"Opening / restoring...");
  return; // No catalog/file access before a successful busy LCD return.
 }
 if(c==Command::Confirm&&manager.state().pending==ds::PendingAction::TrashSelected&&
    (manager.state().trash_session!=session.sessionSerial()||catalog.validateSession()!=ds::Status::Ok)){
  manager.cancelConfirmation();result({ds::Status::StaleToken,"trash_confirm"});++focusEpoch_;return;
 }
 if(c!=Command::Save){
  if(!s3profile::active){
   const char* name="restore";
   switch(c){
    case Command::Files:name="files";break;
    case Command::OpenSelected:name="open_selected";break;
    case Command::New:name="new";break;
    case Command::NextPage:case Command::PreviousPage:name="page";break;
    case Command::ToggleTrash:name="view";break;
    case Command::Trash:name="trash_prepare";break;
    case Command::Confirm:name="confirm";break;
    default:break;
   }
   s3profile::begin(name);
  }
  closeReaders();++focusEpoch_;
  const char* working=c==Command::New?"Creating document...":c==Command::OpenSelected?"Opening / restoring...":c==Command::Confirm?"Applying confirmed action...":"Reading document list...";
  if(!fileExecuting_)progress(working);
 }
 // Keep AUTO versus MANUAL recovery semantics. Only the large outgoing save
 // is deferred; the existing document session still owns the actual switch.
 bool leaving=c==Command::New;
 if(c==Command::Confirm && manager.state().pending==ds::PendingAction::TrashSelected)
  leaving=manager.state().trash_id==session.currentId();
 if(c==Command::OpenSelected && manager.state().view==ds::FileManagerView::Normal){
  const auto& state=manager.state();
  leaving=state.selected<state.page.items.size()&&
      state.page.items[state.selected].id!=session.currentId();
 }
 if(leaving&&persistence.bodyDirty()&&!persistence.backgroundSaveActive()){
  auto saved=persistence.startAutoSave(now,editorPort);
  if(saved.status!=ds::Status::Busy){result(saved);return;}
 }
 if(persistence.backgroundSaveActive()||manualSavePending()){
  pending_=c;notice="Waiting for save";dirtyDisplay=true;return;
 }
 execute(c,now);
}
void App::execute(Command c,uint32_t now){
 if(fileTiming.request_us&&!fileTiming.complete_us&&fileTiming.command==c&&!fileTiming.start_us)fileTiming.start_us=s3profile::clockUs();
 // A deferred command has a second boundary when its save wait completes.
 if(c!=Command::Save)++focusEpoch_;

 switch(c){
 case Command::Save:(void)requestSave(now);break;
 case Command::Files: result({manager.enter(ds::FileManagerView::Normal),"files"});break;
 case Command::New:{createTiming={};auto r=session.startCreate({},now);
  if(session.creating()){notice="Creating document...";dirtyDisplay=true;}
  else {result(r);if(r){manager.cancel();cacheCurrentName();}}break;}
 case Command::OpenSelected:{
  // The Pico shell already captures the selected name before opening. Keep it
  // across menu close instead of opening META again just to draw the title.
  const auto& state=manager.state();
  ds::DocumentId selectedId{};
  draftor::memory::String selectedName;
  if(state.selected<state.page.items.size()){selectedName=state.page.items[state.selected].name;selectedId=state.page.items[state.selected].id;}
  const bool trash=state.view==ds::FileManagerView::Trash;
  auto r=manager.activateSelected(now, trash);result(r);
  if(!trash && !selectedId.empty() && session.currentId()==selectedId){currentName=std::move(selectedName);manager.cancel();}
  if(trash&&(r||r.status==ds::Status::Partial)){
   ds::DocumentMetadata actual;const auto got=catalog.get(selectedId,&actual);
   if((got==ds::Status::Ok||got==ds::Status::OkDegraded)&&actual.area==ds::DocumentArea::Normal){
    std::snprintf(restoredNotice.data(),restoredNotice.size(),"%s%s",r.status==ds::Status::Partial?"Restored (warning): ":"Restored: ",actual.name.c_str());restoredPrefix_=static_cast<uint8_t>(std::strlen(r.status==ds::Status::Partial?"Restored (warning): ":"Restored: "));notice=restoredNotice.data();
   }else notice="Restore state unavailable; reopen list";
  }
  break;
 }
 case Command::ToggleTrash:result({manager.enter(manager.state().view==ds::FileManagerView::Trash?ds::FileManagerView::Normal:ds::FileManagerView::Trash),"view"});break;
 case Command::Trash:result({manager.requestTrashSelected(),"trash_confirm"});break;
 case Command::PreviousPage:result({manager.previousPage(),"page"});break;
 case Command::NextPage:result({manager.nextPage(),"page"});break;
 case Command::RestoreManual:case Command::RestoreLatest:case Command::RestoreProtection:{auto r=manager.requestBodyRestore(c==Command::RestoreManual?ds::RestoreCommand::Manual:c==Command::RestoreLatest?ds::RestoreCommand::Latest:ds::RestoreCommand::BeforeRollback);result({r,"restore_prepare"});break;}
 case Command::Confirm:{
  bool trash=manager.state().pending==ds::PendingAction::TrashSelected;
  bool had=session.hasCurrent();result(manager.confirm(now));
  if(trash){
   if(had&&!session.hasCurrent()){
    afterInstall(this);editor_.discardPreparedTextInput();document_.replaceAll({},0,false);currentName.clear();
   }
   if(lastOperation)notice="Moved to trash; can be restored";
   else if(lastOperation.status==ds::Status::Partial)notice="Trashed; storage warning";
  }else if(lastOperation)manager.cancel();
  break;
 }
 default:break;
 }
 if(fileTiming.start_us&&!fileTiming.complete_us&&fileTiming.command==c)fileTiming.complete_us=s3profile::clockUs();
 dirtyDisplay=true;
}
void App::service(uint32_t now,bool input_pending){now_=now;if(!ready_)return;
 if(manualSavePending()&&!manualSaveIdentityValid()){
  failRequestedSave({ds::Status::SessionChanged,"manual_save_identity"});return;
 }
 if(presentation_.navigationPending())(void)presentation_.prepare(true); // Reconcile geometry before validating a queued target.
 if(presentation_.navigationPending()){
  if(horizontalActive()||filesOpen()||findOpen()||statsOpen_||renaming_||commandBarrier()||session.creating())presentation_.cancelNavigation();
  else{
   editor::presentation::ByteTarget target;bool extend=false;
   const auto status=presentation_.serviceNavigation({presentation_.frame().revision,focusToken()},
       VerticalPresentation::navigation_work_budget,target,extend);
   if(status==editor::presentation::SpatialResult::Ready&&target.valid)
    mutation(editor_.moveToByte(target.byte,extend,target.affinity),now,true);
  }
 }
 if(queuedFile_!=Command::None){
  if(!fileTiming.feedback_us)return; // Failed/unpresented busy frame must retry drawing.
  const auto& state=manager.state();
  if(queuedSession_!=session.sessionSerial()||queuedFocus_!=focusEpoch_||queuedRevision_!=document.revision()||
     (queuedFile_==Command::OpenSelected&&(state.selected>=state.page.items.size()||state.page.items[state.selected].id!=queuedFileId_))){
   cancelFileRequest();notice="Open target changed; cancelled";return;
  }
  // The fixed DocumentSession performs media validation and atomic installation.
  // Its synchronous load is one measured, non-preemptible step until it exposes
  // a bounded prepare/advance/install interface. No shadow loader in the adapter.
  const auto c=queuedFile_;fileExecuting_=true;issue(c,now);fileExecuting_=false;if(pending_==Command::None&&!fileTiming.complete_us)cancelFileRequest();queuedFile_=Command::None;
  return;
 }
 if(session.creating()){
  const auto start=s3profile::clockUs();const auto stage=session.createStage();
  static const char* names[]={"idle","reserve_plan","collection","intent","initial_auto","publish","clear_intent","install","current","finish"};
  unsigned index=0;while(index<9&&std::string_view(stage)!=names[index])++index;
  const auto r=session.advanceCreate();const auto elapsed=s3profile::clockUs()-start;
  createTiming.us[index]+=elapsed;createTiming.max_us[index]=std::max(createTiming.max_us[index],elapsed);++createTiming.steps[index];
  if(!session.creating()){
   ++focusEpoch_;result(r);
   if(r||r.status==ds::Status::Partial){manager.cancel();cacheCurrentName();}
  }
  return;
 }
 syncReaders();
 bool was=persistence.backgroundSaveActive();
 auto r=session.tick(now,{true,false,input_pending&&pending_==Command::None&&!manualSavePending()});
 if(was&&!persistence.backgroundSaveActive()){
  if(!r&&manualSavePending())failRequestedSave(r);else result(r);
  if(!r){
   if(pending_!=Command::None){cancelFileRequest();pending_=Command::None;++focusEpoch_;notice="Save failed; command cancelled";}
  }else if(manualSave_.running){
   stability::event(stability::Kind::Save,(4u<<24)|unsigned(r.status),uint32_t(manualSave_.session),manualSave_.started_revision);
   manualSave_.running=false;
   // Further Save presses during the writer require at most one successor.
   // Later edits without another request are not falsely acknowledged.
   if(manualSave_.requested_revision<=manualSave_.started_revision)manualSave_={};
  }
  if(r.status==ds::Status::Ok)notice=document.dirty()?"Snapshot saved; newer edits":"Saved";
 }
 if(manualSavePending()&&!persistence.backgroundSaveActive())(void)startRequestedSave(now);
 if(pending_!=Command::None&&!persistence.backgroundSaveActive()&&!manualSavePending()){
  auto c=pending_;pending_=Command::None;issue(c,now,true);
 }
 syncReaders();
 if(!input_pending&&!filesOpen()&&!commandBarrier())serviceReaders();
}
void App::allocationFailed(){if(manualSave_.pending)stability::event(stability::Kind::Save,(5u<<24)|unsigned(ds::Status::InstallFailed),uint32_t(manualSave_.session),manualSave_.requested_revision);cancelFileRequest();fileExecuting_=false;session.abortCreate();catalog.invalidateCache();cancelRename();manager.cancelConfirmation();closeReaders();++focusEpoch_;persistence.abortForMemoryFailure();pending_=Command::None;manualSave_={};editor_.discardPreparedTextInput();result({ds::Status::InstallFailed,"memory_exhausted"});notice="Memory low; operation failed";}
void App::disconnected()noexcept{layout_key_held_=false;cancelFileRequest();cancelRename();manager.cancelConfirmation();closeReaders();if(stats.report().state==hanime::text_stats::State::Counting)++scans.stats_cancelled;stats.reset();++focusEpoch_;pending_=Command::None;editor_.discardPreparedTextInput();dirtyDisplay=true;}
bool App::layoutKey(const s3hid::Event& e)noexcept{
 if(e.usage!=61)return false; // USB HID F4, owned by the product command layer.
 if(layout_key_epoch_!=e.epoch){layout_key_epoch_=e.epoch;layout_key_held_=false;}
 if(!e.down){layout_key_held_=false;return true;}
 if(layout_key_held_)return true;
 layout_key_held_=true;
 if(ready_&&!transfer_active_&&!e.mods){closeMenuHelp();presentation_.cancelNavigation();presentation_.toggle();dirtyDisplay=true;BT_OBSERVE(mode(*this,e));BT_MEASURE(f4());}
 return true;
}
void App::key(const s3hid::Event& e,bool chinese_punctuation){
 if(transfer_active_)return;
 if(layoutKey(e))return;
#if DRAFTOR_TELEMETRY_PROFILE
 ime_device::behavior::EditorScope observation(*this,e);
#endif
 if(!ready_||!e.down||e.usage>=224||session.creating())return;
 now_=e.ms;syncReaders();session.onUserActivity(e.ms);
 if(queuedFile_!=Command::None){if(e.usage==41){cancelFileRequest();notice="Command cancelled";}return;}
 if(renaming_){renameKey(e);return;}
 const auto u=e.usage;bool shift=e.mods&0x22,ctrl=e.mods&0x11,gui=e.mods&0x88,alt=e.mods&0x44;
 bool primary=(ctrl!=gui)&&!alt;
 if(u==41&&pending_!=Command::None){cancelFileRequest();pending_=Command::None;manager.cancelConfirmation();++focusEpoch_;notice="Command cancelled";dirtyDisplay=true;return;}
 if(pending_!=Command::None)return;
 if(manager.state().pending!=ds::PendingAction::None){
  if(u==41){manager.cancelConfirmation();++focusEpoch_;notice="Cancelled";dirtyDisplay=true;}
  else if(u==40) { issue(Command::Confirm,e.ms); }
  return;
 }
 if(menuHelpKey(e))return;
 if((primary&&!shift&&u==22)||u==63){issue(Command::Save,e.ms);return;} // Ctrl/Cmd+S, F6
 if((primary&&!shift&&u==18)||u==59){if(filesOpen()&&u==59&&manager.state().view!=ds::FileManagerView::TrashedReturn){manager.cancel();++focusEpoch_;dirtyDisplay=true;}else issue(Command::Files,e.ms);return;}
 if(u==64){issue(Command::RestoreManual,e.ms);return;}
 if(u==65){issue(Command::RestoreProtection,e.ms);return;}
 if(filesOpen()){
  if(u==41){result({manager.escape(),"menu_return"});++focusEpoch_;dirtyDisplay=true;}
  else if(manager.state().view==ds::FileManagerView::TrashedReturn){return;}
  else if(u==82||u==81){manager.moveSelection(u==82?-1:1);dirtyDisplay=true;}
  else if(u==80)issue(Command::PreviousPage,e.ms);
  else if(u==79)issue(Command::NextPage,e.ms);
  else if(u==40)issue(Command::OpenSelected,e.ms);
  else if(u==23&&!ctrl&&!gui&&!alt)issue(Command::ToggleTrash,e.ms);
  else if(u==21&&!ctrl&&!gui&&!alt&&manager.state().view==ds::FileManagerView::Normal)beginRename();
  else if((u==7||u==76)&&!ctrl&&!gui&&!alt&&manager.state().view==ds::FileManagerView::Normal)issue(Command::Trash,e.ms);
  else if(u==17&&!ctrl&&!gui&&!alt&&manager.state().view==ds::FileManagerView::Normal)issue(Command::New,e.ms); // original files N
  else if(u==15&&!ctrl&&!gui&&!alt)issue(Command::RestoreLatest,e.ms); // files L
  return;
 }
 if(primary&&!shift&&u==17&&!findOpen()&&!statsOpen_){issue(Command::New,e.ms);return;}
 if(!session.hasCurrent()){notice="No active document - F2: files / N: new";if(u==17&&!ctrl&&!gui&&!alt)issue(Command::New,e.ms);dirtyDisplay=true;return;}
 if(u==60&&!shift&&!ctrl&&!gui&&!alt){
  if(!statsOpen_)closeReaders();
  statsOpen_=!statsOpen_;++focusEpoch_;dirtyDisplay=true;return;
 }
 if(statsOpen_){
  if(u==41){statsOpen_=false;++focusEpoch_;dirtyDisplay=true;}
  return;
 }
 if(findKey(e))return;
 // CN punctuation owns only its small raw-usage table, after all consumers
 // above. Unlisted symbols and the complete EN path retain UK decoding.
 if(chinese_punctuation){
  const auto text=cnBodyPunctuation(u,e.mods,document_.text(),document_.cursorByte());
  if(!text.empty()){presentation_.cancelNavigation();mutation(editor_.insertUtf8(text),e.ms);return;}
 }
 uint32_t cp=s3hid::uk(u,e.mods);
 // UK printable non-ASCII is an input-platform extension, not an IME commit.
 if(cp>127&&!ctrl&&!gui&&(!alt||((e.mods&0xdd)==0x40&&u==8))){char bytes[3];size_t n;
  if(cp<2048){bytes[0]=0xc0|(cp>>6);bytes[1]=0x80|(cp&63);n=2;}
  else{bytes[0]=0xe0|(cp>>12);bytes[1]=0x80|((cp>>6)&63);bytes[2]=0x80|(cp&63);n=3;}
  mutation(editor_.insertUtf8({bytes,n}),e.ms);return;
 }
 namespace ih=integration::input_host;namespace in=editor::input;
 in::NormalizedKey key;
 if(!ih::normalizeProductHidKey(u,cp<128?static_cast<char>(cp):0,ih::semanticModifiersForPhysicalUsages(e.mods),&key))return;
 auto action=in::InputRouter{}.route(in::InteractionState::DirectBody,key);
 using A=in::ActionKind;
 // A construction-selected geometry can propose a byte target, never edit it.
 namespace ep=editor::presentation;
 ep::SpatialDirection direction{};bool spatial=true;
 switch(action.kind){
 case A::BodyMoveLeft:case A::BodyExtendLeft:direction=ep::SpatialDirection::Left;break;
 case A::BodyMoveRight:case A::BodyExtendRight:direction=ep::SpatialDirection::Right;break;
 case A::BodyMoveVisualUp:case A::BodyExtendVisualUp:direction=ep::SpatialDirection::Up;break;
 case A::BodyMoveVisualDown:case A::BodyExtendVisualDown:direction=ep::SpatialDirection::Down;break;
 default:spatial=false;break;
 }
 ep::ByteTarget target;
 if(spatial){
  const auto status=presentation_.spatialTarget(direction,shift,target,{presentation_.frame().revision,focusToken()});
  if(status!=ep::SpatialResult::Unhandled){
   if(status==ep::SpatialResult::Ready&&target.valid)mutation(editor_.moveToByte(target.byte,shift,target.affinity),e.ms,true);
   return;
  }
 }
 // A non-spatial body action supersedes an unfinished geometric request.
 presentation_.cancelNavigation();
 // Only the donor's editor actions are executed. No old IME executor/core.
 switch(action.kind){
 case A::BodyInsertAscii:mutation(editor_.insertUtf8(std::string_view(&action.ascii,1)),e.ms);break;
 case A::ContextBackspace:mutation(editor_.eraseBackward(),e.ms);break;
 case A::BodyDeleteForward:mutation(editor_.eraseForward(),e.ms);break;
 case A::BodyInsertLineBreak:mutation(editor_.insertLineBreak(),e.ms);break;
 case A::BodyInsertTab:mutation(editor_.insertTab(),e.ms);break;
 case A::BodyCancelSelection:mutation(editor_.cancelSelection(),e.ms);break;
 case A::BodySelectAll:mutation(editor_.selectAll(),e.ms);break;
 case A::BodyCopy:mutation(editor_.copy(),e.ms);break;
 case A::BodyCut:mutation(editor_.cut(),e.ms);break;
 case A::BodyPaste:mutation(editor_.paste(),e.ms);break;
 case A::BodyUndo:case A::BodyRedo:mutation(action.kind==A::BodyUndo?editor_.undo():editor_.redo(),e.ms);if(horizontalActive())navigator.restoreCaretAffinity(editor_.caretAffinity());break;
 case A::BodyMoveLeft:case A::BodyExtendLeft:case A::BodyMoveRight:case A::BodyExtendRight:
  mutation(editor_.moveHorizontal(navigator,(action.kind==A::BodyMoveLeft||action.kind==A::BodyExtendLeft)?vn::VisualHorizontalDirection::Left:vn::VisualHorizontalDirection::Right,shift),e.ms);break;
 case A::BodyMoveVisualUp:case A::BodyExtendVisualUp:case A::BodyMoveVisualDown:case A::BodyExtendVisualDown:
  mutation(editor_.moveVertical(navigator,(action.kind==A::BodyMoveVisualUp||action.kind==A::BodyExtendVisualUp)?vn::VisualDirection::Up:vn::VisualDirection::Down,shift),e.ms);break;
 case A::BodyMoveLogicalLineStart:case A::BodyExtendLogicalLineStart:case A::BodyMoveLogicalLineEnd:case A::BodyExtendLogicalLineEnd:
  if(horizontalActive())
   mutation(editor_.moveLogicalLine(navigator,(action.kind==A::BodyMoveLogicalLineStart||action.kind==A::BodyExtendLogicalLineStart)?editor::CursorMove::LineStart:editor::CursorMove::LineEnd,shift),e.ms);
  else
   mutation(editor_.moveToByte(document_.cursorTarget((action.kind==A::BodyMoveLogicalLineStart||action.kind==A::BodyExtendLogicalLineStart)?editor::CursorMove::LineStart:editor::CursorMove::LineEnd),shift),e.ms);
  break;
 case A::InputModeToggle:notice="English only - IME reserved";dirtyDisplay=true;break;
 default:break;
 }
}
bool App::menuHelpOpen()const noexcept {
 const auto& s=manager.state();
 return menuHelp_&&menuHelpFocus_==focusEpoch_&&!transfer_active_&&!renaming_&&!commandBarrier()&&
  s.pending==ds::PendingAction::None&&s.view==menuHelpView_&&s.page.offset==menuHelpOffset_&&
  s.selected==menuHelpSelected_&&(s.page.items.empty()?menuHelpId_==ds::DocumentId{}:
   s.selected<s.page.items.size()&&s.page.items[s.selected].id==menuHelpId_);
}
void App::closeMenuHelp()noexcept {
 if(menuHelpOpen()){menuHelp_=false;++focusEpoch_;dirtyDisplay=true;}
 else menuHelp_=false;
}
bool App::menuHelpKey(const s3hid::Event& e)noexcept {
 const auto& s=manager.state();
 const bool stable=(s.view==ds::FileManagerView::Normal||s.view==ds::FileManagerView::Trash)&&
  !transfer_active_&&!commandBarrier()&&!renaming_&&!statsOpen_&&!findOpen()&&
  s.pending==ds::PendingAction::None;
 if(!stable)return false;
 const bool primary=bool(e.mods&0x11)!=bool(e.mods&0x88);
 const bool open=(e.usage==58&&!e.mods)||(e.usage==56&&primary&&!(e.mods&0x66));
 if(menuHelpOpen()){
  if(open)return true;
  if(e.usage==41){closeMenuHelp();return true;}
  if((e.usage==79||e.usage==80)&&!e.mods){
   const auto page=e.usage==79?1:0;
   if(menuHelpPage_!=page){menuHelpPage_=uint8_t(page);dirtyDisplay=true;}
   return true;
  }
  // Pass the original event exactly once to its existing App command owner.
  closeMenuHelp();return false;
 }
 if(!open)return false;
 menuHelp_=true;menuHelpPage_=0;menuHelpView_=s.view;menuHelpOffset_=s.page.offset;
 menuHelpSelected_=s.selected;menuHelpId_=s.selected<s.page.items.size()?s.page.items[s.selected].id:ds::DocumentId{};
 menuHelpFocus_=++focusEpoch_;dirtyDisplay=true;return true;
}
void App::progress(const char* text){
 notice=text;dirtyDisplay=true;
 if(progressHook_){progressDrawing_=true;progressHook_(progressContext_);progressDrawing_=false;}
}
void App::cancelRename() noexcept {nameInput_.clear();renameId_={};renaming_=false;++focusEpoch_;}
void App::beginRename(){
 const auto& state=manager.state();if(state.selected>=state.page.items.size())return;
 closeReaders();++focusEpoch_;const auto& meta=state.page.items[state.selected];
 renameId_=meta.id;renameSession_=session.sessionSerial();nameInput_.assign(meta.name);renaming_=true;
 notice="Arrows edit; Enter save; Esc cancel";dirtyDisplay=true;
}
bool App::commitName(std::string_view text,NameStamp expected){
 if(!renaming_||expected.focus!=focusToken()||expected.revision!=renameRevision()||expected.cursor!=renameCursor()||expected.selected!=renameSelected()||renameSession_!=session.sessionSerial()||catalog.validateSession()!=ds::Status::Ok)return false;
 if(!nameInput_.insert(text)){notice="Name limit: 96 valid UTF-8 bytes";dirtyDisplay=true;return false;}
 dirtyDisplay=true;return true;
}
void App::renameKey(const s3hid::Event& e){
 if(e.usage==41){cancelRename();notice="Rename cancelled";dirtyDisplay=true;return;}
 if(renameSession_!=session.sessionSerial()||catalog.validateSession()!=ds::Status::Ok){cancelRename();result({ds::Status::StaleToken,"rename"});return;}
 if(e.usage==40){
  if(!s3profile::active)s3profile::begin("rename");
  progress("Checking name / saving metadata...");
  auto status=session.rename(renameId_,{nameInput_.text(),nameInput_.size()});++focusEpoch_;result({status,"rename"});
  if(status==ds::Status::Ok||status==ds::Status::Partial){if(renameId_==session.currentId())currentName.assign(nameInput_.text(),nameInput_.size());cancelRename();auto refreshed=manager.refresh();if(status==ds::Status::Partial)notice="Name visible; storage warning";else if(refreshed!=ds::Status::Ok)notice="Renamed; list read needs retry";}
  else if(status==ds::Status::InvalidName)notice="Invalid name: empty / control / slash";
  return;
 }
 const bool ctrl=(e.mods&0x11)!=0,gui=(e.mods&0x88)!=0;
 if(e.usage==4&&(ctrl!=gui)&&!(e.mods&0x66)){if(nameInput_.selectAll())dirtyDisplay=true;return;}
 if(e.usage==80||e.usage==79||e.usage==74||e.usage==77||e.usage==42||e.usage==76){
  const bool extend=(e.mods&0x22)!=0;
  bool changed=e.usage==80?nameInput_.left(extend):e.usage==79?nameInput_.right(extend):e.usage==74?nameInput_.home(extend):e.usage==77?nameInput_.end(extend):e.usage==42?nameInput_.backspace():nameInput_.deleteForward();
  if(changed)dirtyDisplay=true;
  return;
 }
 if(e.mods&0x99)return;
 auto cp=s3hid::uk(e.usage,e.mods);if(cp<32)return;
 char bytes[3];size_t n=0;
 if(cp<128)bytes[n++]=char(cp);
 else if(cp<2048){bytes[n++]=char(0xc0|(cp>>6));bytes[n++]=char(0x80|(cp&63));}
 else {bytes[n++]=char(0xe0|(cp>>12));bytes[n++]=char(0x80|((cp>>6)&63));bytes[n++]=char(0x80|(cp&63));}
 commitName({bytes,n},focusToken());
}

}
