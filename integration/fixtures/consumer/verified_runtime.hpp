#pragma once
#include "runtime.hpp"
#include "hanime/base_order.hpp"
#include "release_pin.hpp"

namespace extension_host {
// Only immutable, byte-verified release pins compiled with this consumer.
// The JSON beside the resource is not loaded to establish trust.
struct VerifiedRuntime : Runtime {
  std::shared_ptr<VerifiedResourceReader> shared_reader, scheme_reader, order_reader;
  std::shared_ptr<BaseOrderIndex> order;
  std::shared_ptr<VerifiedResourceReader> open_reader(const std::filesystem::path& data,
      const std::filesystem::path& index,const ResourceReleaseSpec& pin,std::size_t blocks) {
    std::shared_ptr<HostResourceFile> d;
    if(pin.verification_format==1){d=std::make_shared<HostResourceFile>(data.string(),1024,metrics);require(d->valid(),"data open");}
    auto h=std::make_shared<HostResourceFile>(index.string(),1024,metrics);require(h->valid(),"proof open");
    auto v=std::make_shared<VerifiedResourceReader>(d,h,pin,VerifiedResourceConfig{blocks,8,1024});
    require(v->prepare_bootstrap(),"bootstrap begin");
    for(std::size_t i=0;!v->bootstrap_ready();++i){require(i<1000000,"bootstrap timeout");auto s=v->advance_bootstrap(budget);validation.add(s.cost);require(v->healthy(),v->error());}
    return v;
  }
  void mount_pinned(const std::filesystem::path& logical,const std::filesystem::path& transport,
                    const std::string& id,std::size_t blocks=16) {
    require(id=="xiaohe"||id=="full_pinyin","invalid scheme");
    const auto& pin=id=="xiaohe"?hanime_release::xiaohe:hanime_release::full_pinyin;
    const auto& base=id=="xiaohe"?hanime_release::xiaohe_base:hanime_release::full_pinyin_base;
    shared_reader=open_reader({},transport/"shared.hnv",hanime_release::shared,blocks);
    scheme_reader=open_reader(logical/(id+".hnm"),transport/(id+".hnv"),pin,blocks);
    order_reader=open_reader(transport/(id+"_base.hbo"),transport/(id+"_base.hnv"),base,8);
    std::string error;
    require(shared.open_verified(shared_reader,1,{cache_shared,1024},&error),error.c_str());
    require(scheme.open_verified(scheme_reader,shared,{cache_scheme,1024},&error),error.c_str());
    order=std::make_shared<BaseOrderIndex>(order_reader,scheme,shared);
    require(order->ready()&&scheme.attach_base_order(order),"BaseOrder attachment");
    require(admit()==TextAccessStatus::Ready,"trusted text admission");
  }
  void print_verified_memory() const {
    std::size_t owners=0,payload=0,metadata=0;
    for(const auto& v:{shared_reader,scheme_reader,order_reader}){auto m=v->memory_usage();owners+=m.owner_fixed_bytes;payload+=m.data_payload_bytes+m.index_payload_bytes;metadata+=m.data_metadata_bytes+m.index_metadata_bytes;}
    std::cout<<"AUTH_MEMORY owners="<<owners<<" payload="<<payload<<" metadata="<<metadata<<" base_order="<<order->memory_bytes()<<'\n';
  }
};
#ifndef EXTENSION_BASELINE_ONLY
struct QueryMeasurement {
  Status status{Status::Working};
  std::uint64_t steps{0},ns{0},max_tick_ns{0},setup_ns{0},local_tick_ns{0},core_tick_ns{0};
  WorkCost core;
  std::size_t local{0};
};
inline QueryMeasurement measured_query(Runtime& r,Composition& c,bool plan=true,OutputRequest request={},std::uint64_t context=0) {
  QueryMeasurement m;
  const auto begin=std::chrono::steady_clock::now();
  m.status=plan?c.query_next(request,context):c.query_longest();
  m.setup_ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count());
  const auto op=c.operation();
  while(m.status==Status::Working) {
    require(m.steps<5000000,"measured query timeout");
    const auto t=std::chrono::steady_clock::now();auto s=c.tick(op,r.budget);
    const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();
    m.max_tick_ns=std::max(m.max_tick_ns,static_cast<std::uint64_t>(elapsed));
    if(s.core_cost.work_items||s.core_cost.read_calls)m.core_tick_ns+=static_cast<std::uint64_t>(elapsed);
    else m.local_tick_ns+=static_cast<std::uint64_t>(elapsed);
    require(s.core_cost.read_calls<=r.budget.max_read_calls&&s.core_cost.read_bytes<=r.budget.max_read_bytes&&
      s.core_cost.work_items<=r.budget.max_work_items&&s.local_work<=r.budget.max_work_items,"planning tick exceeded budget");
    ++m.steps;m.core.read_calls+=s.core_cost.read_calls;m.core.read_bytes+=s.core_cost.read_bytes;m.core.work_items+=s.core_cost.work_items;m.local+=s.local_work;m.status=s.status;
  }
  m.ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count());
  return m;
}
inline void print_query(std::string_view label,const QueryMeasurement& m,const Composition& c) {
  const auto p=c.planning_report();
  std::cout<<"QUERY label="<<label<<" status="<<name(m.status)<<" begin="<<c.page().begin<<" end="<<c.page().end
    <<" covered="<<p.covered_end<<" limited="<<p.limited<<" incomplete="<<p.incomplete_tail<<" unknown="<<p.unknown_tail
    <<" queries="<<p.queries<<" cache="<<p.cache_hits<<" checked="<<p.checked_ranges<<" plan_reads="<<p.core_cost.read_calls
    <<" plan_bytes="<<p.core_cost.read_bytes<<" plan_work="<<p.core_cost.work_items<<" plan_local="<<p.local_work
    <<" total_reads="<<m.core.read_calls<<" total_bytes="<<m.core.read_bytes<<" total_work="<<m.core.work_items
    <<" local="<<m.local<<" steps="<<m.steps<<" setup_work="<<c.command_work()<<" setup_ns="<<m.setup_ns
    <<" local_tick_ns="<<m.local_tick_ns<<" core_tick_ns="<<m.core_tick_ns<<" host_ns="<<m.ns<<" max_tick_ns="<<m.max_tick_ns<<'\n';
}
#endif
}
