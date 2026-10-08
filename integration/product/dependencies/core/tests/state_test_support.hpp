#pragma once
#include "hanime/bounded_user_state.hpp"
#include "hanime/host_state_files.hpp"
#include "session_fixture.hpp"
#include "rank_allocation_probe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <unistd.h>

namespace hanime::state_test {
using test::require_session;
using test::session_key;
constexpr RegistryNamespaceId test_ns=0x53455353494f4eULL;
constexpr std::uint64_t lineage=0x535445503245ULL;
constexpr StateBudget small_budget{1,1,17,17,1,1};
constexpr SessionConfig session_config{4,3,1,16,32,32};
inline void check(bool b,const char* msg) {require_session(b,msg);}
inline bool pending(SessionStatus s) {return s==SessionStatus::Pending || s==SessionStatus::WaitingState;}
inline StateCost delta(StateCost a,const StateCost& b) {
  a.read_calls-=b.read_calls; a.write_calls-=b.write_calls; a.read_bytes-=b.read_bytes;
  a.write_bytes-=b.write_bytes; a.control_calls-=b.control_calls; a.work_items-=b.work_items; return a;
}
inline void check_cost(const StateStep& s,StateBudget b,const HostStateMetrics& before,const HostStateMetrics& after) {
  auto d=delta(after.requests,before.requests); const auto& c=s.cost;
  check(c.read_calls<=b.read_calls && c.write_calls<=b.write_calls && c.read_bytes<=b.read_bytes &&
      c.write_bytes<=b.write_bytes && c.control_calls<=b.control_calls && c.work_items<=b.work_items,"state budget exceeded");
  check(d.read_calls==c.read_calls && d.write_calls==c.write_calls && d.read_bytes==c.read_bytes &&
      d.write_bytes==c.write_bytes && d.control_calls==c.control_calls,"unmetered state syscall");
  check(c.read_calls+c.write_calls+c.control_calls<=1,"multiple physical calls in one step");
}
struct Totals {StateCost cost; std::uint64_t steps{0};};
template<class F> StateStep measured(HostStateFiles& files,Totals& totals,StateBudget b,F f) {
  auto before=files.metrics; StateStep s;
  {rank_probe::Capture capture; s=f();}
  check_cost(s,b,before,files.metrics); totals.cost.add(s.cost); ++totals.steps; return s;
}
inline void print_cost(const std::string& label,const Totals& t) {
  auto& c=t.cost;
  std::cout<<label<<" steps="<<t.steps<<" read_calls="<<c.read_calls<<" read_bytes="<<c.read_bytes
    <<" write_calls="<<c.write_calls<<" write_bytes="<<c.write_bytes<<" controls="<<c.control_calls<<" work="<<c.work_items<<'\n';
}
struct Ledger {
  std::string a,b; HostStateFiles files; StateV3 disk; BoundedUserState state; Totals totals;
  Ledger(const std::string& dir,std::size_t h=4,std::uint64_t threshold=8,std::uint64_t cap=16*1024*1024,
         RegistryNamespaceId ns=test_ns,std::uint64_t lin=lineage)
    :a(dir+"/a.state"),b(dir+"/b.state"),files(a,b),disk(files,{ns,lin,17,threshold,cap}),state(disk,ns,{h,64,8}) {}
  StateStatus open(bool create=true,StateBudget budget=small_budget) {
    check(disk.begin_open(create)==StateStatus::Pending,"begin open");
    for (unsigned i=0;i<5'000'000;++i) {
      auto s=measured(files,totals,budget,[&]{return disk.advance_open(budget);});
      if (s.status!=StateStatus::Pending) return s.status;
    } throw std::runtime_error("open stalled");
  }
  StateStep background(StateBudget b=small_budget) {
    return measured(files,totals,b,[&]{return state.advance_state(b).io;});
  }
  void flush(StateBudget b=small_budget) {
    for (unsigned i=0;i<5'000'000;++i) {
      const auto h=state.health();
      if (!h.dirty && !h.intent_pending && !h.frozen_write && !disk.info().maintaining) return;
      auto s=background(b); check(s.status==StateStatus::Pending || s.status==StateStatus::Ready || s.status==StateStatus::HostStorageConfirmed,"flush failed");
    } throw std::runtime_error("flush stalled");
  }
  void maintenance(StateBudget b=small_budget) {
    check(disk.begin_maintenance()==StateStatus::Pending,"begin maintenance");
    for (unsigned i=0;i<5'000'000;++i) {
      auto s=measured(files,totals,b,[&]{return disk.advance_maintenance(b);});
      if (s.status==StateStatus::Ready) return;
      check(s.status==StateStatus::Pending,"maintenance failed");
    } throw std::runtime_error("maintenance stalled");
  }
  void write(std::size_t key,std::uint32_t count,StateBudget b=small_budget) {
    auto start=disk.begin_write(session_key(key),count);
    if (start==StateStatus::MaintenanceBlocked) {maintenance(b); start=disk.begin_write(session_key(key),count);}
    check(start==StateStatus::Pending,"begin absolute write");
    for (unsigned i=0;i<1'000'000;++i) {
      auto s=measured(files,totals,b,[&]{return disk.advance_write(b);});
      if (s.status==StateStatus::HostStorageConfirmed) return;
      check(s.status==StateStatus::Pending,"absolute write failed");
    } throw std::runtime_error("write stalled");
  }
  CountResult advance(const CountTicket& t,WorkBudget b={1,17,1}) {
    const auto before=files.metrics; CountResult r;
    {rank_probe::Capture capture; r=state.advance_count(t,b);}
    StateStep s; s.cost={r.cost.read_calls,0,r.cost.read_bytes,0,0,r.cost.work_items};
    check_cost(s,{b.max_read_calls,0,b.max_read_bytes,0,0,b.max_work_items},before,files.metrics);
    totals.cost.add(s.cost); ++totals.steps; return r;
  }
  std::uint32_t count(std::size_t key,bool found=true) {
    auto t=state.begin_count(state.view(),session_key(key));
    for (unsigned i=0;i<1'000'000;++i) {
      auto r=advance(t);
      if (r.status==CountStatus::Found || r.status==CountStatus::NotFound) {
        check(found==(r.status==CountStatus::Found),"unexpected key existence"); state.cancel_count(t); return r.count;
      }
      check(r.status==CountStatus::Pending,"lookup failed");
    } throw std::runtime_error("count stalled");
  }
  StateStatus close() {
    state.begin_close();
    for (unsigned i=0;i<5'000'000;++i) {
      auto s=measured(files,totals,small_budget,[&]{return state.advance_close(small_budget).io;});
      if (s.status==StateStatus::ClosedSaved || s.status==StateStatus::CloseUnsaved) return s.status;
    } throw std::runtime_error("close stalled");
  }
};
inline std::string directory(const std::string& root,const std::string& name) {
  static unsigned id=0;
  auto p=root+"/"+name+"-"+std::to_string(::getpid())+"-"+std::to_string(++id);
  check(std::filesystem::create_directories(p),"test directory already exists"); return p;
}
inline std::vector<char> bytes(const std::string& path) {
  std::ifstream f(path,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}};
}
// Test-only generator/oracle: allowed full maps/resources; never used by owner.
struct Resources {
  test::SessionFixture base;
  explicit Resources(std::size_t n,bool reverse=false):base(n,reverse) {
    for (std::size_t i=1;i<=n;++i) {
      base.input.mappings.push_back({"full_pinyin","k"+std::to_string(i),session_key(i),0});
      base.input.mappings.push_back({"xiaohe","x"+std::to_string(i),session_key(i),0});
    }
    auto built=ResourceBuilder().build(base.input,{96,96}); check(built.ok,"state resource build");
    base.built=std::move(built.resources); base.mount();
  }
};
inline void admit(CandidateSession& s) {
  check(s.begin_text_access()==TextAccessStatus::Pending,"begin text access");
  for (unsigned i=0;i<10'000'000;++i) {
    TextAccessStep r; {rank_probe::Capture c; r=s.advance_text_access({1,17,3});}
    if (r.status==TextAccessStatus::Ready) return;
    check(r.status==TextAccessStatus::Pending,"admit failed");
  } throw std::runtime_error("admit stalled");
}
inline SessionStatus finish(CandidateSession& s,Ledger& l,bool background=false) {
  for (unsigned i=0;i<10'000'000;++i) {
    SessionStep r; {rank_probe::Capture c; r=s.tick({1,17,3});}
    if (background) {l.state.foreground_advanced(); if (l.state.background_due()) l.background();}
    if (!pending(r.status)) return r.status;
  } throw std::runtime_error("session stalled");
}
inline void publish(CandidateSession& s,Ledger& l) {
  check(finish(s,l)==SessionStatus::StagingReady,"staging not ready");
  check(s.publish(s.staging())==SessionStatus::Ready,"publish");
}
inline void first(CandidateSession& s,Ledger& l,std::string_view code="same",ScoringMode mode=ScoringMode::Exact) {
  check(s.begin_input(code,9,3,3,mode)==SessionStatus::Pending,"begin session"); publish(s,l);
}
inline std::vector<RankItem> page(CandidateSession& s) {
  std::vector<RankItem> r;
  check(s.present([&](const CandidateViewPage& v) {
    for (std::size_t i=0;i<v.size;++i) {check(!v.text(i).empty(),"no ready text"); r.push_back(v.items[i].ranked);}
  })==SessionStatus::Ready,"present"); return r;
}
inline CommitResult commit(CandidateSession& s,Ledger& l,std::size_t key,ScoringMode mode=ScoringMode::BaseOnly) {
  first(s,l,"k"+std::to_string(key),mode);
  auto select=s.selection(0); check(s.confirm(select).status==CommitStatus::Preparing,"confirm");
  for (unsigned i=0;i<100000;++i) {
    auto result=s.commit_result();
    if (result.status!=CommitStatus::Preparing && result.status!=CommitStatus::CallingSink) {
      check(result.status==CommitStatus::Accepted,"text not accepted");
      check(s.confirm(select).status==CommitStatus::Accepted,"old accepted confirm changed"); return result;
    }
    s.tick({1,17,3});
  } throw std::runtime_error("commit stalled");
}
inline void intent_done(Ledger& l) {
  for (unsigned i=0;i<1'000'000;++i) {if (!l.state.health().intent_pending) return; l.background({1,0,17,0,0,1});}
  throw std::runtime_error("intent stalled");
}
} // namespace hanime::state_test
