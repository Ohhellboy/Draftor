#include "session_fixture.hpp"
#include "rank_allocation_probe.hpp"
#include <iostream>
#include <optional>

namespace {
using namespace hanime;
using namespace hanime::test;
constexpr SessionConfig small{4,3,1,16,32,32};
void check(bool b,const char* text) {require_session(b,text);}
bool running(SessionStatus s) {return s==SessionStatus::Pending || s==SessionStatus::WaitingState;}
void budget_check(WorkCost c,WorkBudget b) {
  check(c.read_calls<=b.max_read_calls && c.read_bytes<=b.max_read_bytes && c.work_items<=b.max_work_items,"budget exceeded");
}
SessionStep tick(SessionFixture& h,CandidateSession& s,WorkBudget b={1,17,3}) {
  const auto reads=h.read_calls(),bytes=h.read_bytes(),adv=h.counts.frozen.advances,sink_calls=h.sink.calls;
  SessionStep step;
  const auto transaction=s.commit_result();
  // Exclude sink's test string/vector copies and fake learning-map writes from
  // the allocation probe. Commit preparation and its independent pool ARE in.
  if (transaction.status==CommitStatus::Preparing && output_has_text(transaction.output)) step=s.tick(b);
  else {rank_probe::Capture capture; step=s.tick(b);}
  budget_check(step.cost,b);
  if (h.sink.calls==sink_calls)
    check(h.read_calls()-reads==step.cost.read_calls && h.read_bytes()-bytes==step.cost.read_bytes,"unmetered session read");
  else check(step.cost.read_calls==0,"sink call hid core resource I/O"); // Callback may deliberately remount test owner.
  check(h.counts.frozen.advances-adv<=1,"multiple count advances");
  return step;
}
TextAccessStep admit(SessionFixture& h,CandidateSession& s,WorkBudget budget={1,17,3}) {
  check(s.valid(),"session config/allocation failure");
  check(s.begin_text_access()==TextAccessStatus::Pending,"access did not begin");
  for (std::size_t i=0;i<20'000'000;++i) {
    const auto reads=h.read_calls(),bytes=h.read_bytes(); TextAccessStep step;
    {rank_probe::Capture capture; step=s.advance_text_access(budget);}
    budget_check(step.cost,budget);
    check(h.read_calls()-reads==step.cost.read_calls && h.read_bytes()-bytes==step.cost.read_bytes,"unmetered access read");
    if (step.status!=TextAccessStatus::Pending) return step;
  }
  throw std::runtime_error("access stalled");
}
SessionStatus finish(SessionFixture& h,CandidateSession& s,WorkBudget budget={1,17,3}) {
  for (std::size_t i=0;i<20'000'000;++i) {const auto r=tick(h,s,budget); if (!running(r.status)) return r.status;}
  throw std::runtime_error("session stalled");
}
struct Page {std::vector<RankItem> ranked; std::vector<std::string> text; std::vector<OutputStatus> output; bool stale{false}; ScoringMode mode{ScoringMode::Exact};};
Page page(CandidateSession& s) {
  Page p;
  check(s.present([&](const CandidateViewPage& v) {
    p.stale=v.order_stale; p.mode=v.mode;
    for(std::size_t i=0;i<v.size;++i) {p.ranked.push_back(v.items[i].ranked); p.text.emplace_back(v.text(i)); p.output.push_back(v.items[i].output);}
  })==SessionStatus::Ready,"present failed");
  return p;
}
void publish(SessionFixture& h,CandidateSession& s,WorkBudget b={1,17,3}) {
  check(finish(h,s,b)==SessionStatus::StagingReady,"range/text did not finish");
  check(s.publish(s.staging())==SessionStatus::Ready,"publish failed");
}
void first(SessionFixture& h,CandidateSession& s,std::size_t q=3,std::size_t text=3,
           ScoringMode mode=ScoringMode::Exact,std::string_view code="same",WorkBudget b={1,17,3}) {
  check(s.begin_input(code,9,q,text,mode)==SessionStatus::Pending,"input did not begin"); publish(h,s,b);
}
void end(SessionFixture& h,CandidateSession& s) {
  s.cancel_input(s.input_handle()); check(tick(h,s).status==SessionStatus::Cancelled,"cancel input failed");
}
CommitResult settle(SessionFixture& h,CandidateSession& s,WorkBudget b={1,17,3}) {
  for (std::size_t i=0;i<1'000'000;++i) {
    auto r=s.commit_result(); if (r.status!=CommitStatus::Preparing && r.status!=CommitStatus::CallingSink) return r;
    tick(h,s,b);
  }
  throw std::runtime_error("commit stalled");
}
CommitResult commit(SessionFixture& h,CandidateSession& s,std::size_t slot=0) {
  check(s.confirm(s.selection(slot)).status==CommitStatus::Preparing,"confirm failed"); return settle(h,s);
}
void generic_identity() {
  SessionFixture h(8); CandidateSession s(h.full,h.shared,h.counts,h.sink,small);
  check(admit(h,s).status==TextAccessStatus::Ready,"admission");
  const auto key=session_key(8);
  for (const auto& form:{std::string(),std::string("test:regional"),std::string("test:variant")}) {
    check(s.set_output({form,OutputInvocation::Profile})==SessionStatus::Ready,"form config");
    first(h,s,3,3,ScoringMode::Exact,"same",{1,1,1}); const auto p=page(s);
    check(p.ranked[0].key==key && p.ranked[0].path_count==3,"identity or paths changed");
    const auto expected=form.empty() ? "同文𨿍" : (form=="test:regional" ? "R:地区" : "V:𨿍");
    check(p.text[0]==expected,"generic form text mismatch");
    const auto selection=s.selection(0); auto r=commit(h,s);
    check(r.status==CommitStatus::Accepted && r.learning==LearningStatus::RecordedDirty,"accepted learning");
    const auto calls=h.sink.calls,notices=h.counts.notifications;
    check(s.confirm(selection).status==CommitStatus::Accepted && h.sink.calls==calls && h.counts.notifications==notices,"duplicate success");
    check(h.counts.last_namespace==h.input.registry_namespace && h.counts.last_key==key,"wrong learning identity");
    check(s.bind_scheme(h.xiaohe)==SessionStatus::Ready,"idle scheme bind");
    first(h,s,3,3,ScoringMode::Exact,"sx"); check(page(s).ranked[0].effective_count==notices,"scheme did not share count");
    end(h,s); check(s.bind_scheme(h.full)==SessionStatus::Ready,"rebind full");
  }
  // Rebuild changes refs only; same state object remains authoritative.
  auto reversed=ResourceBuilder().build(session_fixture(8,true),{96,96}); check(reversed.ok,"reverse build");
  h.built=std::move(reversed.resources); h.mount();
  check(admit(h,s).status==TextAccessStatus::Ready,"reordered admission");
  check(s.bind_scheme(h.full)==SessionStatus::Ready,"reordered bind");
  first(h,s); const auto p=page(s);
  check(p.ranked[0].key==key && p.ranked[0].ref==1 && p.ranked[0].effective_count==3,"reorder mixed key/count");
  check(p.ranked[1].key!=p.ranked[2].key,"same text merged identities");
}
void restrictions() {
  SessionFixture h(2); CandidateSession s(h.full,h.shared,h.counts,h.sink,small);
  check(admit(h,s).status==TextAccessStatus::Ready,"admit");
  const std::vector<std::pair<std::string,OutputStatus>> cases={{"missing",OutputStatus::FallbackDefault},
    {"condition",OutputStatus::ConditionUndetermined},{"blocked",OutputStatus::Blocked},
    {"explicit",OutputStatus::InvocationDenied},{"multiple",OutputStatus::MultipleRelations}};
  for (const auto& c:cases) {
    check(s.set_output({c.first,OutputInvocation::Profile})==SessionStatus::Ready,"set restricted form");
    first(h,s,2,2); auto p=page(s); check(p.output[0]==c.second,"restriction bypassed");
    const auto calls=h.sink.calls;
    const auto r=commit(h,s);
    if (c.second==OutputStatus::FallbackDefault) check(r.status==CommitStatus::Accepted && h.sink.accepted.back()=="同文𨿍","missing did not fallback");
    else {check(r.status==CommitStatus::OutputRestricted && h.sink.calls==calls,"restricted sink call"); s.cancel_commit(r.transaction); end(h,s);}
  }
  check(s.set_output({"explicit",OutputInvocation::Explicit})==SessionStatus::Ready,"explicit config");
  first(h,s,2,2); check(page(s).text[0]=="许可","permitted invocation failed");
}
void payload_and_admission() {
  SessionFixture h(2);
  auto exact=small; exact.output_bytes=10;
  CandidateSession a(h.full,h.shared,h.counts,h.sink,exact);
  check(admit(h,a,{1,1,1}).status==TextAccessStatus::Ready,"exact L rejected");
  auto short_cfg=small; short_cfg.output_bytes=9;
  CandidateSession b(h.full,h.shared,h.counts,h.sink,short_cfg);
  auto bad=admit(h,b); check(bad.status==TextAccessStatus::Incompatible && bad.required_bytes==10 && bad.location_kind==SharedRecordKind::Lexeme,"L limit not diagnosed");
  short_cfg=small; short_cfg.form_bytes=4;
  CandidateSession f(h.full,h.shared,h.counts,h.sink,short_cfg);
  bad=admit(h,f); check(bad.status==TextAccessStatus::Incompatible && bad.location_kind==SharedRecordKind::Selector,"F limit not diagnosed");
  auto overflow=small; overflow.snapshot_slots=overflow.ranking_slots=std::numeric_limits<std::size_t>::max();
  CandidateSession invalid(h.full,h.shared,h.counts,h.sink,overflow); check(!invalid.valid(),"overflow accepted");
  if (rank_probe::fail_next_allocation()) {
    CandidateSession no_memory(h.full,h.shared,h.counts,h.sink,small);
    check(!no_memory.valid(),"configuration bad_alloc not reported");
  }
  SharedReadCursor c; check(h.shared.begin_read(SharedRecordKind::Lexeme,1,&c)==SharedReadStatus::InProgress,"read lexeme");
  SharedReadResult record;
  do {record=h.shared.advance_read(&c,{1,1,1});} while(record.status==SharedReadStatus::InProgress);
  char dest[16]{}; SharedTextCursor text;
  const auto activation=h.shared.activation_handle();
  check(h.shared.begin_payload_read(activation,record.lexeme.default_payload,dest,sizeof(dest),&text)==SharedReadStatus::InProgress,"payload begin");
  SharedTextResult r;
  do {
    const auto calls=h.read_calls(),bytes=h.read_bytes(); r=h.shared.advance_payload_read(&text,{1,1,1}); budget_check(r.cost,{1,1,1});
    check(h.read_calls()-calls==r.cost.read_calls && h.read_bytes()-bytes==r.cost.read_bytes,"payload unmetered");
    if (r.status==SharedReadStatus::InProgress) check(r.text.empty(),"published partial UTF-8");
  } while(r.status==SharedReadStatus::InProgress);
  check(r.status==SharedReadStatus::Ready && r.text=="同文𨿍" && h.shared.text_is_current(r),"payload bytes");
  const auto copied=std::string(r.text);
  SharedTextCursor old=text;
  check(h.shared.begin_payload_read(activation,record.lexeme.default_payload,dest,9,&text)==SharedReadStatus::InProgress,"short dest begin");
  h.shared.cancel_payload_read(&old); // No effect on the new read.
  do {r=h.shared.advance_payload_read(&text,{1,1,1});} while(r.status==SharedReadStatus::InProgress);
  check(r.status==SharedReadStatus::CapacityExceeded && r.required_bytes==10 && copied=="同文𨿍","capacity/no-op cancel");
  check(h.shared.advance_payload_read(&text,{1,0,1}).status==SharedReadStatus::InvalidBudget,"zero budget");
  h.shared.cancel_payload_read(&text);
  check(h.shared.advance_payload_read(&text,{1,1,1}).status==SharedReadStatus::Cancelled,"payload cancel");
  // Put U+28FCD across a physical 96-byte boundary, as well as 1-byte reads.
  h.input.lexemes[0].default_output_utf8=std::string(86,'x')+"𨿍";
  auto cross=ResourceBuilder().build(h.input,{96,96}); check(cross.ok,"cross-page fixture"); h.built=std::move(cross.resources); h.mount();
  h.shared.begin_read(SharedRecordKind::Lexeme,1,&c);
  do {record=h.shared.advance_read(&c,{1,1,1});} while(record.status==SharedReadStatus::InProgress);
  check((h.shared.info().payload_offset+record.string_descriptor.offset+86)%96==94,"fixture is not cross-page UTF-8");
  char long_text[90]{};
  h.shared.begin_payload_read(h.shared.activation_handle(),record.lexeme.default_payload,long_text,sizeof(long_text),&text);
  do {r=h.shared.advance_payload_read(&text,{1,1,1}); if (r.status==SharedReadStatus::InProgress) check(r.text.empty(),"cross-page partial text");}
  while(r.status==SharedReadStatus::InProgress);
  check(r.status==SharedReadStatus::Ready && r.text==h.input.lexemes[0].default_output_utf8,"cross-page UTF-8 lost bytes");
}
void snapshots_and_ranges() {
  SessionFixture h(25); auto cfg=small; cfg.ranking_slots=cfg.snapshot_slots=10;
  CandidateSession s(h.full,h.shared,h.counts,h.sink,cfg); check(admit(h,s).status==TextAccessStatus::Ready,"admit");
  first(h,s,10,10); auto old=page(s); auto old_selection=s.selection(0);
  check(s.request_range(RangeDirection::After,10,5)==SessionStatus::Pending,"next prefetch");
  check(finish(h,s)==SessionStatus::StagingReady,"prefetch finish");
  check(page(s).ranked[0].key==old.ranked[0].key && page(s).text==old.text,"borrowed rank array overwrote published");
  check(s.request_text(s.staging(),5,5)==SessionStatus::Pending,"finish text window");
  publish(h,s); auto p=page(s);
  check(p.ranked.size()==10 && p.ranked[0].key==session_key(15),"prefetch skipped 11-15");
  check(s.confirm(old_selection).status==CommitStatus::Stale,"old slot selected replacement");
  check(s.request_range(RangeDirection::Before,10,10)==SessionStatus::Pending,"previous range"); publish(h,s);
  check(page(s).ranked[0].key==old.ranked[0].key,"before rebuild");
  // Publish explicit identities-only range: Pending text is not NoMatches.
  check(s.request_range(RangeDirection::After,10,0)==SessionStatus::Pending,"identity page"); publish(h,s);
  p=page(s); check(p.ranked.size()==10 && p.text[0].empty() && p.output[0]==OutputStatus::Pending,"pending identity disappeared");
  check(s.request_text(s.published(),0,1)==SessionStatus::Pending,"visible miss");
  check(finish(h,s)==SessionStatus::StagingReady && page(s).text[0]=="同文𨿍","visible reload");
}
void read_handoff() {
  for (int scenario=0;scenario<3;++scenario) {
    SessionFixture h(9); CandidateSession s(h.full,h.shared,h.counts,h.sink,small);
    check(admit(h,s).status==TextAccessStatus::Ready,"admit"); first(h,s);
    h.sink.accept=false;
    if (scenario==1) h.counts.frozen.delay=1000;
    check(s.request_range(RangeDirection::After,3,scenario==2 ? 3 : 0)==SessionStatus::Pending,"next range");
    if (scenario==0) {
      const auto reads=h.lr->calls;
      for(std::size_t i=0;h.lr->calls==reads && i<100000;++i) tick(h,s,{1,1,1});
      check(h.lr->calls>reads,"did not reach shared read");
    } else if (scenario==1) {
      SessionStatus status=SessionStatus::Pending;
      for(std::size_t i=0;status!=SessionStatus::WaitingState && i<100000;++i) status=tick(h,s).status;
      check(status==SessionStatus::WaitingState,"did not wait for count");
    }
    if (scenario==2) {
      const auto before=s.costs().display.read_bytes;
      for(std::size_t i=0;s.costs().display.read_bytes==before && i<1000000;++i) tick(h,s,{1,1,1});
      check(s.costs().display.read_bytes>before,"did not reach prefetch read");
    }
    const auto cancels=h.counts.frozen.cancels;
    const auto r=commit(h,s);
    check(r.status==CommitStatus::RetryableNoEffect && page(s).ranked[0].key==session_key(9),"confirm damaged displayed identity");
    if (scenario==1) check(h.counts.frozen.cancels==cancels+1,"pending count not released");
    s.cancel_commit(r.transaction); h.counts.frozen.delay=0;
    if (scenario<2) {check(s.resume_range()==SessionStatus::Pending,"resume abandoned range"); publish(h,s);}
    else {check(s.request_text(s.staging(),0,3)==SessionStatus::Pending,"resume cancelled prefetch"); publish(h,s);}
    check(page(s).ranked[0].key==session_key(6),"handoff lost next range");
  }
  SessionFixture h(9); CandidateSession s(h.full,h.shared,h.counts,h.sink,small);
  check(admit(h,s).status==TextAccessStatus::Ready,"priority admit"); first(h,s,3,0);
  check(s.request_range(RangeDirection::After,3,3)==SessionStatus::Pending,"priority prefetch");
  const auto before=s.costs().display.read_bytes;
  while(s.costs().display.read_bytes==before) tick(h,s,{1,1,1});
  check(s.request_range(RangeDirection::First,3,0)==SessionStatus::Pending,"explicit ranking blocked by prefetch");
  check(finish(h,s)==SessionStatus::StagingReady,"priority rank result");
  const auto prepared=s.staging();
  check(s.request_text(prepared,0,3)==SessionStatus::Pending,"staging text request");
  tick(h,s);
  check(s.request_text(s.published(),0,1)==SessionStatus::Pending,"visible text blocked by prefetch");
  check(s.request_text(prepared,0,1)==SessionStatus::Busy,"prefetch displaced visible text");
  check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Busy,"rank displaced visible text");
  check(finish(h,s)==SessionStatus::StagingReady && page(s).text[0]=="同文𨿍","visible text did not survive priority check");
}
void freshness_and_baseonly() {
  SessionFixture h(9); CandidateSession s(h.full,h.shared,h.counts,h.sink,small); check(admit(h,s).status==TextAccessStatus::Ready,"admit");
  first(h,s); auto old=s.selection(0); h.counts.frozen.same_value_flush(); check(!page(s).stale,"flush changed ranking");
  h.counts.frozen.set(session_key(1),100);
  check(page(s).stale && s.request_range(RangeDirection::After,3,3)==SessionStatus::NeedsRankRestart,"old exact boundary accepted");
  check(s.confirm(old).status==CommitStatus::Preparing && settle(h,s).status==CommitStatus::Accepted,"state change revoked selection");
  first(h,s); auto retained=page(s);
  check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"next safe copy");
  check(finish(h,s)==SessionStatus::StagingReady,"safe copy"); h.counts.frozen.set(session_key(2),99);
  check(s.request_text(s.staging(),0,3)==SessionStatus::Pending,"stale-order staging lost identity"); publish(h,s);
  check(page(s).stale,"stale staging mislabelled");
  check(s.restart_first(3,3)==SessionStatus::Pending,"explicit restart"); publish(h,s);
  retained=page(s); const auto copied=s.costs().copied_items;
  check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"partial copy");
  while(s.costs().copied_items==copied) tick(h,s);
  h.counts.frozen.set(session_key(3),98);
  check(finish(h,s)==SessionStatus::NeedsRankRestart && page(s).ranked[0].key==retained.ranked[0].key,"partial invalid copy published");
  check(s.restart_first(3,3)==SessionStatus::Pending,"restart before read fault"); publish(h,s);
  h.counts.frozen.unavailable=true;
  check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"begin faulty-state range");
  check(finish(h,s)==SessionStatus::NeedsRankRestart && page(s).stale,"state read failure hid old ranking");
  h.sink.accept=false; const auto fault_commit=commit(h,s);
  check(fault_commit.status==CommitStatus::RetryableNoEffect,"learning read fault revoked selection");
  s.cancel_commit(fault_commit.transaction); h.sink.accept=true; h.counts.frozen.recover();
  check(s.restart_first(3,3,ScoringMode::Exact)==SessionStatus::Pending,"latched fault restart"); publish(h,s);
  check(page(s).mode==ScoringMode::BaseOnly,"fault recovery silently restored Exact");
  end(h,s);
  for(std::size_t key=1;key<=3;++key) h.counts.frozen.set(session_key(key),0);
  first(h,s,3,3,ScoringMode::BaseOnly); const auto views=h.counts.frozen.view_calls,checks=h.counts.frozen.view_checks,begins=h.counts.frozen.begins;
  h.counts.frozen.reopen(); h.counts.frozen.recover(); h.counts.frozen.set(session_key(9),42);
  check(s.request_range(RangeDirection::After,3,3)==SessionStatus::Pending,"base range");
  const auto bytes=h.lr->requested;
  while(h.lr->requested==bytes) tick(h,s,{1,1,1});
  h.sink.accept=false; auto r=commit(h,s); check(r.status==CommitStatus::RetryableNoEffect,"base failed attempt");
  s.cancel_commit(r.transaction); check(s.resume_range()==SessionStatus::Pending,"base resume"); publish(h,s);
  check(page(s).mode==ScoringMode::BaseOnly && !page(s).stale && h.counts.frozen.view_calls==views &&
      h.counts.frozen.view_checks==checks && h.counts.frozen.begins==begins,"handoff reset BaseOnly");
  h.sink.accept=true; check(commit(h,s).status==CommitStatus::Accepted,"base commit");
  first(h,s); check(page(s).mode==ScoringMode::Exact && page(s).ranked[0].effective_count==42,"new input did not restore exact authority");
}
void commit_retry_and_learning() {
  SessionFixture h(4); CandidateSession s(h.full,h.shared,h.counts,h.sink,small); check(admit(h,s).status==TextAccessStatus::Ready,"admit"); first(h,s);
  const auto selection=s.selection(0); h.sink.accept=false;
  check(s.confirm(selection).status==CommitStatus::Preparing,"confirm");
  check(s.confirm(selection).status==CommitStatus::Preparing && h.sink.calls==0,"preparing duplicate");
  auto one=settle(h,s); check(one.status==CommitStatus::RetryableNoEffect && h.sink.calls==1,"first failure");
  check(s.confirm(selection).status==CommitStatus::RetryableNoEffect && h.sink.calls==1,"confirm retried");
  check(s.retry(one.retry).status==CommitStatus::Preparing,"retry1"); auto two=settle(h,s);
  check(two.status==CommitStatus::RetryableNoEffect && two.attempt==2,"second failure");
  check(s.retry(one.retry).status==CommitStatus::Stale && h.sink.calls==2,"late permit replayed");
  h.sink.accept=true; check(s.retry(two.retry).status==CommitStatus::Preparing,"retry2");
  check(settle(h,s).status==CommitStatus::Accepted && h.counts.notifications==1,"success after retry");
  first(h,s); check(s.confirm(s.selection(0)).status==CommitStatus::Preparing,"io prep"); h.lr->fail=true;
  auto failure=settle(h,s); check(failure.status==CommitStatus::RetryableNoEffect && h.shared.ready(),"prep I/O has no retry exit");
  const auto reads=h.read_calls(),calls=h.sink.calls;
  for(int i=0;i<5;++i) tick(h,s);
  check(h.read_calls()==reads && h.sink.calls==calls,"automatic failed I/O retry");
  h.lr->fail=false; check(s.retry(failure.retry).status==CommitStatus::Preparing,"io retry");
  check(settle(h,s).status==CommitStatus::Accepted,"io retry failed");
  // Same authoritative nonzero value survives BaseOnly and all learning faults.
  h.counts.frozen.set(session_key(4),80);
  first(h,s,3,3,ScoringMode::BaseOnly); check(commit(h,s).status==CommitStatus::Accepted,"base submit");
  check(h.counts.frozen.values.at(session_key(4))==81,"BaseOnly zero overwrote authority");
  for(int fault=0;fault<3;++fault) {
    first(h,s,3,3,ScoringMode::BaseOnly);
    h.counts.reject_learning=fault==0; h.counts.unknown_value=fault==1;
    if (fault==2) h.counts.dirty_capacity=0;
    const auto chosen=page(s).ranked[fault==2 ? 1 : 0].key;
    const auto old_count=h.counts.frozen.values.count(chosen) ? h.counts.frozen.values.at(chosen) : 0;
    const auto dirty=h.counts.dirty;
    auto r=commit(h,s,fault==2 ? 1 : 0);
    const auto expected=fault==0 ? LearningStatus::PausedUnavailable : fault==1 ? LearningStatus::PausedUnknown : LearningStatus::PausedCapacity;
    check(r.status==CommitStatus::Accepted && r.learning==expected,"learning fault blocked text");
    check(h.counts.dirty==dirty && (h.counts.frozen.values.count(chosen) ? h.counts.frozen.values.at(chosen) : 0)==old_count,"dirty lost or pause backfilled");
  }
  h.counts.reject_learning=h.counts.unknown_value=false; h.counts.dirty_capacity=1024;
  h.counts.frozen.set(session_key(4),std::numeric_limits<std::uint32_t>::max()); const auto revision=h.counts.frozen.current.count_revision;
  first(h,s,3,3,ScoringMode::BaseOnly); commit(h,s);
  check(h.counts.frozen.values.at(session_key(4))==std::numeric_limits<std::uint32_t>::max() && h.counts.frozen.current.count_revision==revision,"saturating count changed version");
}
void lifecycle_callbacks() {
  SessionFixture h(6); CandidateSession s(h.full,h.shared,h.counts,h.sink,small); check(admit(h,s).status==TextAccessStatus::Ready,"admit"); first(h,s);
  check(s.set_output({"test:variant",OutputInvocation::Profile})==SessionStatus::Busy && s.bind_scheme(h.xiaohe)==SessionStatus::Busy,"nonempty config changed");
  const auto old_input=s.input_handle(); const auto old=s.selection(0);
  check(s.present([&](const CandidateViewPage& p) {
    const std::string before(p.text(0));
    check(s.begin_input("same",9,2,2)==SessionStatus::Pending,"callback input");
    check(s.tick({1,1,1}).status==SessionStatus::Busy && p.text(0)==before,"present buffer reused recursively");
  })==SessionStatus::Ready,"present callback");
  s.cancel_input(old_input); publish(h,s); check(s.confirm(old).status==CommitStatus::Stale,"old input cancel/selection harmed new");
  std::uintptr_t display_address=0;
  s.present([&](const CandidateViewPage& p){display_address=reinterpret_cast<std::uintptr_t>(p.text(0).data());});
  auto selected=s.selection(0);
  h.sink.callback=[&] {
    check(s.confirm(selected).status==CommitStatus::CallingSink,"calling duplicate");
    check(s.begin_input("same",9,2,2)==SessionStatus::Pending,"sink new input");
    check(s.tick({1,1,1}).status==SessionStatus::Busy,"recursive sink tick");
  };
  auto accepted=commit(h,s); h.sink.callback={};
  check(accepted.status==CommitStatus::Accepted && h.sink.last_address!=display_address,"commit borrowed display pool");
  publish(h,s); check(page(s).ranked.size()==2 && h.sink.accepted.back()=="同文𨿍","old settlement cleared/reused new input");
  h.sink.accept=false; h.sink.callback=[&]{s.begin_input("same",9,2,2);};
  const auto old_failure=commit(h,s); check(old_failure.status==CommitStatus::Stale,"failed sink revived old input");
  h.sink.callback={}; h.sink.accept=true; tick(h,s);
  s.cancel_commit(old_failure.transaction); // Must not stop the new rank/read.
  publish(h,s);
  h.sink.callback=[&]{h.mount_shared(); h.counts.frozen.current.registry_namespace+=1;};
  accepted=commit(h,s); h.sink.callback={};
  check(accepted.status==CommitStatus::Accepted && accepted.learning==LearningStatus::PausedUnavailable &&
      h.counts.last_namespace==h.input.registry_namespace,"accepted text revoked or wrong namespace learned");
  h.counts.frozen.current.registry_namespace=h.input.registry_namespace;
  check(admit(h,s).status==TextAccessStatus::Ready,"reopen access"); first(h,s);
  auto stale=s.selection(0); h.shared.~PagedSharedResource(); new (&h.shared) PagedSharedResource(); h.mount_shared();
  SharedReadCursor fresh; h.shared.begin_read(SharedRecordKind::Lexeme,1,&fresh);
  check(s.confirm(stale).status==CommitStatus::Stale && !s.text_access_ready(),"same-address old snapshot revived");
  tick(h,s); // Old work must not cancel the new owner's read.
  SharedReadResult fresh_result;
  do {fresh_result=h.shared.advance_read(&fresh,{1,17,3});} while(fresh_result.status==SharedReadStatus::InProgress);
  check(fresh_result.status==SharedReadStatus::Ready,"old session harmed new owner read");
  end(h,s);
  // Reconstruct the session at the same address; opaque snapshot still differs.
  std::optional<CandidateSession> other;
  other.emplace(h.full,h.shared,h.counts,h.sink,small); check(admit(h,*other).status==TextAccessStatus::Ready,"other admit"); first(h,*other);
  auto foreign=other->selection(0); other.reset();
  other.emplace(h.full,h.shared,h.counts,h.sink,small); check(admit(h,*other).status==TextAccessStatus::Ready,"other readmit"); first(h,*other);
  check(other->confirm(foreign).status==CommitStatus::Stale,"same session address revived snapshot");
  h.sink.throw_unknown=true; auto unknown=commit(h,*other);
  check(unknown.status==CommitStatus::UnknownOutcome && other->cancel_commit(unknown.transaction).status==CommitStatus::UnknownOutcome,"unknown outcome treated as retryable");
}
void wait_for_count(SessionFixture& h,CandidateSession& s) {
  h.counts.frozen.delay=1000;
  check(s.request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"cleanup next range");
  auto status=SessionStatus::Pending;
  for (unsigned i=0;status!=SessionStatus::WaitingState && i<100000;++i) status=tick(h,s).status;
  check(status==SessionStatus::WaitingState && h.counts.frozen.active.revision,"cleanup did not reach pending count");
}
void terminal_count_cleanup() {
  bool both_clean=true;
  for (int scenario=0;scenario<2;++scenario) {
    SessionFixture h(9); CandidateSession s(h.full,h.shared,h.counts,h.sink,small);
    check(admit(h,s).status==TextAccessStatus::Ready,"cleanup admit"); first(h,s);
    const auto old_key=page(s).ranked[0].key;
    wait_for_count(h,s);
    const auto old=h.counts.frozen.active.revision;
    const auto cancels=h.counts.frozen.cancels;
    if (scenario==0) h.counts.frozen.set(session_key(1),100);
    else h.mount_shared();
    const auto stopped=tick(h,s).status;
    check(stopped==(scenario==0 ? SessionStatus::NeedsRankRestart : SessionStatus::Stale),"cleanup terminal status changed");
    const bool terminal_clean=h.counts.frozen.active.revision!=old;
    if (scenario==0) {
      check(page(s).stale && page(s).ranked[0].key==old_key,"old display identity revoked");
      check(s.restart_first(3,3,ScoringMode::BaseOnly)==SessionStatus::Pending,"cleanup restart"); publish(h,s);
      check(page(s).mode==ScoringMode::BaseOnly,"cleanup reset BaseOnly");
    } else {s.cancel_input(s.input_handle()); tick(h,s);}
    const bool clean=terminal_clean && h.counts.frozen.active.revision!=old && h.counts.frozen.cancels==cancels+1;
    std::cout<<"cleanup scenario="<<scenario<<" status="<<int(stopped)<<" old="<<old
      <<" terminal_clean="<<terminal_clean<<" remaining="<<h.counts.frozen.active.revision
      <<" cancel_delta="<<h.counts.frozen.cancels-cancels<<"\n";
    both_clean=both_clean && clean;
  }
  check(both_clean,"terminated rank retained its pending count request");
}
void cleanup_ownership_and_lifetime() {
  // Input end, replacement, and destruction all release the original request.
  for (int action=0;action<3;++action) {
    SessionFixture h(9);
    auto s=std::make_unique<CandidateSession>(h.full,h.shared,h.counts,h.sink,small);
    check(admit(h,*s).status==TextAccessStatus::Ready,"lifetime admit"); first(h,*s); wait_for_count(h,*s);
    const auto old=h.counts.frozen.active.revision;
    const auto cancels=h.counts.frozen.cancels, reads=h.read_calls(), advances=h.counts.frozen.advances;
    if (action==0) end(h,*s);
    else if (action==1) {
      check(s->begin_input("same",9,3,0,ScoringMode::BaseOnly)==SessionStatus::Pending,"replace input"); tick(h,*s);
    } else s.reset();
    check(h.counts.frozen.active.revision!=old && h.counts.frozen.cancels==cancels+1,"input lifetime retained count");
    check(h.read_calls()==reads && h.counts.frozen.advances==advances,"cleanup performed I/O/advance");
    if (action==1) {publish(h,*s); check(page(*s).mode==ScoringMode::BaseOnly,"replacement was damaged");}
    if (s) {end(h,*s); s.reset();}
    check(h.counts.frozen.cancels==cancels+1,"cleanup was not idempotent");
  }
  // Independently replace either port, then both. Cleanup must neither depend
  // on the other port's freshness nor cancel an external successor.
  for (int replacement=0;replacement<3;++replacement) {
    SessionFixture h(9);
    auto s=std::make_unique<CandidateSession>(h.full,h.shared,h.counts,h.sink,small);
    check(admit(h,*s).status==TextAccessStatus::Ready,"foreign admit"); first(h,*s); wait_for_count(h,*s);
    const auto old=h.counts.frozen.active.revision;
    const auto cancels=h.counts.frozen.cancels;
    CountTicket newer_count; SharedReadCursor newer_shared;
    if (replacement!=0) {
      h.counts.frozen.set(session_key(1),77); h.counts.frozen.delay=0;
      newer_count=h.counts.begin_count(h.counts.view(),session_key(1));
    }
    if (replacement!=1) {
      h.mount_shared();
      check(h.shared.begin_read(SharedRecordKind::Lexeme,2,&newer_shared)==SharedReadStatus::InProgress,"new shared read");
      check(h.shared.advance_read(&newer_shared,{1,1,1}).status==SharedReadStatus::InProgress,"new shared pending");
    }
    const auto reads=h.read_calls(), advances=h.counts.frozen.advances;
    tick(h,*s); s->cancel_input(s->input_handle()); tick(h,*s); s.reset();
    check(h.read_calls()==reads && h.counts.frozen.advances==advances,"old cleanup advanced new ports");
    if (replacement==0) {
      check(h.counts.frozen.active.revision!=old && h.counts.frozen.cancels==cancels+1,"stale shared blocked count cleanup");
    } else {
      check(h.counts.frozen.cancels==cancels && h.counts.frozen.active.revision==newer_count.revision,"external count cancelled");
      const auto r=h.counts.advance_count(newer_count,{1,1,1});
      check(r.status==CountStatus::Found && r.count==77,"external count damaged");
      h.counts.cancel_count(newer_count);
    }
    if (replacement!=1) {
      SharedReadResult r;
      do {r=h.shared.advance_read(&newer_shared,{1,1,1});} while(r.status==SharedReadStatus::InProgress);
      check(r.status==SharedReadStatus::Ready && r.lexeme.identity.stable_key==session_key(2),"external shared read cancelled");
      h.shared.cancel_read(&newer_shared);
    }
  }
  // A ranker used without CandidateSession has the same cleanup obligation.
  for (int action=0;action<4;++action) {
    SessionFixture h(3); RankItem work[4]{}, out[3]{}; RankTicket ticket;
    auto r=std::make_unique<ExactRanker>(h.full,h.shared,h.counts,work,4,1,32);
    check(r->begin_input("same",9)==RankStatus::InProgress,"direct begin");
    check(r->request_range(RangeDirection::First,nullptr,3,out,3,&ticket)==RankStatus::InProgress,"direct range");
    h.counts.frozen.delay=1000; RankStatus status=RankStatus::InProgress;
    for (unsigned i=0;status!=RankStatus::WaitingState && i<100000;++i) status=r->advance(ticket,{1,17,3}).status;
    check(status==RankStatus::WaitingState,"direct waiting");
    if (action==3) h.mount_shared();
    const auto cancels=h.counts.frozen.cancels, reads=h.read_calls(), advances=h.counts.frozen.advances;
    if (action==0) {h.counts.frozen.reopen(); r->cancel(ticket); r->cancel(ticket);}
    else if (action==1) check(r->begin_input("same",9,{},ScoringMode::BaseOnly)==RankStatus::InProgress,"direct new input");
    else if (action==2) check(r->restart_rank(ScoringMode::BaseOnly)==RankStatus::InProgress,"direct restart");
    else r.reset();
    check(h.counts.frozen.active.revision==0 && h.counts.frozen.cancels==cancels+1,"direct cleanup retained count");
    check(h.read_calls()==reads && h.counts.frozen.advances==advances,"direct cleanup I/O");
    if (action==1 || action==2) {
      RankTicket fresh;
      check(r->request_range(RangeDirection::First,nullptr,3,out,3,&fresh)==RankStatus::InProgress,"fresh range");
      r->cancel(ticket); check(!r->abandon_range(ticket),"old ticket abandoned successor");
      do {status=r->advance(fresh,{1,17,3}).status;} while(status==RankStatus::InProgress);
      check(status==RankStatus::Ready && r->result(fresh).size==3 && r->mode()==ScoringMode::BaseOnly,"old cleanup damaged new rank");
      r->release(fresh);
    }
  }
}
void print_cost(const char* label,WorkCost c) {std::cout<<label<<"="<<c.read_calls<<","<<c.read_bytes<<","<<c.work_items<<" ";}
WorkCost cost_difference(WorkCost a,WorkCost b) {
  return {a.read_calls-b.read_calls,a.read_bytes-b.read_bytes,a.work_items-b.work_items};
}
void scale_costs() {
  std::size_t peak0=0;
  for (std::size_t n:{16U,160U}) {
    SessionFixture h(n); rank_probe::start(); std::unique_ptr<CandidateSession> s;
    {rank_probe::Capture capture; s=std::make_unique<CandidateSession>(h.full,h.shared,h.counts,h.sink,small);}
    check(admit(h,*s).status==TextAccessStatus::Ready,"scale admit");
    check(s->set_output({"test:variant",OutputInvocation::Profile})==SessionStatus::Ready,"scale output");
    first(h,*s); const auto after_first=s->costs();
    const auto present_start=std::chrono::steady_clock::now();
    const auto shown=page(*s); check(shown.ranked.size()==3,"scale present");
    const auto present_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-present_start).count();
    check(s->request_range(RangeDirection::After,3,0)==SessionStatus::Pending,"scale next");
    const auto read=h.lr->calls;
    while(h.lr->calls==read) tick(h,*s,{1,1,1});
    h.sink.accept=false; auto failed=commit(h,*s); check(failed.status==CommitStatus::RetryableNoEffect,"scale failed submit");
    const auto before_resume=s->costs().ranking;
    s->cancel_commit(failed.transaction); check(s->resume_range()==SessionStatus::Pending,"scale resume"); publish(h,*s);
    const auto resumed_cost=cost_difference(s->costs().ranking,before_resume);
    check(s->request_text(s->published(),0,1)==SessionStatus::Pending,"scale visible text"); check(finish(h,*s)==SessionStatus::StagingReady,"scale text");
    h.sink.accept=true; check(commit(h,*s).status==CommitStatus::Accepted,"scale accepted submit");
    const auto m=s->memory_usage();
    const auto c=s->costs(); const auto o=s->output_costs(); const auto probe=rank_probe::stop();
    if (probe.supported) {if(!peak0)peak0=probe.peak; else check(probe.peak<=peak0+256,"scale-dependent retained allocation");}
    std::cout<<"synthetic N="<<n<<" W=4 S=3 B=1 L=16 F=32 page=96 cache=1 budget=1,17,3\n";
    std::cout<<"files shared="<<h.lr->size()<<" scheme="<<h.fr->size()<<" "; print_cost("admission",c.admission); print_cost("first_rank",after_first.ranking); print_cost("all_rank",c.ranking);
    print_cost("display",c.display); print_cost("commit",c.commit); print_cost("local",c.local); std::cout<<"\n";
    print_cost("interrupted_rank",cost_difference(before_resume,after_first.ranking));
    print_cost("resumed_rank",resumed_cost); std::cout<<"\n";
    print_cost("output_metadata",o.metadata); print_cost("output_names",o.names); print_cost("output_payload",o.payload);
    std::cout<<"compared_bytes="<<o.compared_bytes<<" name_copied="<<o.name_payload_bytes<<" text_copied="<<o.text_payload_bytes
      <<" abandoned="<<c.abandoned_ranges<<" resumed="<<c.resumed_ranges<<"\n";
    std::cout<<"memory owner="<<m.owner_bytes<<" impl="<<m.impl_bytes<<" work="<<m.rank_work_bytes<<" result="<<m.rank_result_bytes
      <<" snapshot_slots="<<m.snapshot_bytes<<" text_pools="<<m.text_pool_bytes<<" commit_pool="<<m.commit_pool_bytes<<" names="<<m.name_bytes
      <<" form_capacity="<<m.form_capacity<<" code_capacity="<<m.code_capacity<<"\n";
    std::cout<<"included_in_impl output_reader="<<m.output_reader_bytes<<" access_check="<<m.access_check_bytes<<" transaction="<<m.transaction_bytes
      <<" snapshot_common="<<m.snapshot_common_bytes<<" text_cursor="<<m.text_cursor_bytes<<" tokens="<<m.input_token_bytes<<","<<m.snapshot_token_bytes<<","<<m.commit_token_bytes<<" (control blocks extra)\n";
    std::cout<<"rank_impl="<<m.ranking.impl_bytes<<" raw_batch="<<m.ranking.batch_capacity_bytes<<" shared_owner="<<h.shared.memory_usage().owner_fixed_bytes
      <<" shared_cache="<<h.shared.memory_usage().cache_payload_allocated_bytes+h.shared.memory_usage().cache_metadata_allocated_bytes
      <<" captured_current="<<probe.current<<" captured_peak="<<probe.peak<<" allocations="<<probe.allocations<<" supported="<<probe.supported
      <<" sink_host_ns="<<h.sink.nanoseconds<<" present_with_host_copy_ns="<<present_ns<<"\n";
    std::cout<<"transient sizeof text_result="<<sizeof(SharedTextResult)<<" output_step="<<sizeof(OutputStep)
      <<" access_step="<<sizeof(TextAccessStep)<<" session_step="<<sizeof(SessionStep)
      <<" commit_result="<<sizeof(CommitResult)<<" view="<<sizeof(CandidateViewPage)
      <<" selection="<<sizeof(SelectionHandle)<<" retry="<<sizeof(RetryPermit)<<"\n";
  }
}
} // namespace
int main() {
  const std::pair<const char*,void(*)()> tests[]={
    {"D1 generic forms/shared identity",generic_identity},{"D2 relation restrictions",restrictions},
    {"D3 bounded payload/admission",payload_and_admission},{"D4 snapshots/logical ranges",snapshots_and_ranges},
    {"D5 unique read handoff",read_handoff},{"D6 freshness/BaseOnly",freshness_and_baseonly},
    {"D7 once/retry/learning failures",commit_retry_and_learning},{"D8 lifecycle/callback isolation",lifecycle_callbacks},
    {"D9 synthetic bounded costs",scale_costs},{"D10 terminal count cleanup",terminal_count_cleanup},
    {"D11 cleanup ownership/lifetime",cleanup_ownership_and_lifetime}};
  std::size_t passed=0;
  for (const auto& test:tests) {
    try {test.second(); ++passed; std::cout<<"PASS "<<test.first<<"\n";}
    catch(const std::exception& e) {std::cout<<"FAIL "<<test.first<<": "<<e.what()<<"\n";}
  }
  constexpr auto total=sizeof(tests)/sizeof(tests[0]);
  std::cout<<passed<<"/"<<total<<" session groups passed\n"; return passed==total ? 0 : 1;
}
