#define main retained_daily_main
#include "whole_cost.cpp"
#undef main
#include "hanime_extension/lc1_config.hpp"

int main(int argc,char** argv) {
  try {
    require(argc==6,"logical transport scheme locate_slots default|reference");
    const std::string id=argv[3];const bool x=id=="xiaohe";
    const auto rules=x?SchemeRules::R1Xiaohe:SchemeRules::FullPinyin;
    const bool reference=std::string(argv[5])=="reference";
    const std::string today=x?"wojbtm":"wojintian";
    const std::string word=x?"vshxrfmbgshego":"zhonghuarenmingongheguo";
    const std::string three=x?"wjdjle":"wandanle";
    for(unsigned scene=0;scene<6;++scene) {
      R runtime;runtime.scenario=std::array<const char*,6>{"SHORT_MANUAL","LONG_MANUAL","CONT_MANUAL","REOPEN","NO_MANUAL","NO_BENEFIT"}[scene];
      runtime.mount(argv[1],argv[2],id,std::stoul(argv[4]));
      auto config=lc1::input_config(rules);
      if(reference) {
        config.planning.core_work=20000000;config.planning.local_work=20000000;config.planning.read_bytes=64*1024*1024;
        runtime.core_limit=config.planning.core_work;runtime.local_limit=config.planning.local_work;
      }
      Composition input(runtime.scheme,runtime.shared,rules,config,runtime.storage.view());
      const std::string raw=scene==0?today:scene==1?word+word:scene==2?three+today+word:
        scene==3?three+today:scene==4?three+today:"le";
      require(raw.size()<=64&&input.edit_raw(0,0,raw)==Status::Ready,"MR1 bounded generated raw");
      const auto first=runtime.query(input,"first");
      require(first==Status::PageReady||first==Status::Unresolved,"MR1 initial status");
      const auto old_operation=input.operation();const auto old_selection=input.selection(0);
      if(scene==3) {
        require(first==Status::PageReady,"MR1 reopen first");runtime.select(input,"initial_selected","完蛋");
        require(runtime.query(input,"before_reopen_suffix")==Status::PageReady,"MR1 suffix seed");
        runtime.select(input,"initial_suffix");
        require(input.reopen_segment(0)==Status::Ready,"MR1 reopen selected member");
      }
      if(scene!=4) {
        const std::size_t end=scene==0||scene==5?2:scene==1?word.size():(x?2:3);
        require(runtime.query(input,"manual_range",end)==Status::PageReady,"MR1 manual exact query");
        require(input.cancel(old_operation)==Status::Stale&&input.select(old_selection)==Status::Stale,
                "MR1 old operation and Selection retired");
        // This must be a freshly acquired Selection, not a retained planner fact.
        runtime.select(input,"manual_select");
      }
      for(unsigned part=0;input.snapshot().state!=CompositionState::AllSelected;++part) {
        require(part<64,"MR1 bounded completion");
        require(runtime.query(input,"suffix_"+std::to_string(part))==Status::PageReady,"MR1 suffix candidate");
        runtime.select(input,"suffix_select_"+std::to_string(part));
      }
      const std::string expected(input.snapshot().selected_text);
      commit(runtime,input,expected,true);runtime.memory(input);
      std::cout<<"{\"kind\":\"storage\",\"allocations\":"<<runtime.storage.successes<<",\"frees\":"<<runtime.storage.frees<<",\"live\":"<<runtime.storage.active<<",\"peak_payload\":"<<runtime.storage.peak_bytes<<"}\n";
      std::cout<<"{\"kind\":\"task_total\",\"scenario\":"<<std::quoted(runtime.scenario)
        <<",\"reference\":"<<reference<<",\"reads\":"<<runtime.metrics.read_calls
        <<",\"bytes\":"<<runtime.metrics.delivered_bytes<<"}\n";
    }
    return 0;
  } catch(const std::exception& error) {std::cerr<<"MR1_FAIL "<<error.what()<<'\n';return 1;}
}
