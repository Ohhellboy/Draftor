#include "hanime/exact_ranker.hpp"
#include "frozen_count_source.hpp"
#include "rank_allocation_probe.hpp"
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace hanime;
using hanime::test::FrozenCounts;
void check(bool ok,const std::string& msg) {if (!ok) throw std::runtime_error(msg);}
class Reader final : public ResourceReader {
 public:
  explicit Reader(std::vector<std::uint8_t> data):bytes(std::move(data)) {}
  std::size_t size() const override {return bytes.size();}
  bool read(std::size_t offset,void* dest,std::size_t n) const override {
    ++calls; requested+=n;
    if (offset>bytes.size() || n>bytes.size()-offset || fail) return false;
    std::memcpy(dest,bytes.data()+offset,n); return true;
  }
  std::vector<std::uint8_t> bytes;
  mutable std::size_t calls{0},requested{0}; bool fail{false};
};
void add(WorkCost& a,WorkCost b) {a.read_calls+=b.read_calls; a.read_bytes+=b.read_bytes; a.work_items+=b.work_items;}
ResourceBuildInput synthetic(std::size_t n,bool reverse=false) {
  ResourceBuildInput in; in.registry_namespace=0x52414e4b54455354ULL;
  in.source_name="synthetic ranking fixture (NOT real language data)";
  in.source_version="2026-09-08.rank1"; in.relation_basis="artificial identities";
  for (std::size_t i=0;i<n;++i) {
    LexemeSourceRecord l; l.status=RegistryStatus::Approved; l.stable_key.bytes[0]=0x75;
    for (unsigned j=0;j<8;++j) l.stable_key.bytes[31-j]=static_cast<std::uint8_t>((std::uint64_t(i)+1)>>(8*j));
    l.identity_definition="synthetic "+std::to_string(i); l.provenance="synthetic";
    l.base_weight=i+1==n ? 10000 : static_cast<std::int32_t>(i%4);
    l.default_output_utf8="同文"; // Deliberately identical text, distinct full keys.
    if (i==0) l.output_relations.push_back({"restricted","異文","context missing","test",false,
                                          ConditionKind::WordOrSense,2,OutputAdmission::Blocked});
    in.lexemes.push_back(l);
    for (const auto& scheme: {std::string("full_pinyin"),std::string("xiaohe")}) {
      for (int path=0;path<(i==0 ? 9 : 2);++path)
        in.mappings.push_back({scheme,scheme=="full_pinyin" ? "same" : "sx",l.stable_key,-7+path});
    }
  }
  if (reverse) std::reverse(in.lexemes.begin(),in.lexemes.end());
  return in;
}
BuiltResources build(const ResourceBuildInput& in) {
  ResourceBuildOptions o; o.scheme_page_bytes=96; o.shared_page_bytes=96;
  auto r=ResourceBuilder().build(in,o); check(r.ok,r.error); return std::move(r.resources);
}
struct Harness {
  ResourceBuildInput input; BuiltResources built;
  PagedSchemeIndex scheme; PagedSharedResource shared;
  FrozenCounts counts;
  std::shared_ptr<Reader> sr,lr;
  std::string scheme_name;
  explicit Harness(ResourceBuildInput in,std::string name="full_pinyin")
      :input(std::move(in)),built(build(input)),counts(input.registry_namespace),scheme_name(std::move(name)) {mount();}
  void mount() {
    sr=std::make_shared<Reader>(built.scheme_bundles.at(scheme_name));
    lr=std::make_shared<Reader>(built.shared_bundle);
    std::string error;
    check(shared.open(lr,input.registry_namespace,built.manifest.shared_bundle_id,9,{1,17},&error),error);
    validate_shared();
    check(scheme.open(sr,input.registry_namespace,built.manifest.shared_bundle_id,
                      static_cast<std::uint32_t>(input.lexemes.size()),{1,17},&error),error);
    for (;;) {auto s=scheme.advance_validation({1,17,3});
      if (s.status==ValidationStepStatus::Ready) break;
      check(s.status==ValidationStepStatus::InProgress,s.error);}
  }
  void validate_shared() {
    const auto t=shared.validation_cursor();
    for (;;) {auto s=shared.advance_validation(t,{1,17,3});
      if(s.status==ValidationStepStatus::Ready)break;
      check(s.status==ValidationStepStatus::InProgress,s.error);}
  }
  void reopen_shared() {
    std::string e;
    check(shared.open(lr,input.registry_namespace,built.manifest.shared_bundle_id,9,{1,17},&e),e); validate_shared();
  }
  RankStep tick(ExactRanker& rank,const RankTicket& t,WorkBudget b) {
    const auto calls=sr->calls+lr->calls,bytes=sr->requested+lr->requested;
    const auto begins=counts.begins,advances=counts.advances;
    RankStep s;
    {rank_probe::Capture capture; s=rank.advance(t,b);}
    check(s.cost.read_calls<=b.max_read_calls && s.cost.read_bytes<=b.max_read_bytes && s.cost.work_items<=b.max_work_items,"rank exceeded budget");
    check(s.cost.read_calls==sr->calls+lr->calls-calls && s.cost.read_bytes==sr->requested+lr->requested-bytes,"unmetered I/O");
    check(counts.begins-begins<=1 && counts.advances-advances<=1,"state busy loop");
    return s;
  }
  RankStatus finish(ExactRanker& rank,const RankTicket& t,WorkBudget b={1,17,3},WorkCost* total=nullptr) {
    for (std::size_t guard=0;guard<20'000'000;++guard) {
      auto s=tick(rank,t,b); if(total)add(*total,s.cost);
      if(s.status!=RankStatus::InProgress && s.status!=RankStatus::WaitingState)return s.status;
    }
    throw std::runtime_error("rank stalled");
  }
};

