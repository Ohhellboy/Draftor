#define PY1_EVENTS_LIBRARY
#include "py1_events.cpp"
#include <iomanip>
std::string keyHex(const hanime::StableLexemeKey& key){const char* hex="0123456789abcdef";std::string s;for(auto b:key.bytes){s+=hex[b>>4];s+=hex[b&15];}return s;}
void phase(LongRig& r,const std::string& scene,const std::string& label,const IoCounters& before){
 const auto io=metrics.io()-before;const auto p=r.ime.composition()->planning_report();const auto s=r.ime.composition()->snapshot();
 std::cout<<"{\"kind\":\"phase\",\"scene\":"<<std::quoted(scene)<<",\"label\":"<<std::quoted(label)<<",\"reads\":"<<io.reads<<",\"bytes\":"<<io.bytes<<",\"seeks\":"<<io.seeks<<",\"reader_calls\":"<<io.requests<<",\"reader_requested\":"<<io.requested_bytes<<",\"plan_current\":"<<r.ime.composition()->plan_is_current(p)<<",\"limited\":"<<p.limited<<",\"queries\":"<<p.queries<<",\"hits\":"<<p.cache_hits<<",\"core_work\":"<<p.core_cost.work_items<<",\"core_read_calls\":"<<p.core_cost.read_calls<<",\"core_read\":"<<p.core_cost.read_bytes<<",\"score\":"<<p.score<<",\"focus\":"<<s.focus<<",\"members\":[";
 for(size_t i=0;i<s.member_count;++i){if(i)std::cout<<',';const auto& m=s.members[i];std::cout<<"{\"key\":"<<std::quoted(keyHex(m.key))<<",\"ref\":"<<m.ref<<",\"begin\":"<<m.begin<<",\"end\":"<<m.end<<'}';}
 std::cout<<"],\"visible\":[";bool first=true;
 if(auto v=r.ime.view())for(size_t i=0;i<v->count;++i)for(size_t j=0;j<v->rows[i].count;++j){if(!first)std::cout<<',';first=false;const auto& x=v->rows[i].items[j];std::cout<<"{\"key\":"<<std::quoted(keyHex(x.key))<<",\"ref\":"<<x.ref<<",\"begin\":"<<x.begin<<",\"end\":"<<x.end<<",\"text\":"<<std::quoted(std::string(x.text.data(),x.bytes))<<'}';}
 std::cout<<"]}\n";
}
void scenario(bool py,unsigned n){
 const std::string scene=std::string(py?"py_":"xh_")+std::array<const char*,4>{"REOPEN","NO_MANUAL","SHORT","LIMITED"}[n];
 const std::string three=py?"wandanle":"wjdjle",today=py?"wojintian":"wojbtm",word=py?"zhonghuarenmingongheguo":"vshxrfmbgshego";
 const std::string raw=n==2?today:n==3?three+today+word:three+today;
 const auto total=metrics.io();LongRig r(scene);r.start(py,raw);phase(r,scene,"initial",total);
 if(n==0){
  CHECK(r.ime.view()&&std::string(r.ime.view()->rows[0].items[0].text.data(),r.ime.view()->rows[0].items[0].bytes)=="完蛋");
  auto before=metrics.io();r.key(44);r.ready();phase(r,scene,"seed_suffix",before);
  before=metrics.io();r.key(44);r.ready();phase(r,scene,"seed_suffix2",before);
  CHECK(r.accepted==0&&r.ime.composition()->snapshot().member_count==2);
 }
 if(n!=1){
  const auto before=metrics.io();const auto old_choice=r.ime.composition()->selection(0);
  const size_t end=n==2?2:(py?3:2);const auto item=manual(r,end);phase(r,scene,"manual",before);
  CHECK(r.ime.composition()->select(old_choice)==hanime_extension::Status::Stale);
  CHECK(r.ime.targetMatches(item));const auto next=metrics.io();r.key(44);r.ready();phase(r,scene,"after_manual_select",next);
 }
 for(unsigned i=0;r.accepted==0;++i){CHECK(i<64);const auto before=metrics.io();r.key(i%2?30:44);r.ready();phase(r,scene,"suffix_"+std::to_string(i),before);}
 CHECK(r.accepted==1&&r.ime.raw().empty());const auto io=metrics.io()-total;
 std::cout<<"{\"kind\":\"total\",\"scene\":"<<std::quoted(scene)<<",\"reads\":"<<io.reads<<",\"bytes\":"<<io.bytes<<",\"seeks\":"<<io.seeks<<",\"accepted\":1,\"body\":"<<std::quoted(std::string(r.app.document.text()))<<"}\n";
}
int main(){try{for(bool py:{false,true})for(unsigned n=0;n<4;++n)scenario(py,n);return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
