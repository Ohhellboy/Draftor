// Reuse existing final-byte publication fixture, not a second framework.
#define main existing_publisher_test_main
#include "test_resource_publisher.cpp"
#undef main
#include "hanime/base_order.hpp"
#include "hanime/member_eligibility.hpp"
#include "frozen_count_source.hpp"
#include "../src/base_order_build.hpp"
#include "../src/base_order_wire.hpp"
#include <new>

// Only the bounded nothrow directory-pool allocation is faulted. Ordinary
// fixture/cursor allocations remain live, including under ASan.
bool fail_locate_allocation=false;
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {
  if(fail_locate_allocation)return nullptr;
  try{return ::operator new[](n);}catch(...){return nullptr;}
}

namespace {
struct OrderedFixture {
  Fixture fixture;
  PublishVerifiedOptions options;
  PublishVerifiedResult publication;
  std::shared_ptr<Memory> data[3],proof[3];
  std::shared_ptr<VerifiedResourceReader> readers[3];
  PagedSharedResource shared;
  PagedSchemeIndex scheme;
  std::shared_ptr<BaseOrderIndex> order;
  test::FrozenCounts counts{fixture::kRegistryNamespace};
  WorkBudget budget{1,17,3};
  explicit OrderedFixture(const fs::path& root,bool reverse=false,bool extra_codes=false):fixture(root,reverse) {
    if(extra_codes) {
      auto input=fixture::make_small_input(reverse);
      for(unsigned repeat=0;repeat<8;++repeat)for(const auto& lexeme:input.lexemes)
        if(lexeme.status==RegistryStatus::Approved)input.mappings.push_back({"full_pinyin","same",lexeme.stable_key,static_cast<int32_t>(repeat)});
      for(unsigned i=0;i<10;++i)input.mappings.push_back({"full_pinyin",std::string("zextra")+char('a'+i),fixture::kPrototype,1});
      auto built=ResourceBuilder().build(input,{96,96});check(built.ok,built.error);fixture.built=std::move(built.resources);
    }
    options=fixture.options(fixture.built.shared_bundle,fixture.built.scheme_bundles.at("full_pinyin"));
    options.base_order=true;publication=publish_verified_resources(options);check(publication.ok,publication.error);
    for (unsigned i=0;i<3;++i) {
      const auto& r=publication.resources[i];
      data[i]=std::make_shared<Memory>(i==0 ? fixture.built.shared_bundle : i==1 ? fixture.built.scheme_bundles.at("full_pinyin") : read(fs::path(options.output_directory)/r.source_file));
      proof[i]=std::make_shared<Memory>(read(fs::path(options.output_directory)/r.index_file));
      readers[i]=std::make_shared<VerifiedResourceReader>(data[i],proof[i],r.spec,VerifiedResourceConfig{3,1,17});
      check(readers[i]->prepare_bootstrap(),"ordered bootstrap begin");
      for (unsigned n=0;!readers[i]->bootstrap_ready() && n<100000;++n) {
        auto s=readers[i]->advance_bootstrap(budget);check(s.status!=ResourceReadStatus::Error,"ordered bootstrap");
      }
      check(readers[i]->bootstrap_ready(),"bootstrap stalled");
    }
    std::string error;check(shared.open_verified(readers[0],9,{1,17,true},&error),error);
    check(scheme.open_verified(readers[1],shared,{1,17},&error),error);
    order=std::make_shared<BaseOrderIndex>(readers[2],scheme,shared);
    check(order->ready()&&scheme.attach_base_order(order),"attach derived");
  }
};
RankStatus finish_order(ExactRanker& r,const RankTicket& t,WorkBudget b) {
  for (unsigned i=0;i<2000000;++i) {
    auto s=r.advance(t,b);check(s.cost.read_calls<=b.max_read_calls&&s.cost.read_bytes<=b.max_read_bytes&&s.cost.work_items<=b.max_work_items,"order exceeded budget");
    if (s.status!=RankStatus::InProgress&&s.status!=RankStatus::WaitingState) return s.status;
  }
  throw std::runtime_error("order stalled");
}
std::vector<RankItem> all_order(OrderedFixture& h,bool ordered,ScoringMode mode=ScoringMode::BaseOnly) {
  check(h.scheme.attach_base_order(ordered?h.order:nullptr),"toggle derived");
  std::array<RankItem,2> work{},out{};
  ExactRanker rank(h.scheme,h.shared,h.counts,work.data(),2,1);
  check(rank.begin_input("same",9,{},mode)==RankStatus::InProgress,"rank begin");
  check(rank.uses_base_order()==(ordered&&mode==ScoringMode::BaseOnly),"wrong mode specialization");
  std::vector<RankItem> found;RankBoundary boundary;RangeDirection direction=RangeDirection::First;
  for (unsigned page=0;page<20;++page) {
    RankTicket t;check(rank.request_range(direction,direction==RangeDirection::First?nullptr:&boundary,2,out.data(),2,&t)==RankStatus::InProgress,"range");
    check(finish_order(rank,t,h.budget)==RankStatus::Ready,"ready");
    auto v=rank.result(t);found.insert(found.end(),v.items,v.items+v.size);boundary=rank.boundary(t,v.size-1);
    const auto more=v.has_more;rank.release(t);if(!more)break;direction=RangeDirection::After;
  }
  if(found.size()>2) {
    RankTicket t;check(rank.request_range(RangeDirection::Before,&boundary,2,out.data(),2,&t)==RankStatus::InProgress,"before");
    check(finish_order(rank,t,h.budget)==RankStatus::Ready,"before ready");
    auto v=rank.result(t);check(v.size==2 && v.items[0].key==found[found.size()-3].key && v.items[1].key==found[found.size()-2].key,"before strict ordinal");
  }
  return found;
}
void same_items(const std::vector<RankItem>& a,const std::vector<RankItem>& b) {
  check(a.size()==b.size(),"group coverage");
  for (std::size_t i=0;i<a.size();++i)
    check(a[i].key==b[i].key&&a[i].ref==b[i].ref&&a[i].score==b[i].score&&a[i].path_count==b[i].path_count&&a[i].representative_ordinal==b[i].representative_ordinal,"full item differs");
}
void ordered_regressions(const fs::path& dir) {
  OrderedFixture h(dir);
  auto expected=all_order(h,false),got=all_order(h,true);same_items(expected,got);
  check(got.size()==7,"candidate cap became coverage");
  auto input=fixture::make_small_input(false);std::vector<RankItem> oracle;
  RuntimeLexemeRef ref=0;
  for(const auto& lexeme:input.lexemes)if(lexeme.status==RegistryStatus::Approved) {
    RankItem item;item.key=lexeme.stable_key;item.ref=++ref;item.scheme_weight=7;
    item.score=std::int64_t(lexeme.base_weight)+7;item.path_count=8;oracle.push_back(item);
  }
  std::sort(oracle.begin(),oracle.end(),[](const auto& a,const auto& b){return a.score!=b.score?a.score>b.score:a.key<b.key;});
  for(std::size_t i=0;i<got.size();++i)check(got[i].key==oracle[i].key&&got[i].ref==oracle[i].ref&&got[i].score==oracle[i].score&&got[i].path_count==8,"independent input oracle differs");
  const auto calls=h.counts.view_calls+h.counts.begins+h.counts.advances;check(!calls,"BaseOnly consulted counts");
  h.counts.set(expected.back().key,100000);auto learned=all_order(h,true,ScoringMode::Exact);
  check(learned.front().key==expected.back().key,"Exact silently used BaseOnly order");
  {
    OrderedFixture reversed(dir.string()+"_reordered",true);const auto other=all_order(reversed,true);
    check(other.size()==got.size(),"reordered coverage");
    for(std::size_t i=0;i<got.size();++i)
      check(other[i].key==got[i].key&&other[i].score==got[i].score,"physical ref changed rank order");
    check(other.front().ref!=got.front().ref,"reorder test did not reorder refs");
  }
  std::cout<<"PASS O1 bounded W=2 forward/Before/full grouping and Exact fallback\n";

  // Final-byte semantic variants have a valid length; byte authentication is
  // NOT the oracle here. No trusted pin is issued for these forged candidates.
  for (unsigned mutation=0;mutation<6;++mutation) {
    auto bytes=h.data[2]->bytes;detail::BaseHeader hdr;
    check(detail::base_header(bytes.data(),bytes.size(),hdr),"test header");
    if(mutation==0)bytes[detail::kBaseHeader+16]^=1; // code
    if(mutation==1)detail::put32(bytes.data()+detail::kBaseHeader+4,1); // coverage
    if(mutation==2)bytes[hdr.rank_offset+32]^=1; // score
    if(mutation==3)bytes[hdr.rank_offset+31]^=1; // whole-key suffix
    if(mutation==4)detail::put32(bytes.data()+hdr.member_offset+4,0xffffffffU); // inverse map
    if(mutation==5)detail::put32(bytes.data()+hdr.rank_offset+48,999); // paths
    Memory candidate(bytes),source(h.fixture.built.scheme_bundles.at("full_pinyin"));std::string error;
    check(!detail::check_base_order(candidate,source,h.shared,&error),"bad final bytes semantically qualified");
  }
  std::cout<<"PASS O2 final-byte code/coverage/score/key/member/path corruption rejected\n";
  check(h.scheme.attach_base_order(h.order),"reattach");
  std::array<char,128> names{};std::array<char,64> text{};
  MemberEligibility member(*h.order,h.shared,names.data(),names.size(),text.data(),text.size());
  MemberRequest request{h.shared.activation_handle(),"same",got[0].ref,got[0].key,1,1,0,4,4,{}};
  MemberTicket ticket;
  auto run=[&] {
    check(member.begin(request,&ticket)==MemberStatus::Pending,"member begin");
    for(unsigned n=0;n<200000;++n) {
      auto s=member.advance(ticket,h.budget);check(s.cost.read_calls<=1&&s.cost.read_bytes<=17&&s.cost.work_items<=3,"member budget");
      if(s.status!=MemberStatus::Pending)return s.status;
    }
    throw std::runtime_error("member stalled");
  };
  const auto query_revision=h.scheme.current_query_revision();
  check(run()==MemberStatus::Eligible&&member.eligible(ticket,request),"valid member");
  const auto old=ticket;
  request.key.bytes[31]^=128;check(run()==MemberStatus::IdentityMismatch,"member suffix mismatch");request.key=got[0].key;
  request.code="absent";check(run()==MemberStatus::NotMapped,"shared key used without code membership");request.code="same";
  check(member.eligible(old,request),"proof lost after another member");
  for(unsigned variant=0;variant<8;++variant) {
    auto changed=request;
    if(variant==0)++changed.selection_revision;
    if(variant==1)++changed.input_revision;
    if(variant==2)changed.key.bytes[31]^=1;
    if(variant==3)++changed.ref;
    if(variant==4)changed.code="else";
    if(variant==5)--changed.end;
    if(variant==6)++changed.source_bytes;
    if(variant==7)changed.output.form="traditional";
    check(!member.eligible(old,changed),"proof accepted different captured member");
  }
  check(h.scheme.current_query_revision()==query_revision,"member altered current query");
  const auto normal=request;
  request.ref=1;request.key=fixture::kPrototype;request.output.form="zh-Hant";
  check(run()==MemberStatus::Eligible&&member.eligible(ticket,request),"generic form qualification");
  request.ref=7;request.key=fixture::kMultipleVariantForms;request.output.form="zh-Variant";
  check(run()==MemberStatus::OutputRestricted&&!member.eligible(ticket,request),"restricted output qualified");
  request=normal;
  check(run()==MemberStatus::Eligible,"member restored");
  MemberTicket pending;check(member.begin(request,&pending)==MemberStatus::Pending,"pending member");
  BaseOrderCursor external;check(h.order->begin("same",external)==BaseOrderStatus::Pending,"external begin");
  member.cancel(pending);check(h.order->current(external),"old cleanup damaged new derived operation");
  SharedReadCursor sr;check(h.shared.begin_read(SharedRecordKind::Lexeme,got[0].ref,&sr)==SharedReadStatus::InProgress,"external shared");
  member.cancel(pending);
  for(;;){auto s=h.shared.advance_read(&sr,h.budget);if(s.status==SharedReadStatus::Ready){check(s.lexeme.identity.stable_key==got[0].key,"external key");break;}check(s.status==SharedReadStatus::InProgress,"old cleanup damaged shared");}
  std::cout<<"PASS O3 direct membership, full identity, query isolation, foreign cleanup\n";
  const auto activation=h.scheme.activation_context();std::string error;
  check(h.shared.open_verified(h.readers[0],9,{1,17},&error),error);
  check(!h.order->ready()&&!h.scheme.activation_is_current(activation)&&!member.eligible(old,request),"same-package/epoch reopen resurrected proof");
  std::cout<<"PASS O4 same-package activation invalidates derived results and qualification\n";
}
void binding_and_corruption(const fs::path& dir) {
  OrderedFixture h(dir);
  auto bad=h.fixture.built.shared_bundle;bad.back()^=1;
  auto rejected=h.fixture.options(bad,h.fixture.built.scheme_bundles.at("full_pinyin"));
  rejected.base_order=true;h.fixture.rejected(rejected,"derived option rejected invalid primary");
  for(unsigned variant=0;variant<5;++variant) {
    auto pin=h.publication.resources[2].spec;
    if(variant==0)pin.required_scheme_sha256[31]^=1;
    if(variant==1)pin.required_shared_sha256[31]^=1;
    if(variant==2)pin.ranking_rule_version=2;
    if(variant==3)++pin.name_capacity;
    if(variant==4)++pin.registry_namespace;
    auto reader=std::make_shared<VerifiedResourceReader>(h.data[2],h.proof[2],pin,VerifiedResourceConfig{3,1,17});
    if(reader->prepare_bootstrap()) {
      for(unsigned n=0;!reader->bootstrap_ready()&&n<100000;++n) {
        auto s=reader->advance_bootstrap(h.budget);check(s.status!=ResourceReadStatus::Error,"binding bootstrap");
      }
    }
    BaseOrderIndex invalid(reader,h.scheme,h.shared);check(!invalid.ready(),"wrong release qualified");
  }
  for(unsigned variant=0;variant<3;++variant) {
    auto data=std::make_shared<Memory>(h.data[2]->bytes),proof=std::make_shared<Memory>(h.proof[2]->bytes);
    if(variant==0)data->bytes[1024]^=1;
    if(variant==1)proof->bytes[0]^=1;
    if(variant==2)data->bytes.pop_back();
    auto reader=std::make_shared<VerifiedResourceReader>(data,proof,h.publication.resources[2].spec,VerifiedResourceConfig{3,1,17});
    std::array<std::uint8_t,97> out{};ResourceReadContext context{std::make_shared<int>(0),1};bool failed=!reader->healthy();
    for(std::size_t offset=0;!failed&&offset<data->size();offset+=out.size()) {
      ++context.revision;
      for(unsigned n=0;n<100000;++n) {
        auto s=reader->read_budgeted(offset,out.data(),std::min(out.size(),data->size()-offset),h.budget,context);
        if(s.status==ResourceReadStatus::Error){failed=true;break;}
        if(s.status==ResourceReadStatus::Ready)break;
      }
    }
    check(failed&&!reader->healthy(),"corrupt/truncated derived data used");
  }
  std::cout<<"PASS O5 exact dependencies/rule/capacity, corrupt data/proof and truncation fail closed\n";
}
void locate_reuse(const fs::path& dir) {
  OrderedFixture h(dir,false,true);
  auto locate=[&](BaseOrderIndex& index,BaseOrderCursor& cursor,std::string_view code) {
    check(index.begin(code,cursor)==BaseOrderStatus::Pending,"reuse begin");
    for(unsigned n=0;n<100000;++n){auto s=index.locate(cursor,{1,17,1});
      check(s.cost.read_calls<=1&&s.cost.read_bytes<=17&&s.cost.work_items<=1,"reuse budget");
      if(s.status!=BaseOrderStatus::Pending)return s.status;
    }throw std::runtime_error("reuse stalled");
  };
  detail::BaseHeader header;check(detail::base_header(h.data[2]->bytes.data(),h.data[2]->size(),header),"directory test header");
  std::vector<std::string> codes;
  for(uint32_t i=0;i<header.codes;++i){const auto* p=h.data[2]->bytes.data()+detail::kBaseHeader+i*header.width;
    codes.emplace_back(reinterpret_cast<const char*>(p+16),detail::bo32(p));}
  check(codes.size()>8,"eviction fixture requires >8 codes");
  for(std::size_t cap:{0u,4u,8u}) {
    h.order=std::make_shared<BaseOrderIndex>(h.readers[2],h.scheme,h.shared,cap);
    same_items(all_order(h,false),all_order(h,true));
    BaseOrderIndex index(h.readers[2],h.scheme,h.shared,cap);BaseOrderCursor c;
    check(locate(index,c,"same")==BaseOrderStatus::Ready&&c.count()==7,"positive locate");
    auto old=c.count();auto stats=index.locate_stats();
    check(locate(index,c,"same")==BaseOrderStatus::Ready&&c.count()==old,"repeat coverage");
    check(index.locate_stats().hits-stats.hits==(cap?1u:0u),"repeat hit");
    if(cap)check(index.locate_stats().directory.read_calls==stats.directory.read_calls,"hit read I/O");
    for(const auto& code:codes)check(locate(index,c,code)==BaseOrderStatus::Ready,"all codes reachable");
    check(index.locate_stats().resident<=cap,"pool bound");
    if(cap)check(index.locate_stats().evictions>0,"eviction exercised");
    stats=index.locate_stats();
    check(locate(index,c,"not-mapped")==BaseOrderStatus::NotFound,"missing");
    check(locate(index,c,"not-mapped")==BaseOrderStatus::NotFound,"missing repeat");
    check(index.locate_stats().hits==stats.hits,"negative not cached");
    check(index.begin("same",c)==BaseOrderStatus::Pending,"pending");
    stats=index.locate_stats();auto zero=index.locate(c,{0,0,0});
    check(zero.status==BaseOrderStatus::InvalidRequest&&index.locate_stats().hits==stats.hits,"zero budget work");
    index.cancel(c);check(index.locate(c,{1,17,1}).status==BaseOrderStatus::Stale,"cancel lookup");
    BaseOrderCursor newer;check(index.begin("same",newer)==BaseOrderStatus::Pending,"new query");
    index.cancel(c);check(index.current(newer),"old cancel new query");
    check(locate(index,newer,"same")==BaseOrderStatus::Ready,"new query resolves");
    std::cout<<"LOCATE_MEMORY capacity="<<cap<<" index="<<index.memory_bytes()<<" cursor="<<c.memory_bytes()
      <<" pool="<<index.locate_stats().pool_bytes<<" entry="<<index.locate_stats().entry_bytes<<'\n';
  }
  fail_locate_allocation=true;BaseOrderIndex fallback(h.readers[2],h.scheme,h.shared,8);fail_locate_allocation=false;
  BaseOrderCursor c;check(fallback.ready()&&fallback.locate_stats().capacity==0&&locate(fallback,c,"same")==BaseOrderStatus::Ready,"allocation fallback");
  BaseOrderIndex invalid_size(h.readers[2],h.scheme,h.shared,9);check(invalid_size.locate_stats().capacity==0&&locate(invalid_size,c,"same")==BaseOrderStatus::Ready,"invalid size fallback");
  BaseOrderIndex pending(h.readers[2],h.scheme,h.shared,4);
  check(pending.begin("same",c)==BaseOrderStatus::Pending,"unpublished begin");
  check(pending.locate(c,{1,1,1}).status==BaseOrderStatus::Pending,"pending read");pending.cancel(c);
  check(!pending.locate_stats().resident,"pending/cancel published fact");
  check(locate(pending,c,"same")==BaseOrderStatus::Ready,"complete after cancel");
  // Detect a changed immutable transport on an uncached access, then ensure
  // even a previously resident positive fact cannot resurrect the resource.
  auto context=ResourceReadContext{std::make_shared<int>(0),1};
  h.data[2]->bytes.back()^=1;
  std::array<uint8_t,17> scratch{};bool failed=false;
  for(size_t off=0;!failed&&off<h.data[2]->size();off+=scratch.size()) {
    ++context.revision;
    for(unsigned n=0;n<100000;++n){auto step=h.readers[2]->read_budgeted(off,scratch.data(),std::min(scratch.size(),h.data[2]->size()-off),{1,17,3},context);
      if(step.status==ResourceReadStatus::Error){failed=true;break;}
      if(step.status==ResourceReadStatus::Ready)break;
    }
  }
  check(failed&&!pending.ready()&&pending.begin("same",c)==BaseOrderStatus::Stale,"latched authentication failure reused fact");
  auto stats=pending.locate_stats();h.readers[2]->deactivate();
  check(pending.begin("same",c)==BaseOrderStatus::Stale&&!pending.ready()&&pending.locate_stats().hits==stats.hits,"deactivated pool used");
  std::cout<<"PASS O6 locate 0/4/8, eviction, exact coverage, zero budget, cancellation, allocation failure, deactivation\n";
}
}
int main(int argc,char** argv) {
  try {check(argc==2,"test root argument");fs::create_directories(argv[1]);
    const auto dir=fs::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    ordered_regressions(dir);binding_and_corruption(dir.string()+"_negative");locate_reuse(dir.string()+"_reuse");return 0;
  } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
