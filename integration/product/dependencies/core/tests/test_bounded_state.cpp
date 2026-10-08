#include "state_test_support.hpp"
#include <limits>
#include <optional>

namespace {
using namespace hanime;
using namespace hanime::state_test;
std::string root;

void file_basics() {
  Ledger l(directory(root,"basics"));
  check(l.disk.begin_open(false)==StateStatus::Pending,"begin registers only");
  const StateBudget zero_control{1,1,17,17,0,1};
  for(unsigned i=0;i<3;++i) measured(l.files,l.totals,zero_control,[&]{return l.disk.advance_open(zero_control);});
  check(l.files.metrics.requests.control_calls==0,"open ignored zero controls");
  auto invalid=measured(l.files,l.totals,{1,1,17,17,1,0},[&]{return l.disk.advance_open({1,1,17,17,1,0});});
  check(invalid.status==StateStatus::InvalidBudget,"zero work invalid");
  while(l.disk.status()==StateStatus::Pending) measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_open(small_budget);});
  check(l.open(false)==StateStatus::MissingStore,"missing is not empty ready");
  check(l.open()==StateStatus::Ready,"create V3");
  l.write(1,4); l.write(1,9); l.write(2,7);
  check(l.count(1)==9 && l.count(2)==7 && l.count(3,false)==0,"latest tail absolute values");
  auto ticket=l.disk.begin_lookup(StateReadRole::RankLookup,session_key(1));
  for (unsigned i=0;i<5;++i) {
    const StateBudget no_reads{0,1,0,17,1,1};
    auto s=measured(l.files,l.totals,no_reads,[&]{return l.disk.advance_lookup(ticket,no_reads);});
    check(s.status==StateStatus::Pending,"zero read completed unexpectedly");
  }
  auto next=l.disk.begin_lookup(StateReadRole::RankLookup,session_key(2)); l.disk.cancel_read(ticket);
  check(l.disk.read_is_owned(next) && !l.disk.read_is_owned(ticket),"foreign cancel");
  l.disk.cancel_read(next);
  const auto old_view=l.state.view();
  l.maintenance({1,1,1,1,1,1});
  check(l.state.view_is_current(old_view),"equivalent compaction changed logical view");
  check(l.disk.info().active_file==1 && l.disk.info().snapshot_keys==2,"A to B");
  l.write(2,11,{1,1,1,1,1,1}); l.maintenance();
  check(l.disk.info().active_file==0 && l.disk.info().generation==3,"B to A");
  check(l.count(2)==11,"compaction value");
  check(l.close()==StateStatus::ClosedSaved,"close basics");
  print_cost("E1 tiny-budget file operations",l.totals);
}

