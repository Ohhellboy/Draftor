#include "writing_app.hpp"
#include "hanime_document_sd_storage.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
namespace ds=hanime::documents;
#define CHECK(v) do { if(!(v)) throw std::runtime_error(#v); } while(0)
struct Fixture {
 std::string path; s3fs::Fs fs; ds::ArduinoSdStoragePort storage;
 s3host::BuiltinLatinFont font; s3app::App app; uint32_t now=100;
 static std::string temp(){char p[]="/tmp/hanime-v053-meta-review-XXXXXX";const auto r=mkdtemp(p);CHECK(r);return r;}
 explicit Fixture(std::string p=temp()):path(std::move(p)),fs(path),storage(fs),app(storage,{},1234,font){storage.noteMounted();}
 void start(){CHECK(app.startup(now));}
 void key(uint8_t usage,uint8_t mods=0){app.key({1,++now,usage,mods,true});}
 void settle(){unsigned n=0;while(app.commandBarrier()||app.persistence.backgroundSaveActive()){app.service(++now,false);CHECK(++n<12000);}}
 void save(){key(22,1);settle();CHECK(app.lastOperation);}
};
int main(){try{
 Fixture f;f.start();f.key(4);f.save();const auto old=f.app.session.currentId();
 f.key(17,1);unsigned guard=0;
 while(f.app.session.creating()&&std::string_view(f.app.session.createStage())!="publish"){
  f.app.service(++f.now,false);CHECK(++guard<2000);
 }
 CHECK(f.app.session.creating());
 s3fs::faults.write_bytes=ds::RecordStore::kHeaderBytes;
 f.settle();s3fs::faults={};
 std::printf("interrupted META after %zu header bytes: status=%s stage=%s old_current=%u old_body=%s\n",size_t(ds::RecordStore::kHeaderBytes),ds::statusMessage(f.app.lastOperation.status),f.app.lastOperation.stage,f.app.session.currentId()==old,f.app.document.text().c_str());
 for(unsigned i=0;i<3;++i){
  f.key(17,1);f.settle();
  std::printf("retry %u: status=%s stage=%s old_current=%u body=%s\n",i+1,ds::statusMessage(f.app.lastOperation.status),f.app.lastOperation.stage,f.app.session.currentId()==old,f.app.document.text().c_str());
 }
 Fixture reboot(f.path);reboot.start();reboot.key(17,1);reboot.settle();
 std::printf("after restart retry: status=%s stage=%s old_current=%u body=%s\n",ds::statusMessage(reboot.app.lastOperation.status),reboot.app.lastOperation.stage,reboot.app.session.currentId()==old,reboot.app.document.text().c_str());
 std::printf("old saved body=%s\n",reboot.app.persistence.loadLatest(old).snapshot.text.c_str());
 return reboot.app.lastOperation?0:2;
}catch(const std::exception& e){s3fs::faults={};std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
