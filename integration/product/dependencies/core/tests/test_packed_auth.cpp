// Reuse the existing final-byte publisher setup, not a second runtime.
#define main existing_publisher_main_for_packed_auth
#include "test_resource_publisher.cpp"
#undef main
#include "../src/packed_auth_build.hpp"
#include "hanime/composition_commit.hpp"

namespace {
constexpr WorkBudget ordinary{1,1024,1024};

struct PacketPort final : ResourceReader {
  explicit PacketPort(Bytes input) : bytes(std::move(input)) {}
  std::size_t size() const override { return bytes.size(); }
  bool healthy() const override { return live; }
  bool read(std::size_t offset,void* target,std::size_t count) const override {
    ++calls;requested+=count;
    if(!live || !target || offset>bytes.size() || count>bytes.size()-offset)return false;
    std::memcpy(target,bytes.data()+offset,short_read ? count/2 : count);
    return !short_read;
  }
  Bytes bytes;
  bool live{true},short_read{false};
  mutable std::size_t calls{0},requested{0};
};

struct PacketFixture {
  Bytes source;
  PublishedResource publication;
  Bytes packets;
  PackedMerkleLayout layout;
  explicit PacketFixture(std::size_t length=70*512+19) : source(length) {
    for(std::size_t i=0;i<source.size();++i)source[i]=static_cast<std::uint8_t>(i*13+i/211);
    publication.spec.registry_namespace=1;publication.spec.shared_bundle_id=2;
    publication.spec.runtime_ref_count=1;publication.spec.file_bytes=source.size();
    publication.spec.file_sha256=sha256(source.data(),source.size());
    publication.spec.verification_format=2;
    Memory raw(source);std::size_t tree_bytes=0;
    packets=detail::build_packed_auth(raw,publication,&tree_bytes);
    check(packed_merkle_layout(source.size(),&layout),"packet layout");
  }
};

ResourceReadContext ticket() { return {std::make_shared<const int>(0),1}; }
ResourceReadStep step(VerifiedResourceReader& reader,PacketPort& port,std::size_t offset,
    void* destination,std::size_t count,const ResourceReadContext& context,WorkBudget budget) {
  const auto before_calls=port.calls,before_bytes=port.requested;
  const auto result=reader.read_budgeted(offset,destination,count,budget,context);
  check(result.cost.read_calls<=budget.max_read_calls && result.cost.read_bytes<=budget.max_read_bytes &&
        result.cost.work_items<=budget.max_work_items,"packed WorkBudget exceeded");
  check(result.cost.read_calls==port.calls-before_calls && result.cost.read_bytes==port.requested-before_bytes,
        "packed physical cost not reported");
  return result;
}
ResourceReadStep finish(VerifiedResourceReader& reader,PacketPort& port,std::size_t offset,
    void* destination,std::size_t count,const ResourceReadContext& context,WorkBudget budget=ordinary) {
  for(unsigned i=0;i<100000;++i) {
    const auto result=step(reader,port,offset,destination,count,context,budget);
    if(result.status!=ResourceReadStatus::Pending)return result;
  }
  throw std::runtime_error("packet read stalled");
}

void boundaries_and_budget() {
  for(const auto length:{std::size_t{1},std::size_t{512},std::size_t{513},std::size_t{70*512+19}}) {
    PacketFixture fixture(length);
    auto port=std::make_shared<PacketPort>(fixture.packets);
    VerifiedResourceReader reader(nullptr,port,fixture.publication.spec,{3,2,1024});
    check(reader.healthy(),"valid packed reader");
    Bytes actual(length,0);const auto operation=ticket();
    for(unsigned i=0;i<1000000;++i) {
      const WorkBudget budget=i%3==0 ? WorkBudget{1,1,1} :
                              i%3==1 ? WorkBudget{1,17,3} : ordinary;
      const auto result=step(reader,*port,0,actual.data(),actual.size(),operation,budget);
      check(result.status!=ResourceReadStatus::Error,"valid varying budget packet failed");
      if(result.status==ResourceReadStatus::Ready)break;
      check(i+1<1000000,"varying read did not finish");
    }
    check(actual==fixture.source,"packed logical stream differs");
    check(!reader.snapshot().whole_file_checked,"streaming reader claimed whole-file coverage");
    std::array<std::uint8_t,19> last{};
    const auto count=std::min(last.size(),length);
    const auto calls=port->calls;
    check(finish(reader,*port,length-count,last.data(),count,ticket()).status==ResourceReadStatus::Ready,
          "hot ending bytes");
    check(port->calls==calls,"hot packet unexpectedly reloaded");
  }
  PacketFixture fixture;auto port=std::make_shared<PacketPort>(fixture.packets);
  VerifiedResourceReader reader(nullptr,port,fixture.publication.spec,{3,1,1024});
  std::array<std::uint8_t,52> bytes{};
  check(finish(reader,*port,3*512+60,bytes.data(),bytes.size(),ticket()).status==ResourceReadStatus::Ready,
        "cold record");
  check(port->calls==1 && port->requested==fixture.layout.packet_bytes,"cold record not one physical packet");
  PackedMerkleLayout large;
  check(packed_merkle_layout(512ULL*(1ULL<<32),&large) && large.depth==32 && large.packet_bytes==1536,
        "maximum bounded packet layout");
  check(!packed_merkle_layout(0,&large) && !packed_merkle_layout(512ULL*(1ULL<<32)+1,&large),"invalid layout accepted");
  std::cout<<"PASS PA1 exact bytes, degenerate trees, 512 crossings, tiny budgets, hot/cold and bounded dimensions\n";
}

void faults() {
  const PacketFixture fixture;
  for(unsigned kind=0;kind<8;++kind) {
    auto port=std::make_shared<PacketPort>(fixture.packets);
    auto spec=fixture.publication.spec;
    std::size_t offset=3*512+10;
    if(kind==0)port->bytes[3*fixture.layout.packet_bytes+10]^=1;
    if(kind==1)port->bytes[3*fixture.layout.packet_bytes+512]^=1;
    if(kind==2)std::swap_ranges(port->bytes.begin()+3*fixture.layout.packet_bytes,
        port->bytes.begin()+4*fixture.layout.packet_bytes,port->bytes.begin()+4*fixture.layout.packet_bytes);
    if(kind==3)spec.merkle_root[0]^=1;
    if(kind==4)port->short_read=true;
    if(kind==5) { offset=fixture.source.size()-1;port->bytes[(fixture.layout.data_blocks-1)*fixture.layout.packet_bytes+19]=1; }
    if(kind==6)port->bytes.pop_back();
    if(kind==7)spec.verification_format=3;
    VerifiedResourceReader reader(nullptr,port,spec,{3,2,1024});
    std::uint8_t value=0xa5;
    check(finish(reader,*port,offset,&value,1,ticket()).status==ResourceReadStatus::Error,"bad packet used");
    check(value==0xa5 && !reader.healthy(),"unauthenticated byte escaped or error was not latched");
    const auto calls=port->calls;
    check(finish(reader,*port,0,&value,1,ticket()).status==ResourceReadStatus::Error && port->calls==calls,
          "latched failure retried or fell back to source file");
  }
  auto port=std::make_shared<PacketPort>(fixture.packets);
  VerifiedResourceReader reader(nullptr,port,fixture.publication.spec);
  std::uint8_t value=0;
  check(finish(reader,*port,0,&value,1,ticket()).status==ResourceReadStatus::Ready,"healthy packet");
  port->live=false;
  check(!reader.read(0,&value,1) && !reader.healthy(),"unhealthy hot cache survived");
  std::cout<<"PASS PA2 data/proof/position/pin/padding/truncation/version/short-read/health fail closed\n";
}

void interruption() {
  const PacketFixture fixture;
  const WorkBudget budget{1,17,3};
  std::size_t cuts=0;
  for(unsigned cut=0;cut<1600;++cut) {
    auto port=std::make_shared<PacketPort>(fixture.packets);
    auto reader=std::make_unique<VerifiedResourceReader>(nullptr,port,fixture.publication.spec,VerifiedResourceConfig{3,2,17});
    std::array<std::uint8_t,52> old{},fresh{};
    auto first=ticket();std::weak_ptr<const void> lifetime=first.owner;
    auto result=ResourceReadStatus::Pending;
    for(unsigned tick=0;tick<cut && result==ResourceReadStatus::Pending;++tick)
      result=step(*reader,*port,500,old.data(),old.size(),first,budget).status;
    if(result==ResourceReadStatus::Ready)break;
    ++cuts;
    if(cut%4==0)reader->cancel_read_budgeted(first);
    if(cut%4==1) { reader->deactivate();reader=std::make_unique<VerifiedResourceReader>(nullptr,port,fixture.publication.spec); }
    if(cut%4==2)reader=std::make_unique<VerifiedResourceReader>(nullptr,port,fixture.publication.spec);
    const auto next=ticket();
    step(*reader,*port,500,fresh.data(),fresh.size(),next,budget);
    reader->cancel_read_budgeted(first); // Foreign cleanup must not cancel next.
    first={};check(lifetime.expired(),"old operation retained owner token");
    check(finish(*reader,*port,500,fresh.data(),fresh.size(),next).status==ResourceReadStatus::Ready &&
          std::equal(fresh.begin(),fresh.end(),fixture.source.begin()+500),"replacement used partial stale packet");
  }
  check(cuts>600 && cuts<1600,"did not cover load/hash/cache/cross-block interruption boundaries");
  std::cout<<"PASS PA3 cancellation/replacement/deactivate/destruction cuts="<<cuts<<" new work preserved\n";
}

bool bootstrap(VerifiedResourceReader& reader) {
  if(!reader.prepare_bootstrap())return false;
  for(unsigned i=0;i<10000;++i) {
    const auto result=reader.advance_bootstrap(ordinary);
    if(result.status==ResourceReadStatus::Ready)return true;
    if(result.status==ResourceReadStatus::Error)return false;
  }
  throw std::runtime_error("packed bootstrap timeout");
}

void public_outputs_and_rejection(Fixture& fixture) {
  auto input=fixture::make_small_input(false);
  input.lexemes[0].default_output_utf8=std::string(2051,'a')+"𨿍结束";
  auto built=ResourceBuilder().build(input,{4096,4096});check(built.ok,"packed long fixture");
  auto options=fixture.options(built.resources.shared_bundle,built.resources.scheme_bundles.at("full_pinyin"));
  options.text_capacity=4096;options.validation_budget={64,16384,4096};options.packed_auth=true;options.packed_auth_all=true;
  const auto publication=publish_verified_resources(options);
  check(publication.ok && publication.resources.size()==2,"packed final-byte publication: "+publication.error);
  auto shared_port=std::make_shared<PacketPort>(read(fs::path(options.output_directory)/"shared.hnv"));
  auto auth=std::make_shared<VerifiedResourceReader>(nullptr,shared_port,publication.resources[0].spec);
  check(bootstrap(*auth),"packed shared bootstrap");
  PagedSharedResource shared;std::string error;
  check(shared.open_verified(auth,9,{2,1024,true},&error),"packed shared open");
  const auto activation=shared.activation_handle();
  auto output=[&](RuntimeLexemeRef ref,const StableLexemeKey& key,OutputRequest request,
                  OutputStatus expected,const std::string& text) {
    std::array<char,4096> destination{};std::array<char,128> names{};OutputReader reader;
    check(reader.begin(shared,activation,ref,key,request,names.data(),names.size(),destination.data(),destination.size())==
          OutputStatus::Pending,"packed output begin");
    for(unsigned i=0;i<100000;++i) {
      const auto budget=i%2 ? ordinary : WorkBudget{1,17,3};
      const auto result=reader.advance(budget);
      check(result.cost.read_calls<=budget.max_read_calls && result.cost.read_bytes<=budget.max_read_bytes &&
            result.cost.work_items<=budget.max_work_items,"output packed budget");
      if(result.status==OutputStatus::Pending) {check(result.text.empty(),"partial UTF-8 exposed");continue;}
      check(result.status==expected && result.text==text,"packed output eligibility/text mismatch");return;
    }
    throw std::runtime_error("packed output stalled");
  };
  output(1,fixture::kPrototype,{},OutputStatus::Default,input.lexemes[0].default_output_utf8);
  output(1,fixture::kPrototype,{"zh-Hant",OutputInvocation::Profile},OutputStatus::Exact,"原型機");
  output(7,fixture::kMultipleVariantForms,{"zh-Variant",OutputInvocation::Profile},OutputStatus::MultipleRelations,"");
  auto bad_key=fixture::kPrototype;bad_key.bytes[31]^=1;
  output(1,bad_key,{},OutputStatus::Stale,"");
  auto scheme_port=std::make_shared<PacketPort>(read(fs::path(options.output_directory)/"full_pinyin.hnv"));
  auto bad_spec=publication.resources[1].spec;bad_spec.required_shared_sha256[0]^=1;
  auto bad_scheme=std::make_shared<VerifiedResourceReader>(nullptr,scheme_port,bad_spec);
  check(bootstrap(*bad_scheme),"scheme byte authentication setup");
  PagedSchemeIndex scheme;
  check(!scheme.open_verified(bad_scheme,shared,{2,1024},&error),"incorrect dependency got query eligibility");
  auto replacement=std::make_shared<VerifiedResourceReader>(nullptr,shared_port,publication.resources[0].spec);
  check(bootstrap(*replacement) && shared.open_verified(replacement,9,{2,1024,true},&error),"same epoch replacement");
  check(!shared.activation_is_current(activation),"same epoch revived old activation");
  auto malformed=built.resources.shared_bundle;
  std::copy_n(malformed.begin()+shared_address(malformed,0,0),32,malformed.begin()+shared_address(malformed,0,1));
  rehash(malformed,true);checksum_self_consistent(malformed);
  auto invalid=fixture.options(malformed,built.resources.scheme_bundles.at("full_pinyin"));invalid.packed_auth=true;
  fixture.rejected(invalid,"packed semantic rejection");
  auto capacity=fixture.options(built.resources.shared_bundle,built.resources.scheme_bundles.at("full_pinyin"));
  capacity.packed_auth=true;capacity.text_capacity=64;fixture.rejected(capacity,"packed capacity rejection");
  std::cout<<"PASS PA4 final-byte publisher, long UTF-8/generic restrictions/full key/dependency/activation and no pin for invalid source\n";
}

void public_failure_does_not_submit(Fixture& fixture) {
  auto input=fixture::make_small_input(false);
  input.lexemes[0].default_output_utf8=std::string(2051,'a')+"𨿍";
  const auto built=ResourceBuilder().build(input,{4096,4096});
  check(built.ok,"public corruption fixture");
  auto options=fixture.options(built.resources.shared_bundle,built.resources.scheme_bundles.at("full_pinyin"));
  options.text_capacity=4096;
  options.packed_auth=true; // Chosen mixed release: shared format 2, scheme 1.
  const auto publication=publish_verified_resources(options);
  check(publication.ok && publication.resources[1].spec.verification_format==1,"mixed publication");
  const auto packets=read(fs::path(options.output_directory)/"shared.hnv");
  PackedMerkleLayout layout;
  check(packed_merkle_layout(publication.resources[0].spec.file_bytes,&layout),"public packet layout");
  struct Document final : TextCommitSink {
    std::string text;
    unsigned calls{0};
    bool commit(std::string_view value) override { ++calls;text.append(value);return true; }
  };
  for(unsigned fault=0;fault<4;++fault) {
    auto port=std::make_shared<PacketPort>(packets);
    auto auth=std::make_shared<VerifiedResourceReader>(nullptr,port,publication.resources[0].spec);
    check(bootstrap(*auth),"public failure bootstrap");
    PagedSharedResource shared;
    std::string error;
    check(shared.open_verified(auth,9,{2,1024,true},&error),"public failure mount");
    Document document;
    CompositionCommitter committer(shared,document,{2,128,4096,8192});
    const std::array<CompositionMember,2> members{{
      {1,fixture::kPrototype,{0,1},{}},{1,fixture::kPrototype,{1,2},{}}
    }};
    const auto transaction=committer.prepare(shared.activation_handle(),{1,1,{0,2}},members.data(),members.size());
    check(transaction.status==CompositionStatus::Preparing,"public failure preparation");
    if(fault<2) {
      // Record 0 shares the already-authenticated bootstrap block in this
      // small fixture. Target the still-cold long payload instead: one test
      // changes data, the other its proof. No valid cache is modified.
      const auto& source=built.resources.shared_bundle;
      const auto& text=input.lexemes[0].default_output_utf8;
      const auto position=std::search(source.begin(),source.end(),text.begin(),text.end(),
          [](std::uint8_t a,char b) { return a==static_cast<std::uint8_t>(b); });
      check(position!=source.end(),"test payload not found");
      const auto logical=static_cast<std::size_t>(position-source.begin())+512;
      check(logical>=512,"fault must not target bootstrap cache");
      const auto packet=(logical/512)*layout.packet_bytes;
      port->bytes[packet+(fault==0 ? logical%512 : 512)]^=1;
    } else if(fault==2) {
      port->short_read=true;
    } else {
      // End the old operation mid-authentication, then remount the same bytes
      // and epoch. The old receipt must not authorize the replacement owner.
      committer.advance(transaction.transaction,{1,17,3});
      auto replacement=std::make_shared<VerifiedResourceReader>(nullptr,port,publication.resources[0].spec);
      check(bootstrap(*replacement) && shared.open_verified(replacement,9,{2,1024,true},&error),"public replacement");
    }
    auto status=CompositionStatus::Preparing;
    for(unsigned i=0;i<100000 && status==CompositionStatus::Preparing;++i) {
      const auto result=committer.advance(transaction.transaction,ordinary);
      check(result.cost.read_calls<=1 && result.cost.read_bytes<=1024 && result.cost.work_items<=1024,
            "public failure advance budget");
      check(document.text.empty() && document.calls==0,"failed preparation submitted a prefix");
      status=result.result.status;
    }
    check(status==CompositionStatus::ReadError || status==CompositionStatus::Stale,
          "public failure returned unexpected status fault="+std::to_string(fault)+" status="+std::to_string(static_cast<int>(status)));
    committer.confirm(transaction.transaction);
    committer.confirm(transaction.transaction);
    check(document.text.empty() && document.calls==0,"failed receipt submitted text");
  }
  std::cout<<"PASS PA5 mixed release, public preparation data/proof/short-read/remount failures never call final sink\n";
}
} // namespace

#ifndef HANIME_PACKED_TEST_MAIN
#define HANIME_PACKED_TEST_MAIN main
#endif
int HANIME_PACKED_TEST_MAIN(int argc,char** argv) {
  try {
    check(argc==2,"usage: packed_auth_tests WORK_PARENT");
    fs::create_directories(argv[1]);
    const auto path=fs::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Fixture fixture(path);
    boundaries_and_budget();faults();interruption();public_outputs_and_rejection(fixture);
    public_failure_does_not_submit(fixture);
    std::cout<<"packed authentication tests: 5/5\n";
    return 0;
  } catch(const std::exception& error) {std::cerr<<"FAIL packed: "<<error.what()<<'\n';return 1;}
}
