// Production App / StoragePort / RecordStore; no replacement persistence algorithm.
#define main inherited_main
#include "host_tests.cpp"
#undef main
#include <map>

static ds::CreateIntent activeIntent(Fixture& f) {
 ds::RecordStore records(f.storage);
 auto r=records.loadPair(ds::paths::kCreateIntent,ds::RecordKind::CreateIntent,320);
 CHECK(r.status==ds::Status::Ok||r.status==ds::Status::OkDegraded);
 ds::CreateIntent intent;CHECK(ds::decodeCreateIntent(r.record.payload,&intent)==ds::Status::Ok);
 CHECK(intent.active);return intent;
}
static void reachPublish(Fixture& f) {
 f.key(17,1);unsigned guard=0;
 while(f.app.session.creating()&&std::string_view(f.app.session.createStage())!="publish") {
  f.app.service(++f.now,false);CHECK(++guard<2000);
 }
 CHECK(f.app.session.creating());
}
static ds::CreateIntent interruptMeta(Fixture& f,long limit) {
 reachPublish(f);auto intent=activeIntent(f);s3fs::faults.write_bytes=limit;
 f.settle();s3fs::faults={};CHECK(!f.app.lastOperation);
 CHECK(f.app.lastOperation.status==ds::Status::IoError);
 CHECK(std::string_view(f.app.lastOperation.stage)=="publish");return intent;
}
using Files=std::map<std::string,std::string>;
static Files documentFiles(Fixture& f,ds::DocumentId id) {
 Files files;auto root=std::filesystem::path(f.path+std::string(ds::paths::documentRoot(id)));
 for(auto& e:std::filesystem::recursive_directory_iterator(root)) {
  if(e.is_regular_file()) { std::ifstream in(e.path(),std::ios::binary);files[e.path().filename().string()]={std::istreambuf_iterator<char>(in),{}}; }
 }
 return files;
}
static void recovered(Fixture& f,const ds::CreateIntent& intent,ds::DocumentId old,const draftor::memory::String& text) {
 f.key(17,1);f.settle();CHECK(f.app.lastOperation);CHECK(f.app.session.currentId()==intent.metadata.id);
 CHECK(f.app.document.text().empty()&&!f.app.document.dirty());
 CHECK(f.app.persistence.loadLatest(old).snapshot.text==text);
 CHECK(metadata(f,intent.metadata.id).name==intent.metadata.name);
 auto blank=f.app.persistence.loadLatest(intent.metadata.id);CHECK(blank.snapshot.text.empty());
 ds::RecordStore records(f.storage);auto r=records.loadPair(ds::paths::kCreateIntent,ds::RecordKind::CreateIntent,320);
 ds::CreateIntent cleared;CHECK(ds::decodeCreateIntent(r.record.payload,&cleared)==ds::Status::Ok&&!cleared.active);
 CHECK(metadata(f,intent.metadata.id).metadata_generation>=1);
}
static void matrix() {
 for(int kind=0;kind<4;++kind) for(bool restart:{false,true}) {
  Fixture f;f.start();auto trash=f.app.session.currentId();f.key(6);f.save();
  CHECK(f.app.session.create("Old saved work",++f.now));auto old=f.app.session.currentId();
  CHECK(f.app.session.moveToTrash(trash,++f.now));
  // One near-capacity old body also verifies identity/content across the failure.
  draftor::memory::String text=kind==3&&restart?draftor::memory::String(65500,'a'):"a";
  CHECK(s3app::AppTestAccess::document(f.app).replaceAll(text,text.size(),true));s3app::AppTestAccess::editor(f.app).resetForDocument();
  f.app.session.onBodyMutation(++f.now);f.key(80);f.save();auto cursor=f.app.document.cursorByte();
  auto oldFiles=documentFiles(f,old),trashFiles=documentFiles(f,trash);
  auto intent=interruptMeta(f,kind==0?0:(kind==2?39:32));
  CHECK(f.app.session.currentId()==old&&f.app.document.text()==text&&f.app.document.cursorByte()==cursor);
  CHECK(!f.app.document.dirty()&&activeIntent(f).metadata.id==intent.metadata.id);
  if(kind==3) {
   auto bytes=ds::RecordStore::encode(ds::RecordKind::Metadata,1,ds::encodeMetadata(intent.metadata));bytes.back()^=1;
   CHECK(f.storage.write(ds::paths::metadata(intent.metadata.id)+".A",bytes)==ds::Status::Ok);
  }
  if(restart) {
   Fixture reboot(f.path);reboot.start();CHECK(reboot.app.session.currentId()==old&&reboot.app.document.text()==text);
   recovered(reboot,intent,old,text);
  } else { recovered(f,intent,old,text); }
  CHECK(documentFiles(f,old)==oldFiles&&documentFiles(f,trash)==trashFiles);
  std::printf("PASS META case=%d restart=%d old_bytes=%zu stable_target=1 old/trash_files_identical=1\n",kind,restart,text.size());
 }
}
static void repeatedAndOwned() {
 Fixture f;f.start();f.key(4);f.save();auto old=f.app.session.currentId();
 auto intent=interruptMeta(f,32);auto again=interruptMeta(f,39);CHECK(again.metadata.id==intent.metadata.id);
 ds::RecordStore records(f.storage);auto peek=records.peekPair(ds::paths::metadata(intent.metadata.id),ds::RecordKind::Metadata,256);
 CHECK(peek.generation==2);recovered(f,intent,old,"a");CHECK(metadata(f,intent.metadata.id).metadata_generation==3);
 std::puts("PASS repeated META short writes occupy generations 1,2; one retry commits generation 3");
 // A durable intent may repair only its own unfinished metadata, never another ID.
 auto pending=interruptMeta(f,32);auto protectedFiles=documentFiles(f,pending.metadata.id);
 ds::CooperativeRecordWriter writer;draftor::memory::String payload;
 auto wrong=pending;wrong.metadata.name="Wrong intent name";
 CHECK(f.app.catalog.startCreateRecord(ds::DocumentCatalog::CreateRecord::Metadata,wrong,0,writer,payload)==ds::Status::Conflict);
 wrong=pending;wrong.active=false;
 CHECK(f.app.catalog.startCreateRecord(ds::DocumentCatalog::CreateRecord::Metadata,wrong,0,writer,payload)==ds::Status::Conflict);
 CHECK(documentFiles(f,pending.metadata.id)==protectedFiles&&!writer.active());
 s3fs::faults.read=true;
 CHECK(f.app.catalog.startCreateRecord(ds::DocumentCatalog::CreateRecord::Metadata,pending,0,writer,payload)==ds::Status::IoError);
 s3fs::faults={};CHECK(documentFiles(f,pending.metadata.id)==protectedFiles);
 auto durable=records.loadPair(ds::paths::kCreateIntent,ds::RecordKind::CreateIntent,320);
 CHECK(records.commitPair(ds::paths::kCreateIntent,ds::RecordKind::CreateIntent,durable.record.generation+1,ds::encodeCreateIntent({}))==ds::Status::Ok);
 CHECK(f.app.catalog.startCreateRecord(ds::DocumentCatalog::CreateRecord::Metadata,pending,0,writer,payload)==ds::Status::Conflict);
 CHECK(documentFiles(f,pending.metadata.id)==protectedFiles&&!writer.active());
 std::puts("PASS inactive/mismatched/cleared intent and read error preserve incomplete META; no writer started");
}
static void preserveValidAndBounds() {
 Fixture f;f.start();auto old=f.app.session.currentId();f.key(4);f.save();
 auto intent=interruptMeta(f,32);auto changed=intent.metadata;changed.name="Already published name";
 CHECK(f.storage.write(ds::paths::metadata(intent.metadata.id)+".A",ds::RecordStore::encode(ds::RecordKind::Metadata,7,ds::encodeMetadata(changed)))==ds::Status::Ok);
 auto fragment=ds::RecordStore::encode(ds::RecordKind::Metadata,8,ds::encodeMetadata(intent.metadata));fragment.resize(32);
 CHECK(f.storage.write(ds::paths::metadata(intent.metadata.id)+".B",fragment)==ds::Status::Ok);
 auto before=documentFiles(f,intent.metadata.id);intent.metadata=changed;
 recovered(f,intent,old,"a");CHECK(metadata(f,intent.metadata.id).metadata_generation==7);CHECK(documentFiles(f,intent.metadata.id)==before);
 // Existing semantic-invalid, CRC-valid records remain diagnosable; no overwrite.
 auto pending=interruptMeta(f,32);auto semantic=ds::encodeMetadata(pending.metadata);semantic[17]=2; // invalid DocumentArea
 CHECK(f.storage.write(ds::paths::metadata(pending.metadata.id)+".A",ds::RecordStore::encode(ds::RecordKind::Metadata,1,semantic))==ds::Status::Ok);
 before=documentFiles(f,pending.metadata.id);f.key(17,1);f.settle();CHECK(!f.app.lastOperation&&documentFiles(f,pending.metadata.id)==before);
 // The global explicit-generation guard and overflow boundary are unchanged.
 ds::CooperativeRecordWriter writer;draftor::memory::String payload=ds::encodeMetadata(pending.metadata);std::string_view part=payload;
 auto run=[&](uint64_t gen) { CHECK(writer.start(ds::paths::metadata(pending.metadata.id),ds::RecordKind::Metadata,gen,&part,1,f.storage.mediumSession())==ds::Status::Ok);unsigned n=0;while(writer.active()){writer.service(f.storage);CHECK(++n<2000);}CHECK(writer.status()==ds::Status::Conflict); };
 run(1);
 auto full=ds::RecordStore::encode(ds::RecordKind::Metadata,UINT64_MAX,payload);full.resize(32);
 CHECK(f.storage.write(ds::paths::metadata(pending.metadata.id)+".A",full)==ds::Status::Ok);run(0);
 std::puts("PASS valid published META/fallback preserved; semantic corruption and generation overflow refused");
}
int main(int argc,char** argv) {try {
 if(argc==3&&std::string(argv[1])=="--restart-write") {
  Fixture f(argv[2]);f.start();f.key(4);f.save();interruptMeta(f,32);
  std::puts("PASS cross-process META fragment write; old saved body=a");return 0;
 }
 if(argc==3&&std::string(argv[1])=="--restart-read") {
  Fixture f(argv[2]);f.start();auto old=f.app.session.currentId();CHECK(f.app.document.text()=="a");auto intent=activeIntent(f);
  recovered(f,intent,old,"a");std::puts("PASS cross-process META recovery; same pending ID; old saved body=a");return 0;
 }
 matrix();repeatedAndOwned();preserveValidAndBounds();
 std::puts("PASS v0.5.4 META recovery matrix");return 0;
} catch(const std::exception& e) {s3fs::faults={};std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
