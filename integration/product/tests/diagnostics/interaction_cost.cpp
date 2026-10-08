// Host-only current numeric-completion driver; edit before final selection.
// Historical SRP2 retained-suffix fixture remains in the frozen evidence package. No direct Composition query/select/revalidate calls.
// All actions enter the shipped DeviceIme::event and CandidateWindow path.
#include "device_ime.hpp"
#include "chinese_font.hpp"
#include "hanime_document_sd_storage.hpp"
#include "io_profile.hpp"
#include <filesystem>
#include <map>
#include <set>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ime_device;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
namespace {
struct FileReadCost {
 uint64_t reads=0,bytes=0,repeated=0,repeated_bytes=0;
 std::set<std::pair<size_t,size_t>> ranges;
};
std::map<int,std::string> open_paths;
std::map<std::string,FileReadCost> file_reads;
void observeRead(int fd,const char* path,size_t offset,size_t bytes){
 if(path){open_paths[fd]=fs::path(path).filename().string();return;}
 if(offset==SIZE_MAX){open_paths.erase(fd);return;}
 const auto it=open_paths.find(fd);CHECK(it!=open_paths.end());
 auto& cost=file_reads[it->second];++cost.reads;cost.bytes+=bytes;
 if(!cost.ranges.emplace(offset,bytes).second){++cost.repeated;cost.repeated_bytes+=bytes;}
}
std::string hex(std::string_view s){constexpr char h[]="0123456789abcdef";std::string out;for(unsigned char c:s){out+=h[c>>4];out+=h[c&15];}return out;}
struct Rig {
 fs::path path;s3fs::Fs files;hanime::documents::ArduinoSdStoragePort storage;
 ChineseFont font;s3app::App app;Resources resources;DeviceIme ime;
 uint32_t now=100,frame=0;unsigned event=0;
 uint64_t ime_us=0,app_us=0,event_us=0;unsigned services=0;
 explicit Rig(unsigned id):path(fs::current_path()/("interaction-doc-"+std::to_string(id))),files(path.string()),storage(files),app(storage,{},5000+id,font),resources(RESOURCE_ROOT,std::make_shared<Mount>(),productionPins(),INDEX_ROOT),ime(app,resources,font){
  CHECK(!fs::exists(path));fs::create_directories(path);CHECK(font.begin());storage.noteMounted();CHECK(app.startup(now));
 }
 void send(s3hid::Event e){const auto t=clockUs();if(!ime.event(e))app.key(e);event_us+=clockUs()-t;}
 s3hid::Event down(uint8_t usage,uint8_t mods=0){
  s3hid::Event e{1,++now,usage,mods,true,0,frame,ime.attemptSerial()};e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();send(e);
  std::cout<<"KEY event="<<++event<<" usage="<<unsigned(usage)<<" mods="<<unsigned(mods)<<" frame="<<frame<<" attempt="<<e.ime_attempt<<'\n';return e;
 }
 void up(s3hid::Event e){e.ms=++now;e.down=false;send(e);}
 void tap(uint8_t usage,uint8_t mods=0){up(down(usage,mods));}
 void type(std::string_view raw){for(char c:raw){CHECK(c>='a'&&c<='z');tap(uint8_t(c-'a'+4));}}
 void step(){const auto t=clockUs();ime.service();const auto u=clockUs();app.service(++now,false);const auto v=clockUs();ime_us+=u-t;app_us+=v-u;++services;CHECK(resources.state()!=ResourceState::Error);}
 unsigned settle(){
  unsigned n=0;for(;n<2000000;++n){
   step();if(resources.ready()&&ime.composition()&&!ime.hasWork())break;
  }CHECK(n<2000000);frame=ime.framePresented();return n+1;
 }
 void view(const char* label){
  const auto out=resources.shared.output_reuse_stats();const auto page=resources.shared.memory_usage();
  size_t auth=0,anchors=0;for(const auto& m:{resources.sharedMemory(),resources.schemeMemory(),resources.orderMemory()}){auth+=m.owner_fixed_bytes+m.data_payload_bytes+m.index_payload_bytes+m.data_metadata_bytes+m.index_metadata_bytes+m.anchor_bytes;anchors+=m.anchor_bytes;}
  std::cout<<"CACHE_MEMORY label="<<label<<" profile="<<OutputProfile<<" shared_data_kib="<<SharedDataKiB<<" shared_pages="<<SharedPages<<" order_data_kib=8 order_proof_kib=8 auth_total="<<auth<<" anchors="<<anchors<<" shared_page_total="<<page.owner_fixed_bytes+page.cache_payload_allocated_bytes+page.cache_metadata_allocated_bytes<<" output_total="<<out.fixed_bytes+out.metadata_bytes+out.text_bytes+out.form_bytes<<" output_hits="<<out.hits<<" output_resident="<<out.resident<<'\n';
  const auto* v=ime.view();CHECK(v);std::cout<<"VIEW label="<<label<<" frame="<<frame<<" row="<<v->row<<" col="<<v->col<<" count="<<v->count<<" raw_hex="<<hex(ime.raw())<<" selected_hex="<<hex(ime.selected())<<'\n';
  for(size_t row=0;row<v->count;++row){const auto& r=v->rows[row];for(size_t col=0;col<r.count;++col){const auto& x=r.items[col];
   std::cout<<"ITEM row="<<r.index<<" col="<<col<<" page="<<x.page<<" slot="<<x.slot<<" page_generation="<<x.page_generation<<" input="<<x.input<<" revision="<<x.revision<<" scheme="<<x.scheme<<" ref="<<x.ref<<" begin="<<x.begin<<" end="<<x.end<<" text_hex="<<hex({x.text.data(),x.bytes})<<" key_hex="<<hex({reinterpret_cast<const char*>(x.key.bytes.data()),x.key.bytes.size()})<<'\n';
  }}
 }
};
struct AuthTotals {
 uint64_t requests=0,bytes=0,data_hits=0,index_hits=0,loads=0,index_loads=0,work=0;
 explicit AuthTotals(const Resources& r){for(const auto& s:{r.sharedSnapshot(),r.schemeSnapshot(),r.orderSnapshot()}){requests+=s.logical_requests;bytes+=s.logical_bytes;data_hits+=s.data_cache_hits;index_hits+=s.index_cache_hits;loads+=s.authenticated_loads;index_loads+=s.authenticated_index_loads;work+=s.cost.work_items;}}
};
struct Capture {
 IoCounters io=metrics.io();AuthTotals auth;uint64_t start=clockUs(),ime_us,app_us,event_us;unsigned services;
 Measure query=metrics.phases[unsigned(Phase::Query)],text=metrics.phases[unsigned(Phase::VisibleText)];
 Measure prepare=metrics.phases[unsigned(Phase::Submission)];
 std::map<std::string,FileReadCost> files=file_reads;
 std::array<hanime::VerifiedResourceSnapshot,3> roles{};
 std::array<RoleIo,unsigned(FileRole::Count)> physical_roles{};
 explicit Capture(const Rig& r):auth(r.resources),ime_us(r.ime_us),app_us(r.app_us),event_us(r.event_us),services(r.services){std::copy(std::begin(ioProfile.roles),std::end(ioProfile.roles),physical_roles.begin());roles={r.resources.sharedSnapshot(),r.resources.schemeSnapshot(),r.resources.orderSnapshot()};}
 void print(const Rig& r,const std::string& label)const{
  const auto elapsed=clockUs()-start;const auto d=metrics.io()-io;const AuthTotals a(r.resources);
  for(const auto& pair:file_reads){
   const auto& after=pair.second;const auto it=files.find(pair.first);
   const FileReadCost empty{};const auto& before=it==files.end()?empty:it->second;
   if(after.reads!=before.reads)std::cout<<"FILE_COST label="<<label<<" file="<<pair.first<<" reads="<<after.reads-before.reads<<" bytes="<<after.bytes-before.bytes<<" repeated="<<after.repeated-before.repeated<<" repeated_bytes="<<after.repeated_bytes-before.repeated_bytes<<'\n';
  }
  for(unsigned i=0;i<physical_roles.size();++i){
   const auto& before=physical_roles[i];const auto& after=ioProfile.roles[i];
   if(after.requests==before.requests&&after.reads==before.reads&&after.seeks==before.seeks)continue;
   std::cout<<"ROLE_IO label="<<label<<" role="<<IoProfile::roleName(i)<<" requests="<<after.requests-before.requests<<" hits="<<after.hits-before.hits<<" reads="<<after.reads-before.reads<<" seeks="<<after.seeks-before.seeks<<" bytes="<<after.bytes-before.bytes<<" read_us="<<after.read_us-before.read_us<<" seek_us="<<after.seek_us-before.seek_us<<" errors="<<after.errors-before.errors<<'\n';
  }
  const std::array<hanime::VerifiedResourceSnapshot,3> after_roles={r.resources.sharedSnapshot(),r.resources.schemeSnapshot(),r.resources.orderSnapshot()};
  const char* names[]={"shared","scheme","order"};
  for(size_t i=0;i<roles.size();++i){const auto& before=roles[i];const auto& after=after_roles[i];
   std::cout<<"ROLE_COST label="<<label<<" role="<<names[i]<<" loads="<<after.authenticated_loads-before.authenticated_loads<<" index_loads="<<after.authenticated_index_loads-before.authenticated_index_loads<<" data_hits="<<after.data_cache_hits-before.data_cache_hits<<" index_hits="<<after.index_cache_hits-before.index_cache_hits<<'\n';
  }

  const auto& q=metrics.phases[unsigned(Phase::Query)];const auto& t=metrics.phases[unsigned(Phase::VisibleText)];
  const auto qi=q.io-query.io;const auto ti=t.io-text.io;
  std::cout<<"COST label="<<label<<" services="<<r.services-services<<" host_wall_us="<<elapsed<<" ime_service_us="<<r.ime_us-ime_us<<" app_service_us="<<r.app_us-app_us<<" event_us="<<r.event_us-event_us
   <<" tf_logical="<<d.requests<<" tf_logical_bytes="<<d.requested_bytes<<" reads="<<d.reads<<" bytes="<<d.bytes<<" seeks="<<d.seeks<<" read_us="<<d.read_us<<" seek_us="<<d.seek_us
   <<" auth_requests="<<a.requests-auth.requests<<" auth_bytes="<<a.bytes-auth.bytes<<" auth_data_hits="<<a.data_hits-auth.data_hits<<" auth_index_hits="<<a.index_hits-auth.index_hits<<" auth_loads="<<a.loads-auth.loads<<" auth_index_loads="<<a.index_loads-auth.index_loads<<" auth_work="<<a.work-auth.work
   <<" query_ticks="<<q.steps-query.steps<<" query_bytes="<<qi.bytes<<" query_reads="<<qi.reads<<" query_us="<<q.us-query.us
   <<" visible_ticks="<<t.steps-text.steps<<" visible_bytes="<<ti.bytes<<" visible_reads="<<ti.reads<<" visible_us="<<t.us-text.us<<'\n';
 }
};
void item(const Rig& r,size_t column,std::string_view expected){
 const auto* v=r.ime.view();CHECK(v&&v->count&&v->row>=v->rows[0].index&&v->row<v->rows[0].index+v->count);
 const auto& row=v->rows[v->row-v->rows[0].index];CHECK(column<row.count);
 CHECK(std::string_view(row.items[column].text.data(),row.items[column].bytes)==expected);
}
void settle(Rig& r,const std::string& label){const Capture c(r);r.settle();c.print(r,label);}
void selectNumber(Rig& r,size_t column,std::string_view expected,const std::string& label){
 item(r,column,expected);const auto oldRevision=r.ime.composition()->snapshot().revision;const auto body=std::string(r.app.document.text());
 Capture c(r);const auto choice=r.down(uint8_t(30+column));r.up(choice);c.print(r,label+".dispatch");
 c=Capture(r);unsigned steps=0;
 while(r.ime.composition()->snapshot().revision==oldRevision&&r.ime.hasWork()&&steps++<2000000){r.step();}
 CHECK(steps<2000000&&r.ime.composition()->snapshot().revision!=oldRevision);
 c.print(r,label+".selection_publish");CHECK(std::string_view(r.app.document.text())==body);
 const auto snap=r.ime.composition()->snapshot();bool retained=false;
 for(size_t n=0;n<snap.member_count;++n){if(!snap.members[n].valid&&snap.members[n].begin==snap.focus)retained=true;}
 if(retained){
  const auto revision=snap.revision;c=Capture(r);steps=0;
  while(r.ime.composition()->snapshot().revision==revision&&r.ime.hasWork()&&steps++<2000000){r.step();}
  CHECK(steps<2000000&&r.ime.composition()->snapshot().revision!=revision);
  c.print(r,label+".retained_recheck");CHECK(std::string_view(r.app.document.text())==body);
 }
 if(r.ime.composition()->snapshot().state==hanime_extension::CompositionState::AllSelected){
  c=Capture(r);r.settle();c.print(r,label+".prepare_and_accept");
  CHECK(r.ime.receipt().status==hanime::CompositionStatus::Accepted);
  const auto& m=metrics.phases[unsigned(Phase::Submission)];const auto all=metrics.io()-c.io;const auto prep=m.io-c.prepare.io;
  CHECK(all.reads==prep.reads&&all.bytes==prep.bytes);
  std::cout<<"SUBMISSION label="<<label<<" core_advance_steps="<<m.steps-c.prepare.steps<<" core_advance_us="<<m.us-c.prepare.us<<" core_advance_reads="<<prep.reads<<" core_advance_bytes="<<prep.bytes<<" outside_advance_resource_bytes="<<all.bytes-prep.bytes<<" body_hex="<<hex(r.app.document.text())<<" accepted=1\n";
  const auto text=std::string(r.app.document.text());const auto reads=metrics.reads;const auto stamp=r.app.editStamp();
  r.send(choice);r.up(choice);r.step();
  CHECK(std::string_view(r.app.document.text())==text&&r.app.editStamp().revision==stamp.revision&&metrics.reads==reads);
  std::cout<<"CHECK label="<<label<<" repeated_original_selection_no_replay=1\n";
 }else{settle(r,label+".post_selection_query");CHECK(std::string_view(r.app.document.text())==body);}
}
void input(Rig& r,bool pinyin,const std::string& prefix){
 if(pinyin){r.tap(0x2c,1);} // Existing Ctrl+Space in EN selects the Chinese scheme.
 {const Capture c(r);r.tap(0x39);r.settle();c.print(r,prefix+".activation");}
 {const Capture c(r);r.type(pinyin?"wandanle":"wjdjle");c.print(r,prefix+".raw_burst_dispatch");}
 settle(r,prefix+".first_window");r.view((prefix+".initial").c_str());
}
void run(unsigned id,bool pinyin,bool edit){
 const std::string prefix=std::string(pinyin?"py":"xh")+(edit?"_k1":"_basic");std::cout<<"CASE name="<<prefix<<" fresh_resources=1 raw_burst=1\n";
 file_reads.clear();Rig r(id);input(r,pinyin,prefix);
 selectNumber(r,0,"完蛋",prefix+".word");r.view((prefix+".after_word").c_str());
 if(edit){
  {const Capture c(r);r.tap(0x50);r.tap(0x50);CHECK(r.ime.rawCursor()==(pinyin?3u:2u));r.tap(0x51);c.print(r,prefix+".k1_first_prefix_dispatch");}
  settle(r,prefix+".k1_first_prefix_query");r.view((prefix+".k1_first_prefix").c_str());
  selectNumber(r,0,"玩",prefix+".play");r.view((prefix+".after_play_longest").c_str());
  // Production resources also contain '淡了'. Request the shorter syllable
  // explicitly through the confirmed Left/Down rule; never call choose_prefix.
  {const Capture c(r);r.tap(0x50);CHECK(r.ime.rawCursor()==(pinyin?6u:4u));r.tap(0x51);c.print(r,prefix+".k1_second_prefix_dispatch");}
  settle(r,prefix+".k1_second_prefix_query");r.view((prefix+".k1_second_prefix").c_str());
  selectNumber(r,3,"蛋",prefix+".egg");CHECK(r.ime.selected()=="玩蛋"&&r.app.document.text().empty());
 }
 selectNumber(r,0,"了",prefix+".suffix");
 CHECK(r.app.document.text()==(edit?"玩蛋了":"完蛋了"));
 CHECK(r.ime.raw().empty());
 std::cout<<"PASS case="<<prefix<<" body_hex="<<hex(r.app.document.text())<<" real_event_path=1\n";
}
}
int main(){try{
 std::cout<<"HOST_ONLY device_event_path; no BLE/InputRoute/LCD; milliseconds are synthetic dispatch order, not measured device latency\n";
 TfReader::readObserver=observeRead;
 run(1,false,false);run(2,true,false);run(3,false,true);run(4,true,true);
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 std::cout<<"PASS all_real_event_cases=4 handles_balanced=1\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
