#include "r2_fixture.hpp"

static void document() {
  MemoryStorage medium;R2Fixture f(medium);CHECK(f.document.maxBytes()==65536&&f.persistence.bodyCapacity()==65536);
  f.setBody(semantic);hd::EditorView borrowed;CHECK(f.port.capture(&borrowed)==hd::Status::Ok);
  CHECK(borrowed.text.data()==f.document.text().data()&&bytesEqual(borrowed.text,semantic));
  CHECK(!f.port.compositionActive()&&!f.port.inputQueuePending()); // null probes, not product permission
  const auto revision=f.port.revision(),body_revision=f.port.bodyRevision();
  CHECK(f.body.moveToByte(1).cursorChanged());
  CHECK(f.port.revision()!=revision&&f.port.bodyRevision()==body_revision);
  f.edit("z");CHECK(f.port.bodyRevision()!=body_revision);
  const auto before=f.text();CHECK(!f.document.replaceAll(std::string("\xed\xa0\x80",3),0));CHECK(f.text()==before);
  CHECK(!f.document.replaceAll(std::string(65537,'x'),0));CHECK(f.text()==before);
  CHECK(f.document.replaceAll(std::string(65532,'a')+u8"𨿍",0));CHECK(f.document.text().size()==65536);
  std::printf("R2 cap=65536 borrowed_pointer_equal=1 navigation_changes_body_revision=0 null_probes=1\n");
}
static void saving() {
  MemoryStorage medium;R2Fixture f(medium);f.setBody("old");
  const auto captured_revision=f.port.bodyRevision();
  CHECK(f.persistence.startAutoSave(++f.clock,f.port).status==hd::Status::Busy);
  CHECK(f.persistence.backgroundSaveActive()&&f.persistence.snapshotCapacity()==65536);
  f.edit("new-");CHECK(f.port.bodyRevision()!=captured_revision);
  const auto r=f.drainSave();CHECK(ok(r.status)&&r.body_committed&&!r.current_record_committed);
  CHECK(f.document.dirty()&&f.persistence.bodyDirty());
  const auto old=f.persistence.loadLane(f.session.currentId(),hd::SaveLane::Auto);
  CHECK(ok(old.status)&&bytesEqual(old.snapshot.text,"old"));
  const auto saved=f.saveAuto();CHECK(ok(saved.status)&&saved.body_committed);
  CHECK(!f.document.dirty()&&!f.persistence.bodyDirty()&&!f.persistence.positionDirty());
  const auto current=f.persistence.loadLatest(f.session.currentId());CHECK(ok(current.status)&&bytesEqual(current.snapshot.text,"new-old"));
  CHECK(medium.max_io<=512);
  std::printf("R2 Busy->Ok old_credential_keeps_new_dirty=1 AUTO_then_new_AUTO=1 steps=%zu max_io=%zu\n",f.save_steps,medium.max_io);
}
static void partial() {
  MemoryStorage medium;R2Fixture f(medium);f.setBody(semantic);
  medium.fault=MemoryStorage::Fault::Flush;medium.fault_path="/POSITION";
  const auto r=f.saveAuto();CHECK(r.status==hd::Status::Partial&&r.body_committed&&!r.current_record_committed);
  CHECK(medium.fault_hits>0&&!f.document.dirty()&&!f.persistence.bodyDirty()&&f.persistence.positionDirty());
  medium.fault=MemoryStorage::Fault::None;
  const auto saved=f.persistence.loadLane(f.session.currentId(),hd::SaveLane::Auto);
  CHECK(ok(saved.status)&&bytesEqual(saved.snapshot.text,semantic));
  std::printf("R2 status=Partial stage=%s body_committed=1 document_dirty=0 body_dirty=0 position_dirty=1\n",r.stage);
}
static void storage() {
  const auto text=bom+bom+semantic+"\r\n"+controlText()+u8"e\u0301 😀";
  MemoryStorage medium;hd::DocumentId id;
  {
    R2Fixture f(medium);f.setBody(text);id=f.session.currentId();CHECK(ok(f.saveAuto().status));
    auto loaded=f.persistence.loadLatest(id);CHECK(ok(loaded.status)&&bytesEqual(loaded.snapshot.text,text));
    CHECK(ok(f.session.create("second",++f.clock).status));CHECK(f.session.currentId()!=id);
    CHECK(ok(f.session.open(id,++f.clock).status)&&bytesEqual(f.text(),text));
  }
  {R2Fixture reopened(medium,false);CHECK(reopened.session.currentId()==id&&bytesEqual(reopened.text(),text));}
  std::printf("R2 exact_byte_save_load_open_reopen=1 C0_1_to_31_DEL_C1_80_to_9f=1 supplementary=1 FEFF=1\n");
}
static void create() {
  MemoryStorage medium;R2Fixture f(medium);f.setBody("original");const auto before=f.session.currentId();
  const auto installs=f.installs;hd::DocumentId created;
  auto r=f.session.create("second",++f.clock,&created);CHECK(ok(r.status));
  CHECK(created!=before&&f.session.currentId()==created&&f.text().empty()&&f.installs>installs);
  hd::CurrentRecord current;CHECK(ok(f.catalog.loadCurrent(&current)));CHECK(current.document_id==created);
  std::printf("R2 legacy_create_installs_blank_and_changes_CURRENT=1 importer_PASS=0\n");
}
int main(int argc,char** argv) {
  if(argc!=2)return 2;
  const auto live=draftor::memory::allocation_stats.live_allocations;
  const std::string_view name=argv[1];
  if(name=="document")document();else if(name=="saving")saving();else if(name=="partial")partial();
  else if(name=="storage")storage();else if(name=="create")create();else return 2;
  CHECK(draftor::memory::allocation_stats.live_allocations==live);
  std::printf("PASS HB_characterization_%s checks=%zu r2_live_delta=0\n",argv[1],checks);
}
