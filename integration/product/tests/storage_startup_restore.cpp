// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ruotao Wang
#include "writing_app.hpp"
#include "usb_document_import.hpp"
#include "hanime_document_sd_storage.hpp"
#include "editor_hud.hpp"
#include "draftor_exchange/import_job.hpp"
#include "src/hanime_document/paths.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

namespace ds=hanime::documents; namespace ex=draftor::exchange;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x); } while(0)
static constexpr char text[]="R5 USB import/export confirmation.\n微雪正常产品收口确认。\n0123456789 abc XYZ\n";
struct Workspace {
 std::filesystem::path path;
 Workspace(){const char* tmp=std::getenv("TMPDIR");CHECK(tmp);std::string n=(std::filesystem::path(tmp)/"restore-XXXXXX").string();CHECK(mkdtemp(n.data()));path=n;}
 ~Workspace(){s3fs::faults={};s3fs::File::openObserver=nullptr;std::error_code e;std::filesystem::remove_all(path,e);}
};
struct Rig {
 s3fs::Fs fs;ds::ArduinoSdStoragePort storage;s3host::BuiltinSmallFont font;s3app::App app;s3app::UsbDocumentImport port;uint32_t now=0;
 explicit Rig(const std::filesystem::path& p):fs(p.string()),storage(fs),app(storage,{},37113,font),port(storage,app.catalog,app.persistence,app.session,app.editorPort){storage.noteMounted();}
 ds::OperationResult boot(){return app.startup(++now);}
 void save(){CHECK(app.requestSave(++now).status==ds::Status::Busy);for(unsigned i=0;i<20000&&(app.manualSavePending()||app.persistence.backgroundSaveActive());++i)app.service(++now,false);CHECK(!app.manualSavePending()&&!app.persistence.backgroundSaveActive());CHECK(app.lastOperation.status==ds::Status::Ok&&!app.document.dirty());}
};
struct Source final:ex::ImportSource {
 ex::SourceIdentity identity()const noexcept override{return {701,3,sizeof(text)-1};}
 uint64_t sourceLease()const noexcept override{return 701;}
 bool stable()const noexcept override{return true;}
 bool confirmed()const noexcept override{return true;}
 ex::IoResult read(uint64_t offset,void* out,size_t size)noexcept override{if(offset>sizeof(text)-1||size>sizeof(text)-1-offset)return {ex::Status::Bounds,0};std::memcpy(out,text+offset,size);return {ex::Status::Ok,size};}
};
static ds::DocumentId importAndOpen(Rig& r){
 CHECK(r.boot());Source source;ex::ImportIntent intent;CHECK(r.port.allocateOperation(intent.operation)==ex::Status::Ok);intent.source=source.identity();std::strcpy(intent.name.data(),"R5CHECK.TXT");
 ex::ImportJob job(r.port);CHECK(job.begin(source,intent)==ex::Status::Ok);
 for(unsigned i=0;i<30000&&job.phase()!=ex::ImportPhase::Complete;++i){job.tick();CHECK(job.phase()!=ex::ImportPhase::Failed&&job.phase()!=ex::ImportPhase::Uncertain);}
 CHECK(job.phase()==ex::ImportPhase::Complete);ds::DocumentId id;id.bytes=job.result().document;
 CHECK(r.app.enterTransfer());CHECK(r.app.openImported(id,++r.now).status==ds::Status::Ok);r.app.leaveTransfer();
 CHECK(r.app.document.text()==text&&!r.app.document.dirty()&&!r.app.persistence.bodyDirty());
 ds::CurrentRecord current;CHECK(r.app.catalog.loadCurrent(&current)==ds::Status::Ok&&current.has_current&&current.document_id==id);
 auto loaded=r.app.persistence.loadLatest(id);CHECK(loaded.status==ds::Status::Ok&&loaded.snapshot.text==text&&loaded.snapshot.generation==1&&loaded.snapshot.text_crc32==ds::crc32(text));
 CHECK(r.app.persistence.applyPosition(id,&loaded.snapshot)==ds::Status::NotFound);
 std::cout<<"IMPORT body=OK generation="<<loaded.snapshot.generation<<" crc="<<loaded.snapshot.text_crc32<<" CURRENT=OK position=NOT_FOUND dirty=0\n";
 return id;
}
static void report(const char* scenario,const ds::OperationResult& r){std::cout<<scenario<<" status="<<ds::statusMessage(r.status)<<" stage="<<r.stage<<'\n'<<std::flush;}
static void missingPosition(){
 Workspace w;ds::DocumentId id;{Rig r(w.path);id=importAndOpen(r);}
 Rig reboot(w.path);const auto result=reboot.boot();report("NO_EDIT_RESTART",result);
 CHECK(reboot.app.session.currentId()==id&&reboot.app.document.text()==text&&!reboot.app.document.dirty());
 CHECK(result.status==ds::Status::Ok&&std::string_view(result.stage)=="position_default");
 CHECK(reboot.app.document.cursorByte()==0);
 auto body=reboot.app.persistence.loadLatest(id);CHECK(reboot.app.persistence.applyPosition(id,&body.snapshot)==ds::Status::NotFound); // no automatic repair writes
}
static void manualSave(){
 Workspace w;ds::DocumentId id;uint64_t generation=0;
 {Rig r(w.path);id=importAndOpen(r);r.save();auto b=r.app.persistence.loadLatest(id);CHECK(b.status==ds::Status::Ok&&b.snapshot.text==text);generation=b.snapshot.generation;CHECK(generation==2);CHECK(r.app.persistence.applyPosition(id,&b.snapshot)==ds::Status::Ok);}
 Rig reboot(w.path);auto result=reboot.boot();report("UNEDITED_MANUAL_SAVE_RESTART",result);CHECK(result.status==ds::Status::Ok&&std::string_view(result.stage)=="complete");CHECK(reboot.app.session.currentId()==id&&reboot.app.document.text()==text&&!reboot.app.document.dirty());CHECK(reboot.app.persistence.loadLatest(id).snapshot.generation==generation);
}
static void positionCases(){
 for(unsigned scenario=0;scenario<6;++scenario){
  Workspace w;ds::DocumentId id;
  {Rig r(w.path);id=importAndOpen(r);auto b=r.app.persistence.loadLatest(id);
   if(scenario==0)CHECK(r.app.persistence.savePosition(id,b.snapshot.generation,b.snapshot.text_crc32,3,{0,ds::Affinity::Before,0})==ds::Status::Ok);
   if(scenario==1)CHECK(r.app.persistence.savePosition(id,b.snapshot.generation+1,b.snapshot.text_crc32,3,{})==ds::Status::Ok);
   if(scenario==2)CHECK(r.storage.write(ds::paths::position(id)+".A","broken record")==ds::Status::Ok);
   if(scenario==3){CHECK(r.app.persistence.savePosition(id,b.snapshot)==ds::Status::Ok);CHECK(r.storage.write(ds::paths::position(id)+".B","bad secondary")==ds::Status::Ok);}
   if(scenario==4){CHECK(r.app.persistence.savePosition(id,b.snapshot)==ds::Status::Ok);}
   if(scenario==5)CHECK(r.app.persistence.savePosition(id,b.snapshot.generation,b.snapshot.text_crc32,std::string_view(text).find("微")+1,{})==ds::Status::Ok);
  }
  Rig reboot(w.path);
  if(scenario==4)s3fs::File::openObserver=[](const char* p,const char*){if(std::strstr(p,"/POSITION"))s3fs::faults.read=true;};
  const auto result=reboot.boot();s3fs::faults={};s3fs::File::openObserver=nullptr;report("POSITION_CASE",result);
  CHECK(reboot.app.document.text()==text&&reboot.app.session.currentId()==id&&!reboot.app.document.dirty());
  if(scenario==0){CHECK(result.status==ds::Status::Ok&&reboot.app.document.cursorByte()==3);}
  else {CHECK(result.status==ds::Status::OkDegraded);CHECK(s3app::editor_hud::storageWarning(ds::statusMessage(result.status))!=nullptr);}
 }
}
static void bodyFailures(){
 for(bool io:{false,true}){
  Workspace w;ds::DocumentId id;{Rig r(w.path);id=importAndOpen(r);if(!io)CHECK(r.storage.write(ds::paths::lane(id,ds::SaveLane::Auto)+".A","bad CRC/body")==ds::Status::Ok);}
  Rig reboot(w.path);if(io)s3fs::File::openObserver=[](const char* p,const char*){if(std::strstr(p,"/AUTO"))s3fs::faults.read=true;};
  auto result=reboot.boot();s3fs::faults={};s3fs::File::openObserver=nullptr;report("BODY_FAILURE",result);CHECK(!result);CHECK(!reboot.app.session.hasCurrent());CHECK(s3app::editor_hud::storageWarning(ds::statusMessage(result.status))!=nullptr);
 }
}
static void degradedBody(){
 Workspace w;ds::DocumentId id;{Rig r(w.path);id=importAndOpen(r);CHECK(r.storage.write(ds::paths::lane(id,ds::SaveLane::Auto)+".B","bad secondary")==ds::Status::Ok);}
 Rig reboot(w.path);auto result=reboot.boot();report("DEGRADED_BODY",result);CHECK(result.status==ds::Status::OkDegraded&&std::string_view(result.stage)=="body_recovered");CHECK(reboot.app.document.text()==text);
}
int main(){try{missingPosition();manualSave();positionCases();bodyFailures();degradedBody();std::cout<<"PASS startup restore: optional missing position, save responsibility, valid/mismatched/corrupt/degraded/IO/UTF8 position and body failures\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