// Independent Host oracle: map all raw paths by ref, then full std::sort, then
// caller slices by integer positions. Never invokes runtime boundary/insertion.
std::vector<RankItem> reference(const Harness& h,RankRule rule={},bool base=false) {
  std::map<StableLexemeKey,std::size_t> refs;
  for(std::size_t i=0;i<h.input.lexemes.size();++i)refs[h.input.lexemes[i].stable_key]=i+1;
  std::map<std::size_t,RankItem> groups; std::uint32_t ordinal=0;
  for(const auto& m:h.input.mappings) {
    if(m.scheme_id!=h.scheme_name)continue;
    const auto ref=refs.at(m.stable_key); auto& g=groups[ref];
    if(!g.path_count || m.scheme_weight>g.scheme_weight) {g.scheme_weight=m.scheme_weight; g.representative_ordinal=ordinal;}
    ++g.path_count; ++ordinal; g.ref=static_cast<std::uint32_t>(ref); g.key=m.stable_key;
  }
  std::vector<RankItem> all;
  for(auto [ref,g]:groups) {
    auto count=h.counts.values.find(g.key);
    g.effective_count=base || count==h.counts.values.end() ? 0 : count->second;
    g.score=std::int64_t(h.input.lexemes[ref-1].base_weight)+std::int64_t(g.scheme_weight)+std::int64_t(g.effective_count)*rule.lambda;
    all.push_back(g);
  }
  std::sort(all.begin(),all.end(),[](const auto& a,const auto& b){
    if(a.score!=b.score)return a.score>b.score;
    return a.key.bytes<b.key.bytes;
  });
  return all;
}
void equal(const RankItem& a,const RankItem& b) {
  check(a.key==b.key && a.score==b.score && a.ref==b.ref && a.scheme_weight==b.scheme_weight &&
      a.effective_count==b.effective_count && a.path_count==b.path_count && a.representative_ordinal==b.representative_ordinal,"rank differs from independent oracle");
}
struct Run {
  Harness& h; std::vector<RankItem> work,out; ExactRanker rank; RankTicket t;
  Run(Harness& host,std::size_t w,std::size_t b=3):h(host),work(w),out(w),rank(h.scheme,h.shared,h.counts,work.data(),w,b) {}
  void begin(ScoringMode mode=ScoringMode::Exact,RankRule rule={}) {
    RankStatus status;
    {rank_probe::Capture capture;
      status=rank.begin_input(h.scheme_name=="full_pinyin" ? "same" : "sx",9,rule,mode);}
    check(status==RankStatus::InProgress,"begin input");
  }
  RankStatus page(RangeDirection d=RangeDirection::First,const RankBoundary* b=nullptr,std::size_t q=0,WorkBudget budget={1,17,3},WorkCost* total=nullptr) {
    RankStatus status;
    {rank_probe::Capture capture; status=rank.request_range(d,b,q ? q : work.size(),out.data(),out.size(),&t);}
    check(status==RankStatus::InProgress,"request range");
    return h.finish(rank,t,budget,total);
  }
};
void all_pages(Run& r,const std::vector<RankItem>& expected,WorkBudget budget={1,17,3},WorkCost* total=nullptr) {
  RankBoundary previous; std::size_t offset=0;
  while(offset<expected.size()) {
    check(r.page(offset ? RangeDirection::After : RangeDirection::First,offset ? &previous : nullptr,0,budget,total)==RankStatus::Ready,"not ready");
    auto v=r.rank.result(r.t); const auto n=std::min(r.work.size(),expected.size()-offset);
    check(v.size==n && v.has_more==(offset+n<expected.size()),"incorrect page size/exhaustion");
    for(std::size_t i=0;i<n;++i)equal(v.items[i],expected[offset+i]);
    offset+=n; previous=r.rank.boundary(r.t,n-1); r.rank.release(r.t);
  }
  check(r.page(RangeDirection::After,&previous,0,budget,total)==RankStatus::RangeExhausted,"tail not exhausted");
  r.rank.release(r.t);
}

