#include "writing_app.hpp"
#include "usb_document_import.hpp"
#include "hanime_document_sd_storage.hpp"
#include "draftor_exchange/import_job.hpp"
#include "src/hanime_document/paths.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

namespace fs=std::filesystem;namespace ds=hanime::documents;namespace ex=draftor::exchange;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
struct Workspace {
 fs::path path;
 Workspace(){const char* tmp=std::getenv("TMPDIR");CHECK(tmp);std::string name=(fs::path(tmp)/"usb-doc-XXXXXX").string();CHECK(mkdtemp(name.data()));path=name;}
 ~Workspace(){s3fs::faults={};s3fs::File::openObserver=nullptr;draftor::memory::allocation_fail_after=-1;std::error_code error;fs::remove_all(path,error);}
};
struct Rig {
 s3fs::Fs files;ds::ArduinoSdStoragePort storage;s3host::BuiltinSmallFont font;s3app::App app;s3app::UsbDocumentImport port;
 uint32_t now=0;
 explicit Rig(const fs::path& path):files(path.string()),storage(files),app(storage,{},37113,font),port(storage,app.catalog,app.persistence,app.session,app.editorPort){storage.noteMounted();CHECK(app.startup(++now));}
 void save(){const auto result=app.persistence.startManualSave(++now,app.editorPort);CHECK(result.status==ds::Status::Busy);for(unsigned i=0;i<20000&&app.persistence.backgroundSaveActive();++i)app.service(++now,false);CHECK(!app.persistence.backgroundSaveActive());CHECK(!app.document.dirty());}
 std::size_t count(){ds::ListPage page;CHECK(app.catalog.list(ds::DocumentArea::Normal,0,16,&page)==ds::Status::Ok);return page.items.size();}
};
struct Source final:ex::ImportSource {
 std::string bytes;ex::SourceIdentity id{};bool live=true,complete=true;
 explicit Source(std::string data,std::uint64_t serial=701):bytes(std::move(data)),id{serial,3,bytes.size()}{}
 ex::SourceIdentity identity()const noexcept override{return id;}
 std::uint64_t sourceLease()const noexcept override{return id.session;}
 bool stable()const noexcept override{return live;}
 bool confirmed()const noexcept override{return complete;}
 ex::IoResult read(std::uint64_t offset,void* out,std::size_t size)noexcept override{if(offset>bytes.size()||size>bytes.size()-offset)return {ex::Status::Bounds,0};std::memcpy(out,bytes.data()+offset,size);return {ex::Status::Ok,size};}
};
ex::ImportIntent intent(Rig& rig,Source& source,const char* name="incoming.txt"){
 ex::ImportIntent result;CHECK(rig.port.allocateOperation(result.operation)==ex::Status::Ok);result.source=source.id;std::strncpy(result.name.data(),name,result.name.size()-1);return result;
}
void run(ex::ImportJob& job){for(unsigned i=0;i<30000;++i){const auto phase=job.tick();if(phase==ex::ImportPhase::Complete||phase==ex::ImportPhase::Failed||phase==ex::ImportPhase::Uncertain)return;}CHECK(false);}
void finish(s3app::UsbDocumentImport& port,std::uint64_t operation,std::size_t bytes){ex::Status status=ex::Status::Busy;for(unsigned i=0;i<30000&&status==ex::Status::Busy;++i)status=port.finish(operation,bytes);CHECK(status==ex::Status::Ok);}
void publish(s3app::UsbDocumentImport& port,std::uint64_t operation){ex::Status status=ex::Status::Busy;for(unsigned i=0;i<30000&&status==ex::Status::Busy;++i)status=port.publish(operation);CHECK(status==ex::Status::Ok);}