void learning() {
  Ledger l(directory(root,"learning")); check(l.open()==StateStatus::Ready,"open");
  l.write(1,4); l.write(6,std::numeric_limits<std::uint32_t>::max());
  Resources h(8); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
  const auto calls=h.base.sink.calls;
  auto one=commit(s,l,1); check(one.learning==LearningStatus::Pending,"unknown old count must pend");
  check(l.state.notify_committed(one.transaction,test_ns,session_key(1))==LearningStatus::Pending,"repeat pending");
  auto paused=commit(s,l,2); check(paused.learning==LearningStatus::PausedCapacity,"intent busy must pause");
  intent_done(l); auto receipt=l.state.observe_learning(one.transaction.learning_ticket());
  check(receipt.status==LearningStatus::RecordedDirty && receipt.local_work==8 && l.count(1)==5,"learn from true old count once");
  check(l.state.notify_committed(one.transaction,test_ns,session_key(1))==LearningStatus::RecordedDirty && l.count(1)==5,"duplicate count");
  check(l.state.notify_committed(paused.transaction,test_ns,session_key(2))==LearningStatus::PausedCapacity,"paused replay");
  check(l.state.notify_committed(one.transaction,test_ns+1,session_key(1))==LearningStatus::Stale,"foreign namespace");
  auto known=commit(s,l,1); check(known.learning==LearningStatus::RecordedDirty && l.count(1)==6,"known RAM count");
  for (std::size_t k:{3,4,5}) {auto r=commit(s,l,k); check(r.learning==LearningStatus::Pending,"reserve other slot"); intent_done(l);}
  check(l.state.health().dirty==4,"H dirty");
  check(commit(s,l,7).learning==LearningStatus::PausedCapacity,"full dirty did not pause");
  check(commit(s,l,1).learning==LearningStatus::RecordedDirty && l.count(1)==7,"full dirty blocked safe known learning");
  check(h.base.sink.calls==calls+8,"text duplicated");
  check(s.bind_scheme(h.base.xiaohe)==SessionStatus::Ready,"real state scheme switch");
  check(s.set_output({"test:variant",OutputInvocation::Profile})==SessionStatus::Ready,"generic form switch");
  first(s,l,"x1"); auto same_identity=page(s);
  check(same_identity.size()==1 && same_identity[0].key==session_key(1) && same_identity[0].effective_count==7,"scheme/form forked learning");
  s.cancel_input(s.input_handle()); s.tick({1,17,3});
  check(s.bind_scheme(h.base.full)==SessionStatus::Ready && s.set_output({})==SessionStatus::Ready,"return full/default");
  l.flush(); auto saturated=commit(s,l,6); check(saturated.learning==LearningStatus::Pending,"saturation load");
  const auto v=l.state.view(); intent_done(l);
  check(l.state.observe_learning(saturated.transaction.learning_ticket()).status==LearningStatus::NoChangeSaturated && l.state.view_is_current(v),"saturation changed version");
  l.flush();
  LearningTicket survivor;
  {
    CandidateSession transient(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(transient);
    auto r=commit(transient,l,8); survivor=r.transaction.learning_ticket();
    check(r.learning==LearningStatus::Pending,"transient intent");
  }
  check(l.state.health().intent_pending,"session dtor lost learning");
  for(unsigned i=0;i<3;++i) l.background();
  check(l.open(false)==StateStatus::Ready && l.state.health().intent_pending,"I/O reopen lost accepted intent");
  intent_done(l);
  check(l.state.observe_learning(survivor).status==LearningStatus::RecordedDirty && l.count(8)==1,"intent outlives session");
  check(l.close()==StateStatus::ClosedSaved,"learning close");
}

void late_work() {
  Ledger l(directory(root,"late")); check(l.open()==StateStatus::Ready,"open"); l.write(1,4); l.write(2,77);
  Resources h(6); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
  auto old=l.state.begin_count(l.state.view(),session_key(1));
  for (unsigned i=0;i<7;++i) check(l.advance(old).status==CountStatus::Pending,"read did not wait");
  auto r=commit(s,l,1); check(r.learning==LearningStatus::Pending,"intent"); intent_done(l);
  for (unsigned i=0;i<8 && !l.state.health().frozen_write;++i) l.background({0,0,0,0,0,1});
  check(l.state.health().frozen_write,"did not freeze value 5");
  check(commit(s,l,1).learning==LearningStatus::RecordedDirty,"RAM 6 update");
  check(l.state.health().rank_pending,"test replaced old pending count prematurely");
  check(l.advance(old).status==CountStatus::Stale && l.count(1)==6,"late 4 overwrote 6");
  auto newer=l.state.begin_count(l.state.view(),session_key(2)); l.state.cancel_count(old);
  CountResult result; do {result=l.advance(newer);} while(result.status==CountStatus::Pending);
  check(result.status==CountStatus::Found && result.count==77,"old cancel damaged new count"); l.state.cancel_count(newer);
  // Intent advancement may have frozen 5 already. Finish it explicitly and
  // observe its own event; newer 6 must remain dirty until its own write.
  bool saw5=false,saw6=false;
  for (unsigned i=0;i<100000 && l.state.health().dirty;++i) {
    auto before=l.files.metrics; auto step=l.state.advance_state(small_budget); check_cost(step.io,small_budget,before,l.files.metrics);
    if (step.event.confirmed) {
      if (step.event.count==5) {saw5=true; check(l.state.health().dirty==1 && l.count(1)==6,"write5 cleared dirty6");}
      if (step.event.count==6) saw6=true;
    }
  }
  check(saw5 && saw6,"did not exercise frozen 5 then 6");
  const auto view=l.state.view(); l.maintenance(); check(l.state.view_is_current(view),"maintenance invalidated view");
  // Real pending count held by session rank must be released on version change.
  first(s,l); check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"next range");
  bool waiting=false;
  for (unsigned i=0;i<100000;++i) {
    auto step=s.tick({1,1,1});
    if (step.status==SessionStatus::WaitingState) {waiting=true; break;}
    check(pending(step.status),"next range prematurely ended");
  }
  check(waiting,"real count did not wait");
  // Re-notification is idempotent; a separate accepted transaction changes view.
  Resources independent(6);
  CandidateSession other(independent.base.full,independent.base.shared,l.state,h.base.sink,session_config); admit(other);
  auto accepted=commit(other,l,1); check(accepted.learning==LearningStatus::RecordedDirty,"change view");
  check(s.tick({1,17,3}).status==SessionStatus::NeedsRankRestart,"old ranking not stale");
  check(!l.state.health().rank_pending,"terminated real count retained ticket");
  auto ext=l.state.begin_count(l.state.view(),session_key(2));
  s.cancel_input(s.input_handle()); s.tick({1,17,3});
  do {result=l.advance(ext);} while(result.status==CountStatus::Pending);
  check(result.status==CountStatus::Found && result.count==77,"terminal cleanup hurt external count");
  l.state.cancel_count(ext); l.flush(); check(l.close()==StateStatus::ClosedSaved,"close late");
}

