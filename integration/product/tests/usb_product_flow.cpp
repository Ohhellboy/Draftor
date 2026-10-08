#include "usb_transfer.hpp"
#include "hanime_document_sd_storage.hpp"
#include "media_bridge.hpp"
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace fs=std::filesystem;
namespace u=draftor::usb;
namespace ds=hanime::documents;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
// Host transport model; product App, persistence, import transaction and
// UsbTransfer are the exact production components. Real FAT tests are separate.
struct Platform final:s3app::UsbPlatform,u::InboxMediaPort,u::InboxFilesystemPort,u::BlockDevice,u::MediaPort {
 u::MediaBridge bridge;std::string wire="\xef\xbb\xbfhello\r\n世界\rnext\n";
 std::string filename="incoming.txt";
 std::uint64_t session=0;unsigned allocs=0;bool sealed=false,mounted=false,opened=false,enumerated=false,allocated=false,hold=false;
 u::MediaPort& media()noexcept override{return *this;}
 u::InboxMediaPort& backing()noexcept override{return *this;}
 u::InboxFilesystemPort& files()noexcept override{return *this;}
 u::Status prepare_result=u::Status::Ok,allocate_result=u::Status::Ok,backing_result=u::Status::Ok,publish_result=u::Status::Ok,mount_result=u::Status::Ok;
 unsigned inbox_alloc_calls=0,inbox_free_calls=0;bool unsafe_free=false;
 u::Status prepare(std::uint32_t)noexcept override{return prepare_result;}
 void cancelPreparation()noexcept override{}
 u::Status allocateInbox()noexcept override{++inbox_alloc_calls;if(allocate_result!=u::Status::Ok)return allocate_result;allocated=true;return u::Status::Ok;}
 void freeInbox()noexcept override{++inbox_free_calls;const auto s=state();unsafe_free|=opened||mounted||s.accepting||!s.drained;allocated=false;}
 char* allocate(std::size_t n)noexcept override{auto p=static_cast<char*>(std::malloc(n));if(p)++allocs;return p;}
 void deallocate(char* p,std::size_t)noexcept override{if(p){--allocs;std::free(p);}}
 u::Status publish(u::BlockDevice& b,u::Access a,std::uint32_t& g)noexcept override{return publish_result==u::Status::Ok?bridge.publish(b,a,g):publish_result;}
 void withdraw(u::EndReason e)noexcept override{bridge.withdraw(e);}
 u::MediaState state()const noexcept override{auto s=bridge.state();if(hold)s.drained=false;return s;}
 u::Status detach()noexcept override{return hold?u::Status::Busy:bridge.detach();}
 u::Status prepare(std::uint64_t s)noexcept override{session=s;sealed=mounted=opened=enumerated=false;if(backing_result!=u::Status::Ok)return backing_result;return allocated?u::Status::Ok:u::Status::NotReady;}
 u::BlockDevice& device()noexcept override{return *this;}
 u::Status seal(std::uint64_t s)noexcept override{if(s!=session)return u::Status::Stale;sealed=true;return u::Status::Ok;}
 bool stable(std::uint64_t s)const noexcept override{return sealed&&s==session;}
 void release()noexcept override{sealed=false;}
 u::Status mountReadOnly(std::uint64_t s)noexcept override{if(!stable(s))return u::Status::NotReady;if(mount_result!=u::Status::Ok)return mount_result;mounted=true;return u::Status::Ok;}
 u::Status next(u::InboxCandidate& c)noexcept override{
  if(enumerated)return u::Status::NotFound;
  enumerated=true;c={};c.source={session,1,wire.size()};std::strncpy(c.name.data(),filename.c_str(),c.name.size()-1);return u::Status::Ok;
 }
 u::Status open(const u::InboxCandidate& c)noexcept override{if(!mounted||c.source.session!=session)return u::Status::Stale;opened=true;return u::Status::Ok;}
 void closeFile()noexcept override{opened=false;}
 void unmount()noexcept override{mounted=false;}
 std::uint32_t sectorCount()const noexcept override{return 1024;}
 u::IoResult read(std::uint64_t at,void* b,std::size_t n)noexcept override{
  if(opened){if(at>wire.size()||n>wire.size()-at)return {u::Status::Bounds,0};std::memcpy(b,wire.data()+at,n);}
  else std::memset(b,0,n);
  return {u::Status::Ok,n};
 }
 u::IoResult write(std::uint64_t,const void*,std::size_t n)noexcept override{return {u::Status::Ok,n};}
 u::Status sync()noexcept override{return u::Status::Ok;}
};
struct Rig {
 fs::path path;s3fs::Fs files;ds::ArduinoSdStoragePort storage;s3host::BuiltinSmallFont font;s3app::App app;
 Platform platform;s3app::UsbDocumentImport sink;s3app::UsbTransfer transfer;std::uint32_t now=0;
 static fs::path temp(){char p[]="usb-product-XXXXXX";CHECK(mkdtemp(p));return fs::absolute(p);}
 Rig():path(temp()),files(path.string()),storage(files),app(storage,{},917,font),sink(storage,app.catalog,app.persistence,app.session,app.editorPort),transfer(app,platform,sink){storage.noteMounted();CHECK(app.startup(++now));}
 ~Rig(){draftor::memory::allocation_fail_after=-1;s3fs::File::openObserver=nullptr;platform.hold=false;transfer.cancel();for(unsigned i=0;i<10000&&transfer.active();++i)transfer.tick(++now,{});std::error_code ec;fs::remove_all(path,ec);}
 void until(s3app::UsbUiPhase phase){for(unsigned i=0;i<100000;++i){transfer.tick(++now,{});if(transfer.phase()==phase)return;}CHECK(false);}
 void key(std::uint8_t u){CHECK(transfer.key({1,++now,u,0,true},{}));}
 void close(){key(41);until(s3app::UsbUiPhase::Idle);CHECK(!platform.allocated&&!platform.allocs);}
 void receive(){CHECK(transfer.request(true,{})==u::Status::Ok);until(s3app::UsbUiPhase::Receiving);transfer.tick(++now,{});CHECK(platform.state().accepting);platform.withdraw(u::EndReason::Ejected);until(s3app::UsbUiPhase::Selecting);transfer.tick(++now,{});transfer.tick(++now,{});key(40);until(s3app::UsbUiPhase::Result);}
};
void exported(Rig& r,std::string expected){
 CHECK(r.transfer.request(false,{})==u::Status::Ok);
 for(unsigned i=0;i<100000&&!r.platform.state().accepting;++i)r.transfer.tick(++r.now,{});
 CHECK(r.platform.state().accepting);const auto g=r.platform.state().generation;
 std::string out(expected.size(),0);auto read=r.platform.bridge.read(g,u::ExportVolume::DataStart*512,out.data(),out.size());
 CHECK(read.status==u::Status::Ok&&out==expected);
 char byte=0;CHECK(r.platform.bridge.write(g,0,&byte,1).status==u::Status::ReadOnly);
 CHECK(r.transfer.request(true,{})==u::Status::Busy);
 r.platform.withdraw(u::EndReason::Ejected);r.until(s3app::UsbUiPhase::Result);r.close();
}
void flows(){
 for(bool vertical:{false,true}){
  Rig r;if(vertical){r.app.layoutKey({1,++r.now,61,0,true});r.app.layoutKey({1,++r.now,61,0,false});}
  CHECK(r.app.commitText("original\n稿件",r.app.editStamp()));r.app.moveToByte(3,true);
  const auto id=r.app.session.currentId();const auto cursor=r.app.document.cursorByte();const auto selection=r.app.editor.selection();const auto undo=r.app.editor.undoCount();const auto layout=r.app.layoutMode();
  exported(r,"original\n稿件");CHECK(r.app.session.currentId()==id&&r.app.document.cursorByte()==cursor&&r.app.editor.selection().anchor==selection.anchor&&r.app.editor.undoCount()==undo&&r.app.layoutMode()==layout);
  r.receive();CHECK(r.transfer.imported().outcome==u::Outcome::Imported);auto receipt=r.transfer.imported();
  CHECK(r.app.session.currentId()==id&&r.app.document.text()=="original\n稿件");
  ds::DocumentId imported;imported.bytes=receipt.document;CHECK(imported!=id&&!imported.empty());r.close();
  CHECK(r.app.enterTransfer());CHECK(r.app.openImported(imported,++r.now));r.app.leaveTransfer();CHECK(r.app.document.text()=="hello\n世界\nnext\n");
  CHECK(r.app.commitText("changed ",r.app.editStamp()));CHECK(r.app.session.manualSave(++r.now));
  const auto expected=std::string(r.app.document.text());
  s3app::App reopened(r.storage,{},918,r.font);CHECK(reopened.startup(++r.now));CHECK(reopened.session.currentId()==imported&&std::string_view(reopened.document.text())==expected);
  exported(r,expected);
  CHECK(r.app.commitText("unsaved ",r.app.editStamp()));const auto outgoing=std::string(r.app.document.text());
  r.receive();ds::DocumentId second;second.bytes=r.transfer.imported().document;CHECK(second!=imported);
  r.key(40);r.until(s3app::UsbUiPhase::Idle);CHECK(r.app.session.currentId()==second&&r.app.document.text()=="hello\n世界\nnext\n");
  const auto saved=r.app.persistence.loadLatest(imported);CHECK(saved.status==ds::Status::Ok&&std::string_view(saved.snapshot.text)==outgoing);
  std::cout<<"PASS real_product_flow layout="<<vertical<<" import_publish_continue_open_save_reopen_export bytes="<<expected.size()<<'\n';
 }
}
void controls(){
 Rig r;CHECK(r.app.commitText("keep",r.app.editStamp()));const auto id=r.app.session.currentId();
 CHECK(r.transfer.request(false,{true,false,false})==u::Status::Busy);CHECK(!r.transfer.active());
 CHECK(r.transfer.request(true,{})==u::Status::Ok);r.until(s3app::UsbUiPhase::Receiving);r.transfer.tick(++r.now,{});
 r.platform.hold=true;r.transfer.cancel();for(unsigned i=0;i<4;++i)r.transfer.tick(++r.now,{});
 CHECK(r.transfer.phase()==s3app::UsbUiPhase::Closing&&r.platform.allocated);CHECK(r.transfer.request(false,{})==u::Status::Busy);
 CHECK(!r.app.commitText("bad",r.app.editStamp()));r.platform.hold=false;r.until(s3app::UsbUiPhase::Idle);CHECK(r.app.document.text()=="keep");
 for(unsigned i=0;i<12;++i){CHECK(r.transfer.request(true,{})==u::Status::Ok);r.until(s3app::UsbUiPhase::Receiving);r.transfer.tick(++r.now,{});r.platform.withdraw(u::EndReason::Disconnected);r.until(s3app::UsbUiPhase::Result);CHECK(r.transfer.imported().outcome!=u::Outcome::Imported);r.close();}
 CHECK(r.app.session.currentId()==id);ds::ListPage page;CHECK(r.app.catalog.list(ds::DocumentArea::Normal,0,16,&page)==ds::Status::Ok);CHECK(page.items.size()==1);
 r.platform.wire=std::string("bad\0text",8);r.receive();CHECK(r.transfer.imported().outcome==u::Outcome::Aborted);r.close();CHECK(r.app.document.text()=="keep");
 std::cout<<"PASS control_cancel_drain_busy_unconfirmed_bad_text_cycles original_retained=1\n";
}
static bool failCurrentWrite=false;
static void memoryAtCurrentWrite(const char* path,const char* mode){
 if(failCurrentWrite&&std::strstr(path,"/CURRENT.")&&mode[0]=='w'){
  failCurrentWrite=false;draftor::memory::allocation_fail_after=0;
 }
}
void openMemoryFailures(){
 for(bool afterInstall:{false,true}){
  Rig r;CHECK(r.app.commitText("original",r.app.editStamp()));
  const auto original=r.app.session.currentId();r.platform.filename="memory-open-test-document.txt";
  r.receive();ds::DocumentId target;target.bytes=r.transfer.imported().document;
  r.key(40);r.until(s3app::UsbUiPhase::Opening);
  // Complete the outgoing save before injecting the installation failure.
  for(unsigned i=0;i<100000&&(r.app.persistence.bodyDirty()||r.app.persistence.backgroundSaveActive());++i)r.transfer.tick(++r.now,{});
  CHECK(r.transfer.phase()==s3app::UsbUiPhase::Opening);
  const auto failures=draftor::memory::allocation_stats.failures;
  if(afterInstall){failCurrentWrite=true;s3fs::File::openObserver=&memoryAtCurrentWrite;}
  else draftor::memory::allocation_fail_after=0;
  r.transfer.tick(++r.now,{});
  draftor::memory::allocation_fail_after=-1;s3fs::File::openObserver=nullptr;
  CHECK(draftor::memory::allocation_stats.failures>failures);
  CHECK(r.transfer.phase()==s3app::UsbUiPhase::Result);
  if(afterInstall){
   CHECK(!failCurrentWrite&&r.app.session.currentId()==target);
   CHECK(r.transfer.imported().outcome==u::Outcome::Opened);
   CHECK(r.app.currentName=="memory-open-test-document"&&r.app.document.text()=="hello\n世界\nnext\n");
  }else{
   CHECK(r.app.session.currentId()==original&&r.app.document.text()=="original");
   CHECK(r.transfer.imported().outcome==u::Outcome::Imported);
  }
  r.close();CHECK(r.app.session.currentId()==(afterInstall?target:original));
 }
 std::cout<<"PASS open OOM before/after installation reports actual current draft and preserves display name\n";
}
void cancelPendingOpen(){
 const char* names[]={"Closing","Opening_before_save","Opening_during_save","normal_open_control"};
 for(bool vertical:{false,true}){
  for(unsigned scenario=0;scenario<4;++scenario){
   Rig r;
   if(vertical){
    r.app.layoutKey({1,++r.now,61,0,true});
    r.app.layoutKey({1,++r.now,61,0,false});
   }
   const std::string originalText="original unsaved\n稿件";
   CHECK(r.app.commitText(originalText,r.app.editStamp()));
   r.app.moveToByte(3,true);
   const auto original=r.app.session.currentId();
   const auto session=r.app.session.sessionSerial();
   const auto cursor=r.app.document.cursorByte();
   const auto selection=r.app.editor.selection();
   const auto undo=r.app.editor.undoCount();
   const auto redo=r.app.editor.redoCount();
   const auto layout=r.app.layoutMode();
   ds::EditorView before;
   CHECK(r.app.editorPort.capture(&before)==ds::Status::Ok);
   CHECK(selection.has_selection&&undo>0);

   r.receive();
   const auto receipt=r.transfer.imported();
   CHECK(receipt.outcome==u::Outcome::Imported&&receipt.operation);
   ds::DocumentId imported;
   imported.bytes=receipt.document;
   CHECK(imported!=original);
   CHECK(r.app.persistence.bodyDirty()&&!r.app.persistence.backgroundSaveActive());
   r.key(40);
   CHECK(r.transfer.phase()==s3app::UsbUiPhase::Closing);
   if(scenario==1||scenario==2)r.until(s3app::UsbUiPhase::Opening);
   if(scenario==2){
    r.transfer.tick(++r.now,{});
    CHECK(r.app.persistence.backgroundSaveActive());
   }
   if(scenario!=3){
    r.key(41);
    CHECK(r.transfer.phase()==s3app::UsbUiPhase::Closing);
    CHECK(r.app.transferActive());
   }
   r.until(s3app::UsbUiPhase::Idle);
   CHECK(!r.app.persistence.backgroundSaveActive()&&!r.app.transferActive());
   CHECK(!r.platform.allocated&&!r.platform.allocs);

   // Cancelling opening never cancels or duplicates the already-published import.
   ds::DocumentMetadata metadata;
   CHECK(r.app.catalog.get(imported,&metadata)==ds::Status::Ok);
   ds::ListPage page;
   CHECK(r.app.catalog.list(ds::DocumentArea::Normal,0,16,&page)==ds::Status::Ok);
   CHECK(page.items.size()==2);
   u::ImportResult queried;
   CHECK(r.sink.query(receipt.operation,queried)==u::Status::Ok);
   CHECK(queried.operation==receipt.operation&&queried.document==receipt.document);
   CHECK(queried.outcome==u::Outcome::Imported);
   const auto importedBody=r.app.persistence.loadLatest(imported);
   CHECK(importedBody.status==ds::Status::Ok&&importedBody.snapshot.text=="hello\n世界\nnext\n");

   if(scenario!=3){
    CHECK(r.app.session.currentId()==original&&r.app.session.sessionSerial()==session);
    CHECK(std::string_view(r.app.document.text())==originalText);
    CHECK(r.app.document.cursorByte()==cursor);
    const auto actual=r.app.editor.selection();
    CHECK(actual.begin==selection.begin&&actual.end==selection.end);
    CHECK(actual.anchor==selection.anchor&&actual.active==selection.active);
    CHECK(actual.version==selection.version&&actual.active_affinity==selection.active_affinity);
    CHECK(actual.has_selection==selection.has_selection);
    CHECK(r.app.editor.undoCount()==undo&&r.app.editor.redoCount()==redo);
    CHECK(r.app.layoutMode()==layout);
    ds::EditorView after;
    CHECK(r.app.editorPort.capture(&after)==ds::Status::Ok);
    CHECK(after.viewport.byte_offset==before.viewport.byte_offset);
    CHECK(after.viewport.affinity==before.viewport.affinity);
    CHECK(after.viewport.layout_revision==before.viewport.layout_revision);
    CHECK(r.transfer.imported().outcome==u::Outcome::Imported);
    // A not-yet-started save stays dirty; an already-started save completes.
    CHECK(r.app.persistence.bodyDirty()==(scenario!=2));
    CHECK(r.app.document.dirty()==(scenario!=2));
   }else{
    CHECK(r.app.session.currentId()==imported);
    CHECK(r.transfer.imported().outcome==u::Outcome::Opened);
    CHECK(r.app.document.text()=="hello\n世界\nnext\n");
   }
   if(scenario==2||scenario==3){
    const auto saved=r.app.persistence.loadLatest(original);
    CHECK(saved.status==ds::Status::Ok&&std::string_view(saved.snapshot.text)==originalText);
    CHECK(saved.snapshot.cursor_byte==cursor);
   }
   std::cout<<"PASS cancel_pending_open layout="<<vertical<<" case="<<names[scenario]
            <<" imported_retained=1 same_operation=1 original_saved="<<(scenario>=2)<<'\n';
  }
 }
}
int main(){try{flows();controls();openMemoryFailures();cancelPendingOpen();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
