// Synthetic helpers and the original packet assertions stay test-only.
#define HANIME_PACKED_TEST_MAIN original_packed_main_for_output_perf
#include "test_packed_auth.cpp"
#undef HANIME_PACKED_TEST_MAIN
#include "session_fixture.hpp"
#include <new>

namespace {
using hanime::test::session_key;
struct ReuseFixture {
  Fixture files;
  BuiltResources built;
  PublishedResource published;
  Bytes packets;
  explicit ReuseFixture(const fs::path& path) : files(path) {
    auto input=hanime::test::session_fixture(40);
    input.lexemes.back().default_output_utf8=std::string(2051,'x')+"𨿍结束";
    auto made=ResourceBuilder().build(input,{4096,4096});
    check(made.ok,"output fixture build"); built=std::move(made.resources);
    auto options=files.options(built.shared_bundle,built.scheme_bundles.at("full_pinyin"));
    options.text_capacity=4096; options.packed_auth=true;
    options.registry_namespace=input.registry_namespace;
    const auto result=publish_verified_resources(options);
    check(result.ok,"final-byte output publication: "+result.error);
    published=result.resources[0]; packets=read(fs::path(options.output_directory)/"shared.hnv");
  }
};
struct ReuseRuntime {
  std::shared_ptr<PacketPort> port;
  std::shared_ptr<VerifiedResourceReader> auth;
  PagedSharedResource shared;
  const ReuseFixture& fixture;
  OutputReuseConfig config;
  ReuseRuntime(const ReuseFixture& f,OutputReuseConfig c={4,64,64}) : fixture(f),config(c) {mount();}
  void mount() {
    shared.deactivate();
    port=std::make_shared<PacketPort>(fixture.packets);
    auth=std::make_shared<VerifiedResourceReader>(nullptr,port,fixture.published.spec,
                                               VerifiedResourceConfig{3,1,1024,4});
    check(auth->prepare_bootstrap(),"output bootstrap start");
    for (unsigned guard=0;!auth->bootstrap_ready();++guard) {
      check(guard<100000,"output bootstrap timeout");
      check(auth->advance_bootstrap(ordinary).status!=ResourceReadStatus::Error,"output bootstrap error");
    }
    std::string error;
    check(shared.open_verified(auth,9,{1,1024,true,config},&error),"output mount: "+error);
  }
};
struct OutputValue {
  OutputStatus status;
  std::string text;
  OutputCapacityRequirement capacity;
  WorkCost cost;
  std::size_t steps{0};
};
OutputStep finish_output(OutputReader& reader,WorkBudget budget=ordinary,WorkCost* sum=nullptr,std::size_t* steps=nullptr) {
  for (unsigned guard=0;guard<1000000;++guard) {
    const auto s=reader.advance(budget);
    check(s.cost.read_calls<=budget.max_read_calls && s.cost.read_bytes<=budget.max_read_bytes &&
          s.cost.work_items<=budget.max_work_items,"output reuse exceeded budget");
    if (sum) {sum->read_calls+=s.cost.read_calls;sum->read_bytes+=s.cost.read_bytes;sum->work_items+=s.cost.work_items;}
    if (steps) ++*steps;
    if (s.status!=OutputStatus::Pending) return s;
    check(s.text.empty(),"output exposed partial text");
  }
  throw std::runtime_error("output reuse stalled");
}
OutputValue output(ReuseRuntime& r,RuntimeLexemeRef ref,OutputRequest request={},
    std::size_t text_capacity=4096,std::size_t name_capacity=128,WorkBudget b=ordinary) {
  std::array<char,4096> text{};std::array<char,256> name{};OutputReader reader;
  OutputValue value{OutputStatus::Pending,{},{},{},0};
  value.status=reader.begin(r.shared,r.shared.activation_handle(),ref,session_key(ref),request,
                            name.data(),name_capacity,text.data(),text_capacity);
  if (value.status!=OutputStatus::Pending) return value;
  const auto s=finish_output(reader,b,&value.cost,&value.steps);
  value.status=s.status;value.text=s.text;value.capacity=reader.capacity_requirement();return value;
}
void equal(const OutputValue& a,const OutputValue& b) {
  check(a.status==b.status && a.text==b.text && a.capacity.bytes==b.capacity.bytes && a.capacity.name==b.capacity.name,
        "reuse changed output semantics/capacity");
}
void identity_forms_capacity(const ReuseFixture& f) {
  ReuseRuntime off(f,{0,64,64}),on(f,{4,64,64});
  for (const auto& request:std::array<OutputRequest,10>{{{}, {"test:regional"},{"test:variant"},{"not-present"},
      {"condition"},{"blocked"},{"multiple"},{"explicit"},{"explicit",OutputInvocation::Explicit},
      {"explicit",OutputInvocation::Expanded}}}) {
    const auto a=output(off,1,request),b=output(on,1,request); equal(a,b);
    const auto count=on.port->calls;
    const auto hits=on.shared.output_reuse_stats().hits;
    const auto again=output(on,1,request,4096,128,{1,1,1}); equal(a,again);
    if (output_has_text(a.status)) {
      check(on.port->calls==count && on.shared.output_reuse_stats().hits==hits+1,"complete output did not reuse");
    } else check(on.shared.output_reuse_stats().hits==hits,"restriction cached as success");
  }
  on.mount(); output(on,1);output(on,2);
  check(on.shared.output_reuse_stats().resident==2,"same text merged different identities");
  PagedSharedResource invalid_config;
  std::string error;
  check(!invalid_config.open_verified(on.auth,9,{1,1024,true,{65,64,64}},&error),"unbounded output config accepted");
  equal(output(off,1,{},1),output(on,1,{},1));
  equal(output(off,1,{"x"},4096,128),output(on,1,{"x"},4096,128));
  equal(output(off,1,{"x"},4096,1),output(on,1,{"x"},4096,1));
  std::array<char,128> name{},text{};OutputReader wrong;
  check(wrong.begin(on.shared,on.shared.activation_handle(),1,session_key(2),{},name.data(),name.size(),text.data(),text.size())==OutputStatus::Pending,"wrong identity begin");
  check(finish_output(wrong).status==OutputStatus::Stale,"cache bypassed full-key mismatch");
  ReuseRuntime small(f,{1,3,3});
  equal(output(off,40),output(small,40));
  check(small.shared.output_reuse_stats().resident==0,"long UTF8 silently truncated into cache");
  ReuseRuntime evicted(f,{2,64,64});
  output(evicted,1);output(evicted,2);output(evicted,3);
  const auto hits=evicted.shared.output_reuse_stats().hits;
  equal(output(off,1),output(evicted,1));
  check(evicted.shared.output_reuse_stats().hits==hits && evicted.shared.output_reuse_stats().evictions>=2,"bounded LRU did not evict");
  std::cout<<"PASS OP1 disabled/hot/evicted, full identity, UTF8, generic forms, restrictions and exact capacity\n";
}
void lifecycle(const ReuseFixture& f) {
  ReuseRuntime r(f,{1,64,64});
  output(r,1);const auto old=r.shared.activation_handle();
  std::array<char,128> names{},bytes{};OutputReader reader;
  check(reader.begin(r.shared,old,1,session_key(1),{},names.data(),names.size(),bytes.data(),bytes.size())==OutputStatus::Pending,"hit begin");
  auto one=reader.advance({1,1,3});check(one.status==OutputStatus::Pending,"partial hit fixture");
  output(r,2); // Evicts the hit being copied; no borrowed cache pointer escapes.
  check(reader.advance(ordinary).status==OutputStatus::Stale,"replaced entry supplied partial output");
  SharedReadCursor fresh;
  check(r.shared.begin_read(SharedRecordKind::Lexeme,3,&fresh)==SharedReadStatus::InProgress,"external new read");
  reader.cancel();
  for (unsigned g=0;;++g) {
    check(g<100000,"new read stalled");const auto s=r.shared.advance_read(&fresh,ordinary);
    if (s.status==SharedReadStatus::InProgress) continue;
    check(s.status==SharedReadStatus::Ready && s.lexeme.identity.stable_key==session_key(3),"old cancel hurt new read");break;
  }
  r.mount();
  check(!r.shared.activation_is_current(old),"same owner/same epoch reopen resurrected activation");
  check(reader.begin(r.shared,old,1,session_key(1),{},names.data(),names.size(),bytes.data(),bytes.size())==OutputStatus::Stale,"old binding reused");
  check(r.shared.output_reuse_stats().resident==0,"reopen retained old cache");
  output(r,1);
  check(reader.begin(r.shared,r.shared.activation_handle(),1,session_key(1),{},
        names.data(),names.size(),bytes.data(),bytes.size())==OutputStatus::Pending,"pre-remount cache lookup");
  reader.advance({1,1,3});
  r.mount();
  check(reader.advance(ordinary).status==OutputStatus::Stale,"pending hit dereferenced destroyed pool");
  reader.cancel();
  output(r,1);r.port->live=false;
  check(output(r,1).status==OutputStatus::Stale,"cache hid unhealthy resource");
  r.mount();output(r,1);r.port->short_read=true;
  const auto failed=output(r,40);check(!output_has_text(failed.status),"short read published long output");
  check(output(r,1).status==OutputStatus::Stale,"authentication failure bypassed via cache");

  // Address reuse with a retained activation, without violating owner-outlives-
  // reader: readers are gone before owner destruction, only the handle survives.
  alignas(PagedSharedResource) std::array<std::byte,sizeof(PagedSharedResource)> storage{};
  r.mount();auto* first=new(storage.data()) PagedSharedResource;
  std::string error;check(first->open_verified(r.auth,9,{1,1024,true,{1,64,64}},&error),"placement open");
  const auto handle=first->activation_handle();first->~PagedSharedResource();
  auto* second=new(storage.data()) PagedSharedResource;
  check(second->open_verified(r.auth,9,{1,1024,true,{1,64,64}},&error),"placement reopen");
  check(!second->activation_is_current(handle) && second->output_reuse_stats().resident==0,"reused address cache identity");
  second->~PagedSharedResource();
  std::cout<<"PASS OP2 cancellation, cache-slot replacement, external ownership, same-epoch/address reopen and health\n";
}
void partial_publication(const ReuseFixture& f) {
  std::size_t cuts=0;
  for (unsigned cut=0;cut<18;++cut) {
    ReuseRuntime r(f,{1,64,64});std::array<char,128> names{},bytes{};OutputReader reader;
    check(reader.begin(r.shared,r.shared.activation_handle(),1,session_key(1),{"test:variant"},
          names.data(),names.size(),bytes.data(),bytes.size())==OutputStatus::Pending,"partial store begin");
    // Reach the first cache copy without relying on an internal phase number.
    for (unsigned g=0;r.shared.output_reuse_stats().stored_bytes==0;++g) {
      check(g<100000,"store not reached");const auto s=reader.advance({1,1024,1});
      check(s.status==OutputStatus::Pending,"cache prematurely published");
    }
    for (unsigned j=0;j<cut;++j) {
      auto s=reader.advance({1,1,1});
      if (s.status!=OutputStatus::Pending) break;
    }
    const auto completed=r.shared.output_reuse_stats().inserts;
    reader.cancel();check(reader.advance(ordinary).status==OutputStatus::Cancelled,"cancel resumed output");
    check(r.shared.output_reuse_stats().resident==completed,"partial/abandoned output marked valid");
    const auto before=r.shared.output_reuse_stats().hits;
    const auto again=output(r,1,{"test:variant"});
    check(again.status==OutputStatus::Exact && again.text=="V:𨿍","cancel restart text");
    check(r.shared.output_reuse_stats().hits==before+completed,"cancelled entry became a cache hit");++cuts;
  }
  std::cout<<"PASS OP3 "<<cuts<<" cache-publication cancellation cuts; no partial successful text\n";
}
void atomic_commit(const ReuseFixture& f) {
  ReuseRuntime r(f,{4,64,64});output(r,1);output(r,2);
  hanime::test::SessionSink sink;
  CompositionCommitter committer(r.shared,sink,{3,128,128,384});
  const std::array<CompositionMember,3> members{{{1,session_key(1),{0,2},{}},
      {2,session_key(2),{2,4},{"test:regional"}}, {1,session_key(1),{4,6},{}}}};
  auto prepare=[&](const auto& m) {
    auto result=committer.prepare(r.shared.activation_handle(),{1,1,{0,6}},m.data(),m.size());
    for (unsigned g=0;result.status==CompositionStatus::Preparing;++g) {
      check(g<100000,"commit prepare stalled");const auto s=committer.advance(result.transaction,{1,1024,17});
      check(s.cost.work_items<=17 && s.cost.read_calls<=1 && s.cost.read_bytes<=1024,"commit cache budget");result=s.result;
    }
    return result;
  };
  auto result=prepare(members);check(result.status==CompositionStatus::Ready && !sink.calls,"partial body before preparation");
  check(committer.confirm(result.transaction).status==CompositionStatus::Accepted,"atomic cache commit");
  check(committer.confirm(result.transaction).status==CompositionStatus::Accepted && sink.calls==1,"repeat sink");
  check(sink.accepted.front()=="同文𨿍R:地区同文𨿍","atomic full UTF8");
  auto invalid=members;invalid[1].expected_key=session_key(3);
  result=prepare(invalid);check(result.status==CompositionStatus::IdentityMismatch && sink.calls==1,"output cache replaced member identity check");
  invalid=members;invalid[1].output={"blocked"};
  result=prepare(invalid);check(result.status==CompositionStatus::OutputRestricted && sink.calls==1,"blocked commit bypass");
  result=prepare(members);sink.throw_unknown=true;
  check(committer.confirm(result.transaction).status==CompositionStatus::UnknownOutcome,"unknown sink outcome");
  check(committer.confirm(result.transaction).status==CompositionStatus::UnknownOutcome && sink.calls==2,"unknown replay");
  hanime::test::SessionSink retry_sink;retry_sink.accept=false;
  CompositionCommitter retry_owner(r.shared,retry_sink,{3,128,128,384});
  auto retry_result=retry_owner.prepare(r.shared.activation_handle(),{2,2,{0,6}},members.data(),members.size());
  auto retry_ready=[&](CompositionResult value) {
    for (unsigned g=0;value.status==CompositionStatus::Preparing;++g) {
      check(g<100000,"retry guard");value=retry_owner.advance(value.transaction,{1,17,3}).result;
    }
    check(value.status==CompositionStatus::Ready,"cache retry preparation");return value;
  };
  retry_result=retry_ready(retry_result);
  retry_result=retry_owner.confirm(retry_result.transaction);
  check(retry_result.status==CompositionStatus::RetryableNoEffect && retry_sink.accepted.empty(),"explicit reject wrote body");
  const auto permit=retry_result.retry;
  retry_result=retry_ready(retry_owner.retry(permit));retry_sink.accept=true;
  check(retry_owner.confirm(retry_result.transaction).status==CompositionStatus::Accepted && retry_sink.calls==2,
        "cache legal retry");
  check(retry_owner.retry(permit).status==CompositionStatus::Accepted && retry_sink.calls==2,"old retry rearmed sink");
  retry_owner.close();
  std::cout<<"PASS OP4 public atomic commit, same/different identities, revalidation, restricted output and Unknown; learning Disabled\n";
}
void session_selection_and_learning() {
  // The legacy single-word path still owns the only successful learning
  // notification. Cache hits cannot resurrect a replaced page/selection.
  hanime::test::SessionFixture h(9);
  std::string error;
  check(h.shared.open(h.lr,h.input.registry_namespace,h.built.manifest.shared_bundle_id,9,
                      {1,17,false,{4,64,64}},&error),error);
  for (;;) {
    const auto step=h.shared.advance_validation(h.shared.validation_cursor(),{1,17,3});
    if (step.status==ValidationStepStatus::Ready) break;
    check(step.status==ValidationStepStatus::InProgress,"session revalidation");
  }
  CandidateSession session(h.full,h.shared,h.counts,h.sink,{4,3,1,16,32,32});
  check(session.begin_text_access()==TextAccessStatus::Pending,"session access start");
  for (unsigned g=0;;++g) {
    check(g<100000,"session admission guard");const auto s=session.advance_text_access({1,17,3});
    if (s.status==TextAccessStatus::Ready) break;
    check(s.status==TextAccessStatus::Pending,"session admission");
  }
  auto drain=[&] {
    for (unsigned g=0;g<100000;++g) {
      const auto s=session.tick({1,17,3});
      check(s.cost.read_calls<=1 && s.cost.read_bytes<=17 && s.cost.work_items<=3,"session tiny budget");
      if (s.status!=SessionStatus::Pending && s.status!=SessionStatus::WaitingState) return s.status;
    }
    throw std::runtime_error("session drain guard");
  };
  auto publish=[&] {
    check(drain()==SessionStatus::StagingReady,"session range complete");
    check(session.publish(session.staging())==SessionStatus::Ready,"session publish");
  };
  check(session.begin_input("same",9,3,3)==SessionStatus::Pending,"session input");publish();
  const auto old=session.selection(0);
  check(session.request_range(RangeDirection::After,3,3)==SessionStatus::Pending,"session next");publish();
  check(session.confirm(old).status==CommitStatus::Stale && h.sink.calls==0,"old page resurrected");
  session.cancel_input(session.input_handle());drain();
  check(session.bind_scheme(h.xiaohe)==SessionStatus::Ready,"cross scheme idle");
  check(session.begin_input("sx",9,3,3)==SessionStatus::Pending,"new input");publish();
  check(session.confirm(old).status==CommitStatus::Stale,"old input selection resurrected");
  const auto current=session.selection(0);
  const auto hits=h.shared.output_reuse_stats().hits;
  check(session.confirm(current).status==CommitStatus::Preparing,"single confirm");
  for (unsigned g=0;session.commit_result().status==CommitStatus::Preparing;++g) {
    check(g<100000,"session commit guard");session.tick({1,17,3});
  }
  check(session.commit_result().status==CommitStatus::Accepted && h.sink.calls==1 && h.counts.notifications==1,
        "single learning contract");
  check(h.shared.output_reuse_stats().hits>hits,"single commit did not use output reuse");
  check(session.confirm(current).status==CommitStatus::Accepted && h.sink.calls==1 && h.counts.notifications==1,
        "single duplicate learned twice");
  std::cout<<"PASS OP6 cache-enabled old page/input rejection, cross-scheme access, single-word submit/learning once\n";
}
void anchors_and_bad_proofs() {
  const PacketFixture f(4097*512+19);
  auto raw=std::make_shared<PacketPort>(f.packets),fast=std::make_shared<PacketPort>(f.packets);
  VerifiedResourceReader ordinary_reader(nullptr,raw,f.publication.spec,{3,1,1024});
  VerifiedResourceReader fast_reader(nullptr,fast,f.publication.spec,{3,1,1024,4});
  std::array<std::uint8_t,19> a{},b{};
  for (const auto block:{0U,1U,15U,255U,256U,257U,511U,1024U,1025U,0U}) {
    const auto offset=std::size_t(block)*512+7;
    check(finish(ordinary_reader,*raw,offset,a.data(),a.size(),ticket()).status==ResourceReadStatus::Ready,"reference leaf");
    check(finish(fast_reader,*fast,offset,b.data(),b.size(),ticket(),{1,17,3}).status==ResourceReadStatus::Ready,"anchor leaf");
    check(a==b,"anchor changed authenticated bytes");
  }
  check(fast->requested<raw->requested && fast_reader.snapshot().anchor_hits>0,"anchor did not reduce cold proof bytes");
  auto wrong_pin=f.publication.spec;wrong_pin.merkle_root[31]^=1;
  auto wrong_port=std::make_shared<PacketPort>(f.packets);
  VerifiedResourceReader wrong(nullptr,wrong_port,wrong_pin,{3,1,1024,4});
  check(finish(wrong,*wrong_port,0,b.data(),b.size(),ticket()).status==ResourceReadStatus::Error,
        "wrong root gained anchor trust");
  for (unsigned fault=0;fault<3;++fault) {
    auto port=std::make_shared<PacketPort>(f.packets);
    VerifiedResourceReader reader(nullptr,port,f.publication.spec,{3,1,1024,4});
    check(finish(reader,*port,0,b.data(),b.size(),ticket()).status==ResourceReadStatus::Ready,"anchor warmup");
    const auto target=std::size_t{7};
    if (fault==0) port->bytes[target*f.layout.packet_bytes+1]^=1;
    if (fault==1) port->bytes[target*f.layout.packet_bytes+512+31]^=1;
    if (fault==2) port->short_read=true;
    b.fill(0xee);
    check(finish(reader,*port,target*512,b.data(),b.size(),ticket()).status==ResourceReadStatus::Error,"bad abbreviated proof accepted");
    check(std::all_of(b.begin(),b.end(),[](auto x){return x==0xee;}),"unverified bytes used");
  }
  // Upper proof is deliberately not requested on an anchor hit. This does NOT
  // claim the ignored transport suffix is checked. Cold full proof rejects it.
  auto port=std::make_shared<PacketPort>(f.packets);
  VerifiedResourceReader hot(nullptr,port,f.publication.spec,{3,1,1024,4});
  check(finish(hot,*port,0,b.data(),b.size(),ticket()).status==ResourceReadStatus::Ready,"anchor source");
  port->bytes[9*f.layout.packet_bytes+512+32*10]^=1;
  check(finish(hot,*port,9*512,b.data(),b.size(),ticket()).status==ResourceReadStatus::Ready,"unused proof suffix affected logical bytes");
  check(std::equal(b.begin(),b.end(),f.source.begin()+9*512),"anchor accepted wrong target bytes");
  VerifiedResourceReader cold(nullptr,port,f.publication.spec,{3,1,1024,4});
  check(finish(cold,*port,9*512,b.data(),b.size(),ticket()).status==ResourceReadStatus::Error,"new activation reused old anchor");
  for (unsigned cut=0;cut<64;++cut) {
    auto p=std::make_shared<PacketPort>(f.packets);
    VerifiedResourceReader reader(nullptr,p,f.publication.spec,{3,1,1024,4});
    const auto old=ticket(),fresh=ticket();
    for (unsigned j=0;j<cut;++j) step(reader,*p,512,b.data(),b.size(),old,{1,17,31});
    step(reader,*p,257*512,a.data(),a.size(),fresh,{1,17,31});
    reader.cancel_read_budgeted(old);
    check(finish(reader,*p,257*512,a.data(),a.size(),fresh).status==ResourceReadStatus::Ready,"anchor old cleanup hurt replacement");
    check(std::equal(a.begin(),a.end(),f.source.begin()+257*512),"replacement bytes");
  }
  std::cout<<"PASS OP5 cold proof reduction, collision/eviction, corruption/short read, ignored suffix distinction and 64 ownership cuts\n";
}
}
int main(int argc,char** argv) {
  try {
    check(argc==2,"expected test output directory");
    fs::create_directories(argv[1]);
    ReuseFixture fixture(fs::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    identity_forms_capacity(fixture);lifecycle(fixture);partial_publication(fixture);atomic_commit(fixture);
    anchors_and_bad_proofs();session_selection_and_learning();
    return 0;
  } catch (const std::exception& e) {std::cerr<<"FAIL output reuse: "<<e.what()<<'\n';return 1;}
}