void full_range() {
  for (auto w:{1U,3U,16U}) for(const auto& scheme:{std::string("full_pinyin"),std::string("xiaohe")}) {
    Harness h(synthetic(40),scheme); Run r(h,w,1); r.begin();
    auto expected=reference(h); check(expected.front().ref==40,"highest must scan last");
    all_pages(r,expected,w==1 ? WorkBudget{1,1,1} : WorkBudget{1,17,3});
  }
}
void boundaries_and_requests() {
  Harness h(synthetic(19)); Run r(h,3); r.begin(); auto expected=reference(h);
  check(r.page()==RankStatus::Ready,"first");
  auto end=r.rank.boundary(r.t,2); auto old=r.t; const auto before=h.scheme.current_query_revision();
  RankTicket rejected;
  check(r.rank.request_range(RangeDirection::First,nullptr,4,r.out.data(),3,&rejected)==RankStatus::CapacityExceeded,"Q over cap");
  check(r.rank.request_range(RangeDirection::First,nullptr,0,r.out.data(),3,&rejected)==RankStatus::InvalidRequest,"Q zero");
  check(r.rank.request_range(RangeDirection::First,nullptr,1,r.work.data(),3,&rejected)==RankStatus::InvalidRequest,"overlap");
  check(r.rank.result(old).size==3 && before==h.scheme.current_query_revision(),"invalid request harmed ready work");
  check(r.rank.advance(old,{0,1,1}).status==RankStatus::InvalidBudget,"invalid budget");
  r.rank.release(r.t); check(r.page(RangeDirection::After,&end)==RankStatus::Ready,"next");
  auto first=r.rank.boundary(r.t,0); r.rank.release(r.t);
  check(r.page(RangeDirection::Before,&first,2)==RankStatus::Ready,"before");
  auto v=r.rank.result(r.t); equal(v.items[0],expected[1]); equal(v.items[1],expected[2]);
  r.rank.release(r.t); check(r.page()==RankStatus::Ready,"first again");
  first=r.rank.boundary(r.t,0); r.rank.release(r.t);
  check(r.page(RangeDirection::Before,&first)==RankStatus::RangeExhausted,"head exhausted"); r.rank.release(r.t);
  check(r.rank.begin_input("absent",9)==RankStatus::InProgress,"absent begin");
  check(r.page()==RankStatus::NoMatches,"missing != range exhaustion");
}
void identities_and_numbers() {
  RankRule rule{7,65535}, parsed;
  check(!RankRule::make(1,65536,&parsed) && !RankRule::make(0,1,&parsed) && RankRule::make(7,65535,&parsed),"rule range");
  for (bool reverse:{false,true}) for(auto lambda:{0U,65535U}) {
    auto in=synthetic(20,reverse);
    for (std::size_t i=0;i<in.lexemes.size();++i)in.lexemes[i].base_weight=i==0 ? INT32_MIN : INT32_MAX;
    for(auto& m:in.mappings)m.scheme_weight=m.stable_key==in.lexemes[0].stable_key ? INT32_MIN : INT32_MAX;
    Harness h(std::move(in));
    for(std::size_t i=1;i<h.input.lexemes.size();++i)h.counts.values[h.input.lexemes[i].stable_key]=UINT32_MAX;
    rule.lambda=static_cast<std::uint16_t>(lambda); Run r(h,3); r.begin(ScoringMode::Exact,rule);
    auto expected=reference(h,rule); all_pages(r,expected);
    check(expected.back().score==-4294967296LL,"signed lower extreme");
    if(lambda)check(expected.front().score==281474976645119LL,"upper extreme");
  }
  Harness a(synthetic(23)),b(synthetic(23,true)); Run ra(a,16),rb(b,16); ra.begin(); rb.begin();
  auto x=reference(a),y=reference(b);
  for(std::size_t i=0;i<x.size();++i)check(x[i].key==y[i].key && x[i].score==y[i].score,"oracle reorder");
  all_pages(ra,x); all_pages(rb,y);
}