void endToEnd(){
 Workspace workspace;ds::DocumentId imported,original;std::uint64_t operation=0;std::array<uint8_t,16> collection{};
 const std::string expected="中文 first\nsecond\n";
 {
  Rig rig(workspace.path);CHECK(rig.app.commitText("manual original",rig.app.editStamp()));rig.save();original=rig.app.session.currentId();
  CHECK(rig.app.commitText(" dirty",rig.app.editStamp()));const auto before=rig.app.document.text();const auto cursor=rig.app.document.cursorByte();const auto revision=rig.app.document.revision();const auto session=rig.app.session.sessionSerial();
  Source source("\xEF\xBB\xBF中文 first\r\nsecond\r\n");const auto request=intent(rig,source);operation=request.operation;
  ex::ImportJob job(rig.port);CHECK(job.begin(source,request)==ex::Status::Ok);CHECK(rig.app.session.busy());CHECK(rig.app.session.create({},++rig.now).status==ds::Status::Busy);
  run(job);CHECK(job.phase()==ex::ImportPhase::Complete);CHECK(job.result().outcome==ex::Outcome::Imported);
  imported.bytes=job.result().document;collection=job.result().collection;CHECK(!imported.empty()&&imported!=original);CHECK(collection==rig.app.catalog.boundSession().collection_id.bytes);
  CHECK(rig.app.document.text()==before&&rig.app.document.cursorByte()==cursor&&rig.app.document.revision()==revision&&rig.app.session.sessionSerial()==session&&rig.app.document.dirty());CHECK(rig.count()==2);CHECK(!rig.app.session.busy());
  Source unavailable("unused");unavailable.live=false;ex::ImportJob retry(rig.port);CHECK(retry.begin(unavailable,request)==ex::Status::Ok);CHECK(retry.phase()==ex::ImportPhase::Complete);CHECK(retry.result().document==imported.bytes&&rig.count()==2);
  auto changed=request;++changed.source.entry;ex::ImportResult receipt;CHECK(rig.port.reserve(changed,receipt)==ex::Status::Conflict);
  CHECK(rig.app.session.open(imported,++rig.now));CHECK(rig.app.document.text()==std::string_view(expected));CHECK(rig.app.persistence.loadLane(original,ds::SaveLane::Manual).snapshot.text=="manual original");
  CHECK(rig.app.commitText("saved edit",rig.app.editStamp()));rig.save();
 }
 {
  Rig reboot(workspace.path);CHECK(reboot.app.session.currentId()==imported);CHECK(reboot.app.document.text()==std::string_view("saved edit"+expected));ex::ImportResult receipt;CHECK(reboot.port.query(operation,receipt)==ex::Status::Ok);CHECK(receipt.document==imported.bytes&&receipt.collection==collection);CHECK(reboot.count()==2);
 }
 std::cout<<"PASS real App import/publication/normal open/save/reconstructed startup and durable idempotent receipt\n";
}
void maxBodyAndDuplicateName(){
 Workspace workspace;Rig rig(workspace.path);const auto original=rig.app.session.currentId();
 std::string wire="\xEF\xBB\xBF";for(std::size_t i=0;i<65536;++i)wire+="\r\n";
 Source source(wire);auto request=intent(rig,source,"boundary.txt");ex::ImportJob job(rig.port);CHECK(job.begin(source,request)==ex::Status::Ok);run(job);CHECK(job.phase()==ex::ImportPhase::Complete&&job.result().bodyBytes==65536);ds::DocumentId id;id.bytes=job.result().document;auto loaded=rig.app.persistence.loadLatest(id);CHECK(loaded.status==ds::Status::Ok&&loaded.snapshot.text==std::string_view(std::string(65536,'\n')));
 Source source2("second",702);auto second=intent(rig,source2,"boundary.txt");ex::ImportJob job2(rig.port);CHECK(job2.begin(source2,second)==ex::Status::Ok);run(job2);CHECK(job2.phase()==ex::ImportPhase::Complete&&job2.result().document!=job.result().document);CHECK(rig.app.session.currentId()==original&&rig.count()==3);
 std::cout<<"PASS W=131075/B=65536 real transaction and duplicate name CreateNew\n";
}
void failuresAndCancellation(){
 for(unsigned scenario=0;scenario<6;++scenario){
  Workspace workspace;Rig rig(workspace.path);CHECK(rig.app.commitText("protected dirty original",rig.app.editStamp()));const auto before=rig.app.document.text();const auto id=rig.app.session.currentId();
  Source source(scenario==0?std::string("bad\xff",4):std::string(2000,'x'));
  auto request=intent(rig,source);ex::ImportJob job(rig.port);CHECK(job.begin(source,request)==ex::Status::Ok);
  if(scenario==1){job.tick();job.cancel();}
  else if(scenario==2){while(job.phase()!=ex::ImportPhase::Verifying)job.tick();s3fs::faults.write_bytes=0;run(job);}
  else if(scenario==3){rig.storage.noteUnmounted();job.tick();}
  else if(scenario==4){source.live=false;job.tick();}
  else if(scenario==5){CHECK(rig.app.commitText(" invalid mutation",rig.app.editStamp()));job.tick();}
  else run(job);
  CHECK(job.phase()==ex::ImportPhase::Failed);CHECK(rig.app.session.currentId()==id);CHECK(scenario==5||rig.app.document.text()==before);CHECK(rig.app.document.dirty());CHECK(!rig.app.session.busy());
  s3fs::faults={};if(scenario!=3&&scenario!=5){CHECK(rig.count()==1);Source next("recovery",999);auto nextIntent=intent(rig,next);ex::ImportJob again(rig.port);CHECK(again.begin(next,nextIntent)==ex::Status::Ok);run(again);CHECK(again.phase()==ex::ImportPhase::Complete);}
 }
 std::cout<<"PASS bad UTF8/cancel/short write/remount/source retirement/revision fence preserve original\n";
}
void restartReservedAndVerified(){
 for(bool verified:{false,true}){
  Workspace workspace;std::uint64_t operation=0;ex::ImportIntent request;std::array<uint8_t,16> reserved{};ds::DocumentId original;
  {
   Rig rig(workspace.path);Source source("retained");request=intent(rig,source);operation=request.operation;original=rig.app.session.currentId();ex::ImportResult receipt;CHECK(rig.port.reserve(request,receipt)==ex::Status::Ok);reserved=receipt.document;CHECK(rig.port.begin(operation)==ex::Status::Ok);CHECK(rig.port.append(operation,"retained").status==ex::Status::Ok);if(verified)finish(rig.port,operation,8);
   // Simulate loss of the mounted session before process destruction; abort
   // cannot write to the retired mount and the durable intent remains intact.
   rig.storage.noteUnmounted();rig.port.abort(operation);
  }
  {
   Rig reboot(workspace.path);CHECK(reboot.app.session.currentId()==original&&reboot.count()==1);Source source("retained");ex::ImportJob job(reboot.port);CHECK(job.begin(source,request)==ex::Status::Ok);run(job);CHECK(job.phase()==ex::ImportPhase::Complete&&job.result().document==reserved);CHECK(reboot.count()==2);
  }
 }
 std::cout<<"PASS reconstructed reserved/verified operation resumes same complete 128-bit document identity\n";
}
static bool failReceipt=false;
static void failReceiptOpen(const char* path,const char* mode){if(failReceipt&&std::strstr(path,"/imports/")&&mode[0]=='w'){failReceipt=false;s3fs::faults.write_bytes=0;}}
void publishingRecovery(){
 Workspace workspace;ex::ImportIntent request;std::array<uint8_t,16> id{};ds::DocumentId original;ds::ReadResult verifiedRecord;
 {
  Rig rig(workspace.path);Source source("published bytes");request=intent(rig,source);ex::ImportResult receipt;CHECK(rig.port.reserve(request,receipt)==ex::Status::Ok);id=receipt.document;original=rig.app.session.currentId();CHECK(rig.port.begin(request.operation)==ex::Status::Ok);CHECK(rig.port.append(request.operation,"published bytes").status==ex::Status::Ok);finish(rig.port,request.operation,15);
  char slot[64];std::snprintf(slot,sizeof(slot),"/imports/%016llX.B",static_cast<unsigned long long>(request.operation));verifiedRecord=rig.storage.read(draftor::memory::String(ds::paths::kRoot)+slot,544);CHECK(verifiedRecord.status==ds::Status::Ok);
  // First publish call durably records commit intent. Failure is then injected
  // only into the final receipt after the cooperative META has been verified.
  CHECK(rig.port.publish(request.operation)==ex::Status::Busy);failReceipt=true;s3fs::File::openObserver=failReceiptOpen;
  ex::Status status=ex::Status::Busy;for(unsigned i=0;i<10000&&status==ex::Status::Busy;++i)status=rig.port.publish(request.operation);CHECK(status==ex::Status::Uncertain);s3fs::faults={};s3fs::File::openObserver=nullptr;CHECK(rig.app.session.currentId()==original);
 }
 {
  Rig reboot(workspace.path);CHECK(reboot.app.session.currentId()==original);
  ds::DocumentId created;created.bytes=id;CHECK(reboot.app.session.open(created,++reboot.now));CHECK(reboot.app.commitText("later edit ",reboot.app.editStamp()));reboot.save();
  CHECK(reboot.app.catalog.rename(created,"renamed after reboot")==ds::Status::Ok);
  ex::ImportResult receipt;CHECK(reboot.port.query(request.operation,receipt)==ex::Status::Ok);CHECK(receipt.outcome==ex::Outcome::Imported&&receipt.document==id&&reboot.count()==2);CHECK(reboot.app.session.currentId()==created);CHECK(reboot.app.document.text()=="later edit published bytes");
  ds::DocumentMetadata meta;CHECK(reboot.app.catalog.get(created,&meta)==ds::Status::Ok&&meta.name=="renamed after reboot");
  // Corrupt the final receipt slot after the creation intent was cleared. The
  // older Publishing receipt must recover its existing ID, never reserve one.
  char suffix[64];std::snprintf(suffix,sizeof(suffix),"/imports/%016llX.B",static_cast<unsigned long long>(request.operation));
  CHECK(reboot.storage.write(draftor::memory::String(ds::paths::kRoot)+suffix,"torn")==ds::Status::Ok);
  Source source("published bytes");ex::ImportJob job(reboot.port);CHECK(job.begin(source,request)==ex::Status::Ok&&job.phase()==ex::ImportPhase::Complete);CHECK(job.result().document==id&&reboot.count()==2);
  CHECK(reboot.storage.write(draftor::memory::String(ds::paths::kRoot)+suffix,verifiedRecord.bytes)==ds::Status::Ok);
  suffix[std::strlen(suffix)-1]='A';CHECK(reboot.storage.write(draftor::memory::String(ds::paths::kRoot)+suffix,"torn")==ds::Status::Ok);
  CHECK(job.begin(source,request)==ex::Status::Ok&&job.phase()==ex::ImportPhase::Complete);CHECK(job.result().document==id&&reboot.count()==2);
  ds::CreateIntent pending;std::uint64_t generation=0;CHECK(reboot.app.catalog.readCreateIntent(&pending,&generation)==ds::Status::Ok&&!pending.active);
  CHECK(reboot.app.commitText("dirty",reboot.app.editStamp()));CHECK(reboot.app.persistence.startManualSave(++reboot.now,reboot.app.editorPort).status==ds::Status::Busy);
  CHECK(reboot.port.query(request.operation,receipt)==ex::Status::Busy);
  for(unsigned i=0;i<10000&&reboot.app.persistence.backgroundSaveActive();++i)reboot.app.service(++reboot.now,false);
  CHECK(!reboot.app.persistence.backgroundSaveActive());CHECK(reboot.port.query(request.operation,receipt)==ex::Status::Ok);
 }
 std::cout<<"PASS durable META/final receipt write failure then same-operation reconstruction recovery\n";
}
void allocationAndCycles(){
 Workspace workspace;Rig rig(workspace.path);
 for(long allocation=0;allocation<32;++allocation){
  Source source("reserve OOM");auto request=intent(rig,source);ex::ImportResult receipt;draftor::memory::allocation_fail_after=allocation;
  const auto status=rig.port.reserve(request,receipt);draftor::memory::allocation_fail_after=-1;
  CHECK(status==ex::Status::Ok||status==ex::Status::Exhausted);
  if(status==ex::Status::Ok)rig.port.abort(request.operation);
  CHECK(!rig.app.session.busy());CHECK(rig.count()==1);
 }
 {
  Source source("oom");auto request=intent(rig,source);ex::ImportResult result;CHECK(rig.port.reserve(request,result)==ex::Status::Ok);draftor::memory::allocation_fail_after=0;CHECK(rig.port.begin(request.operation)==ex::Status::Exhausted);draftor::memory::allocation_fail_after=-1;
  // If an OOM also prevented the abort receipt, explicitly retry the same ID.
  ex::ImportJob recovery(rig.port);const auto start=recovery.begin(source,request);CHECK(start==ex::Status::Ok||start==ex::Status::Conflict);if(start==ex::Status::Ok){run(recovery);CHECK(recovery.phase()==ex::ImportPhase::Complete);}
 }
 auto baseline=draftor::memory::allocation_stats.live_allocations;
 for(unsigned i=0;i<30;++i){Source source("cancel");auto request=intent(rig,source);{ex::ImportJob job(rig.port);CHECK(job.begin(source,request)==ex::Status::Ok);job.tick();job.cancel();}if(i==0)baseline=draftor::memory::allocation_stats.live_allocations;CHECK(draftor::memory::allocation_stats.live_allocations==baseline);}
 std::cout<<"PASS allocation failure and 30 cancelled lifecycle cycles have no live allocation growth\n";
}
int main(){try{endToEnd();maxBodyAndDuplicateName();failuresAndCancellation();restartReservedAndVerified();publishingRecovery();allocationAndCycles();std::cout<<"PASS USB_DOCUMENT_TRANSACTION 6 behavior groups\n";return 0;}catch(const std::exception& error){s3fs::faults={};s3fs::File::openObserver=nullptr;draftor::memory::allocation_fail_after=-1;std::cerr<<error.what()<<'\n';return 1;}}