void file_faults() {
  const auto dir=directory(root,"faults"); Ledger l(dir); check(l.open()==StateStatus::Ready,"open"); l.write(1,4);
  check(l.disk.begin_write(session_key(1),5)==StateStatus::Pending,"start uncertain");
  l.files.fault={StateFileOperation::Write,0,1,0,true};
  auto failed=measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_write(small_budget);});
  check(failed.status==StateStatus::Unavailable && l.disk.info().tail_frames==1,"unconfirmed published");
  const auto original=bytes(l.a);
  {
    Ledger restart(dir); check(restart.open(false)==StateStatus::TailUncertain,"new process model guessed tail");
    check(bytes(l.a)==original,"uncertain tail mutated");
  }
  for (unsigned i=0;i<10000;++i) {
    auto step=measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_write(small_budget);});
    if (step.status==StateStatus::HostStorageConfirmed) break;
    check(step.status==StateStatus::Pending,"own writer failed retry");
  }
  check(l.count(1)==5 && l.disk.info().tail_frames==2,"retry duplicated tail");
  l.files.fault={StateFileOperation::Write,0,3,1,false}; l.write(2,9);
  check(l.count(2)==9,"short write completion");
  l.maintenance(); const auto valid_a=bytes(l.a),valid_b=bytes(l.b);
  for(const auto& change:{std::make_pair(8U,99),std::make_pair(12U,2)}) {
    const auto mutant=directory(root,"incompatible"); auto a=valid_a; a[change.first]=static_cast<char>(change.second);
    {std::ofstream f(mutant+"/a.state",std::ios::binary); f.write(a.data(),static_cast<std::streamsize>(a.size()));
     std::ofstream g(mutant+"/b.state",std::ios::binary); g.write(valid_b.data(),static_cast<std::streamsize>(valid_b.size()));}
    Ledger reject(mutant); check(reject.open(false)==StateStatus::Incompatible && bytes(reject.a)==a,"unknown version/key format not preserved");
  }
  {
    Ledger foreign(dir,4,8,16*1024*1024,test_ns+1);
    check(foreign.open(false)==StateStatus::Incompatible,"wrong namespace");
    check(bytes(l.a)==valid_a && bytes(l.b)==valid_b,"wrong namespace overwrote files");
  }
  check(l.close()==StateStatus::ClosedSaved,"fault close");
  const auto old=directory(root,"v2");
  {std::ofstream a(old+"/a.state"); a<<"HANIME_USER_STATE_V2\n"; std::ofstream b(old+"/b.state"); b<<"HANIME_USER_STATE_V2\n";}
  const auto original_v2=bytes(old+"/a.state"); Ledger v2(old);
  check(v2.open()==StateStatus::Incompatible && bytes(old+"/a.state")==original_v2,"V2 overwritten or wrong diagnosis");
  const auto missing=directory(root,"partial"); {std::ofstream a(missing+"/a.state"); a<<"existing";}
  Ledger part(missing); check(part.open()==StateStatus::Corrupt && !std::filesystem::exists(missing+"/b.state"),"partial missing created fresh store");
  // Corrupt highest committed snapshot must not silently select the old one.
  const auto target=l.disk.info().active_file==0 ? l.a : l.b;
  {std::fstream f(target,std::ios::in|std::ios::out|std::ios::binary); f.seekp(128); f.put('\0');}
  const auto broken=bytes(target); Ledger bad(dir);
  check(bad.open(false)==StateStatus::Corrupt && bytes(target)==broken,"corrupt highest fell back or repaired");
  print_cost("E4 failures and reconciliation",l.totals);
}

void maintenance_and_close() {
  const auto dir=directory(root,"maint"); Ledger l(dir,4,4); check(l.open()==StateStatus::Ready,"open");
  for (unsigned i=1;i<=4;++i) l.write(i,i);
  const auto active=l.disk.info().active_file; const auto prior=bytes(active ? l.b : l.a);
  check(l.disk.begin_maintenance()==StateStatus::Pending,"maint begin");
  // Advance past valid staging header and zero seal, then interrupt a write.
  for (unsigned i=0;i<12;++i) measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_maintenance(small_budget);});
  {
    Ledger restart(dir); check(restart.open(false)==StateStatus::Ready && restart.count(4)==4,"staging replaced authority");
  }
  l.files.fault={StateFileOperation::Write,0,1,0,false};
  bool failed=false;
  for (unsigned i=0;i<10000;++i) {
    auto r=measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_maintenance(small_budget);});
    if (r.status==StateStatus::Unavailable) {failed=true; break;}
  }
  check(failed && bytes(active ? l.b : l.a)==prior && l.count(4)==4,"maintenance destroyed active");
  Resources h(8); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
  auto learned=commit(s,l,4); check(learned.learning==LearningStatus::Pending,"maintenance RAM learning");
  intent_done(l); check(l.count(4)==5,"maintenance lost new learning");
  l.flush(); check(l.count(4)==5,"cut dropped post-cut dirty");
  l.maintenance(); check(l.disk.info().active_file==active,"A B A reuse");
  commit(s,l,4); check(l.state.health().dirty==1,"close dirty missing");
  l.files.fault={StateFileOperation::Sync,0,20,0,false};
  check(l.close()==StateStatus::CloseUnsaved && l.state.health().dirty==1,"close failure claimed saved");
  auto r=commit(s,l,4); check(r.learning==LearningStatus::PausedClosed && r.status==CommitStatus::Accepted,"closing blocked text");
  l.files.fault={}; check(l.close()==StateStatus::ClosedSaved,"retry close");
  Ledger reopened(dir); check(reopened.open(false)==StateStatus::Ready && reopened.count(4)==6,"close/reopen count");
  // File limit reports maintenance blocked; never deletes keys to fit.
  Ledger cap(directory(root,"capacity"),4,1,236); check(cap.open()==StateStatus::Ready,"capacity create");
  cap.write(1,1); cap.write(2,2);
  check(cap.disk.begin_maintenance()==StateStatus::Pending,"capacity maintain");
  StateStatus status=StateStatus::Pending;
  for (unsigned i=0;i<10000 && status==StateStatus::Pending;++i)
    status=measured(cap.files,cap.totals,small_budget,[&]{return cap.disk.advance_maintenance(small_budget);}).status;
  check(status==StateStatus::Ready && cap.disk.begin_write(session_key(3),3)==StateStatus::MaintenanceBlocked &&
      cap.disk.begin_maintenance()==StateStatus::MaintenanceBlocked && cap.count(1)==1 && cap.count(2)==2,"capacity removed key or endless maintenance");
}