std::uint32_t u32(const std::vector<std::uint8_t>& b,std::size_t o) {
  return std::uint32_t(b.at(o)) | (std::uint32_t(b.at(o+1))<<8) | (std::uint32_t(b.at(o+2))<<16) | (std::uint32_t(b.at(o+3))<<24);
}
void put32(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v) {for(unsigned i=0;i<4;++i)b.at(o+i)=static_cast<std::uint8_t>(v>>(8*i));}
std::vector<std::size_t> leaf_refs(std::vector<std::uint8_t>& b) {
  const auto page=u32(b,44),leaves=u32(b,64); const auto pages=40+u32(b,72);
  std::vector<std::size_t> refs;
  for(std::size_t i=0;i<leaves;++i) {
    const auto start=pages+i*page; const auto n=b.at(start+6)+(unsigned(b.at(start+7))<<8);
    auto o=start+16;
    for(unsigned j=0;j<n;++j) {const auto len=b.at(o)+(unsigned(b.at(o+1))<<8); o+=2+len; refs.push_back(o); o+=12;}
  }
  return refs;
}
void hash_scheme(std::vector<std::uint8_t>& b) {
  std::uint64_t h=14695981039346656037ULL;
  for(std::size_t i=40;i<b.size();++i)h=(h^b[i])*1099511628211ULL;
  for(unsigned i=0;i<8;++i)b.at(24+i)=static_cast<std::uint8_t>(h>>(8*i));
}
void defensive_groups() {
  // Artificial checksum-consistent wire edits, NOT natural builder ordering.
  for(bool bad_order:{false,true}) {
    Harness h(synthetic(12)); auto& wire=h.built.scheme_bundles.at("full_pinyin"); auto refs=leaf_refs(wire);
    if(bad_order) {put32(wire,refs[0],2); put32(wire,refs[1],1);}
    else {put32(wire,refs[0]+4,static_cast<std::uint32_t>(-20)); put32(wire,refs[8]+4,77);}
    hash_scheme(wire);
    if (bad_order) {
      // API0.9 final-byte publication now rejects this earlier. Preserve the
      // ranker's independent defensive assertion below by deliberately
      // violating the immutable-reader contract AFTER a valid test mount.
      PagedSchemeIndex invalid; std::string error;
      check(invalid.open(std::make_shared<Reader>(wire),h.input.registry_namespace,
          h.built.manifest.shared_bundle_id,12,{1,17},&error),error);
      ValidationStepResult v;
      do {v=invalid.advance_validation({1,17,3});} while(v.status==ValidationStepStatus::InProgress);
      check(v.status==ValidationStepStatus::Error && !invalid.ready(),"decreasing ref passed final-byte validation");
      h.sr->bytes=wire;
    } else h.mount();
    Run r(h,3,1); r.begin();
    if(bad_order) {check(r.page()==RankStatus::GroupOrderError,"decreasing ref admitted"); check(!r.rank.result(r.t).items,"partial result published");}
    else {
      // Set independent logical raw input to the same two changed paths.
      const auto ordinal0=u32(wire,refs[0]+8),ordinal8=u32(wire,refs[8]+8);
      std::uint32_t ordinal=0;
      for(auto& m:h.input.mappings)if(m.scheme_id=="full_pinyin") {
        if(ordinal==ordinal0)m.scheme_weight=-20;
        if(ordinal==ordinal8)m.scheme_weight=77;
        ++ordinal;
      }
      all_pages(r,reference(h),{1,1,1});
    }
  }
}

