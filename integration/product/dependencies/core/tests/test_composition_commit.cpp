#include "hanime/composition_commit.hpp"
#include "session_fixture.hpp" // Only synthetic builder/reader helpers used; NO state owner instantiated.
#include "rank_allocation_probe.hpp"
#include <array>
#include <iostream>
#include <optional>

namespace {
using namespace hanime;
using namespace hanime::test;
void check(bool v,const char* m) {require_session(v,m);}
constexpr WorkBudget tiny{1,1,1};
constexpr CompositionConfig config{3,32,32,96};
struct Document final : TextCommitSink {
  std::array<char,4096> storage{};
  std::size_t size{0},calls{0};
  bool reject{false},throw_before{false},throw_after{false};
  std::function<void()> callback;
  std::string_view text() const {return {storage.data(),size};}
  bool commit(std::string_view text) override {
    ++calls;
    if (callback) callback();
    if (throw_before) throw std::runtime_error("unknown before acceptance");
    if (reject || text.size()>storage.size()-size) return false;
    if (!text.empty()) std::memcpy(storage.data()+size,text.data(),text.size());
    size+=text.size();
    if (throw_after) throw std::runtime_error("unknown AFTER final document changed");
    return true;
  }
};
struct Fixture {
  BuiltResources built;
  PagedSharedResource shared;
  std::shared_ptr<SessionReader> reader;
  Document doc;
  explicit Fixture(std::size_t repeats=0) {
    auto input=session_fixture(3);
    if (repeats) for (auto& lexeme:input.lexemes) {
      lexeme.default_output_utf8.clear();
      for (std::size_t i=0;i<repeats;++i) lexeme.default_output_utf8+="𨿍";
    }
    auto b=ResourceBuilder().build(input,{96,96}); check(b.ok,"synthetic build");
    built=std::move(b.resources); mount();
  }
  void mount() {
    reader=std::make_shared<SessionReader>(built.shared_bundle); std::string error;
    check(shared.open(reader,0x53455353494f4eULL,built.manifest.shared_bundle_id,9,{1,17},&error),"open");
    const auto ticket=shared.validation_cursor();
    for (unsigned i=0;!shared.ready();++i) {
      check(i<100000,"validation stalled"); const auto s=shared.advance_validation(ticket,{1,17,3});
      check(s.status==ValidationStepStatus::InProgress || s.status==ValidationStepStatus::Ready,"validation");
    }
  }
};
std::array<CompositionMember,3> members() {
  return {{{1,session_key(1),{0,2},{}},{2,session_key(2),{2,4},{}},{1,session_key(1),{4,6},{}}}};
}
CompositionOrigin origin(std::size_t n=3,std::uint64_t revision=1) {return {revision,1,{0,2*n}};}
CompositionResult begin(Fixture& f,CompositionCommitter& c,std::size_t n=3) {
  const auto input=members(); const auto reads=f.reader->calls;
  auto r=c.prepare(f.shared.activation_handle(),origin(n),input.data(),n);
  check(r.status==CompositionStatus::Preparing,"prepare"); check(f.reader->calls==reads,"capture I/O"); return r;
}
CompositionStep advance(Fixture& f,CompositionCommitter& c,const CompositionHandle& h,WorkBudget b=tiny) {
  const auto calls=f.reader->calls,bytes=f.reader->requested;
  auto s=c.advance(h,b);
  check(s.cost.read_calls<=b.max_read_calls && s.cost.read_bytes<=b.max_read_bytes && s.cost.work_items<=b.max_work_items,"budget");
  check(s.cost.read_calls==f.reader->calls-calls && s.cost.read_bytes==f.reader->requested-bytes,"unmetered I/O");
  return s;
}
CompositionResult ready(Fixture& f,CompositionCommitter& c,const CompositionHandle& h,WorkBudget b=tiny) {
  const auto old_size=f.doc.size,old_calls=f.doc.calls;
  for (unsigned i=0;i<200000;++i) {
    const auto s=advance(f,c,h,b);
    check(f.doc.size==old_size && f.doc.calls==old_calls,"preparation submitted a prefix");
    if (s.result.status!=CompositionStatus::Preparing) return s.result;
  }
  throw std::runtime_error("preparation stalled");
}
void strings_and_capture() {
  Fixture f; CompositionCommitter c(f.shared,f.doc,config);
  auto m=members(); std::string form="test:variant"; m[1].output={form,OutputInvocation::Profile};
  auto r=c.prepare(f.shared.activation_handle(),origin(),m.data(),3);
  check(c.prepared(r.transaction,[](const auto&) {throw std::runtime_error("partial view");})==CompositionStatus::Preparing,"not pending");
  form.assign("blocked"); m[0].ref=3; m[0].expected_key=session_key(3);
  check(ready(f,c,r.transaction).status==CompositionStatus::Ready,"three ready");
  check(c.prepared(r.transaction,[&](const PreparedCompositionView& v) {
    check(v.text=="同文𨿍V:𨿍同文𨿍" && is_valid_utf8(v.text),"UTF8/captured name");
    check(v.size==3 && v.members[0].selected.expected_key==session_key(1) &&
        v.members[2].selected.expected_key==session_key(1),"repeat key lost");
    check(v.members[1].selected.output.form=="test:variant","borrowed caller form");
    check(v.members[0].text.begin==0 && v.members[1].text.begin==v.members[0].text.end &&
        v.members[2].text.end==v.text.size(),"member text spans");
  })==CompositionStatus::Ready,"view");
  const auto reads=f.reader->calls;
  auto accepted=c.confirm(r.transaction);
  check(accepted.status==CompositionStatus::Accepted && accepted.learning==CompositionLearning::DisabledByConfiguration,"disabled accept");
  check(f.doc.calls==1 && f.reader->calls==reads,"confirm I/O or sink count");
  check(c.confirm(r.transaction).status==CompositionStatus::Accepted && r.transaction.result().status==CompositionStatus::Accepted && f.doc.calls==1,"repeat accept");
  check(c.cancel(r.transaction).status==CompositionStatus::Accepted,"accepted cancelled");
  auto two=begin(f,c,2); check(ready(f,c,two.transaction).status==CompositionStatus::Ready,"two ready");
  c.prepared(two.transaction,[](const auto& v) {check(v.members[0].selected.expected_key!=v.members[1].selected.expected_key && v.text=="同文𨿍同文𨿍","same text merged");});
  check(c.confirm(two.transaction).status==CompositionStatus::Accepted && f.doc.calls==2,"two accept");
  check(c.cancel(r.transaction).status==CompositionStatus::Stale && r.transaction.result().status==CompositionStatus::Accepted,"old receipt overwritten");
}
void restrictions() {
  Fixture f;
  for (const auto& entry:{std::make_pair("condition",OutputStatus::ConditionUndetermined),
      std::make_pair("blocked",OutputStatus::Blocked),std::make_pair("explicit",OutputStatus::InvocationDenied),
      std::make_pair("multiple",OutputStatus::MultipleRelations)}) {
    CompositionCommitter c(f.shared,f.doc,config); auto m=members(); m[1].output={entry.first,OutputInvocation::Profile};
    auto r=c.prepare(f.shared.activation_handle(),origin(),m.data(),3); r=ready(f,c,r.transaction);
    check(r.status==CompositionStatus::OutputRestricted && r.member_index==1 && r.output==entry.second,"restricted output bypass");
    check(c.confirm(r.transaction).status==r.status && f.doc.calls==0,"restricted submitted");
  }
  CompositionCommitter c(f.shared,f.doc,config); auto m=members();
  m[0].output={"missing",OutputInvocation::Profile}; m[1].output={"explicit",OutputInvocation::Explicit};
  auto r=c.prepare(f.shared.activation_handle(),origin(),m.data(),3);
  check(ready(f,c,r.transaction).status==CompositionStatus::Ready,"legal output failed");
  c.prepared(r.transaction,[](const auto& v) {check(v.text=="同文𨿍许可同文𨿍" && v.members[0].resolved==OutputStatus::FallbackDefault,"fallback/explicit");});
  check(c.confirm(r.transaction).status==CompositionStatus::Accepted,"explicit accepted");
}
void errors_and_capacity() {
  // Existing public aggregate decomposition must remain source-compatible.
  const auto [old_status,old_cost,old_text,old_payload]=OutputStep{};
  check(old_status==OutputStatus::Pending && old_cost.read_calls==0 && old_text.empty() && old_payload==kNoSharedString,"OutputStep compatibility");
  Fixture f; CompositionCommitter c(f.shared,f.doc,config); auto m=members();
  auto r=begin(f,c);
  check(c.advance(r.transaction,{0,1,1}).result.status==CompositionStatus::InvalidBudget,"invalid budget");
  check(r.transaction.result().status==CompositionStatus::Preparing,"bad budget mutated");
  m[1].source.begin=1;
  check(c.prepare(f.shared.activation_handle(),origin(),m.data(),3).status==CompositionStatus::InvalidRequest,"overlap");
  m=members(); m[1].source.begin=3;
  check(c.prepare(f.shared.activation_handle(),origin(),m.data(),3).status==CompositionStatus::InvalidRequest,"gap");
  m=members(); m[1].expected_key=session_key(3);
  r=c.prepare(f.shared.activation_handle(),origin(),m.data(),3); r=ready(f,c,r.transaction);
  check(r.status==CompositionStatus::IdentityMismatch && r.member_index==1 && f.doc.calls==0,"full key mismatch");
  m=members(); m[0].expected_key.bytes[1]=1; // Same prefix/suffix is not full equality.
  r=c.prepare(f.shared.activation_handle(),origin(),m.data(),3);
  check(ready(f,c,r.transaction).status==CompositionStatus::IdentityMismatch,"key prefix comparison");
  m=members(); m[0].ref=0;
  check(c.prepare(f.shared.activation_handle(),origin(),m.data(),3).status==CompositionStatus::InvalidRequest,"zero ref");
  check(c.prepare({},origin(),m.data(),3).status==CompositionStatus::Stale,"invalid binding");
  for (const auto& spec:{std::make_pair(CompositionConfig{2,32,32,96},CompositionCapacity::Members),
      std::make_pair(CompositionConfig{3,3,32,96},CompositionCapacity::RequestName),
      std::make_pair(CompositionConfig{3,32,2,96},CompositionCapacity::MemberText),
      std::make_pair(CompositionConfig{3,32,32,11},CompositionCapacity::TotalText),
      std::make_pair(CompositionConfig{3,1,32,96},CompositionCapacity::ResolverName)}) {
    CompositionCommitter limited(f.shared,f.doc,spec.first); m=members();
    if (spec.second==CompositionCapacity::RequestName) m[0].output={"test:variant",OutputInvocation::Profile};
    if (spec.second==CompositionCapacity::ResolverName) m[0].output={"x",OutputInvocation::Profile};
    auto s=limited.prepare(f.shared.activation_handle(),origin(),m.data(),3);
    if (s.status==CompositionStatus::Preparing) s=ready(f,limited,s.transaction);
    check(s.status==CompositionStatus::CapacityExceeded && s.capacity==spec.second && s.required_capacity>0,"capacity diagnostics");
    if (s.capacity==CompositionCapacity::TotalText) check(s.required_capacity==20 && s.member_index==1,"prefix lower bound");
    if (s.capacity==CompositionCapacity::MemberText) check(s.required_capacity==10,"exact member capacity");
    check(f.doc.calls==0,"capacity called sink");
  }
  CompositionCommitter invalid(f.shared,f.doc,{std::numeric_limits<std::size_t>::max(),32,32,96});
  check(!invalid.valid(),"overflow config");
  if (rank_probe::fail_next_allocation()) {CompositionCommitter failed(f.shared,f.doc,config); check(!failed.valid(),"allocation failure");}
  m=members(); r=c.prepare(f.shared.activation_handle(),origin(),m.data(),3);
  if (rank_probe::fail_next_allocation()) {
    check(c.prepare(f.shared.activation_handle(),origin(),m.data(),3).status==CompositionStatus::AllocationFailure,"receipt allocation");
    check(r.transaction.result().status==CompositionStatus::Preparing,"failed capture cancelled original");
  }
}
void io_and_activation() {
  for (unsigned steps:{0U,3U,90U,150U}) {
    Fixture f; CompositionCommitter c(f.shared,f.doc,config); auto r=begin(f,c);
    for (unsigned i=0;i<steps;++i) check(advance(f,c,r.transaction).result.status==CompositionStatus::Preparing,"early fixture end");
    f.reader->fail=true;
    auto s=ready(f,c,r.transaction);
    check(s.status==CompositionStatus::ReadError && f.doc.calls==0,"I/O failure submitted prefix");
    check(c.confirm(r.transaction).status==CompositionStatus::ReadError,"failed re-armed");
  }
  for (bool after_ready:{false,true}) {
    Fixture f; CompositionCommitter c(f.shared,f.doc,config); auto r=begin(f,c);
    if (after_ready) check(ready(f,c,r.transaction).status==CompositionStatus::Ready,"ready");
    else advance(f,c,r.transaction);
    f.mount(); // SAME package and epoch, NEW activation.
    auto s=after_ready ? c.confirm(r.transaction) : advance(f,c,r.transaction).result;
    check(s.status==CompositionStatus::Stale && f.doc.calls==0,"same activation revival");
    auto next=begin(f,c); check(ready(f,c,next.transaction).status==CompositionStatus::Ready,"new activation");
    check(c.confirm(next.transaction).status==CompositionStatus::Accepted,"new activation submit");
  }
}
void retry_and_unknown() {
  Fixture f; CompositionCommitter c(f.shared,f.doc,config); auto r=begin(f,c);
  check(ready(f,c,r.transaction).status==CompositionStatus::Ready,"ready"); f.doc.reject=true;
  r=c.confirm(r.transaction); check(r.status==CompositionStatus::RetryableNoEffect && f.doc.size==0 && f.doc.calls==1,"reject effects");
  const auto permit=r.retry;
  check(c.confirm(r.transaction).status==CompositionStatus::RetryableNoEffect && f.doc.calls==1,"confirmation retried");
  const auto reads=f.reader->calls;
  check(c.retry(permit).status==CompositionStatus::Preparing,"permit refused");
  check(c.retry(permit).attempt==2,"duplicate retry attempt");
  check(ready(f,c,r.transaction).status==CompositionStatus::Ready && f.reader->calls>reads,"retry did not reread");
  f.doc.reject=false;
  check(c.confirm(r.transaction).status==CompositionStatus::Accepted && f.doc.calls==2,"retry accept");
  check(c.retry(permit).status==CompositionStatus::Accepted && f.doc.calls==2,"old retry rearmed");
  auto next=begin(f,c); c.cancel(next.transaction);
  check(c.confirm(next.transaction).status==CompositionStatus::Cancelled && f.doc.calls==2,"cancelled called sink");
  for (bool after:{false,true}) {
    Document doc; doc.throw_before=!after; doc.throw_after=after; CompositionHandle retained;
    {
      CompositionCommitter u(f.shared,doc,config); auto m=members();
      auto q=u.prepare(f.shared.activation_handle(),origin(),m.data(),3); retained=q.transaction;
      check(ready(f,u,retained).status==CompositionStatus::Ready,"unknown ready");
      q=u.confirm(retained); check(q.status==CompositionStatus::UnknownOutcome,"unknown disguised as reject");
      check(doc.size==(after ? 30U : 0U),"unknown fixture effects");
      check(u.cancel(retained).status==CompositionStatus::UnknownOutcome,"unknown cancelled");
      check(u.confirm(retained).status==CompositionStatus::UnknownOutcome,"unknown rearmed");
      check(u.prepare(f.shared.activation_handle(),origin(),m.data(),3).status==CompositionStatus::UnknownOutcome,"unknown bypassed");
      u.close(); check(doc.calls==1,"unknown replay");
    }
    check(retained.result().status==CompositionStatus::UnknownOutcome,"destroy lost unknown receipt");
  }
  // Rejected text cannot retry against a new activation.
  auto s=begin(f,c); ready(f,c,s.transaction); f.doc.reject=true; s=c.confirm(s.transaction); f.mount();
  check(c.retry(s.retry).status==CompositionStatus::Stale,"retry ignored resource change");
}
void lifecycle_cleanup() {
  Fixture f;
  // Cover metadata, selector/name/payload and copy stages, plus unfinished
  // destruction and Ready. New external work must survive each old cleanup.
  for (unsigned stop=0;stop<400;stop+=3) {
    std::optional<CompositionCommitter> c; c.emplace(f.shared,f.doc,config);
    auto m=members(); m[0].output={"test:variant",OutputInvocation::Profile};
    auto r=c->prepare(f.shared.activation_handle(),origin(),m.data(),3);
    for (unsigned j=0;j<stop && r.transaction.result().status==CompositionStatus::Preparing;++j) advance(f,*c,r.transaction,{1,17,3});
    SharedReadCursor fresh; check(f.shared.begin_read(SharedRecordKind::Lexeme,3,&fresh)==SharedReadStatus::InProgress,"external read");
    const auto calls=f.reader->calls;
    if (stop%2==0) c->cancel(r.transaction);
    c.reset();
    check(f.reader->calls==calls,"cleanup advanced I/O");
    check(r.transaction.result().status==CompositionStatus::Cancelled,"destroy unfinished receipt");
    SharedReadResult s;
    do {s=f.shared.advance_read(&fresh,tiny);} while (s.status==SharedReadStatus::InProgress);
    check(s.status==SharedReadStatus::Ready && s.lexeme.identity.stable_key==session_key(3),"old cleanup cancelled external read");
  }
  CompositionHandle old;
  alignas(CompositionCommitter) unsigned char storage[sizeof(CompositionCommitter)];
  auto* a=new(storage) CompositionCommitter(f.shared,f.doc,config); old=begin(f,*a).transaction; a->~CompositionCommitter();
  auto* b=new(storage) CompositionCommitter(f.shared,f.doc,config); auto current=begin(f,*b);
  check(b->cancel(old).status==CompositionStatus::Stale,"address reused owner identity");
  check(b->advance(old,tiny).result.status==CompositionStatus::Stale,"foreign advance");
  check(ready(f,*b,current.transaction).status==CompositionStatus::Ready,"foreign work hurt current");
  check(b->confirm(current.transaction).status==CompositionStatus::Accepted,"new address owner submit");
  b->~CompositionCommitter(); check(current.transaction.result().status==CompositionStatus::Accepted,"destroy accepted");
}
void reentry_and_origin() {
  Fixture f; CompositionCommitter c(f.shared,f.doc,config); auto r=begin(f,c); ready(f,c,r.transaction);
  std::uint64_t visible_input=1; bool queued=false; auto m=members();
  f.doc.callback=[&] {
    visible_input=2; queued=true; // One bounded external replacement event.
    check(r.transaction.result().status==CompositionStatus::CallingSink,"calling state not fixed");
    check(c.prepare(f.shared.activation_handle(),origin(3,2),m.data(),3).status==CompositionStatus::Busy,"recursive prepare");
    check(c.cancel(r.transaction).status==CompositionStatus::Busy && c.close().status==CompositionStatus::Busy,"recursive cleanup");
    check(c.confirm(r.transaction).status==CompositionStatus::Busy && c.advance(r.transaction,tiny).result.status==CompositionStatus::Busy,"recursive sink/advance");
  };
  auto accepted=c.confirm(r.transaction);
  if (accepted.origin.input_revision==visible_input) visible_input=0;
  check(visible_input==2 && queued && f.doc.calls==1,"old result cleared new input"); f.doc.callback={};
  auto next=c.prepare(f.shared.activation_handle(),origin(3,2),m.data(),3);
  check(c.cancel(r.transaction).status==CompositionStatus::Stale,"old cancellation entered new input");
  check(ready(f,c,next.transaction).status==CompositionStatus::Ready,"new work");
  bool caught=false;
  try {c.prepared(next.transaction,[](const auto&) {throw std::runtime_error("view exception");});} catch (...) {caught=true;}
  check(caught && c.confirm(next.transaction).status==CompositionStatus::Accepted,"callback guard not released");
  c.close(); check(c.prepare(f.shared.activation_handle(),origin(),m.data(),3).status==CompositionStatus::Closed,"closed owner reused");
}
void memory_costs() {
  const auto m=members();
  for (const auto& cfg:{CompositionConfig{2,32,32,64},CompositionConfig{3,32,32,96},
      CompositionConfig{8,128,128,1024},CompositionConfig{3,32,64,192}}) {
    Fixture f(cfg.member_text_bytes==64 ? 12 : 0);
    rank_probe::start();
    std::optional<CompositionCommitter> empty;
    {rank_probe::Capture capture; empty.emplace(f.shared,f.doc,cfg);}
    const auto constructed=rank_probe::stop(); empty.reset();
    check(!constructed.supported || rank_probe::stop().current==0,"constructor allocation release");
    std::optional<CompositionCommitter> c; CompositionHandle h; CompositionMemoryUsage usage;
    rank_probe::start();
    {
      rank_probe::Capture capture; c.emplace(f.shared,f.doc,cfg); usage=c->memory_usage();
      const auto n=std::min(cfg.max_members,std::size_t(3));
      h=c->prepare(f.shared.activation_handle(),origin(n),m.data(),n).transaction;
      check(ready(f,*c,h).status==CompositionStatus::Ready,"memory ready");
      check(c->confirm(h).status==CompositionStatus::Accepted,"memory accept");
    }
    const auto resident=rank_probe::stop(); const auto cost=c->costs(); const auto text_size=h.result().text_bytes;
    c.reset(); h={};
    const auto freed=rank_probe::stop();
    check(!freed.supported || freed.current==0,"owned allocation retained");
    std::cout<<"MEM M="<<cfg.max_members<<" F="<<cfg.form_bytes<<" L="<<cfg.member_text_bytes<<" T="<<cfg.total_text_bytes
      <<" owner="<<usage.owner_bytes<<" impl="<<usage.impl_bytes<<" members="<<usage.members_bytes
      <<" requests="<<usage.request_names_bytes<<" name_scratch="<<usage.resolver_name_bytes
      <<" member_scratch="<<usage.member_text_bytes<<" whole_text="<<usage.total_text_bytes
      <<" owner_token_payload="<<usage.owner_token_bytes<<" receipt_payload="<<usage.receipt_bytes
      <<" default_new_current="<<resident.current<<" peak="<<resident.peak<<" allocations="<<resident.allocations
      <<" constructed_new="<<constructed.current<<" receipt_block="<<resident.current-constructed.current<<" actual_text="<<text_size
      <<" capture_bytes="<<cost.captured_bytes<<" prepare_reads="<<cost.preparation.read_calls
      <<" prepare_bytes="<<cost.preparation.read_bytes<<" work="<<cost.preparation.work_items<<" freed="<<freed.current<<'\n';
  }
}
}
int main() {
  const std::array<std::pair<const char*,void(*)()>,8> tests{{{"M1 capture/UTF8/atomic/Disabled",strings_and_capture},
      {"M2 generic restrictions",restrictions},{"M3 structure/identity/capacity",errors_and_capacity},
      {"M4 I/O and activation",io_and_activation},{"M5 retry/unknown",retry_and_unknown},
      {"M6 cleanup/owner isolation",lifecycle_cleanup},{"M7 callback/origin isolation",reentry_and_origin},
      {"M8 memory and budgets",memory_costs}}};
  unsigned passed=0;
  for (const auto& t:tests) {try {t.second(); ++passed; std::cout<<"PASS "<<t.first<<'\n';}
    catch (const std::exception& e) {std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<'\n';}}
  std::cout<<"Composition "<<passed<<"/"<<tests.size()<<'\n'; return passed==tests.size() ? 0 : 1;
}
