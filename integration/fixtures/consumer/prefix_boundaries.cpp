#define main retained_daily_main
#include "whole_cost.cpp"
#undef main

int main(int argc,char** argv) {
  try {
    require(argc==5,"logical transport scheme locate_slots");
    const std::string id=argv[3];
    const auto rules=id=="xiaohe"?SchemeRules::R1Xiaohe:SchemeRules::FullPinyin;
    const std::string raw=id=="xiaohe"?"wojbtmwojbtm":"wojintianwojintian";
    R r;r.scenario="PREFIX_LIFETIME";r.mount(argv[1],argv[2],id,std::stoul(argv[4]));
    Composition c(r.scheme,r.shared,rules,lc1::input_config(rules),r.storage.view());
    require(c.edit_raw(0,0,"le")==Status::Ready,"no-chain seed");
    require(r.query(c,"no_chain")==Status::PageReady,"no-chain formal result");
    require(c.planning_report().queries==0&&observe::counters.prefix_offers==0&&
            observe::counters.prefix_saves==0,"no fabricated prefix state on direct path");
    require(c.edit_raw(0,2,raw)==Status::Ready,"prefix raw");
    require(c.query_next({},1)==Status::Working,"single-work query");
    auto operation=c.operation();observe::counters={};
    Status status=Status::Working;std::size_t ticks=0;
    while(status==Status::Working) {
      require(++ticks<1000000,"bounded one-work slices");
      const auto step=c.tick(operation,{1,1024,1});
      require(step.core_cost.work_items<=1&&step.local_work<=1&&step.core_cost.read_calls<=1,
              "checkpoint fits the actual one-work tick");
      require(c.planning_report().core_cost.work_items<=200000&&c.planning_report().local_work<=200000,
              "checkpoint and pool accounting fit total budgets");
      status=step.status;
    }
    require(status==Status::PageReady,"one-work query finishes");
    std::cout<<"PREFIX_BOUNDARY PASS one-work ticks="<<ticks<<" saves="<<observe::counters.prefix_saves<<'\n';

    require(c.drop_planning_cache()==Status::Ready,"fresh cancellation round");
    observe::counters={};require(c.query_next({},2)==Status::Working,"start completed-source cancellation");
    operation=c.operation();
    for(std::size_t count=0;observe::counters.prefix_saves==0;++count) {
      require(count<100000,"reach actual completed prefix checkpoint");
      require(c.tick(operation,r.budget).status==Status::Working,"pending graph before cancellation");
    }
    const auto retained_raw=std::string(c.snapshot().raw);
    require(c.cancel(operation)==Status::Cancelled&&c.snapshot().raw==retained_raw,"cancel preserves editable input");
    require(r.query(c,"after_cancel",0,2)==Status::PageReady,"new round after cancellation");
    require(observe::counters.prefix_resets>=8,"new round clears every old prefix slot before use");
    require(c.cancel(operation)==Status::Stale,"old operation cannot cancel successor");
    const auto old_selection=c.selection(0);
    require(r.query(c,"context_changed",0,3)==Status::PageReady&&observe::counters.retained==0,
            "context retires MR1 facts and prefix session independently");
    require(c.select(old_selection)==Status::Stale,"no Selection privilege from continuation");
    std::cout<<"PREFIX_BOUNDARY PASS completed-source cancellation/context/Selection\n";

    for(bool local_limit:{false,true}) {
      auto config=lc1::input_config(rules);
      if(local_limit)config.planning.local_work=1;else config.planning.core_work=1;
      Composition small(r.scheme,r.shared,rules,config,r.storage.view());
      require(small.edit_raw(0,0,raw)==Status::Ready,"budget negative raw");
      observe::counters={};status=small.query_next({},4);const auto small_operation=small.operation();
      for(std::size_t count=0;status==Status::Working;++count) {
        require(count<100000,"budget negative bounded");
        const auto step=small.tick(small_operation,r.budget);status=step.status;
        require(small.planning_report().core_cost.work_items<=config.planning.core_work&&
                small.planning_report().local_work<=config.planning.local_work,"no cleanup/checkpoint budget overrun");
      }
      require(status==Status::Unresolved&&small.planning_report().limited&&
              small.snapshot().raw==raw&&observe::counters.prefix_saves==0,
              "budget exhaustion is Unknown/Limited, never an authenticated miss");
    }
    std::cout<<"PREFIX_BOUNDARY PASS core/local one-unit exhaustion\n";
    return 0;
  } catch(const std::exception& e){std::cerr<<"PREFIX_BOUNDARY FAIL "<<e.what()<<'\n';return 1;}
}
