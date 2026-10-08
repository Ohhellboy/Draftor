#include "device_ime.hpp"
#include "raw_viewport.hpp"
#include "loading_status.hpp"
#include "hanime/resource_publisher.hpp"
#include <thread>
#include <chrono>
#include "chinese_font.hpp"
#include "hanime_document_sd_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <unistd.h>
extern bool fail_next_new;
using namespace ime_device;
namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
namespace {
// All fixture names below are relative to one process-owned directory. A CTest
// peer or a second manual invocation cannot remove this process's fault files.
struct FixtureRun {
 fs::path parent=fs::current_path(),path;bool success=false;
 FixtureRun(){
  auto name=(parent/"adapter-tests-XXXXXX").string();const auto made=::mkdtemp(name.data());CHECK(made);path=made;fs::current_path(path);
  std::cout<<"FIXTURE_RUN pid="<<::getpid()<<" path="<<path.string()<<'\n';
 }
 ~FixtureRun(){
  std::error_code ec;fs::current_path(parent,ec);
  if(!ec&&success)fs::remove_all(path,ec);
  if(ec||!success)std::cerr<<"FIXTURE_RETAINED path="<<path.string()<<" cleanup="<<ec.message()<<'\n';
 }
};
void removeFaultFile(const fs::path& file){
 CHECK(fs::is_regular_file(file));CHECK(fs::remove(file));CHECK(!fs::exists(file));
 std::cout<<"FAULT_INJECT removed="<<file.string()<<'\n';
}
void xorFaultByte(const fs::path& file,std::size_t at,unsigned mask){
 CHECK(fs::is_regular_file(file));const auto length=fs::file_size(file);CHECK(at<length);
 std::fstream out(file,std::ios::in|std::ios::out|std::ios::binary);CHECK(out.is_open());
 char old=0;out.seekg(std::streamoff(at));CHECK(out.good());out.get(old);CHECK(out.good()&&out.gcount()==1);
 const auto changed=char(uint8_t(old)^mask);out.seekp(std::streamoff(at));CHECK(out.good());out.put(changed);CHECK(out.good());out.flush();CHECK(out.good());out.close();CHECK(!out.fail());
 std::ifstream verify(file,std::ios::binary);CHECK(verify.is_open());char actual=0;verify.seekg(std::streamoff(at));CHECK(verify.good());verify.get(actual);CHECK(verify.good()&&verify.gcount()==1&&actual==changed);verify.close();CHECK(!verify.fail());CHECK(fs::file_size(file)==length);
 std::cout<<"FAULT_INJECT flipped="<<file.string()<<" offset="<<at<<" before="<<unsigned(uint8_t(old))<<" after="<<unsigned(uint8_t(actual))<<" bytes="<<length<<'\n';
}
std::array<hanime::ResourceReleaseSpec,5> fixtureSpecs;
ReleasePins fixturePins(){return {&fixtureSpecs[0],&fixtureSpecs[1],&fixtureSpecs[2],&fixtureSpecs[3],&fixtureSpecs[4]};}
fs::path fixture(){
 auto path=fs::current_path()/"adapter-fixture";fs::remove_all(path);fs::create_directories(path);
 hanime::ResourceBuildInput input;input.registry_namespace=Resources::Namespace;input.source_name="device adapter test fixture";input.source_version="1";input.relation_basis="synthetic test only";
 auto add=[&](std::string text,std::string xiaohe,std::string pinyin){
  hanime::LexemeSourceRecord x;x.stable_key.bytes[31]=uint8_t(input.lexemes.size()+1);x.status=hanime::RegistryStatus::Approved;x.kind=hanime::LexemeKind::Word;x.identity_definition="fixture "+std::to_string(input.lexemes.size());x.provenance="test only";x.default_output_utf8=text;x.base_weight=1000-int(input.lexemes.size());input.lexemes.push_back(x);
  input.mappings.push_back({"xiaohe",xiaohe,x.stable_key,0});input.mappings.push_back({"full_pinyin",pinyin,x.stable_key,0});
 };
 add("完蛋","wjdj","wandan");add("了","le","le");add("玩","wj","wan");add("蛋","dj","dan");add("乐","le","le");add("我","wo","wo");add("觉得","jtde","juede");add("吧","ba","ba");add("写完","xpwj","xiewan");add("我的","wode","wode");
 for(unsigned i=0;i<48;++i)add(i%7==0?"较长词条":i%3==0?"同名":"字","yi","yi");
 auto result=hanime::ResourceBuilder().build(input,{256,256});CHECK(result.ok);
 auto put=[&](const char* name,const auto& bytes){std::ofstream f(path/name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));CHECK(f.good());f.flush();CHECK(f.good());f.close();CHECK(!f.fail());CHECK(fs::file_size(path/name)==bytes.size());};
 put("shared.hns",result.resources.shared_bundle);put("xiaohe.hnm",result.resources.scheme_bundles.at("xiaohe"));put("full_pinyin.hnm",result.resources.scheme_bundles.at("full_pinyin"));
 hanime::PublishVerifiedOptions options;options.shared_path=(path/"shared.hns").string();options.output_directory=(path/"verified").string();options.registry_namespace=Resources::Namespace;
 options.base_order=true;options.schemes={{"xiaohe",(path/"xiaohe.hnm").string()},{"full_pinyin",(path/"full_pinyin.hnm").string()}};
 const auto published=hanime::publish_verified_resources(options);CHECK(published.ok&&published.resources.size()==5);
 for(const auto& r:published.resources){const unsigned i=r.logical_name=="shared"?0:r.logical_name=="xiaohe"?1:r.logical_name=="full_pinyin"?2:r.logical_name=="xiaohe_base"?3:4;fixtureSpecs[i]=r.spec;const auto name=fs::path(r.index_file).filename();CHECK(fs::copy_file(path/"verified"/name,path/name));if(r.logical_name.find("_base")!=std::string::npos)CHECK(fs::copy_file(path/"verified"/r.source_file,path/r.source_file));}
 return path;
}
struct Rig {
 fs::path path;s3fs::Fs files;hanime::documents::ArduinoSdStoragePort storage;ChineseFont font;s3app::App app;Resources resources;DeviceIme ime;uint32_t now=100,frame=0;
 Rig(const fs::path& root,unsigned n,ReleasePins pins=fixturePins()):path(fs::current_path()/("adapter-doc-"+std::to_string(n))),files(path.string()),storage(files),app(storage,{},1234+n,font),resources(root.string(),std::make_shared<Mount>(),pins),ime(app,resources,font){fs::remove_all(path);fs::create_directories(path);CHECK(font.begin());storage.noteMounted();CHECK(app.startup(now));}
 void send(s3hid::Event e){if(!ime.event(e))app.key(e);}
 // Each call represents a fresh report in this synchronous fixture. Captured
 // events keep these identities when sent later; send() never re-stamps them.
 s3hid::Event freshEvent(uint8_t u,bool down,uint8_t mods=0){
  s3hid::Event e{1,++now,u,mods,down,0,frame,ime.attemptSerial()};
  e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();return e;
 }
 void tap(uint8_t u,uint8_t mods=0){for(bool down:{true,false})send(freshEvent(u,down,mods));}
 void type(std::string_view text){for(char c:text)tap(uint8_t(c-'a'+4));}
 void step(){ime.service();app.service(++now,false);}
 void settle(){
  for(size_t i=0;i<50000000;++i){step();const auto* w=ime.window();
   if(resources.state()==ResourceState::Error)throw std::runtime_error("unexpected resource error in "+path.string()+": "+resources.error());
   if(i>1&&resources.ready()&&(ime.composition()||!ime.chinese())&&(!w||!w->busy())&&!ime.hasWork()&&ime.receipt().status!=hanime::CompositionStatus::Preparing&&!app.persistence.backgroundSaveActive()&&!app.commandBarrier()){frame=ime.framePresented();return;}
  }throw std::runtime_error("settle exceeded: "+path.string()+" resource="+std::to_string(unsigned(resources.state()))+" window="+std::to_string(ime.window()?unsigned(ime.window()->state()):99)+" raw="+std::string(ime.raw())+" work="+std::to_string(ime.hasWork()));
 }
 void cn(){tap(0x39);settle();CHECK(ime.chinese());}
};
void loadingWork(Rig& r){
 for(unsigned i=0;i<32;++i){if(r.resources.state()==ResourceState::ValidateShared&&r.resources.stageWork()>0)return;r.step();}
 throw std::runtime_error("shared bootstrap did not yield measurable pending work");
}
void verifiedFaults(const fs::path& root){
 {
  Rig r(root,201);r.type("ab");const auto admissionIo=metrics.phases[unsigned(Phase::Admission)].io.bytes;
  r.cn();CHECK(metrics.phases[unsigned(Phase::Admission)].io.bytes==admissionIo);
  CHECK(r.resources.cacheBytes()==65536&&metrics.resource_handles==6&&!r.resources.sharedSnapshot().whole_file_checked);
  r.type("wo");r.settle();r.tap(30);r.settle();CHECK(r.app.document.text()=="ab我"&&r.ime.receipt().status==hanime::CompositionStatus::Accepted);
  // Arrivals captured before retirement but still waiting in the physical FIFO.
  const auto oldDown=r.freshEvent(6,true),oldUp=r.freshEvent(6,false);
  const auto oldChoice=r.freshEvent(30,true),oldChoiceUp=r.freshEvent(30,false);
  const auto acceptedRevision=r.app.document.textRevision();const auto acceptedUndo=r.app.editor.undoCount();
  const auto reads=metrics.requests;++r.resources.mount()->epoch;CHECK(!r.resources.shared.ready());CHECK(!r.resources.ready());CHECK(loadSnapshot(r.font,r.resources).stage==LoadStage::ResourceError);r.step();
  CHECK(r.resources.state()==ResourceState::Error&&!r.ime.chinese()&&!r.ime.composition()&&metrics.resource_handles==0&&metrics.requests==reads);
  CHECK(r.ime.receipt().status==hanime::CompositionStatus::Accepted&&r.app.document.text()=="ab我");
  CHECK(oldDown.resource_owner!=r.ime.resourceOwner());
  r.send(oldDown);r.send(oldDown);r.send(oldUp);r.send(oldChoice);r.send(oldChoiceUp);
  CHECK(r.app.document.text()=="ab我"&&r.app.document.textRevision()==acceptedRevision&&r.app.editor.undoCount()==acceptedUndo);
  CHECK(!r.ime.composition()&&!r.ime.window()&&!r.ime.candidateAvailable()&&r.ime.receipt().status==hanime::CompositionStatus::Accepted);
  std::cout<<"PASS stale FIFO owner="<<oldDown.resource_owner<<" current="<<r.ime.resourceOwner()<<" EN_penetration=0 candidate_authority=0 duplicate_commit=0 release_via_event=1\n";
  const auto newDown=r.freshEvent(6,true);CHECK(newDown.resource_owner==r.ime.resourceOwner());
  std::cout<<"OWNER event="<<newDown.resource_owner<<" current="<<r.ime.resourceOwner()<<" body_before="<<r.app.document.text()<<'\n';
  r.send(newDown);r.send(r.freshEvent(6,false));
  std::cout<<"AFTER_INPUT body="<<r.app.document.text()<<" dirty="<<r.app.document.dirty()<<'\n';
  CHECK(r.app.document.text()=="ab我c"&&r.app.editor.undoCount()==acceptedUndo+1&&r.app.document.textRevision()==acceptedRevision+1);
  r.now+=100000;for(unsigned i=0;i<10000&&r.app.document.dirty();++i)r.step();
  std::cout<<"AFTER_WAIT body="<<r.app.document.text()<<" text_ok="<<(r.app.document.text()=="ab我c")<<" dirty="<<r.app.document.dirty()<<'\n';
  CHECK(r.app.document.text()=="ab我c"&&!r.app.document.dirty());
  r.tap(0x39);r.step();CHECK(!r.ime.chinese()&&r.resources.state()==ResourceState::Error&&metrics.resource_handles==0); // Latched; no silent full-scan/retry.
 }
 {
  const auto damaged=fs::current_path()/"auth-unvisited-data";fs::remove_all(damaged);fs::copy(root,damaged,fs::copy_options::recursive);
  const auto file=damaged/"shared.hns";std::ifstream in(file,std::ios::binary);CHECK(in.is_open());std::string bytes((std::istreambuf_iterator<char>(in)),{});CHECK(!in.bad()&&bytes.size()==fs::file_size(file));const auto at=bytes.find("我");CHECK(at!=std::string::npos&&at>=1024);in.close();
  xorFaultByte(file,at,0x10);
  Rig r(damaged,202);r.type("ab");r.cn();CHECK(!r.resources.sharedSnapshot().whole_file_checked);r.type("wo");
  for(unsigned i=0;i<20000&&r.resources.state()!=ResourceState::Error;++i){r.step();}
  r.step();
  CHECK(r.resources.state()==ResourceState::Error&&!r.ime.chinese()&&!r.ime.active()&&r.app.document.text()=="ab"&&metrics.resource_handles==0);
  CHECK(r.resources.error().find("shared.hns/.hnv authentication")!=std::string::npos);fs::remove_all(damaged);
 }
 {
  const auto damaged=fs::current_path()/"auth-bad-proof";fs::remove_all(damaged);fs::copy(root,damaged,fs::copy_options::recursive);
  xorFaultByte(damaged/"shared.hnv",0,1);
  Rig r(damaged,203);r.type("ab");r.tap(0x39);r.type("wo");for(unsigned i=0;i<20000&&r.resources.state()!=ResourceState::Error;++i){r.step();}
  r.step();
  CHECK(r.resources.state()==ResourceState::Error&&!r.ime.chinese()&&r.app.document.text()=="ab"&&metrics.resource_handles==0);fs::remove_all(damaged);
 }
 {
  auto incompatible=fixtureSpecs;incompatible[0].text_capacity=65;const ReleasePins pins{&incompatible[0],&incompatible[1],&incompatible[2]};
  Rig r(root,204,pins);r.tap(0x39);for(unsigned i=0;i<20000&&r.resources.state()!=ResourceState::Error;++i){r.step();}
  r.step();
  CHECK(r.resources.state()==ResourceState::Error&&r.resources.error().find("certificate")!=std::string::npos&&!r.ime.chinese());
 }
 {
  const auto partial=fs::current_path()/"auth-current-scheme-only";fs::remove_all(partial);fs::copy(root,partial,fs::copy_options::recursive);removeFaultFile(partial/"full_pinyin.hnv");
  Rig r(partial,205);r.type("ab");r.cn();r.type("wo");r.settle();CHECK(r.ime.window()->state()==WindowState::Ready);const auto sharedWork=metrics.phases[unsigned(Phase::SharedValidation)].steps;
  r.tap(0x2c,1);CHECK(r.ime.raw().empty());for(unsigned i=0;i<20000&&r.resources.state()!=ResourceState::Error;++i){r.step();}
  r.step();
  CHECK(metrics.phases[unsigned(Phase::SharedValidation)].steps==sharedWork&&r.resources.error().find("full_pinyin.hnv")!=std::string::npos);
  CHECK(!r.ime.chinese()&&r.app.document.text()=="ab"&&metrics.resource_handles==0);fs::remove_all(partial);
 }
 for(const char* file:{"xiaohe_base.hbo","xiaohe_base.hnv"}){
  const auto damaged=fs::current_path()/(std::string("order-fault-")+file);fs::remove_all(damaged);fs::copy(root,damaged,fs::copy_options::recursive);
  if(std::string(file).find(".hbo")!=std::string::npos)removeFaultFile(damaged/file);
  else xorFaultByte(damaged/file,0,1);
  Rig r(damaged,206);r.type("ab");r.tap(0x39);unsigned steps=0;for(;steps<20000&&r.resources.state()!=ResourceState::Error;++steps)r.step();r.step();
  std::cout<<"FAULT_EXPECT file="<<file<<" steps="<<steps<<" state="<<unsigned(r.resources.state())<<" error="<<r.resources.error()<<" resource_handles="<<metrics.resource_handles<<" document_handles="<<metrics.document_handles<<" body_ab="<<(r.app.document.text()=="ab")<<" file_exists="<<fs::exists(damaged/file)<<" epoch="<<r.resources.mount()->epoch<<" mounted="<<r.resources.mount()->available<<" fail_next_new="<<fail_next_new<<'\n';
  CHECK(r.resources.state()==ResourceState::Error&&r.resources.error().find("xiaohe_base")!=std::string::npos&&r.app.document.text()=="ab"&&metrics.resource_handles==0);
  r.type("c");r.now+=100000;for(unsigned n=0;n<10000&&r.app.document.dirty();++n)r.step();CHECK(r.app.document.text()=="abc"&&!r.app.document.dirty());fs::remove_all(damaged);
 }
 {
  Rig r(root,207);r.cn();CHECK(r.resources.optimized()&&Resources::MaximumOpenFiles==9&&metrics.resource_handles==6);
  const auto sharedOwner=r.resources.sharedCacheAllocation();const auto work=metrics.phases[unsigned(Phase::SharedValidation)].steps;
  r.tap(0x2c,1);CHECK(metrics.resource_handles==2);r.settle();CHECK(r.resources.optimized()&&metrics.resource_handles==6&&r.resources.sharedCacheAllocation()==sharedOwner&&metrics.phases[unsigned(Phase::SharedValidation)].steps==work);
 }
 std::cout<<"PASS required order missing/proof fault fail explicitly with English saving; switch peak six resource plus three document bound, shared owner reused\n";
 std::cout<<"PASS verified activation: certificate zero I/O, active scheme only, 64KiB bounded authentication cache, first unvisited corrupt text/proof rejected, incompatible qualification, cache-hit epoch guard without I/O, Accepted survives error and English save, latched failure without replay/full scan\n";
}
void loadingEmptyRegression(const fs::path& root){
 Rig r(root,101);r.type("ab");r.tap(0x39);r.type("a");
 CHECK(!r.ime.composition()&&r.ime.raw()=="a"&&r.app.document.text()=="ab");
 r.tap(0x2a);std::cout<<"after raw emptied: "<<r.ime.notice()<<" body="<<r.app.document.text()<<'\n';
 CHECK(r.ime.raw().empty());r.tap(0x2a);CHECK(r.app.document.text()=="a");r.step();
 std::cout<<"after fresh body Backspace + service: "<<r.ime.notice()<<'\n';
 CHECK(std::string(r.ime.notice())!="Input target expired");
 std::cout<<"PASS loading empty releases old target before body Backspace\n";
}
void loadingLifecycle(const fs::path& root){
 {
  Rig r(root,102);r.type("ab");r.tap(0x39);r.type("a");loadingWork(r);
  CHECK(r.resources.state()==ResourceState::ValidateShared&&!r.ime.composition());
  const auto sharedOwner=r.resources.sharedCacheAllocation();const auto openSteps=metrics.phases[unsigned(Phase::Open)].steps;
  auto down=s3hid::Event{1,++r.now,0x2a,0,true,0,r.frame,0,0,0,r.ime.ingressContext(),r.ime.resourceOwner()};r.send(down);
  CHECK(r.ime.raw().empty()&&!r.ime.active()&&r.app.document.text()=="ab");
  r.send(down);auto held=down;held.ms=++r.now;r.send(held);CHECK(r.app.document.text()=="ab");
  r.send({1,++r.now,0x2a,0,false,0,r.frame,0,0,0,r.ime.ingressContext(),r.ime.resourceOwner()});r.send(down);CHECK(r.app.document.text()=="ab");
  r.tap(0x50);CHECK(r.app.document.cursorByte()==1);r.step();
  CHECK(std::string(r.ime.notice())!="Input target expired");
  r.type("wo");r.settle();CHECK(r.ime.raw()=="wo"&&r.ime.window()->state()==WindowState::Ready);
  CHECK(r.resources.sharedCacheAllocation()==sharedOwner&&metrics.phases[unsigned(Phase::Open)].steps==openSteps+7);
  r.tap(30);r.settle();CHECK(r.app.document.text()=="a我b");
 }
 {
  Rig r(root,103);r.type("ab");r.tap(0x39);r.type("a");r.tap(0x2a);r.type("wo");r.settle();
  CHECK(r.ime.raw()=="wo"&&r.app.document.text()=="ab"&&r.ime.window()->state()==WindowState::Ready);
 }
 {
  Rig r(root,104);r.type("ab");r.tap(0x39);r.type("wo");
  const auto stamp=r.app.editStamp();const auto session=r.app.session.sessionSerial();
  r.now+=100000;r.app.service(r.now,false);CHECK(r.app.persistence.backgroundSaveActive()&&!r.app.commandBarrier());
  for(unsigned i=0;i<10000&&r.app.persistence.backgroundSaveActive();++i){r.step();CHECK(r.ime.raw()=="wo"&&std::string(r.ime.notice())!="Input target expired");}
  CHECK(!r.app.document.dirty()&&r.app.session.sessionSerial()==session&&r.app.editStamp().revision==stamp.revision&&r.app.editStamp().focus==stamp.focus);
  r.settle();CHECK(r.ime.window()->state()==WindowState::Ready&&r.ime.raw()=="wo");
 }
 for(unsigned event=0;event<3;++event){
  Rig r(root,105+event);r.type("ab");r.tap(0x39);r.type("wo");
  if(event==0)r.app.key({1,++r.now,0x50,0,true}); // Bypass IME to model external cursor change.
  else if(event==1)r.app.disconnected(); // Focus/interrupt generation.
  else {r.app.key({1,++r.now,0x11,1,true});} // External Ctrl+N / actual document command.
  r.ime.service();CHECK(!r.ime.active()&&r.ime.raw().empty()&&std::string(r.ime.notice())=="Input target expired");
  CHECK(r.app.document.text().find("我")==std::string::npos);
 }
 {
  Rig r(root,108);r.tap(0x39);r.type("wo");loadingWork(r);const auto work=r.resources.stageWork();const auto open=metrics.phases[unsigned(Phase::Open)].steps;
  CHECK(r.resources.state()==ResourceState::ValidateShared&&work>0);r.tap(0x2c,1);
  CHECK(r.ime.raw().empty()&&r.resources.rules()==hanime_extension::SchemeRules::FullPinyin&&r.resources.stageWork()==work);
  r.type("wan");r.settle();std::cout<<"LOADING_SWITCH raw="<<r.ime.raw()<<" state="<<unsigned(r.ime.window()->state())<<" expected_state="<<unsigned(WindowState::Ready)<<" open_delta="<<metrics.phases[unsigned(Phase::Open)].steps-open<<" expected_open_delta=7 owner="<<r.ime.resourceOwner()<<std::endl;CHECK(r.ime.raw()=="wan"&&r.ime.window()->state()==WindowState::Ready&&metrics.phases[unsigned(Phase::Open)].steps==open+7);
 }
 {
  Rig r(root,109);r.cn();r.type("a");r.tap(0x2a);CHECK(!r.ime.active());r.tap(0x50);r.step();CHECK(std::string(r.ime.notice())!="Input target expired");
 }
 {
  Rig r(root,110);r.tap(0x39);loadingWork(r);const auto work=r.resources.stageWork();
  const auto opens=metrics.phases[unsigned(Phase::Open)].steps;r.tap(0x39);CHECK(!r.ime.chinese());
  r.type("ab");r.step();CHECK(r.resources.stageWork()>work&&r.app.document.text()=="ab");
  char mode[40];modeCaption(mode,sizeof(mode),r.ime.chinese(),r.resources.rules(),r.app.document.dirty());CHECK(std::string(mode).find("EN | ")==0);
  r.settle();CHECK(r.resources.ready()&&!r.ime.composition()&&metrics.phases[unsigned(Phase::Open)].steps==opens+7);
  r.tap(0x39);r.type("wo");r.settle();CHECK(r.ime.window()->state()==WindowState::Ready&&r.app.document.text()=="ab");
 }
 {
  Rig r(root/"missing-resources",111);r.type("ab");r.tap(0x39);r.type("wo");r.step();
  CHECK(r.resources.state()==ResourceState::Error&&!r.ime.chinese()&&r.ime.raw().empty()&&r.app.document.text()=="ab");
  r.type("c");r.now+=100000;for(unsigned i=0;i<10000&&r.app.document.dirty();++i)r.step();
  CHECK(r.app.document.text()=="abc"&&!r.app.document.dirty());
 }
 std::cout<<"PASS loading lifecycle: held/replayed/release isolation, fresh body cursor, new-target commit, immediate retype/auto query, autosave target preserved, actual cursor/focus/document invalidation, scheme switch shares validation, ready-empty parity\n";
}
void loadingFeedback(const fs::path& root){
 ChineseFont font;Resources resources(root.string(),std::make_shared<Mount>(),fixturePins());CHECK(font.start(std::string(ASSET_ROOT)+"/matched28.hbf1",resources.mount()));
 auto snap=loadSnapshot(font,resources);CHECK(snap.stage==LoadStage::FontRead&&snap.done==0&&snap.total==ChineseFont::Bytes);
 LoadingStatus ui;CHECK(ui.update(snap,100));font.service();snap=loadSnapshot(font,resources);CHECK(snap.done==ChineseFont::ReadBytes&&!ui.update(snap,599)&&ui.update(snap,600));
 while(font.loadedBytes()<ChineseFont::Bytes){font.service();}snap=loadSnapshot(font,resources);CHECK(snap.stage==LoadStage::FontIndex&&snap.total==ChineseFont::Glyphs&&ui.update(snap,601));
 while(!font.ready()){font.service();}resources.start(hanime_extension::SchemeRules::R1Xiaohe);CHECK(loadSnapshot(font,resources).stage==LoadStage::SharedOpen);
 resources.service();resources.service();resources.service();snap=loadSnapshot(font,resources);CHECK(snap.stage==LoadStage::SharedCheck&&snap.total==0&&snap.done>0);
 CHECK(ui.update(snap,602));auto later=snap;later.done+=1;CHECK(!ui.update(later,603)&&ui.update(later,1102));
 bool admission=false,scheme=false;
 for(size_t i=0;i<1000000&&!resources.ready();++i){resources.service();const auto v=loadSnapshot(font,resources);admission|=v.stage==LoadStage::TextAdmission;scheme|=v.stage==LoadStage::SchemeCheck;CHECK(resources.state()!=ResourceState::Error);}
 CHECK(admission&&scheme&&loadSnapshot(font,resources).stage==LoadStage::Ready);
 Resources missing((root/"absent").string());missing.start(hanime_extension::SchemeRules::R1Xiaohe);missing.service();missing.service();CHECK(loadSnapshot(font,missing).stage==LoadStage::ResourceError);
 std::cout<<"PASS independent loading feedback: font byte/CRC then index totals, shared work without false percent, text and scheme stages, Ready/error, progress <=2Hz with immediate transitions\n";
}
void cachedReads(const fs::path& root){
 const auto path=root/"cache.bin";std::array<char,20011> expected{};for(size_t i=0;i<expected.size();++i)expected[i]=char((i*37+19)%251);
 {std::ofstream f(path,std::ios::binary);f.write(expected.data(),expected.size());}
 auto mount=std::make_shared<Mount>();const auto live=metrics.cache_live_bytes;
 {
  TfReader cached(mount,path.string(),TfReader::CacheLines);CHECK(cached.cacheBytes()==16384);std::array<char,1024> bytes{};
  for(size_t i=0;i<expected.size();i+=137){const auto n=std::min(size_t(73),expected.size()-i);CHECK(cached.read(i,bytes.data(),n));CHECK(std::equal(bytes.begin(),bytes.begin()+n,expected.begin()+i));}
  const auto before=cached.io();CHECK(cached.read(19870,bytes.data(),1));CHECK(cached.io().hits==before.hits+1);
  CHECK(!cached.read(20010,bytes.data(),2)&&!cached.read(0,bytes.data(),1025)&&!cached.readBlock(0,bytes.data(),1));++mount->epoch;CHECK(!cached.read(19870,bytes.data(),1));
 }
 CHECK(metrics.cache_live_bytes==live);
 {
  TfReader blocks(mount,path.string());std::array<char,4096> bytes{};
  CHECK(blocks.readBlock(17,bytes.data(),bytes.size()));CHECK(std::equal(bytes.begin(),bytes.end(),expected.begin()+17));
  const auto before=blocks.io();CHECK(!blocks.read(17,bytes.data(),4096)&&!blocks.readBlock(0,bytes.data(),4097));
  CHECK(!blocks.readBlock(blocks.size(),bytes.data(),1)&&!blocks.readBlock(0,nullptr,1));
  CHECK(blocks.readBlock(blocks.size(),nullptr,0)&&blocks.io().requests==before.requests);
  CHECK(blocks.readBlock(expected.size()-73,bytes.data(),73));CHECK(std::equal(bytes.begin(),bytes.begin()+73,expected.end()-73));
  ++mount->epoch;CHECK(!blocks.readBlock(17,bytes.data(),bytes.size()));
 }
 {
  draftor::memory::allocation_fail_after=0;TfReader fallback(mount,path.string(),16);draftor::memory::allocation_fail_after=-1;
  CHECK(fallback.valid()&&fallback.cacheBytes()==0);char c;CHECK(fallback.read(77,&c,1)&&c==expected[77]);
 }
 {
  TfReader shortRead(mount,path.string(),16);{std::ofstream f(path);f<<"a";}char c='?';CHECK(!shortRead.read(0,&c,1));CHECK(shortRead.io().hits==0);CHECK(!shortRead.read(0,&c,1));
 }
 CHECK(metrics.cache_live_bytes==live);
 std::cout<<"PASS bounded cache exact bytes across eviction/EOF, epoch before cache hit, short-fill refusal, allocation-failure uncached fallback, owner memory released\n";
}
void allocationFailure(const fs::path& root){
 Rig r(root,99);r.resources.start(hanime_extension::SchemeRules::R1Xiaohe);while(!r.resources.ready())r.resources.service();r.tap(0x39);CHECK(r.ime.chinese());fail_next_new=true;r.step();CHECK(!fail_next_new&&!r.ime.chinese()&&!r.ime.composition());r.step();r.type("abc");CHECK(r.app.document.text()=="abc");std::cout<<"PASS failed IME owner allocation leaves working English model and no partial owner\n";
}
void fontChecks(const fs::path& root){
 ChineseFont font;CHECK(font.begin());const auto reads=metrics.reads;
 for(auto cp:{0x6211U,0x4f23U,0x9f5aU,0x28fcdU}){auto g=font.glyph(cp);CHECK(!g.missing&&g.bits&&g.width==28);}
 CHECK(font.glyph(0x10ffff).missing);CHECK(metrics.reads==reads);
 const auto broken=root/"broken-font.hbf1";fs::copy_file(fs::path(ASSET_ROOT)/"matched28.hbf1",broken,fs::copy_options::overwrite_existing);
 {std::fstream f(broken,std::ios::in|std::ios::out|std::ios::binary);f.seekp(1000);f.put('x');}
 ChineseFont bad;CHECK(bad.start(broken.string(),std::make_shared<Mount>()));for(size_t i=0;!bad.failed()&&!bad.ready()&&i<10000;++i)bad.service();CHECK(bad.failed()&&!bad.allocation());
 std::cout<<"PASS font default delta, supplementary scalar, explicit missing box, no glyph I/O, corrupt whole-font CRC rejected\n";
}
void readerTests(const fs::path& root){
 auto mount=std::make_shared<Mount>();TfReader reader(mount,(root/"shared.hns").string());CHECK(reader.valid());char out[1024];CHECK(reader.read(0,out,1024));CHECK(!reader.read(0,out,1025));CHECK(!reader.read(reader.size(),out,1));CHECK(reader.read(reader.size(),nullptr,0));CHECK(!reader.read(0,nullptr,1));++mount->epoch;CHECK(!reader.read(0,out,1));
 const auto tmp=root/"short.bin";{std::ofstream f(tmp);f<<"12345678";}TfReader shortReader(mount,tmp.string());{std::ofstream f(tmp);f<<"1";}CHECK(!shortReader.read(0,out,8));
 std::cout<<"PASS precise bounds, short read, null buffer, original mount epoch\n";
}
void controls(const fs::path& root){
 Rig r(root,1);r.cn();r.type("wjdjle");r.settle();CHECK(r.app.document.text().empty());
 r.tap(0x50);CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==4);
 r.tap(0x50);CHECK(r.ime.rawCursor()==2);r.tap(0x50);CHECK(r.ime.rawCursor()==0);r.tap(0x50);CHECK(r.ime.rawCursor()==0);
 r.tap(0x4f);CHECK(r.ime.rawCursor()==2);r.tap(0x4f);CHECK(r.ime.rawCursor()==4);r.tap(0x4f);CHECK(r.ime.rawCursor()==6);r.tap(0x4f);CHECK(r.ime.rawCursor()==6);
 r.tap(0x50);r.tap(0x2a);CHECK(r.ime.raw()=="wjdle"&&r.ime.rawCursor()==3);r.tap(0x4c);CHECK(r.ime.raw()=="wjde");
 // Invalid manual endpoint keeps the automatic range and the editing cursor.
 r.tap(0x51);r.settle();CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==3&&r.ime.raw()=="wjde");
 r.tap(0x29);r.tap(0x29);CHECK(!r.ime.active()); // Raw Esc returns; ordinary candidate Esc cancels.
 r.type("zzzz");r.settle();CHECK(r.ime.window()->state()==WindowState::NoResults&&r.ime.raw()=="zzzz");
 for(auto u:{uint8_t(22),uint8_t(9),uint8_t(18)})r.tap(u,1);
 r.tap(0x3b);r.tap(0x3c);r.tap(0x39);CHECK(!r.app.filesOpen()&&!r.app.findOpen()&&!r.app.statsOpen()&&r.ime.chinese()&&r.ime.raw()=="zzzz");
 auto admitted=metrics.phases[unsigned(Phase::Admission)].steps;r.tap(0x2c,1);CHECK(r.ime.raw().empty());r.settle();CHECK(r.resources.rules()==hanime_extension::SchemeRules::FullPinyin);CHECK(metrics.phases[unsigned(Phase::Admission)].steps==admitted);CHECK(metrics.resource_handles==6);
 r.type("wandanle");r.settle();r.tap(0x50);CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==6);
 r.tap(0x29); // Return to candidates before the existing cancellation path.
 r.tap(0x29);r.tap(0x2c,1);r.settle();r.type("wjdjle");r.settle();r.tap(0x2c);r.settle();CHECK(r.ime.selected()=="完蛋");CHECK(r.app.document.text().empty());
 auto down=s3hid::Event{1,++r.now,0x2c,0,true,0,r.frame,0,0,0,r.ime.ingressContext(),r.ime.resourceOwner()};r.send(down);r.settle();CHECK(r.ime.raw().empty());CHECK(r.app.document.text()=="完蛋了");
 r.send(down);down.ms=++r.now;r.send(down);r.settle();CHECK(r.app.document.text()=="完蛋了");
 r.send({1,++r.now,0x2c,0,false,0,r.frame,0,0,0,r.ime.ingressContext(),r.ime.resourceOwner()});r.settle();CHECK(r.app.document.text()=="完蛋了"&&r.ime.raw().empty());r.send(down);CHECK(r.app.document.text()=="完蛋了");
 r.tap(0x39);CHECK(!r.ime.chinese());r.type("abc");CHECK(r.app.document.text()=="完蛋了abc");r.tap(22,1);r.settle();CHECK(!r.app.document.dirty());
 const auto savedId=r.app.session.currentId();
 {s3fs::Fs files(r.path.string());hanime::documents::ArduinoSdStoragePort st(files);st.noteMounted();s3app::App reopened(st,{},456,r.font);CHECK(reopened.startup(++r.now));CHECK(reopened.session.currentId()==savedId&&reopened.document.text()=="完蛋了abc");}
 r.tap(0x2c,1);r.settle();CHECK(!r.ime.chinese()&&r.app.document.text()=="完蛋了abc");
 std::cout<<"PASS syllable cursor, one-letter delete, Down, no-result retention, function blocking, switch discard, shared admission reuse, last-selection release barrier, one commit, Chinese save/reopen, English writing\n";
}
// Public window selection constructs retained-member fixtures for reopen tests.
// Real digit/Space events now share final commit; see space_commit.
void k1RawDown(const fs::path& root){
 {
  Rig r(root,81);r.cn();r.type("wjdjle");
  r.tap(0x50);r.tap(0x50);CHECK(r.ime.rawCursor()==2);
  r.tap(0x51);r.settle();
  const auto page=r.ime.composition()->page();
  CHECK(page.begin==0&&page.end==2&&!r.ime.rawEditing());
  CHECK(r.ime.view()->row==0&&r.ime.view()->col==0);
  CHECK(r.ime.raw()=="wjdjle"&&r.ime.composition()->snapshot().member_count==0&&r.app.document.text().empty());
  r.ime.window()->choose(0,r.frame);r.settle();CHECK(r.ime.selected()=="玩");
  r.ime.window()->choose(0,r.frame);r.settle();r.ime.window()->choose(0,r.frame);r.settle();
  CHECK(r.ime.selected()=="玩蛋了"&&r.app.document.text().empty());
 }
 for(bool start:{false,true}){
  Rig r(root,start?83:82);r.cn();r.type("wjdjle");r.settle();r.ime.window()->choose(0,r.frame);r.settle();
  CHECK(r.ime.selected()=="完蛋");r.tap(0x50);CHECK(r.ime.rawCursor()==6);r.tap(0x50);r.tap(0x50);if(start){r.tap(0x50);}
  CHECK(r.ime.rawCursor()==(start?0U:2U));r.tap(0x51);r.settle();
  auto snap=r.ime.composition()->snapshot();const auto page=r.ime.composition()->page();
  CHECK(!snap.members[0].valid&&page.begin==0&&page.end==(start?4U:2U)&&r.ime.raw()=="wjdjle");
  CHECK(r.ime.view()->row==0&&r.ime.view()->col==0&&r.app.document.text().empty());
  if(!start){r.ime.window()->choose(0,r.frame);r.settle();CHECK(r.ime.selected()=="玩"&&r.ime.raw()=="wjdjle"&&r.app.document.text().empty());}
 }
 {
  Rig r(root,84);r.cn();r.type("wjdjle");r.settle();r.ime.window()->choose(0,r.frame);r.settle();r.ime.window()->choose(0,r.frame);r.settle();
  r.tap(0x50);CHECK(r.ime.rawCursor()==4);r.tap(0x51);r.settle();
  auto snap=r.ime.composition()->snapshot();auto page=r.ime.composition()->page();
  CHECK(snap.members[0].valid&&!snap.members[1].valid&&page.begin==4&&page.end==6);
  CHECK(r.ime.view()->row==0&&r.ime.view()->col==0&&r.app.document.text().empty());
 }
 {
  Rig r(root,85);r.cn();r.type("wjdjle");r.settle();r.tap(0x50);r.tap(0x50);
  const auto oldEnd=r.ime.composition()->page().end;r.tap(0x28);r.settle();
  CHECK(r.ime.rawEditing()&&r.ime.composition()->page().end==oldEnd&&r.app.document.text().empty());
  r.tap(0x50);r.tap(0x51);CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==0&&r.ime.raw()=="wjdjle");
 }
 std::cout<<"PASS K1 raw Down: unselected explicit prefix, selected interior/start and right-hand boundary reopen, first item focus, no auto-selection/body commit, Enter excluded, zero-prefix retained; initial Down separately verified\n";
}
void fullPinyinNavigation(const fs::path& root){
 Rig r(root,90);r.cn();r.tap(0x2c,1);r.settle();r.type("wandanle");r.settle();
 const auto reads=metrics.reads;
 r.tap(0x50);CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==6);
 const auto analyzed=metrics.phases[unsigned(Phase::Navigation)].steps;
 for(auto pos:{3U,0U,0U}){r.tap(0x50);CHECK(r.ime.rawCursor()==pos);}
 for(auto pos:{3U,6U,8U,8U}){r.tap(0x4f);CHECK(r.ime.rawCursor()==pos);}
 CHECK(metrics.reads==reads&&metrics.phases[unsigned(Phase::Navigation)].steps==analyzed);
 r.tap(0x50);r.tap(0x2a);CHECK(r.ime.raw()=="wandale"&&r.ime.rawCursor()==5);
 r.tap(0x4c);CHECK(r.ime.raw()=="wandae"&&r.ime.rawCursor()==5);
 CHECK(metrics.phases[unsigned(Phase::Navigation)].steps==analyzed+2);
 r.tap(0x29);r.tap(0x29);r.type("xian");r.settle();r.tap(0x50);
 CHECK(r.ime.rawCursor()==0&&std::string(r.ime.notice()).find("ambiguous")!=std::string::npos);
 r.tap(0x4f);CHECK(r.ime.rawCursor()==4);
 r.tap(0x29);r.tap(0x29);r.type("zzzz");r.settle();r.tap(0x50);CHECK(r.ime.rawEditing());
 r.tap(0x2a);CHECK(r.ime.raw().size()==3);r.tap(0x4f);CHECK(r.ime.rawCursor()<=r.ime.raw().size());
 r.tap(0x29);r.tap(0x29);
 r.type("wandanle");r.settle();r.ime.window()->choose(0,r.frame);r.settle();r.ime.window()->choose(0,r.frame);r.settle();
 CHECK(r.ime.selected()=="完蛋了"&&r.app.document.text().empty());
 const auto suffix_key=r.ime.composition()->snapshot().members[1].key;
 r.tap(0x50);r.tap(0x50);CHECK(r.ime.rawCursor()==3);r.tap(0x51);r.settle();
 const auto snap=r.ime.composition()->snapshot();
 CHECK(r.ime.composition()->page().end==3&&!snap.members[0].valid);
 CHECK(r.ime.view()->row==0&&r.ime.view()->col==0&&r.ime.raw()=="wandanle"&&r.app.document.text().empty());
 r.ime.window()->choose(0,r.frame);r.settle();CHECK(r.ime.selected()=="玩了");r.ime.window()->choose(0,r.frame);r.settle();
 if(r.ime.composition()->snapshot().state!=hanime_extension::CompositionState::AllSelected){r.ime.window()->choose(0,r.frame);r.settle();}
 CHECK(r.ime.selected()=="玩蛋了"&&r.app.document.text().empty());
 const auto final=r.ime.composition()->snapshot();
 CHECK(final.state==hanime_extension::CompositionState::AllSelected&&final.member_count==3&&final.members[2].valid&&final.members[2].key==suffix_key);
 r.tap(0x28);r.settle();CHECK(r.app.document.text()=="玩蛋了");
 const auto m=r.ime.navigator()->memory_usage();
 std::cout<<"PASS F1 device full pinyin 0/3/6/8, one-letter edits, ambiguous/unknown retained, navigation-only zero reads, K1 short prefix + later segment + explicit whole commit; HOST nav owner="<<m.owner<<" impl="<<m.impl<<" workspace="<<m.workspace<<" spans="<<m.spans<<" boundaries="<<m.boundaries<<" const_table="<<m.constant_table<<'\n';
}
void visualFixes(const fs::path& root){
 s3host::BuiltinSmallFont small;
 for(char ch:{'w','i','j','a'})for(size_t n=0;n<=64;++n)for(size_t cursor=0;cursor<=n;++cursor){
  const auto v=rawViewport(std::string(n,ch),cursor,true,small);
  CHECK(v.begin<=cursor&&cursor<=v.end&&v.end<=n);
  CHECK(v.cursor_x<=v.width&&v.width<284);
 }
 // Original AD2 long-raw leftward edit keeps the cursor within the pixel strip.
 CHECK(rawViewport(std::string(64,'a'),24,true,small).begin<=24);
 Rig r(root,91);r.cn();r.type(std::string(64,'a'));r.settle();r.tap(0x52);
 CHECK(r.ime.boundaryVisible(uint32_t(clockUs()/1000))&&r.app.dirtyDisplay);
 r.app.dirtyDisplay=false;
 r.ime.serviceVisuals(uint32_t(clockUs()/1000));CHECK(!r.app.dirtyDisplay);
 std::this_thread::sleep_for(std::chrono::milliseconds(300));
 r.ime.service();CHECK(r.app.dirtyDisplay&&!r.ime.boundaryVisible(uint32_t(clockUs()/1000)));
 r.app.dirtyDisplay=false;r.ime.service();CHECK(!r.app.dirtyDisplay);
 r.tap(0x52);r.app.dirtyDisplay=false;
 // Independent visual service also expires during a busy keyboard/font path.
 r.ime.serviceVisuals(uint32_t(clockUs()/1000)+300);CHECK(r.app.dirtyDisplay);
 r.app.dirtyDisplay=false;r.ime.serviceVisuals(uint32_t(clockUs()/1000)+301);CHECK(!r.app.dirtyDisplay);
 std::cout<<"PASS AD2 every 0..64 raw cursor visible in <=39 letters + cursor; AD3 expiry redraw exactly once via idle service and busy-path visual service\n";
}
void windows(const fs::path& root){
 Rig r(root,2);r.cn();r.type("yi");r.tap(0x51);r.settle();auto v=r.ime.window()->view();CHECK(v.row==1&&v.col==0&&v.expanded&&v.count==3);
 // Initial right is exactly the second item, never the next row.
 r.tap(0x29);r.type("yi");r.settle();r.tap(0x4f);r.settle();CHECK(r.ime.window()->view().row==0&&r.ime.window()->view().col==1);
 auto* w=r.ime.window();const auto old=*w->presented(r.frame);const auto expected=old.rows[0].items[0].key;
 const auto oldFrame=r.frame;w->request(5,0,true);CHECK(w->busy());w->choose(0,oldFrame);r.settle();CHECK(r.ime.composition()->snapshot().members[0].key==expected);CHECK(r.app.document.text().empty());
 r.tap(0x29);r.type("yi");r.settle();w=r.ime.window();w->request(999,0,true);r.settle();CHECK(w->state()==WindowState::Boundary);CHECK(r.ime.raw()=="yi");
 const auto before=r.ime.composition()->snapshot().member_count;w->choose(0,0xdeadbeef);CHECK(r.ime.composition()->snapshot().member_count==before&&w->state()==WindowState::Stale);
 r.tap(0x29);r.type("wjdj");r.settle();r.tap(0x4f);r.settle();CHECK(w->state()==WindowState::Boundary&&w->view().col==0);CHECK(r.ime.composition()->snapshot().member_count==0);
 const auto expired=r.frame;r.tap(0x29);r.tap(0x2c,1);r.settle();r.type("yi");r.settle();CHECK(r.frame!=expired);r.ime.window()->choose(0,expired);CHECK(r.ime.window()->state()==WindowState::Stale&&r.ime.composition()->snapshot().member_count==0);
 std::cout<<"PASS initial Down pending exact target, initial Right, three rows, deep real end, old displayed choice full-key reauthorization, expired-frame refusal, missing second item\n";
}
void queuedAttempts(const fs::path& root){
 Rig r(root,70);EditorSink fill(r.app);while(r.app.document.text().size()<65533){CHECK(fill.bind(100,1));CHECK(fill.commit(std::string(std::min(size_t(8192),65533-r.app.document.text().size()),'q')));}r.cn();r.type("wjdjle");r.settle();r.tap(0x2c);r.settle();r.tap(0x2c);r.settle();
 CHECK(r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect);
 const auto first=r.ime.attemptSerial();auto oldCancel=s3hid::Event{1,++r.now,0x29,0,true,0,r.frame,first,0,0,r.ime.ingressContext(),r.ime.resourceOwner()};auto oldSwitch=s3hid::Event{1,++r.now,0x2c,0x01,true,0,r.frame,first,0,0,r.ime.ingressContext(),r.ime.resourceOwner()};
 r.tap(0x28);r.step();CHECK(r.ime.attemptSerial()!=first);r.send(oldCancel);CHECK(r.ime.composition()->edit_raw(0,0,"x")==hanime_extension::Status::Busy);
 r.send({1,++r.now,0x29,0,false,0,r.frame,first,0,0,r.ime.ingressContext(),r.ime.resourceOwner()});r.settle();CHECK(r.app.document.text().size()==65533&&r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect);
 const auto current=r.ime.attemptSerial();r.send(oldSwitch);CHECK(!r.ime.raw().empty()&&r.ime.attemptSerial()==current&&r.resources.rules()==hanime_extension::SchemeRules::R1Xiaohe);r.send({1,++r.now,0x2c,0x01,false,0,r.frame,first,0,0,r.ime.ingressContext(),r.ime.resourceOwner()});
 r.tap(0x29);CHECK(r.ime.raw().empty());
 r.type("wjdjle");r.settle();r.tap(30);r.settle();r.tap(30);r.settle();r.tap(0x28);r.tap(0x29);r.settle();CHECK(r.ime.raw().empty()&&r.app.document.text().size()==65533);
 std::cout<<"PASS original queued attempt survives retry; late Esc or scheme switch cannot cancel newer attempt after retry/rejection; Esc before prepare clears queued confirmation\n";
}
void bridge(const fs::path& root){
 Rig r(root,3);EditorSink sink(r.app);CHECK(sink.bind(1,1));CHECK(sink.commit("中文𨿍"));CHECK(r.app.document.text()=="中文𨿍");CHECK(!sink.commit("重复"));
 CHECK(sink.bind(2,2));r.app.key({1,1001,0x50,0,true});const auto before=std::string(r.app.document.text());const auto rev=r.app.document.revision();CHECK(!sink.commit("过期"));CHECK(std::string_view(r.app.document.text())==before&&r.app.document.revision()==rev);
 CHECK(sink.bind(3,3));CHECK(!sink.commit(std::string(65537,'x')));CHECK(std::string_view(r.app.document.text())==before&&r.app.document.revision()==rev);
 r.cn();r.type("wjdjle");r.settle();r.ime.window()->choose(0,r.frame);r.settle();r.ime.window()->choose(0,r.frame);r.settle();
 // Actual public editor bridge under a real core transaction. Throw after the
 // final model acceptance to prove Unknown does not trigger replay/rollback.
 struct ThrowAfter:hanime::TextCommitSink {EditorSink& sink;unsigned calls=0;explicit ThrowAfter(EditorSink& s):sink(s){}bool commit(std::string_view t)override{++calls;CHECK(sink.commit(t));throw std::runtime_error("injected after actual App acceptance");}} wrapped(sink);
 auto* c=r.ime.composition();CHECK(sink.bind(c->snapshot().input_id,c->snapshot().revision));
 hanime_extension::Submission submission(*c,r.resources.shared,wrapped);auto result=submission.prepare();auto original=result.transaction;
 for(unsigned i=0;result.core.status==hanime::CompositionStatus::Preparing&&i<100000;++i)result=submission.advance(original,Resources::Budget).result;
 CHECK(result.core.status==hanime::CompositionStatus::Ready);result=submission.confirm(original);CHECK(result.core.status==hanime::CompositionStatus::UnknownOutcome);const auto accepted=std::string(r.app.document.text());
 CHECK(submission.retry(original).core.status==hanime::CompositionStatus::UnknownOutcome);CHECK(submission.cancel(original).core.status==hanime::CompositionStatus::UnknownOutcome);CHECK(wrapped.calls==1&&std::string_view(r.app.document.text())==accepted);CHECK(c->cancel_input(c->snapshot().input_id)==hanime_extension::Status::Busy);
 ++r.resources.mount()->epoch;r.resources.service();CHECK(r.resources.state()==ResourceState::Error);
 CHECK(submission.cancel(original).core.status==hanime::CompositionStatus::UnknownOutcome&&submission.confirm(original).core.status==hanime::CompositionStatus::UnknownOutcome);
 CHECK(wrapped.calls==1&&std::string_view(r.app.document.text())==accepted);
 std::cout<<"PASS actual App sink, zero-effect stale/capacity rejection, Unicode supplementary, Unknown after actual acceptance, no replay or fake cancellation\n";
}
}
int adapterTestMain(int argc,char** argv){try{const auto root=fixture();if(argc==2&&std::string(argv[1])=="--faults"){verifiedFaults(root);CHECK(metrics.resource_handles==0&&metrics.document_handles==0);return 0;}if(argc==2&&std::string(argv[1])=="--loading"){loadingEmptyRegression(root);loadingLifecycle(root);loadingFeedback(root);cachedReads(root);return 0;}verifiedFaults(root);loadingEmptyRegression(root);loadingLifecycle(root);loadingFeedback(root);cachedReads(root);readerTests(root);fontChecks(root);allocationFailure(root);controls(root);k1RawDown(root);fullPinyinNavigation(root);visualFixes(root);windows(root);queuedAttempts(root);bridge(root);CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS handles balanced; simultaneous peak="<<metrics.handle_peak<<" sizeof(window)="<<sizeof(CandidateWindow)<<" sizeof(controller)="<<sizeof(DeviceIme)<<" max_read="<<metrics.largest_read<<" reauth="<<metrics.reauthorizations<<"\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

int main(int argc,char** argv){try{FixtureRun run;const auto result=adapterTestMain(argc,argv);run.success=result==0;return result;}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