void state_consistency() {
  Harness h(synthetic(18)); Run r(h,3); h.counts.delay=2;
  h.counts.values[h.input.lexemes[0].stable_key]=200;
  r.begin(); check(r.rank.request_range(RangeDirection::First,nullptr,3,r.out.data(),3,&r.t)==RankStatus::InProgress,"start");
  std::size_t waiting=0;
  for (;;) {auto s=h.tick(r.rank,r.t,{1,17,3});
    if(s.status==RankStatus::WaitingState){++waiting;check(!r.rank.result(r.t).items,"pending published");}
    else if(s.status==RankStatus::Ready)break;
    else check(s.status==RankStatus::InProgress,"state phase");}
  check(waiting==36,"Pending not preserved per key");
  auto expected=reference(h); for(std::size_t i=0;i<3;++i)equal(r.out[i],expected[i]);
  const auto anchor=r.rank.boundary(r.t,2); h.counts.same_value_flush(); check(r.rank.boundary_is_current(anchor),"flush invalidates");
  r.rank.release(r.t); h.counts.set(h.input.lexemes[1].stable_key,900);
  check(!r.rank.boundary_is_current(anchor),"count version accepted old boundary");
  check(r.rank.request_range(RangeDirection::After,&anchor,3,r.out.data(),3,&r.t)==RankStatus::Stale,"old view reuse");
  check(r.rank.restart_rank(ScoringMode::Exact)==RankStatus::InProgress,"new exact view");
  all_pages(r,reference(h));
  // A delayed read view replacement is also stale, not zero/NotFound.
  r.begin(); check(r.rank.request_range(RangeDirection::First,nullptr,3,r.out.data(),3,&r.t)==RankStatus::InProgress,"delayed start");
  while(h.tick(r.rank,r.t,{1,17,3}).status!=RankStatus::WaitingState) {}
  h.counts.set(h.input.lexemes[2].stable_key,4);
  check(h.tick(r.rank,r.t,{1,17,3}).status==RankStatus::Stale,"pending view changed");
}
void base_only_latch() {
  Harness h(synthetic(21)); Run r(h,3); h.counts.values[h.input.lexemes[0].stable_key]=500;
  r.begin(); check(r.page()==RankStatus::Ready,"exact first"); auto exact_anchor=r.rank.boundary(r.t,2); r.rank.release(r.t);
  h.counts.fail_on_begin=h.counts.begins+4;
  check(r.page(RangeDirection::After,&exact_anchor)==RankStatus::StateUnavailable,"mid-range fault");
  check(!r.rank.result(r.t).items && !r.rank.boundary_is_current(exact_anchor),"mixed/old score exposed");
  const auto late=h.counts.active;
  check(r.rank.restart_rank(ScoringMode::BaseOnly)==RankStatus::InProgress,"base restart");
  check(r.rank.request_range(RangeDirection::After,&exact_anchor,3,r.out.data(),3,&r.t)==RankStatus::Stale,"exact anchor enters base");
  check(r.page()==RankStatus::Ready,"base first"); auto first=r.rank.boundary(r.t,0),last=r.rank.boundary(r.t,2);
  check(r.rank.result(r.t).reason==BaseOnlyReason::StateUnavailable,"missing degradation reason");
  auto base=reference(h,{},true);
  for(std::size_t i=0;i<3;++i)equal(r.out[i],base[i]);
  r.rank.release(r.t);
  const auto calls=h.counts.begins+h.counts.advances+h.counts.view_calls+h.counts.view_checks;
  h.counts.recover(); h.counts.set(h.input.lexemes[1].stable_key,1000); h.counts.reopen();
  check(r.rank.boundary_is_current(first) && r.rank.boundary_is_current(last),"recovery invalidates base");
  check(r.page(RangeDirection::After,&last)==RankStatus::Ready,"base after");
  for(std::size_t i=0;i<3;++i)equal(r.out[i],base[3+i]);
  auto second=r.rank.boundary(r.t,0); r.rank.release(r.t);
  check(r.page(RangeDirection::Before,&second)==RankStatus::Ready,"base before");
  for(std::size_t i=0;i<3;++i)equal(r.out[i],base[i]);
  r.rank.release(r.t);
  check(r.rank.restart_rank(ScoringMode::Exact)==RankStatus::InProgress && r.rank.mode()==ScoringMode::BaseOnly,"restart escaped input latch");
  all_pages(r,base);
  check(calls==h.counts.begins+h.counts.advances+h.counts.view_calls+h.counts.view_checks,"base polled CountSource");
  r.begin(); check(r.rank.mode()==ScoringMode::Exact,"next same-code input did not recover");
  all_pages(r,reference(h));
  // Fault ticket cannot consume/mutate a newer exact read; deliberately return
  // that old ticket once to test the ranker's own late-result guard too.
  h.counts.delay=1; r.begin(); check(r.rank.request_range(RangeDirection::First,nullptr,3,r.out.data(),3,&r.t)==RankStatus::InProgress,"late ticket start");
  while(h.tick(r.rank,r.t,{1,17,3}).status!=RankStatus::WaitingState) {}
  const auto active=h.counts.active;
  check(h.counts.advance_count(late,{1,1,1}).status==CountStatus::Stale,"source accepted foreign ticket");
  check(h.counts.active.revision==active.revision,"old ticket changed current read");
  h.counts.old=late; h.counts.inject_old=true;
  check(h.tick(r.rank,r.t,{1,17,3}).status==RankStatus::Stale,"late exact count accepted");
  // Explicit Pending -> BaseOnly is input-latched too; no timeout policy.
  check(r.rank.restart_rank(ScoringMode::BaseOnly)==RankStatus::InProgress,"explicit base start");
  check(r.page()==RankStatus::Ready && r.rank.result(r.t).reason==BaseOnlyReason::HostSelected,"host reason");
  r.rank.release(r.t);
  check(r.rank.restart_rank(ScoringMode::Exact)==RankStatus::InProgress && r.rank.mode()==ScoringMode::BaseOnly,"explicit latch escaped");
}