void ranking_scale(std::size_t n) {
  const auto dir=directory(root,"scale"); Ledger l(dir); check(l.open()==StateStatus::Ready,"open scale");
  std::map<StableLexemeKey,std::uint32_t> oracle;
  for (std::size_t k=1;k<=n;++k) {const auto count=static_cast<std::uint32_t>((k*7)%19+1); l.write(k,count); oracle[session_key(k)]=count;}
  l.maintenance();
  auto measure_begin=l.totals;
  l.write(n,oracle.at(session_key(n)));
  print_cost("N"+std::to_string(n)+" one append+sync+readback",{delta(l.totals.cost,measure_begin.cost),l.totals.steps-measure_begin.steps});
  measure_begin=l.totals;
  check(l.disk.begin_write(session_key(n),oracle.at(session_key(n)))==StateStatus::Pending,"retry cost begin");
  l.files.fault={StateFileOperation::Write,0,1,0,true};
  for (unsigned i=0;i<10000;++i) {
    const auto st=measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_write(small_budget);}).status;
    if (st==StateStatus::HostStorageConfirmed) break;
    check(st==StateStatus::Pending || st==StateStatus::Unavailable,"retry cost failed");
  }
  print_cost("N"+std::to_string(n)+" uncertain retry",{delta(l.totals.cost,measure_begin.cost),l.totals.steps-measure_begin.steps});
  // Full small tail, with repeated and out-of-order keys, for O(U+L^2) cost.
  for (unsigned k=1;k<=6;++k) l.write(k,oracle.at(session_key(k)));
  measure_begin=l.totals; l.maintenance();
  print_cost("N"+std::to_string(n)+" maintenance tail8",{delta(l.totals.cost,measure_begin.cost),l.totals.steps-measure_begin.steps});
  check(l.close()==StateStatus::ClosedSaved,"seed close");
  Resources resources(n); auto& h=resources.base;
  rank_probe::start(); std::optional<Ledger> holder;
  {rank_probe::Capture capture; holder.emplace(dir);}
  auto& run=*holder; check(run.open(false)==StateStatus::Ready,"reopen scale");
  Totals opening=run.totals; print_cost("N"+std::to_string(n)+" open",opening);
  auto c0=run.totals; check(run.count(n)==oracle.at(session_key(n)),"cold value");
  print_cost("N"+std::to_string(n)+" cold",{delta(run.totals.cost,c0.cost),run.totals.steps-c0.steps});
  std::optional<CandidateSession> session;
  {rank_probe::Capture capture; session.emplace(h.full,h.shared,run.state,h.sink,session_config);}
  auto& s=*session; admit(s);
  for (std::size_t k=1;k<=4;++k) {check(commit(s,run,k).learning==LearningStatus::Pending,"scale learn"); intent_done(run); ++oracle[session_key(k)];}
  check(run.state.health().dirty==4,"scale all dirty");
  c0=run.totals; check(run.count(1)==oracle.at(session_key(1)),"hot");
  print_cost("N"+std::to_string(n)+" hot",{delta(run.totals.cost,c0.cost),run.totals.steps-c0.steps});
  std::vector<RankItem> expected;
  for (std::size_t k=1;k<=n;++k) {RankItem i; i.key=session_key(k); i.score=static_cast<std::int64_t>(k)+oracle.at(i.key)*100; expected.push_back(i);}
  std::sort(expected.begin(),expected.end(),[](const auto& a,const auto& b){return a.score!=b.score ? a.score>b.score : a.key<b.key;});
  const auto rank_io_start=run.files.metrics.requests;
  const auto rank_state_start=run.state.costs();
  first(s,run); std::vector<RankItem> got=page(s); const auto first_page=got;
  for (;;) {
    check(s.request_range(RangeDirection::After,3,3)==SessionStatus::Pending,"next");
    auto st=finish(s,run);
    if (st==SessionStatus::RangeExhausted) break;
    check(st==SessionStatus::StagingReady && s.publish(s.staging())==SessionStatus::Ready,"page advance");
    auto items=page(s); got.insert(got.end(),items.begin(),items.end());
  }
  check(got.size()==expected.size(),"candidate coverage shortened by H");
  for (std::size_t i=0;i<n;++i) check(got[i].key==expected[i].key && got[i].score==expected[i].score && got[i].path_count==3,"full-range oracle mismatch");
  first(s,run);
  check(s.request_range(RangeDirection::After,3,3)==SessionStatus::Pending,"after first"); publish(s,run);
  check(s.request_range(RangeDirection::Before,3,3)==SessionStatus::Pending,"before"); publish(s,run);
  auto previous=page(s); check(previous.size()==first_page.size(),"before size");
  for (std::size_t i=0;i<previous.size();++i) check(previous[i].key==first_page[i].key,"before mismatch");
  const auto ranking_io=delta(run.files.metrics.requests,rank_io_start);
  auto ranking_state=delta(run.state.costs(),rank_state_start);
  check(ranking_io.read_calls==ranking_state.read_calls && ranking_io.read_bytes==ranking_state.read_bytes,"session file cost omitted");
  print_cost("N"+std::to_string(n)+" full forward+before STATE ONLY",{ranking_state,0});
  const auto probe=rank_probe::stop();
  auto um=run.state.memory_usage(); auto fm=run.disk.memory_usage(); auto sm=s.memory_usage();
  std::cout<<"N"<<n<<" memory owner_impl="<<um.impl<<" H_slots="<<um.slots<<" entry="<<um.entry<<" state_file_impl="<<fm.impl
    <<" reads_included="<<fm.reads<<" validation_included="<<fm.validation<<" maintenance_included="<<fm.maintenance
    <<" write_included="<<fm.write_copy<<" receipt="<<um.receipt<<" HostStateFiles="<<sizeof(HostStateFiles)
    <<" SessionImpl="<<sm.impl_bytes<<" ranking_runtime_probe_current="<<probe.current<<" peak="<<probe.peak<<" supported="<<probe.supported<<'\n';
  const auto shared=h.shared.memory_usage();
  std::cout<<"N"<<n<<" coexist shared_owner="<<shared.owner_fixed_bytes<<" shared_cache="<<shared.cache_payload_allocated_bytes
    <<" shared_cache_meta="<<shared.cache_metadata_allocated_bytes<<" rank_impl="<<sm.ranking.impl_bytes
    <<" rank_work="<<sm.rank_work_bytes<<" rank_result="<<sm.rank_result_bytes<<" snapshots="<<sm.snapshot_bytes
    <<" text_pool="<<sm.text_pool_bytes<<" commit_pool="<<sm.commit_pool_bytes<<" name_pool="<<sm.name_bytes
    <<" raw_batch="<<sm.ranking.batch_capacity_bytes<<" query_impl="<<sm.ranking.raw.impl_bytes
    <<" query_page="<<sm.ranking.raw.page_capacity_bytes<<" io_ticket="<<sizeof(StateIoTicket)<<" count_ticket="<<sizeof(CountTicket)<<'\n';
  auto all=run.files.metrics.requests; all.work_items=opening.cost.work_items+run.state.costs().work_items;
  print_cost("N"+std::to_string(n)+" all measured file traffic",{all,0});
  std::cout<<"N"<<n<<" Host syscall ns="<<run.files.metrics.calls_ns<<" max_ns="<<run.files.metrics.max_call_ns
    <<" pending="<<run.state.health().pending<<" paused="<<run.state.health().paused<<" retries="<<run.state.health().retries<<'\n';
  s.cancel_input(s.input_handle()); s.tick({1,17,3});
  // Same resources, smaller H in a NEW owner after saving: coverage unchanged.
  const auto close_start=run.state.costs();
  check(run.close()==StateStatus::ClosedSaved,"scale close");
  print_cost("N"+std::to_string(n)+" close with four dirty",{delta(run.state.costs(),close_start),0});
  Ledger small(dir,1); check(small.open(false)==StateStatus::Ready,"H1 reopen");
  CandidateSession one(h.full,h.shared,small.state,h.sink,session_config); admit(one); first(one,small);
  auto top=page(one); for (std::size_t i=0;i<top.size();++i) check(top[i].key==expected[i].key && top[i].score==expected[i].score,"H changed result");
  std::size_t visited=top.size();
  for (;;) {
    check(one.request_range(RangeDirection::After,3,3)==SessionStatus::Pending,"H1 next");
    const auto st=finish(one,small); if (st==SessionStatus::RangeExhausted) break;
    check(st==SessionStatus::StagingReady && one.publish(one.staging())==SessionStatus::Ready,"H1 publish");
    for (const auto& item:page(one)) {check(visited<n && item.key==expected[visited].key && item.score==expected[visited].score,"H1 coverage"); ++visited;}
  }
  check(visited==n,"H1 lost later candidate");
  check(small.close()==StateStatus::ClosedSaved,"small close");
}

