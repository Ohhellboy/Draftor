#define main inherited_main
#include "host_tests.cpp"
#undef main
void failStages(){
 for(const char* target:{"collection","intent","initial_auto","publish","clear_intent","current"}){
  Fixture f;f.start();auto old=f.app.session.currentId();f.key(4);f.save();f.key(17,1);
  unsigned guard=0;while(f.app.session.creating()&&std::string_view(f.app.session.createStage())!=target){f.app.service(++f.now,false);CHECK(++guard<2000);}
  CHECK(f.app.session.creating());s3fs::faults.write_bytes=0;f.settle();s3fs::faults={};CHECK(!f.app.lastOperation);
  if(std::string_view(target)=="current"){CHECK(f.app.session.currentId()!=old&&f.app.lastOperation.status==ds::Status::Partial);}
  else CHECK(f.app.session.currentId()==old&&f.app.document.text()=="a");
  CHECK(f.app.persistence.loadLatest(old).snapshot.text=="a");
  // A retry resumes or creates safely; failed initial files cannot be published as valid.
  f.key(17,1);f.settle();std::printf("create_retry target=%s result=%s stage=%s bytes=%zu\n",target,ds::statusMessage(f.app.lastOperation.status),f.app.lastOperation.stage,f.app.document.text().size());CHECK(f.app.lastOperation&&f.app.document.text().empty());
  CHECK(f.app.persistence.loadLatest(f.app.session.currentId()).snapshot.text.empty());
 }
 Fixture moved;moved.start();moved.key(17,1);while(std::string_view(moved.app.session.createStage())!="clear_intent")moved.app.service(++moved.now,false);
 s3fs::faults.write_bytes=0;moved.settle();s3fs::faults={};CHECK(moved.app.lastOperation.status==ds::Status::Partial);
 ds::ListPage page;CHECK(moved.app.catalog.list(ds::DocumentArea::Normal,0,16,&page)==ds::Status::Ok);auto abandoned=page.items[0].id;
 CHECK(moved.app.session.moveToTrash(abandoned,++moved.now));moved.key(17,1);moved.settle();CHECK(moved.app.lastOperation&&moved.app.session.currentId()!=abandoned&&metadata(moved,abandoned).area==ds::DocumentArea::Trash);
 // Valid header with truncated payload: resumed initial must be verified/repaired.
 Fixture f;f.start();ds::CreateIntent intent;CHECK(f.app.catalog.reserveCreate("Interrupted",&intent)==ds::Status::Ok);
 ds::Snapshot blank;auto payload=ds::encodeSnapshot(blank);auto bytes=ds::RecordStore::encode(ds::RecordKind::Snapshot,1,payload);bytes.resize(ds::RecordStore::kHeaderBytes);
 CHECK(f.storage.write(ds::paths::lane(intent.metadata.id,ds::SaveLane::Auto)+".A",bytes)==ds::Status::Ok);
 f.key(17,1);f.settle();CHECK(f.app.lastOperation&&f.app.session.currentId()==intent.metadata.id);
 CHECK(f.app.persistence.loadLatest(intent.metadata.id).snapshot.text.empty());
}
int main(){try{failStages();
 Fixture f;f.start();auto old=f.app.session.currentId();f.key(4);f.key(17,1);CHECK(f.app.commandBarrier());
 f.key(17,1);f.key(5);f.settle();CHECK(f.app.session.currentId()!=old&&f.app.document.text().empty());CHECK(f.app.persistence.loadLatest(old).snapshot.text=="a");
 auto fresh=f.app.session.currentId();f.key(6);f.save();f.key(17,1);CHECK(f.app.commandBarrier());
 s3fs::faults.write_bytes=0;f.settle();s3fs::faults={};CHECK(f.app.session.currentId()==fresh&&f.app.document.text()=="c");
 // Failed reservation is retryable; no installed target until verified initial AUTO and metadata.
 f.key(17,1);CHECK(f.app.commandBarrier());f.app.disconnected();CHECK(f.app.commandBarrier());f.settle();CHECK(f.app.session.currentId()!=fresh&&f.app.document.text().empty());
 auto id=f.app.session.currentId();CHECK(f.app.persistence.loadLatest(id).snapshot.text.empty());
 auto bytes=f.app.document.text().capacity();for(unsigned i=0;i<5;++i){f.key(17,1);f.settle();CHECK(f.app.document.text().capacity()<=bytes);}
 std::puts("PASS creation: cooperative barrier, old dirty saved, duplicate/input blocked, write failure old scene retained, disconnect finishes owned transaction, verified blank load, no growing body capacity");return 0;
}catch(const std::exception& e){s3fs::faults={};std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