void lifecycle() {
  for (unsigned which=0;which<6;++which) {
    Harness h(synthetic(18)); Run r(h,3); r.begin();
    check(r.rank.request_range(RangeDirection::First,nullptr,3,r.out.data(),3,&r.t)==RankStatus::InProgress,"life start");
    if(which==0) h.reopen_shared();
    else if(which==4 || which==5) {
      // Suspend during the first shared metadata read, not just before scan.
      const auto baseline=h.lr->calls;
      while(h.lr->calls==baseline)check(h.tick(r.rank,r.t,{1,1,1}).status==RankStatus::InProgress,"reach shared read");
      if(which==5) {h.shared.~PagedSharedResource(); new (&h.shared) PagedSharedResource();}
      h.reopen_shared();
    }
    else if(which==1) {ExactQueryCursor foreign; std::string e;check(h.scheme.begin_exact_query("same",9,&foreign,&e),e);}
    else if(which==2) {
      check(h.finish(r.rank,r.t)==RankStatus::Ready,"consumer ready");
      auto old=r.t; r.rank.release(old); r.begin();
      check(r.rank.request_range(RangeDirection::First,nullptr,3,r.out.data(),3,&r.t)==RankStatus::InProgress,"callback created new input");
      r.rank.cancel(old); check(h.tick(r.rank,old,{1,17,3}).status==RankStatus::Stale,"old callback ticket");
      check(h.finish(r.rank,r.t)==RankStatus::Ready,"callback new work damaged"); continue;
    } else {
      auto old=r.t; r.rank.cancel(old); check(r.rank.advance(old,{1,1,1}).status==RankStatus::Cancelled,"cancel current");
      r.begin(); check(r.page()==RankStatus::Ready,"new after cancel"); r.rank.cancel(old);
      check(r.rank.result(r.t).size==3,"old cancel newwork"); continue;
    }
    SharedReadCursor fresh; check(h.shared.begin_read(SharedRecordKind::Lexeme,2,&fresh)==SharedReadStatus::InProgress,"fresh read");
    const auto revision=h.scheme.current_query_revision();
    const auto calls=h.sr->calls+h.lr->calls;
    check(h.tick(r.rank,r.t,{1,1,1}).status==RankStatus::Stale,"old rank not stale"); r.rank.cancel(r.t);
    check(revision==h.scheme.current_query_revision() && calls==h.sr->calls+h.lr->calls,"stale work mutates/reads owner");
    for (;;) {auto s=h.shared.advance_read(&fresh,{1,17,3}); if(s.status==SharedReadStatus::Ready)break;
      check(s.status==SharedReadStatus::InProgress,"new shared read damaged");}
  }
  Harness h(synthetic(12)); ExactQueryCursor a,b; std::string e;
  check(h.scheme.begin_exact_query("same",9,&a,&e),e); auto context=h.scheme.query_context(a);
  check(h.scheme.restart_exact_query(context,&a,&e),e);
  auto latest=h.scheme.query_context(a); check(!h.scheme.query_is_current(context),"old raw context current");
  check(!h.scheme.restart_exact_query(latest,&b,&e) && h.scheme.query_is_current(latest),"foreign cursor rescan mutated owner");
  check(h.scheme.begin_exact_query("same",9,&b,&e),e); auto newer=h.scheme.query_context(b);
  check(!h.scheme.restart_exact_query(latest,&a,&e) && h.scheme.query_is_current(newer),"external query adopted as rescan");
  std::array<RankItem,3> work{},out{};
  auto dying=std::make_unique<ExactRanker>(h.scheme,h.shared,h.counts,work.data(),3,1);
  check(dying->begin_input("same",9)==RankStatus::InProgress,"destructor fixture");
  RankTicket old_ticket;
  check(dying->request_range(RangeDirection::First,nullptr,3,out.data(),3,&old_ticket)==RankStatus::InProgress,"destructor start");
  h.reopen_shared(); SharedReadCursor fresh;
  check(h.shared.begin_read(SharedRecordKind::Lexeme,1,&fresh)==SharedReadStatus::InProgress,"destructor new read");
  dying.reset();
  for (;;) {auto s=h.shared.advance_read(&fresh,{1,17,3}); if(s.status==SharedReadStatus::Ready)break;
    check(s.status==SharedReadStatus::InProgress,"old destructor damaged new read");}
}

