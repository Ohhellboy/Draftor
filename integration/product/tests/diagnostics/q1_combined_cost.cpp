// Additional production adapter measurement. Public planner observations only.
#define main original_interaction_cost_main
#include "interaction_cost.cpp"
#undef main

void planMemory(Rig& r,const std::string& label){
 const auto* c=r.ime.composition();const auto p=c->planning_report();
 const auto m=c->planning_memory_usage();const auto b=c->memory_usage();
 const auto s=r.ime.submission()->memory_usage();
 std::cout<<"Q1_PLAN label="<<label<<" current="<<c->plan_is_current(p)
  <<" end="<<p.end<<" limited="<<p.limited<<" queries="<<p.queries
  <<" hits="<<p.cache_hits<<" local_work="<<p.local_work
  <<" core_work="<<p.core_cost.work_items<<" core_bytes="<<p.core_cost.read_bytes
  <<" owner="<<m.owner<<" impl="<<m.impl<<" buffers="<<m.buffers
  <<" constant_table="<<m.constant_table<<" composition_owner="<<b.owner_bytes
  <<" composition_impl="<<b.impl_bytes<<" composition_buffers="<<b.composition_buffers
  <<" submission_owner="<<s.owner_bytes<<" submission_impl="<<s.impl_bytes
  <<" capture="<<s.capture_array_bytes<<" font_payload="<<ChineseFont::Bytes
  <<" candidate_window="<<sizeof(CandidateWindow)<<'\n';
 // Rank/query/output/core-submission internals and allocator/whole-system peak
 // are intentionally not summed by this selected-component observation.
}
void plannedCase(unsigned id,bool py,bool stepped){
 Rig r(id);const std::string label=std::string(py?"py":"xh")+(stepped?"_keys":"_burst");
 if(py)r.tap(0x2c,1);
 r.tap(0x39);r.settle();
 const std::string raw=py?"wojintian":"wojbtm";
 const Capture all(r);
 for(size_t i=0;i<raw.size();++i){
  const Capture one(r);r.type(std::string_view(raw).substr(i,1));
  if(stepped){r.settle();one.print(r,label+".key"+std::to_string(i+1));planMemory(r,label+".key"+std::to_string(i+1));}
 }
 r.settle();all.print(r,label+".first");planMemory(r,label+".first");r.view(label.c_str());
 item(r,0,"我");CHECK(r.ime.composition()->page().end==2);
 const auto old=r.ime.composition()->planning_report();
 {const Capture c(r);r.tap(stepped?0x2c:30);r.settle();c.print(r,label+".suffix");}
 CHECK(!r.ime.composition()->plan_is_current(old));
 CHECK(r.ime.selected()=="我"&&r.app.document.text().empty());item(r,0,"今天");
 planMemory(r,label+".suffix");
 {const Capture c(r);const auto e=r.down(stepped?30:0x2c);r.up(e);r.settle();c.print(r,label+".commit");
  CHECK(r.ime.receipt().status==hanime::CompositionStatus::Accepted&&r.app.document.text()=="我今天");
  const auto stamp=r.app.editStamp();r.send(e);r.up(e);r.step();
  CHECK(r.app.document.text()=="我今天"&&r.app.editStamp().revision==stamp.revision);}
 CHECK(r.ime.raw().empty());r.tap(0x39);r.type("abc");r.tap(22,1);r.settle();
 for(unsigned n=0;r.app.persistence.backgroundSaveActive()||r.app.commandBarrier();++n){CHECK(n<100000);r.step();}
 CHECK(!r.app.document.dirty()&&r.app.document.text()=="我今天abc");
 const auto saved=r.app.session.currentId();
 {s3fs::Fs fs2(r.path.string());hanime::documents::ArduinoSdStoragePort storage(fs2);storage.noteMounted();
  s3app::App reopened(storage,{},881,r.font);CHECK(reopened.startup(++r.now));
  CHECK(reopened.session.currentId()==saved&&reopened.document.text()=="我今天abc");}
 std::cout<<"PASS q1_case="<<label<<" accepted_once=1 saved_reopened=1\n";
}
int main(){try{
 std::cout<<"HOST_ONLY Q1 combined production keys; no LCD/BLE/device timing or whole-system peak\n";
 TfReader::readObserver=observeRead;
 for(bool py:{false,true})for(bool stepped:{false,true})plannedCase(21+unsigned(py)*2+unsigned(stepped),py,stepped);
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