void policy_and_fairness() {
  const auto dir=directory(root,"policy"); Ledger l(dir); check(l.open()==StateStatus::Ready,"open");
  for (unsigned k=1;k<=8;++k) l.write(k,k);
  l.maintenance(); Resources h(8); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
  check(commit(s,l,1).learning==LearningStatus::Pending,"learn1"); intent_done(l);
  // No write/control allowance: it may capture the one immutable write but
  // cannot make a syscall; front-end lookups still proceed.
  const auto io=l.files.metrics.requests;
  for (unsigned i=0;i<12;++i) l.background({0,0,0,0,0,1});
  auto d=delta(l.files.metrics.requests,io);
  check(!d.read_calls && !d.write_calls && !d.control_calls,"zero direction stole I/O");
  const auto before=l.state.view(); l.files.fault={StateFileOperation::Write,0,10,0,false};
  for (unsigned i=0;i<100;++i) {auto r=l.background(); if(r.status==StateStatus::Unavailable) break;}
  check(l.state.health().write_health==StateStatus::Unavailable && l.state.view_is_current(before),"write failure changed read version");
  first(s,l); auto p=page(s); check(p[0].effective_count>0,"write failure forced base only");
  // Necessary cold count read fails. Keep valid published selection, explicitly
  // restart BaseOnly and keep it across recovery within this input.
  l.files.fault={StateFileOperation::Read,0,1,0,false};
  check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"range");
  check(finish(s,l)==SessionStatus::NeedsRankRestart,"read failure not visible");
  check(!l.state.health().rank_pending,"faulted real count not cleaned");
  bool order_stale=false;
  s.present([&](const CandidateViewPage& v){order_stale=v.order_stale;}); check(order_stale,"old page not stale order");
  check(s.confirm(s.selection(0)).status==CommitStatus::Preparing,"old page selection revoked");
  for(unsigned i=0;i<100000 && s.commit_result().status==CommitStatus::Preparing;++i) s.tick({1,17,3});
  check(s.commit_result().status==CommitStatus::Accepted,"state failure blocked text");
  first(s,l,"same",ScoringMode::BaseOnly);
  l.files.fault={}; l.flush();
  check(l.open(false)==StateStatus::Ready,"I/O reopen");
  const auto reads=l.files.metrics.requests.read_calls;
  check(s.request_range(RangeDirection::After,3,3)==SessionStatus::Pending,"base next"); publish(s,l);
  check(l.files.metrics.requests.read_calls==reads,"BaseOnly recovered mid input");
  // Reopen resources while a real count request is pending; clean only its
  // ticket, not accepted intent/dirty or a replacement on the source.
  check(commit(s,l,1).learning==LearningStatus::RecordedDirty,"same owner dirty before resource reopen");
  first(s,l);
  check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"next real");
  for(unsigned i=0;i<100000;++i) {auto r=s.tick({1,1,1}); if(r.status==SessionStatus::WaitingState) break; check(pending(r.status),"pending resource test");}
  check(l.state.health().rank_pending,"no count waiting");
  h.base.mount_shared(); check(s.tick({1,17,3}).status==SessionStatus::Stale,"shared reopen accepted");
  check(!l.state.health().rank_pending && l.state.health().dirty==1,"shared reopen retained count or lost dirty");
  // Fairness is explicit host scheduling: eight front steps then one back.
  s.cancel_input(s.input_handle()); s.tick({1,17,3});
  admit(s); check(s.bind_scheme(h.base.full)==SessionStatus::Ready,"bind after remount");
  auto receipt=commit(s,l,3); check(receipt.learning==LearningStatus::Pending,"fairness intent");
  check(s.begin_input("same",9,3,0)==SessionStatus::Pending,"fairness input");
  unsigned backgrounds=0,front=0,max_front=0;
  for(unsigned i=0;i<100000 && l.state.health().intent_pending;++i) {
    s.tick({1,17,3}); l.state.foreground_advanced(); ++front;
    if(l.state.background_due()) {max_front=std::max(max_front,front); front=0; ++backgrounds; l.background();}
  }
  check(backgrounds && max_front<=8 && !l.state.health().intent_pending && l.count(3)==4,"background starved");
  std::cout<<"E8 scheduler G=8 observed max_front="<<max_front<<" background_steps="<<backgrounds<<'\n';
  check(l.close()==StateStatus::ClosedSaved,"policy close");
}
CountResult finish_count(Ledger& l,const CountTicket& t) {
  for(unsigned i=0;i<100000;++i) {auto r=l.advance(t); if(r.status!=CountStatus::Pending) return r;}
  throw std::runtime_error("review count timeout");
}
StateStatus finish_reopen(Ledger& l) {
  for(unsigned i=0;i<100000;++i) {
    auto r=measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_open(small_budget);});
    if(r.status!=StateStatus::Pending) return r.status;
  }
  throw std::runtime_error("review reopen timeout");
}
void recovery_without_view() {
  for(bool background:{false,true}) {
    Ledger l(directory(root,"recover-no-view")); check(l.open()==StateStatus::Ready,"open");
    l.write(1,4); l.write(2,8);
    Resources h(3); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
    auto failed=l.state.begin_count(l.state.view(),session_key(1));
    l.files.fault={StateFileOperation::Read,0,1,0,false};
    check(finish_count(l,failed).status==CountStatus::Unavailable,"read fault missing"); l.state.cancel_count(failed);
    const auto fault_view=l.state.view(); const auto io=l.files.metrics.requests;
    for(unsigned i=0;i<3;++i) {
      auto t=l.state.begin_count(l.state.view(),session_key(1));
      check(finish_count(l,t).status==CountStatus::Unavailable,"view masked real fault"); l.state.cancel_count(t);
    }
    check(l.files.metrics.requests.read_calls==io.read_calls,"fault unexpectedly retried");
    auto paused=commit(s,l,3); check(paused.learning==LearningStatus::PausedUnavailable,"fault should pause unknown learning");
    first(s,l,"k2",ScoringMode::BaseOnly); const auto select=s.selection(0);
    const auto sink_calls=h.base.sink.calls;
    check(l.open(false)==StateStatus::Ready,"same source reopen");
    if(background) l.background(); // Recovery must also work without this.
    const auto reads=l.files.metrics.requests.read_calls;
    check(page(s)[0].effective_count==0,"recovery changed input BaseOnly");
    check(l.files.metrics.requests.read_calls==reads,"BaseOnly polled counts");
    check(s.confirm(select).status==CommitStatus::Preparing,"recovery revoked old selection");
    for(unsigned i=0;i<100000 && s.commit_result().status==CommitStatus::Preparing;++i) s.tick({1,17,3});
    const auto accepted=s.commit_result();
    check(accepted.status==CommitStatus::Accepted && accepted.learning==LearningStatus::Pending,"Ready recovery still requires view before learning");
    check(h.base.sink.calls==sink_calls+1 && l.files.metrics.requests.read_calls==reads,"notify duplicated text or performed state I/O");
    const auto recovered=l.state.view();
    check(recovered.count_revision==fault_view.count_revision && recovered.availability_revision!=fault_view.availability_revision,"recovery versions");
    check(l.state.notify_committed(paused.transaction,test_ns,session_key(3))==LearningStatus::PausedUnavailable,"replayed terminal pause");
    check(l.state.notify_committed(accepted.transaction,test_ns,session_key(2))==LearningStatus::Pending,"duplicate pending");
    intent_done(l); check(l.count(2)==9,"recovery learned wrong count");
    check(l.state.notify_committed(accepted.transaction,test_ns,session_key(2))==LearningStatus::RecordedDirty && l.count(2)==9,"duplicate recorded");
    first(s,l,"k1",ScoringMode::Exact); check(page(s)[0].effective_count==4,"new input did not resume Exact");
    check(l.close()==StateStatus::ClosedSaved,"recovery close");
    std::cout<<"F1a background="<<background<<" no_view_learning=Pending count=9 new_input_count=4\n";
  }
}
void pending_reopen_read() {
  Ledger l(directory(root,"pending-read")); check(l.open()==StateStatus::Ready,"open"); l.write(1,4);
  const auto before=l.state.view(); check(l.disk.begin_open(false)==StateStatus::Pending,"begin reopen");
  const auto opening=l.state.view(); auto old=l.state.begin_count(opening,session_key(1));
  const auto io=l.files.metrics.requests;
  for(unsigned i=0;i<12;++i) check(l.advance(old).status==CountStatus::Pending,"opening was classified as a read failure");
  check(l.files.metrics.requests.read_calls==io.read_calls,"opening cold query performed I/O");
  check(finish_reopen(l)==StateStatus::Ready,"finish reopen");
  const auto ready=l.state.view();
  check(before.count_revision==ready.count_revision && opening.availability_revision!=ready.availability_revision,"Ready transition failed to invalidate opening view");
  auto next=l.state.begin_count(ready,session_key(1)); l.state.cancel_count(old);
  check(l.advance(old).status==CountStatus::Stale,"old ticket survived Ready");
  auto got=finish_count(l,next); check(got.status==CountStatus::Found && got.count==4,"Ready cold value locked unavailable");
  l.state.cancel_count(next); check(l.close()==StateStatus::ClosedSaved,"read close");
  std::cout<<"F1b during=Pending old=Stale after_ready=Found count=4\n";
}
void reopen_intent(unsigned phase) {
  Ledger l(directory(root,"intent-reopen")); check(l.open()==StateStatus::Ready,"open"); l.write(1,4); l.write(2,77);
  Resources h(2); std::optional<CandidateSession> s;
  s.emplace(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(*s);
  auto c=commit(*s,l,1); check(c.learning==LearningStatus::Pending,"accepted intent");
  if(phase>=1) l.background({1,0,17,0,0,1});
  if(phase==2) {
    bool read=false;
    for(unsigned i=0;i<100000;++i) {auto r=l.background({1,0,17,0,0,1}); if(r.status==StateStatus::Found) {read=true; break;}}
    check(read && l.state.health().intent_pending && l.state.health().reserved==1,"did not stop after old value before increment");
  }
  const auto v=l.state.view(); const auto calls=h.base.sink.calls;
  s.reset(); // Accepted intent must outlive its submitting session.
  check(l.disk.begin_open(false)==StateStatus::Pending,"begin reopen");
  const auto io=l.files.metrics.requests;
  for(unsigned i=0;i<10;++i) {
    l.background({0,0,0,0,0,1});
    check(l.state.observe_learning(c.transaction.learning_ticket()).status==LearningStatus::Pending,"reopen prematurely terminated accepted intent");
    check(l.state.health().reserved==1 && !l.state.health().dirty,"reopen dropped reservation or applied old value");
  }
  check(l.files.metrics.requests.read_calls==io.read_calls && l.files.metrics.requests.write_calls==io.write_calls,"waiting stole I/O");
  check(l.state.notify_committed(c.transaction,test_ns,session_key(1))==LearningStatus::Pending,"duplicate changed pending intent");
  check(finish_reopen(l)==StateStatus::Ready,"finish reopen");
  auto external=l.disk.begin_lookup(StateReadRole::RankLookup,session_key(2));
  const auto resume=l.totals;
  intent_done(l);
  const auto reload=delta(l.totals.cost,resume.cost);
  check(reload.read_calls==8 && reload.read_bytes==128 && !reload.write_calls && !reload.control_calls,"new activation did not re-read the original two-frame authority");
  check(l.disk.read_is_owned(external),"intent recovery canceled other read role");
  StateStep found;
  do {found=measured(l.files,l.totals,small_budget,[&]{return l.disk.advance_lookup(external,small_budget);});} while(found.status==StateStatus::Pending);
  check(found.status==StateStatus::Found && found.count==77,"external read damaged"); l.disk.cancel_read(external);
  check(l.state.observe_learning(c.transaction.learning_ticket()).status==LearningStatus::RecordedDirty && l.count(1)==5,"intent recovery did not learn exactly once");
  check(l.state.view().count_revision==v.count_revision+1 && h.base.sink.calls==calls,"recovery duplicated semantic increment or text");
  check(l.state.notify_committed(c.transaction,test_ns,session_key(1))==LearningStatus::RecordedDirty && l.count(1)==5,"duplicate after recovery");
  print_cost("F2 phase"+std::to_string(phase)+" resume+other-read+checks",{delta(l.totals.cost,resume.cost),l.totals.steps-resume.steps});
  check(l.close()==StateStatus::ClosedSaved,"intent close");
}
void reopen_failure_policy() {
  Ledger l(directory(root,"reopen-failure")); check(l.open()==StateStatus::Ready,"open"); l.write(1,4);
  Resources h(2); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
  auto c=commit(s,l,1); check(c.learning==LearningStatus::Pending,"intent");
  check(l.disk.begin_open(false)==StateStatus::Pending,"reopen");
  l.files.fault={StateFileOperation::Read,0,1,0,false};
  check(finish_reopen(l)==StateStatus::Unavailable,"real reopen failure");
  intent_done(l);
  check(l.state.observe_learning(c.transaction.learning_ticket()).status==LearningStatus::PausedUnavailable && !l.state.health().reserved,"failure should terminate reservation");
  check(l.open(false)==StateStatus::Ready,"second recovery"); l.background();
  check(l.state.notify_committed(c.transaction,test_ns,session_key(1))==LearningStatus::PausedUnavailable && l.count(1)==4,"replayed failed intent");
  auto next=commit(s,l,1); check(next.learning==LearningStatus::Pending,"fresh learning after validated recovery"); intent_done(l);
  check(l.count(1)==5,"fresh learning wrong");
  for(unsigned i=0;i<8 && !l.state.health().frozen_write;++i) l.background({0,0,0,0,0,1});
  check(l.state.health().frozen_write && l.disk.begin_open(false)==StateStatus::InvalidRequest,"reopen crossed frozen writer");
  l.flush(); check(l.disk.begin_maintenance()==StateStatus::Pending && l.disk.begin_open(false)==StateStatus::InvalidRequest,"reopen crossed maintenance");
  l.flush(); check(l.close()==StateStatus::ClosedSaved,"failure close");
}
}
int main(int argc,char** argv) {
  if (argc!=2) {std::cerr<<"usage: hanime_state_tests dedicated-test-root\n"; return 2;} root=argv[1];
  const std::pair<const char*,void(*)()> tests[]={
    {"E1 V3 layout, lookup, tiny budget and A/B/A",file_basics},
    {"E2 learning, saturation, receipts, intent lifetime",learning},
    {"E3 real pending ownership, late 4/6 and write 5/6",late_work},
    {"E4 physical faults, uncertain tail, incompatible and corrupt",file_faults},
    {"E5 maintenance interleaving, capacity and explicit close",maintenance_and_close},
    {"E6 synthetic N32 full ranking",+[]{ranking_scale(32);}},
    {"E7 synthetic N320 full ranking",+[]{ranking_scale(320);}},
    {"E8 availability, BaseOnly, zero budgets, cleanup and fairness",policy_and_fairness},
    {"E9 F1 recovery without view",recovery_without_view},
    {"E10 F1 read during reopen",pending_reopen_read},
    {"E11 F2 phase0 reopen",+[]{reopen_intent(0);}},
    {"E12 F2 phase1 reopen",+[]{reopen_intent(1);}},
    {"E13 F2 value returned before increment",+[]{reopen_intent(2);}},
    {"E14 real reopen failure and terminal pause",reopen_failure_policy}
  };
  unsigned passed=0;
  for (const auto& t:tests) {try {t.second(); ++passed; std::cout<<"PASS "<<t.first<<'\n';} catch(const std::exception& e) {std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<'\n';}}
  constexpr auto total=sizeof(tests)/sizeof(tests[0]);
  std::cout<<passed<<'/'<<total<<" bounded state groups passed\n"; return passed==total ? 0 : 1;
}
