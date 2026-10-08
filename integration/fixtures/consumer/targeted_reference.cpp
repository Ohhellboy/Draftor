#define main retained_ten_scripts_main
#include "whole_cost.cpp"
#undef main
namespace {
Config sufficient_config(SchemeRules rules) {
  auto config = lc1::input_config(rules);
  config.planning.core_work = 50000000;
  config.planning.local_work = 2000000;
  config.planning.read_bytes = 128 * 1024 * 1024;
  return config;
}

bool navigable(Status s){return s==Status::PageReady||s==Status::Unresolved||s==Status::NoPrefixMatch;}
std::string fill(std::size_t n,const std::string& pattern){std::string s;while(s.size()<n)s+=pattern;s.resize(n);return s;}
void expect_geometry(bool allowed,const char* why){
 const bool reused=observe::counters.phases[10]>0;
#if defined(PLAIN_REPLAY)
 (void)allowed;(void)why;
#elif defined(EXPECT_GEOMETRY)
 require(reused==allowed,why);
#else
 (void)allowed;(void)why;require(!reused,"baseline has no retained geometry path");
#endif
 std::cout<<"{\"kind\":\"geometry_check\",\"reason\":"<<std::quoted(why)<<",\"eligible\":"<<allowed<<",\"used\":"<<reused<<"}\n";
}
}
int main(int argc,char** argv){try{
 require(argc==5,"logical transport scheme locate_slots");const std::string id=argv[3];const bool x=id=="xiaohe";const auto rules=x?SchemeRules::R1Xiaohe:SchemeRules::FullPinyin;const auto pool=std::stoul(argv[4]);
 const std::string today=x?"wojbtm":"wojintian",three=x?"wjdjle":"wandanle",word=x?"vshxrfmbgshego":"zhonghuarenmingongheguo";
 const std::array<std::pair<std::size_t,std::size_t>,11> shapes={{{16,2},{16,0},{24,0},{24,12},{49,16},{49,24},{47,24},{22,0},{22,4},{34,0},{24,0}}};
 for(std::size_t i=0;i<shapes.size();++i){R r;r.core_limit=50000000;r.local_limit=2000000;r.scenario="SYN"+std::to_string(i+1);r.mount(argv[1],argv[2],id,pool);Composition c(r.scheme,r.shared,rules,sufficient_config(rules),r.storage.view());auto n=shapes[i].first,focus=shapes[i].second;std::string raw=fill(focus,"wo")+fill(n-focus,three+today);if(n%2)raw.back()='b';require(c.edit_raw(0,0,raw)==Status::Ready,"synthetic raw");
  // The observed positions shape this synthetic script; original words are unknown.
  for(std::size_t f=2;f<=focus;f+=2){require(r.query(c,"seed_prefix_"+std::to_string(f),f)==Status::PageReady,"seed explicit wo");r.select(c,"seed_select","我");}
  require(navigable(r.query(c,"synthetic_first")),"synthetic first status");r.memory(c);require(navigable(r.query(c,"synthetic_hot")),"synthetic hot status");expect_geometry(true,"same raw after complete geometry, even if word graph limited");
  std::cout<<"{\"kind\":\"synthetic_shape\",\"scenario\":"<<std::quoted(r.scenario)<<",\"raw_bytes\":"<<n<<",\"focus\":"<<focus<<",\"original_text_known\":false}\n";
 }
 for(unsigned scene=0;scene<3;++scene){R r;r.core_limit=50000000;r.local_limit=2000000;r.scenario="CONT"+std::to_string(scene+1);r.mount(argv[1],argv[2],id,pool);Composition c(r.scheme,r.shared,rules,sufficient_config(rules),r.storage.view());const std::string raw=scene==0?three+today+word:scene==1?today+today+today:word+three;require(c.edit_raw(0,0,raw)==Status::Ready,"continuous raw");
  for(unsigned segment=0;c.snapshot().state!=CompositionState::AllSelected;++segment){require(segment<16,"member budget continuous");auto status=r.query(c,"continuous_"+std::to_string(segment));require(status==Status::PageReady,"continuous recommendation");if(segment)expect_geometry(true,"forward selected focus recomputes word optimum with reusable geometry");r.select(c,"continuous_select");}
  const std::string text(c.snapshot().selected_text);commit(r,c,text,true);r.memory(c);
 }
 std::cout<<"TARGETED PASS SYN11 and CONT3 only\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