void costs_and_memory() {
  std::size_t fixed=0,peak=0;
  for (auto n:{32U,320U}) {
    Harness h(synthetic(n));
    rank_probe::start();
    std::unique_ptr<Run> holder;
    {rank_probe::Capture capture; holder=std::make_unique<Run>(h,16,3);}
    auto& r=*holder; r.begin(); WorkCost first,total,back;
    check(r.page(RangeDirection::First,nullptr,0,{1,17,3},&first)==RankStatus::Ready,"cost first");
    const auto mem=r.rank.memory_usage();
    if(fixed)check(fixed==mem.impl_bytes,"N-dependent rank object");
    fixed=mem.impl_bytes;
    check(mem.workspace_bytes==16*sizeof(RankItem) && mem.result_bytes==16*sizeof(RankItem) && mem.batch_capacity_bytes<=3*sizeof(RawQueryHit),"rank capacity");
    auto anchor=r.rank.boundary(r.t,r.rank.result(r.t).size-1); r.rank.release(r.t);
    total=first; std::size_t ranges=1; RankBoundary deep_first; WorkCost deep;
    while(true) {
      WorkCost cost; const auto status=r.page(RangeDirection::After,&anchor,0,{1,17,3},&cost); add(total,cost); ++ranges;
      if(status==RankStatus::RangeExhausted){r.rank.release(r.t);break;}
      check(status==RankStatus::Ready,"cost subsequent");
      deep_first=r.rank.boundary(r.t,0); deep=cost;
      anchor=r.rank.boundary(r.t,r.rank.result(r.t).size-1); r.rank.release(r.t);
    }
    check(r.page(RangeDirection::Before,&deep_first,0,{1,17,3},&back)==RankStatus::Ready,"cost back");
    const auto allocation=rank_probe::stop();
    if(allocation.supported) {
      if(peak)check(peak==allocation.peak,"measured rank-operation heap peak grew with N");
      peak=allocation.peak;
    }
    std::cout<<"SYNTHETIC N="<<n<<" W=16 B=3 page=96 cache=1/1 raw="<<h.scheme.info().mapping_count
      <<" scheme_bytes="<<h.sr->size()<<" shared_bytes="<<h.lr->size()<<" depth="<<h.scheme.info().tree_depth<<" ranges_including_exhaustion="<<ranges<<'\n';
    for(const auto& p: {std::make_pair("first",first),std::make_pair("deep",deep),std::make_pair("all_forward",total),std::make_pair("before",back)})
      std::cout<<p.first<<" calls="<<p.second.read_calls<<" bytes="<<p.second.read_bytes<<" work="<<p.second.work_items<<'\n';
    std::cout<<"MEM owner="<<mem.owner_bytes<<" impl="<<mem.impl_bytes<<" lifetime="<<mem.lifetime_bytes
      <<" workspace="<<mem.workspace_bytes<<" output="<<mem.result_bytes<<" batch_capacity="<<mem.batch_capacity_bytes
      <<" group(included)="<<mem.group_bytes<<" pending(included)="<<mem.pending_item_bytes
      <<" shared_cursor(included)="<<mem.shared_cursor_bytes<<" shared_record(included)="<<mem.shared_record_bytes
      <<" count_ticket(included)="<<mem.count_ticket_bytes<<" boundary="<<mem.boundary_bytes
      <<" RankItem="<<sizeof(RankItem)<<" RankTicket="<<sizeof(RankTicket)<<" RawHit="<<sizeof(RawQueryHit)<<'\n';
    const auto shared=h.shared.memory_usage();
    std::cout<<"RAW_CURSOR impl="<<mem.raw.impl_bytes<<" code_capacity="<<mem.raw.code_capacity_bytes
      <<" code_heap="<<mem.raw.code_heap_bytes<<" page_capacity="<<mem.raw.page_capacity_bytes
      <<" scheme_resident_pages="<<h.scheme.resident_page_bytes()
      <<" SHARED owner="<<shared.owner_fixed_bytes<<" cache_payload="<<shared.cache_payload_allocated_bytes
      <<" cache_metadata="<<shared.cache_metadata_allocated_bytes
      <<" TRANSIENT shared_result="<<sizeof(SharedReadResult)<<" raw_result="<<sizeof(ExactQueryStepResult)
      <<" count_result="<<sizeof(CountResult)<<'\n';
    std::cout<<"ALLOC_PROBE supported="<<allocation.supported<<" live_requested="<<allocation.current
      <<" peak_requested="<<allocation.peak<<" allocation_calls="<<allocation.allocations
      <<" test_Run_wrapper(included)="<<sizeof(Run)<<" (rank calls + buffers only; preexisting owners/readers/state excluded)\n";
  }
}
}
int main() {
  unsigned passed=0;
  const std::pair<const char*,std::function<void()>> tests[]={
    {"K1 full range W=1/3/16, both schemes, minimum budget",full_range},
    {"K2 Before/After and invalid request isolation",boundaries_and_requests},
    {"K3 full-key identities and numeric extremes",identities_and_numbers},
    {"K4 artificial later maximum and decreasing ref",defensive_groups},
    {"K5 Pending/NotFound/view/flush consistency",state_consistency},
    {"K6 fault to input-latched BaseOnly and next-input recovery",base_only_latch},
    {"K7 resource/query/ticket isolation and conditional rescan",lifecycle},
    {"K8 synthetic N/10N costs and memory",costs_and_memory}
  };
  for(const auto& t:tests)try {t.second(); ++passed; std::cout<<"PASS "<<t.first<<'\n';}
    catch(const std::exception& e){std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<'\n';}
  std::cout<<passed<<"/8 ranking groups passed\n";
  return passed==8 ? 0 : 1;
}
