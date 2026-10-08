#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
#include "io_profile.hpp"
#include <functional>
struct Sample {
 Rig&r;const char* label;IoCounters io=metrics.io();IoProfile roles=ioProfile;hanime::OutputReuseStats outputs;hanime::VerifiedResourceSnapshot shared,scheme,order;uint64_t start=clockUs();uint64_t work[unsigned(Phase::Count)]{};
 Sample(Rig& v,const char* name):r(v),label(name),outputs(v.resources.shared.output_reuse_stats()),shared(v.resources.sharedSnapshot()),scheme(v.resources.schemeSnapshot()),order(v.resources.orderSnapshot()){for(unsigned i=0;i<unsigned(Phase::Count);++i)work[i]=metrics.phases[i].work;}
 void end(){
  auto d=metrics.io()-io;auto o=r.resources.shared.output_reuse_stats();const auto s=r.resources.sharedSnapshot(),sc=r.resources.schemeSnapshot(),ord=r.resources.orderSnapshot();
  std::cout<<"PROFILE label="<<label<<" profile="<<OutputProfile<<" bytes="<<d.bytes<<" reads="<<d.reads<<" seeks="<<d.seeks<<" logical="<<d.requested_bytes<<" host_us="<<clockUs()-start<<" hits="<<o.hits-outputs.hits<<" misses="<<o.misses-outputs.misses<<" evictions="<<o.evictions-outputs.evictions<<" resident="<<o.resident<<" anchor_hits="<<s.anchor_hits-shared.anchor_hits<<" anchor_misses="<<s.anchor_misses-shared.anchor_misses<<" proof_saved="<<s.proof_bytes_saved-shared.proof_bytes_saved<<" work="<<s.cost.work_items-shared.cost.work_items+sc.cost.work_items-scheme.cost.work_items+ord.cost.work_items-order.cost.work_items<<'\n';
  for(unsigned i=0;i<unsigned(FileRole::Count);++i){const auto&a=ioProfile.roles[i];const auto&b=roles.roles[i];std::cout<<"PROFILE_ROLE label="<<label<<" role="<<IoProfile::roleName(i)<<" bytes="<<a.bytes-b.bytes<<" reads="<<a.reads-b.reads<<" seeks="<<a.seeks-b.seeks<<'\n';}
  for(unsigned i=0;i<unsigned(Phase::Count);++i)if(metrics.phases[i].work!=work[i])std::cout<<"PROFILE_PHASE label="<<label<<" phase="<<i<<" work="<<metrics.phases[i].work-work[i]<<'\n';
  const auto& view=*r.ime.view();
  for(size_t row=0;row<view.count;++row){for(size_t col=0;col<view.rows[row].count;++col){
   const auto& item=view.rows[row].items[col];char key[65]{};constexpr char hex[]="0123456789abcdef";
   for(size_t i=0;i<32;++i){key[2*i]=hex[item.key.bytes[i]>>4];key[2*i+1]=hex[item.key.bytes[i]&15];}
   std::cout<<"SEMANTIC_ITEM label="<<label<<" row="<<view.rows[row].index<<" col="<<col<<" key="<<key<<" ref="<<item.ref<<" begin="<<item.begin<<" end="<<item.end<<" width="<<item.width<<" bytes="<<item.bytes<<" text="<<std::string_view(item.text.data(),item.bytes)<<'\n';
  }}
  std::cout<<"SEMANTIC label="<<label<<" view="<<digest(*r.ime.view())<<" selected="<<r.ime.selected()<<" body="<<r.app.document.text()<<'\n';
 }
};
void settled(Rig&r){unsigned n=0;while(r.ime.hasWork()){r.step();CHECK(++n<2000000);}r.frame=r.ime.framePresented();}
void resetToBody(Rig&); void query(Rig&r,const char*raw,const char*label){resetToBody(r);Sample s(r,label);r.type(raw);settled(r);CHECK(r.ime.window()->state()==WindowState::Ready);s.end();}
void press(Rig&r,uint8_t key){s3hid::Event e{1,++r.now,key,0,true,0,r.frame,r.ime.attemptSerial()};e.ime_context=r.ime.ingressContext();e.resource_owner=r.ime.resourceOwner();r.ime.event(e);e.down=false;e.ms=++r.now;r.ime.event(e);}
void run(bool py){
 Rig r;r.tap(0x39);r.settle();if(py){r.tap(0x2c,1);r.settle();}
 query(r,py?"shi":"ui","ui.cold_first_row");query(r,py?"shi":"ui","ui.repeat_first_row");
 query(r,"le","le.cold_first_row");query(r,"le","le.repeat_first_row");
 query(r,py?"wojintian":"wojbtm","three.cold_plan_row");query(r,py?"wojintian":"wojbtm","three.repeat_plan_row");
 {Sample s(r,"three.first_choice_suffix");press(r,0x2c);settled(r);CHECK(r.ime.selected()=="我");s.end();}
 {Sample s(r,"three.final_number_commit");press(r,30);settled(r);CHECK(r.app.document.text()=="我今天");s.end();}
 query(r,py?"wandanle":"wjdjle","wandan.first_row");
 {Sample s(r,"wandan.choice_le");press(r,0x2c);settled(r);CHECK(r.ime.selected()=="完蛋");s.end();}
 {Sample s(r,"wandan.final_space_commit");press(r,0x2c);settled(r);CHECK(r.app.document.text()=="我今天完蛋了");s.end();}
 query(r,py?"shi":"ui","navigation.start");
 unsigned action=0;for(size_t row:{1u,2u,3u,2u,1u,0u,10u,11u,10u,0u,25u,24u,0u}){
  std::string label="nav."+std::to_string(action++)+".row"+std::to_string(row);Sample s(r,label.c_str());r.ime.window()->request(row,0,true);settled(r);CHECK(r.ime.window()->state()==WindowState::Ready);s.end();
 }
 const auto o=r.resources.shared.output_reuse_stats();const auto m=r.resources.sharedMemory();
 std::cout<<"PROFILE_MEMORY py="<<py<<" profile="<<OutputProfile<<" output_fixed="<<o.fixed_bytes<<" output_metadata="<<o.metadata_bytes<<" output_text="<<o.text_bytes<<" output_form="<<o.form_bytes<<" anchor="<<m.anchor_bytes<<" controller="<<sizeof(DeviceIme)<<" window="<<sizeof(CandidateWindow)<<" event="<<sizeof(s3hid::Event)<<" snapshot="<<sizeof(Sample)<<" scope=Host_not_device_peak\n";
}
int main(){try{run(false);run(true);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

// Fixture reset only: existing fresh physical Esc reports flow through Rig::tap
// into DeviceIme::event and then App::key if unconsumed. No internal reset API.
// Defined after the original test so every original assertion line is preserved.
void resetToBody(Rig& r){
 const std::string body(r.app.document.text());
 const auto cursor=r.app.document.cursorByte();
 const auto revision=r.app.document.textRevision();
 const auto undo=r.app.editor.undoCount();
 unsigned presses=0;
 if(r.ime.active()&&!r.ime.rawEditing()){
  r.tap(0x29);++presses;r.settle();
  CHECK(r.ime.rawEditing());
 }
 if(r.ime.rawEditing()){
  r.tap(0x29);++presses;r.settle();
 }
 CHECK(presses<=2);
 CHECK(!r.ime.active()&&r.ime.raw().empty()&&!r.ime.rawEditing());
 CHECK(!r.ime.candidateAvailable()&&r.ime.view()&&r.ime.view()->count==0);
 CHECK(r.ime.window()&&r.ime.window()->state()==WindowState::Empty&&!r.ime.hasWork());
 CHECK(std::string_view(r.app.document.text())==body);
 CHECK(r.app.document.cursorByte()==cursor&&r.app.document.textRevision()==revision);
 CHECK(r.app.editor.undoCount()==undo);
 std::cout<<"RESET_BODY physical_esc="<<presses<<" inactive=1 raw_empty=1 candidate_count=0 body_bytes="<<body.size()<<" cursor="<<cursor<<" revision="<<revision<<" body_unchanged=1\n";
}
