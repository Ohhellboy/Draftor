#include "r2_fixture.hpp"
#include "model_support.hpp"

// HB adapter only. freeze/probes/epochs are future owner obligations, NOT
// existing product modal APIs. All calls run in this serialized Host owner.
struct R2Owner final : dx::OwnerPort {
  R2Fixture& f;
  bool frozen=false;
  std::uint64_t revision_epoch=1;
  bool revision_reliable=true,composition=false,semantic_pending=false,pending=false;
  std::size_t starts=0,services=0,borrows=0;
  hd::OperationResult native{};
  explicit R2Owner(R2Fixture& fixture):f(fixture){}
  dx::OwnerState probe() noexcept override {
    dx::OwnerState s;
    s.identity.collection=f.catalog.boundSession().collection_id.bytes;
    s.identity.document=f.session.currentId().bytes;
    s.identity.mount=f.storage.medium.mount_epoch;
    s.identity.document_session=f.session.sessionSerial();
    s.identity.revision_epoch=revision_epoch;
    s.identity.body_revision=f.document.textRevision();
    s.body_bytes=f.document.text().size();
    s.has_current=f.session.hasCurrent()&&ok(f.catalog.validateSession());
    s.composition=composition;s.semantic_pending=semantic_pending;s.pending_body_edits=pending;
    s.file_operation=f.session.busy()||f.session.creating();s.revision_reliable=revision_reliable;
    s.save_busy=f.persistence.backgroundSaveActive();
    s.body_dirty=f.document.dirty()||f.persistence.bodyDirty();s.position_dirty=f.persistence.positionDirty();
    return s;
  }
  bool freeze() noexcept override { CHECK(!frozen);frozen=true;return true; }
  void unfreeze() noexcept override { CHECK(frozen);frozen=false; }
  dx::SaveReply map(hd::OperationResult result) {
    native=result;
    const auto state=f.persistence.backgroundSaveActive()?dx::SaveState::Pending:
        result.status==hd::Status::Partial?dx::SaveState::Partial:
        ok(result.status)?dx::SaveState::Complete:dx::SaveState::Failed;
    // OperationResult has NO position_committed field. This bit represents
    // current-position acknowledgement observed from positionDirty(), never
    // the unrelated current_record_committed field.
    return {state,static_cast<int>(result.status),result.body_committed,
        state==dx::SaveState::Complete&&!f.persistence.positionDirty()};
  }
  dx::SaveReply beginAutoSave() noexcept override {
    ++starts;
    native={}; // A new AUTO attempt cannot inherit a prior attempt's commit bit.
    try {
      return map(f.document.dirty()||f.persistence.bodyDirty()
          ?f.persistence.startAutoSave(++f.clock,f.port)
          :f.persistence.flushCurrentPosition(f.port));
    } catch(const std::bad_alloc&) { return memoryFailure(); }
  }
  dx::SaveReply serviceSave() noexcept override {
    ++services;
    try { return map(f.persistence.tick(++f.clock,{true,false,false},f.port)); }
    catch(const std::bad_alloc&) { return memoryFailure(); }
  }
  dx::SaveReply memoryFailure() noexcept {
    // Test-owner exception translation, not a native OperationResult from R2.
    // Retain the last observed body commit; never force dirty or reinstall.
    const bool committed=native.body_committed;
    f.persistence.abortForMemoryFailure();
    return map({committed?hd::Status::Partial:hd::Status::ResourceExhausted,
                "HB_caught_bad_alloc",committed,false});
  }
  dx::BorrowReply borrow(std::size_t offset,std::size_t max,void* ctx,dx::BorrowConsumer use) noexcept override {
    CHECK(frozen);++borrows;hd::EditorView borrowed;
    if(f.port.capture(&borrowed)!=hd::Status::Ok||offset>=borrowed.text.size())return {};
    const auto n=std::min(max,borrowed.text.size()-offset);
    use(ctx,borrowed.text.substr(offset,n));return {true,n};
  }
};
static void finish(dx::ExportSession& s) {
  for(unsigned n=0;n<10000&&(s.phase()==dx::Phase::Saving||s.phase()==dx::Phase::Copying);++n)s.tick();
}
static void exportHv(bool vertical) {
  MemoryStorage medium;R2Fixture f(medium);
  f.setBody("MANUAL recovery");CHECK(ok(f.session.manualSave(++f.clock).status));
  const auto text=bom+semantic+"\r\nline\r tail  ";f.setBody(text);f.edit("+");
  const auto with_edit=f.text();CHECK(f.body.moveToByte(4,true).selectionChanged());
  CHECK(f.body.undoCount()==1&&f.body.selection().has_selection);
  if(vertical)f.presentation.toggle();
  f.presentation.prepare();f.presentation.followCaret(f.body.caretAffinity());
  CHECK(f.presentation.framePresented(f.presentation.frame()));
  EditorEvidence before(f);R2Owner owner(f);TrackingAllocator a;dx::ExportSession session(owner,a,{65536,7});
  CHECK(session.begin()==dx::SessionError::None);ready(session);CHECK(owner.starts==1&&owner.services>0);
  CHECK(!f.document.dirty()&&!f.persistence.bodyDirty()&&!f.persistence.positionDirty());
  const auto p=f.persistence.snapshotAllocation();CHECK(p&&p!=a.pointer&&f.persistence.snapshotCapacity()==65536);
  dx::Lease l;CHECK(session.acquire(l)==dx::ReaderStatus::Ok);CHECK(l.source_bytes==with_edit.size());
  CHECK(bytesEqual(leaseBytes(l),referenceExport(with_edit))&&leaseBytes(l).substr(1,3)==bom);
  before.unchanged(f);session.cancel();CHECK(session.phase()==dx::Phase::Closing&&owner.frozen);
  CHECK(bytesEqual(leaseBytes(l),referenceExport(with_edit)));
  session.release(l.token,dx::ReaderEnd::SimulatedSendComplete);CHECK(session.phase()==dx::Phase::Closed&&!a.live&&!owner.frozen);
  CHECK(f.persistence.snapshotAllocation()==p);before.unchanged(f);
  CHECK(f.body.undo().textChanged()&&bytesEqual(f.text(),text));
  CHECK(f.body.redo().textChanged()&&bytesEqual(f.text(),with_edit));
  std::printf("R2 %s real_port=1 synthetic_font=1 no_install=1 CURRENT_MANUAL_unchanged=1 history_undo_redo=1 AUTO_cleared_dirty=1 source=%zu output=%zu save_calls=%zu\n",
      vertical?"V":"H",l.source_bytes,l.size,owner.services+owner.starts);
}
static void oldSave() {
  MemoryStorage medium;R2Fixture f(medium);f.setBody("old");
  CHECK(f.persistence.startAutoSave(++f.clock,f.port).status==hd::Status::Busy);f.edit("new-");
  EditorEvidence before(f);R2Owner owner(f);TrackingAllocator a;dx::ExportSession s(owner,a,{65536,1});
  CHECK(s.begin()==dx::SessionError::None);
  while(f.persistence.backgroundSaveActive())CHECK(s.tick()==dx::Phase::Saving);
  CHECK(s.phase()==dx::Phase::Saving&&f.document.dirty()&&f.persistence.bodyDirty()&&!a.live);
  const auto old=f.persistence.loadLane(f.session.currentId(),hd::SaveLane::Auto);CHECK(ok(old.status)&&bytesEqual(old.snapshot.text,"old"));
  ready(s);CHECK(owner.starts==1&&!f.document.dirty());dx::Lease l;s.acquire(l);CHECK(leaseBytes(l)=="new-old");
  s.release(l.token);s.stop();before.unchanged(f);
  std::printf("HB old_save_then_required_new_AUTO=1 READY_after_new_revision_ack=1\n");
}
static void partial() {
  for(auto fault:{MemoryStorage::Fault::Write,MemoryStorage::Fault::Flush}){
    MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);EditorEvidence before(f);
    medium.fault=fault;medium.fault_path="/POSITION";
    R2Owner owner(f);TrackingAllocator a;dx::ExportSession s(owner,a,{65536});s.begin();finish(s);
    CHECK(s.phase()==dx::Phase::Closed&&s.error()==dx::SessionError::SavePartial&&!a.live&&!owner.frozen);
    CHECK(s.lastSave().body_committed&&!s.lastSave().position_committed);
    CHECK(!f.document.dirty()&&!f.persistence.bodyDirty()&&f.persistence.positionDirty()&&medium.fault_hits);
    medium.fault=MemoryStorage::Fault::None;
    const auto saved=f.persistence.loadLane(f.session.currentId(),hd::SaveLane::Auto);
    CHECK(ok(saved.status)&&bytesEqual(saved.snapshot.text,semantic));before.unchanged(f);
    std::printf("HB Partial fault=%s stage=%s body_committed=1 body_dirty=0 position_dirty=1 READY=0 AUTO_preserved=1\n",
        fault==MemoryStorage::Fault::Write?"write":"flush",owner.native.stage);
  }
}
static void failures() {
  for(auto fault:{MemoryStorage::Fault::Write,MemoryStorage::Fault::ShortWrite,MemoryStorage::Fault::Open}){
    MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);EditorEvidence before(f);
    medium.fault=fault;medium.fault_path="/AUTO";
    R2Owner owner(f);TrackingAllocator a;dx::ExportSession s(owner,a,{65536});s.begin();finish(s);
    CHECK(s.phase()==dx::Phase::Closed&&s.error()==dx::SessionError::SaveFailed&&!a.live);
    CHECK(!s.lastSave().body_committed&&f.document.dirty()&&f.persistence.bodyDirty()&&medium.fault_hits);
    CHECK(!f.persistence.backgroundSaveActive());before.unchanged(f);
  }
  {MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);R2Owner owner(f);TrackingAllocator a;
   dx::ExportSession s(owner,a,{65536});s.begin();CHECK(!f.persistence.snapshotAllocation());
   draftor::memory::allocation_fail_after=0;s.tick();draftor::memory::allocation_fail_after=-1;
   CHECK(s.error()==dx::SessionError::SaveFailed&&owner.native.status==hd::Status::ResourceExhausted);
   CHECK(f.document.dirty()&&f.persistence.bodyDirty()&&!a.live&&!f.persistence.backgroundSaveActive());}
  {MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);EditorEvidence before(f);
   R2Owner owner(f);TrackingAllocator a;a.fail_after=0;dx::ExportSession s(owner,a,{65536});s.begin();finish(s);
   CHECK(s.error()==dx::SessionError::AllocationFailed&&!f.document.dirty()&&!f.persistence.bodyDirty());
   before.unchanged(f);CHECK(!a.live);}
  {MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);R2Owner owner(f);TrackingAllocator a;
   dx::ExportSession s(owner,a,{65536});s.begin();s.tick();CHECK(f.persistence.backgroundSaveActive());
   for(unsigned n=0;n<100&&medium.handle.empty();++n)s.tick();
   CHECK(!medium.handle.empty()); // cancel while existing R2 owns an open file
   s.cancel();CHECK(s.phase()==dx::Phase::Closed&&!owner.frozen);
   CHECK(ok(f.drainSave().status)&&!f.document.dirty());}
  {MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);R2Owner owner(f);TrackingAllocator a;
   dx::ExportSession s(owner,a,{65536});s.begin();s.tick();
   ++medium.medium.mount_epoch;s.tick();
   CHECK(s.error()==dx::SessionError::IdentityChanged&&s.phase()==dx::Phase::Closed&&!a.live);
   const auto ended=f.drainSave();CHECK(ended.status==hd::Status::SessionChanged&&f.document.dirty());}
  for(bool after_body_commit:{false,true}){
    MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);EditorEvidence before(f);
    R2Owner owner(f);TrackingAllocator a;dx::ExportSession s(owner,a,{65536});s.begin();s.tick();
    if(after_body_commit){
      unsigned limit=0;
      while(std::string_view(owner.native.stage)!="body_ack"&&limit++<1000)s.tick();
      CHECK(std::string_view(owner.native.stage)=="body_ack"&&!f.document.dirty());
    }
    draftor::memory::allocation_fail_after=0;finish(s);draftor::memory::allocation_fail_after=-1;
    CHECK(std::string_view(owner.native.stage)=="HB_caught_bad_alloc");
    CHECK(s.error()==(after_body_commit?dx::SessionError::SavePartial:dx::SessionError::SaveFailed));
    CHECK(s.lastSave().body_committed==after_body_commit&&f.document.dirty()!=after_body_commit);
    CHECK(f.persistence.bodyDirty()!=after_body_commit&&f.persistence.positionDirty());
    CHECK(!f.persistence.backgroundSaveActive()&&medium.handle.empty()&&!a.live);before.unchanged(f);
    if(after_body_commit){auto saved=f.persistence.loadLane(f.session.currentId(),hd::SaveLane::Auto);
      CHECK(ok(saved.status)&&bytesEqual(saved.snapshot.text,semantic));}
  }
  std::printf("HB AUTO_io_short_open_faults=3 existing_P_allocation_failure=1 E_failure_after_successful_AUTO=1 cancel_owner_save_continues=1\n");
  std::printf("HB caught_R2_bad_alloc_before_and_after_body_ack=2 translated_Partial_preserves_AUTO=1\n");
}
static void storageControls() {
  MemoryStorage medium;const auto text=bom+bom+semantic+controlText()+u8"e\u0301 😀";
  {R2Fixture f(medium);f.setBody(text);R2Owner owner(f);TrackingAllocator a;dx::ExportSession s(owner,a,{65536,1});
   s.begin();ready(s);dx::Lease l;s.acquire(l);CHECK(bytesEqual(leaseBytes(l),referenceExport(text)));
   const auto record=f.persistence.loadLatest(f.session.currentId());CHECK(ok(record.status)&&bytesEqual(record.snapshot.text,text));
   s.release(l.token);s.stop();}
  {R2Fixture reopened(medium,false);CHECK(bytesEqual(reopened.text(),text));}
  std::printf("HB controls_export_exact_oracle=1 stored_original_CR_preserved=1 reopened_original=1\n");
}
static void bounds() {
  for(const auto& text:{std::string{},std::string(65532,'a')+u8"𨿍"}){
    MemoryStorage medium;R2Fixture f(medium);f.setBody(text);EditorEvidence before(f);
    R2Owner owner(f);TrackingAllocator a;dx::ExportSession s(owner,a,{65536,256});s.begin();ready(s);
    dx::Lease l;s.acquire(l);CHECK(bytesEqual(leaseBytes(l),text));CHECK(a.peak_bytes==text.size());
    CHECK(s.stats().borrowed_bytes==text.size()&&s.stats().max_borrow<=256);
    s.release(l.token);s.stop();before.unchanged(f);CHECK(!a.live&&medium.max_io<=512);
  }
  std::printf("HB empty_and_65536_with_last_supplementary=1 new_full_buffers_max=1\n");
}
int main(int argc,char** argv) {
  if(argc!=2)return 2;const auto live=draftor::memory::allocation_stats.live_allocations;
  const std::string_view name=argv[1];
  if(name=="horizontal")exportHv(false);else if(name=="vertical")exportHv(true);else if(name=="old_save")oldSave();
  else if(name=="partial")partial();else if(name=="failures")failures();else if(name=="storage_controls")storageControls();
  else if(name=="bounds")bounds();else return 2;
  CHECK(draftor::memory::allocation_stats.live_allocations==live);
  std::printf("PASS HB_export_%s checks=%zu r2_live_delta=0\n",argv[1],checks);
}
