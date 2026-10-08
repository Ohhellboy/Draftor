#include "hanime/resource_publisher.hpp"
#include "hanime/paged_shared_resource.hpp"
#include "hanime/bounded_output.hpp"
#include "hanime/small_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hanime;
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
void check(bool ok,const std::string& message) {if(!ok) throw std::runtime_error(message);}
std::uint32_t u32(const Bytes& b,std::size_t o) {
  return b.at(o)|(std::uint32_t(b.at(o+1))<<8U)|(std::uint32_t(b.at(o+2))<<16U)|(std::uint32_t(b.at(o+3))<<24U);
}
std::uint64_t u64(const Bytes& b,std::size_t o) {return u32(b,o)|(std::uint64_t(u32(b,o+4))<<32U);}
void put32(Bytes& b,std::size_t o,std::uint32_t v) {for(unsigned i=0;i<4;++i)b.at(o+i)=static_cast<std::uint8_t>(v>>(8U*i));}
void put64(Bytes& b,std::size_t o,std::uint64_t v) {put32(b,o,static_cast<std::uint32_t>(v));put32(b,o+4,static_cast<std::uint32_t>(v>>32U));}
void rehash(Bytes& b,bool shared) {
  std::uint64_t hash=14695981039346656037ULL,id=hash;
  for(std::size_t i=8;i<16;++i)id=(id^b[i])*1099511628211ULL;
  for(std::size_t i=40;i<b.size();++i){hash=(hash^b[i])*1099511628211ULL;id=(id^b[i])*1099511628211ULL;}
  put64(b,24,hash);if(shared)put64(b,16,id);
}
void checksum_self_consistent(const Bytes& b) {
  std::uint64_t hash=14695981039346656037ULL;
  for(std::size_t i=40;i<b.size();++i)hash=(hash^b[i])*1099511628211ULL;
  check(u64(b,24)==hash,"test mutation was not self-checksummed");
}
std::size_t shared_address(const Bytes& b,std::size_t section,std::size_t record) {
  const auto d=64+24*section;const auto page=u32(b,56),width=u32(b,d+12),per=(page-16)/width;
  return static_cast<std::size_t>(u64(b,d))+(record/per)*page+16+(record%per)*width;
}
void write(const fs::path& path,const Bytes& bytes) {
  std::ofstream out(path,std::ios::binary);check(out.good(),"create test resource");
  out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
  out.close();check(!out.fail(),"write test resource");
}
Bytes read(const fs::path& path) {
  std::ifstream in(path,std::ios::binary);check(in.good(),"read test file");
  return Bytes(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}
class Memory final:public ResourceReader {
 public:
  explicit Memory(Bytes input):bytes(std::move(input)){}
  std::size_t size()const override{return bytes.size();}
  bool read(std::size_t offset,void* dest,std::size_t n)const override {
    ++calls;requested+=n;
    if(!dest||offset>bytes.size()||n>bytes.size()-offset)return false;
    if(n)std::memcpy(dest,bytes.data()+offset,n);
    return true;
  }
  Bytes bytes;
  mutable std::size_t calls{0},requested{0};
};
bool shared_validator_rejects(const Bytes& bytes) {
  PagedSharedResource shared;std::string error;
  if(!shared.open(std::make_shared<Memory>(bytes),fixture::kRegistryNamespace,0,9,{1,17},&error))return true;
  for(unsigned i=0;i<1000000;++i){const auto step=shared.advance_validation(shared.validation_cursor(),{2,73,19});
    if(step.status==ValidationStepStatus::Ready)return false;
    if(step.status==ValidationStepStatus::Error){check(step.error.find("checksum")==std::string::npos,"semantic case only failed checksum");return true;}
    check(step.status==ValidationStepStatus::InProgress,"unexpected shared validation status");
  }
  throw std::runtime_error("shared diagnostic stalled");
}
bool scheme_validator_rejects(const Bytes& bytes,SharedBundleId bundle) {
  PagedSchemeIndex scheme;std::string error;
  if(!scheme.open(std::make_shared<Memory>(bytes),fixture::kRegistryNamespace,bundle,7,{1,17},&error))return true;
  for(unsigned i=0;i<1000000;++i){const auto step=scheme.advance_validation({2,73,19});
    if(step.status==ValidationStepStatus::Ready)return false;
    if(step.status==ValidationStepStatus::Error){check(step.error.find("checksum")==std::string::npos,"scheme semantic case only failed checksum");return true;}
    check(step.status==ValidationStepStatus::InProgress,"unexpected scheme validation status");
  }
  throw std::runtime_error("scheme diagnostic stalled");
}
struct Fixture {
  BuiltResources built;
  fs::path directory;
  unsigned serial{0};
  explicit Fixture(fs::path root,bool reverse=false):directory(std::move(root)) {
    auto input=fixture::make_small_input(reverse);
    // Same-code groups span leaves and repeated raw paths are intentional.
    for(unsigned repeat=0;repeat<8;++repeat)
      for(const auto& lexeme:input.lexemes)
        if(lexeme.status==RegistryStatus::Approved)
          input.mappings.push_back({"full_pinyin","same",lexeme.stable_key,static_cast<std::int32_t>(repeat)});
    auto b=ResourceBuilder().build(input,{96,96});check(b.ok,"build publisher fixture: "+b.error);
    built=std::move(b.resources);check(fs::create_directory(directory),"create fresh test root");
  }
  PublishVerifiedOptions options(const Bytes& shared,const Bytes& scheme,const std::string& id="full_pinyin") {
    const auto item=directory/("case_"+std::to_string(++serial));check(fs::create_directory(item),"create fresh case");
    write(item/"shared.hns",shared);write(item/"scheme.hnm",scheme);
    PublishVerifiedOptions options;options.shared_path=(item/"shared.hns").string();
    options.schemes.push_back({id,(item/"scheme.hnm").string()});
    options.output_directory=(item/"release").string();options.registry_namespace=fixture::kRegistryNamespace;
    options.validation_budget={2,73,19};return options;
  }
  void rejected(PublishVerifiedOptions options,const std::string& label) {
    const auto result=publish_verified_resources(options);
    check(!result.ok && !result.error.empty() && result.resources.empty(),label+" qualified malformed input");
    check(!fs::exists(fs::path(options.output_directory)/"release_pin.hpp") &&
          !fs::exists(fs::path(options.output_directory)/"release_manifest.json"),label+" emitted accepted outputs");
  }
};
void successes(Fixture& f) {
  for(const std::string id:{"full_pinyin","xiaohe"}) {
    auto options=f.options(f.built.shared_bundle,f.built.scheme_bundles.at(id),id);
    const auto before_shared=sha256(f.built.shared_bundle.data(),f.built.shared_bundle.size());
    const auto before_scheme=sha256(f.built.scheme_bundles.at(id).data(),f.built.scheme_bundles.at(id).size());
    const auto result=publish_verified_resources(options);check(result.ok,"publish valid final bytes: "+result.error);
    check(result.resources.size()==2,"single-scheme publication silently included another scheme");
    check(result.resources[0].spec.file_sha256==before_shared && result.resources[1].spec.file_sha256==before_scheme,
          "final content hash wrong");
    check(result.resources[1].spec.required_shared_sha256==before_shared,"scheme lost exact dependency");
    check(result.resources[1].spec.bootstrap_root_offset>=40,"published bogus root span");
    check(read(options.shared_path)==f.built.shared_bundle && read(options.schemes[0].path)==f.built.scheme_bundles.at(id),
          "publisher modified input resources");
    for(const auto& resource:result.resources) {
      const auto index=read(fs::path(options.output_directory)/resource.index_file);
      check(index.size()==resource.index_bytes && sha256(index.data(),index.size())==resource.index_sha256,"index bytes/hash");
      auto data=std::make_shared<Memory>(resource.logical_name=="shared"?f.built.shared_bundle:f.built.scheme_bundles.at(id));
      VerifiedResourceReader verified(data,std::make_shared<Memory>(index),resource.spec,{3,2,17});
      ResourceReadContext context{std::make_shared<int>(1),0};
      std::array<std::uint8_t,97> value{};
      for(std::size_t offset=0;offset<data->size();) {
        ++context.revision;
        const auto n=std::min(value.size(),data->size()-offset);bool ready=false;
        for(unsigned step=0;step<100000;++step){const auto s=verified.read_budgeted(offset,value.data(),n,{1,17,3},context);
          check(s.cost.read_calls<=1&&s.cost.read_bytes<=17&&s.cost.work_items<=3,"published read exceeded budget");
          check(s.status!=ResourceReadStatus::Error,std::string("publisher/runtime authenticated read: ")+verified.error());
          if(s.status==ResourceReadStatus::Ready){ready=true;break;}
        }
        check(ready&&std::equal(value.begin(),value.begin()+n,data->bytes.begin()+offset),"authenticated resource differs");offset+=n;
      }
    }
    const auto pin=read(fs::path(options.output_directory)/"release_pin.hpp");
    const auto second=publish_verified_resources(options);check(!second.ok,"publication overwrite accepted");
    check(read(fs::path(options.output_directory)/"release_pin.hpp")==pin,"rejected overwrite changed pin");
  }
  std::cout<<"PASS V1 independent scheme publication, exact hashes, immutable inputs, authenticated bytes\n";
}
void shared_semantics(Fixture& f) {
  for(unsigned kind=0;kind<5;++kind) {
    auto bytes=f.built.shared_bundle;
    if(kind==0)std::copy_n(bytes.begin()+shared_address(bytes,0,0),32,bytes.begin()+shared_address(bytes,0,1));
    else if(kind==1)put32(bytes,shared_address(bytes,4,0),u32(bytes,shared_address(bytes,4,1)));
    else if(kind==2)put64(bytes,64+24,200); // overlapping sections
    else if(kind==3)put32(bytes,shared_address(bytes,0,1)+40,0); // wrong relation ownership
    else bytes.at(static_cast<std::size_t>(u64(bytes,184)))=0xf5; // invalid Unicode
    rehash(bytes,true);checksum_self_consistent(bytes);
    check(shared_validator_rejects(bytes),"complete shared validator admitted semantic mutation");
    f.rejected(f.options(bytes,f.built.scheme_bundles.at("full_pinyin")),"shared semantic "+std::to_string(kind));
  }
  std::cout<<"PASS V2 self-checksummed shared structure, key uniqueness, coverage, relation, UTF-8 rejection\n";
}
void scheme_semantics(Fixture& f) {
  const auto& source=f.built.scheme_bundles.at("full_pinyin");
  const auto page_bytes=u32(source,44),leaves=u32(source,64),root=u32(source,52);
  const auto pages=static_cast<std::size_t>(40+u64(source,72));
  const auto root_at=pages+std::size_t(root)*page_bytes;
  check(source.at(root_at+4)==2 && leaves>1,"fixture lacks internal coverage");
  for(unsigned kind=0;kind<4;++kind) {
    auto bytes=source;
    if(kind==0) {
      auto record=root_at+16;const auto length=std::size_t(bytes.at(record))+(std::size_t(bytes.at(record+1))<<8U);
      const auto first=record+2+length;record=first+4;
      const auto next_len=std::size_t(bytes.at(record))+(std::size_t(bytes.at(record+1))<<8U);
      check(record+2+next_len+4<=root_at+u32(bytes,root_at+12),"fixture root needs two subtrees");
      put32(bytes,first,u32(bytes,record+2+next_len)); // omit subtree, duplicate another
    } else if(kind==1) bytes.at(root_at+18)^=1; // directory fence no longer subtree maximum
    else if(kind==2) bytes.at(root_at+5)=static_cast<std::uint8_t>(bytes.at(root_at+5)+1);
    else {
      std::vector<std::size_t> refs;
      for(std::uint32_t leaf=0;leaf<leaves;++leaf) {
        auto record=pages+std::size_t(leaf)*page_bytes+16;
        const auto end=pages+std::size_t(leaf)*page_bytes+u32(bytes,pages+std::size_t(leaf)*page_bytes+12);
        while(record<end){const auto n=std::size_t(bytes.at(record))+(std::size_t(bytes.at(record+1))<<8U);record+=2;
          if(std::string(reinterpret_cast<const char*>(bytes.data()+record),n)=="same")refs.push_back(record+n);
          record+=n+12;
        }
      }
      check(refs.size()>20&&u32(bytes,refs.front())!=u32(bytes,refs.back()),"same-code group fixture inadequate");
      const auto first=u32(bytes,refs.front());put32(bytes,refs.front(),u32(bytes,refs.back()));put32(bytes,refs.back(),first);
    }
    rehash(bytes,false);checksum_self_consistent(bytes);
    check(scheme_validator_rejects(bytes,f.built.manifest.shared_bundle_id),"complete scheme validator admitted semantic mutation");
    f.rejected(f.options(f.built.shared_bundle,bytes),"scheme semantic "+std::to_string(kind));
  }
  std::cout<<"PASS V3 self-checksummed directory fence, hierarchy, coverage and same-code ref-order rejection\n";
}
void binding_capacity(Fixture& f) {
  const auto& scheme=f.built.scheme_bundles.at("full_pinyin");
  auto options=f.options(f.built.shared_bundle,scheme);options.registry_namespace^=1;f.rejected(options,"namespace");
  options=f.options(f.built.shared_bundle,scheme);options.expected_shared_bundle_id=f.built.manifest.shared_bundle_id^1;f.rejected(options,"bundle");
  auto changed=scheme;put64(changed,16,u64(changed,16)^1);f.rejected(f.options(f.built.shared_bundle,changed),"scheme shared dependency");
  f.rejected(f.options(f.built.shared_bundle,scheme,"xiaohe"),"role versus serialized ID");
  options=f.options(f.built.shared_bundle,scheme);options.text_capacity=1;f.rejected(options,"output text capacity");
  options=f.options(f.built.shared_bundle,scheme);options.name_capacity=1;f.rejected(options,"output name capacity");
  auto truncated=f.built.shared_bundle;truncated.pop_back();f.rejected(f.options(truncated,scheme),"truncated shared");
  truncated=scheme;truncated.pop_back();f.rejected(f.options(f.built.shared_bundle,truncated),"truncated scheme");
  options=f.options(f.built.shared_bundle,scheme);options.validation_budget={0,1,1};f.rejected(options,"invalid budget");
  std::cout<<"PASS V4 binding, capacities, truncation and invalid-budget rejection without accepted output\n";
}
void fast_owners(Fixture& f) {
  const auto options=f.options(f.built.shared_bundle,f.built.scheme_bundles.at("full_pinyin"));
  const auto publication=publish_verified_resources(options);check(publication.ok,publication.error);
  const auto shared_index=read(fs::path(options.output_directory)/"shared.hnv");
  const auto scheme_index=read(fs::path(options.output_directory)/"full_pinyin.hnv");
  auto bounded=[](const WorkCost& cost,const WorkBudget& budget) {
    check(cost.read_calls<=budget.max_read_calls&&cost.read_bytes<=budget.max_read_bytes&&cost.work_items<=budget.max_work_items,
          "fast owner work exceeded budget");
  };
  auto bootstrap=[&](VerifiedResourceReader& reader,const WorkBudget& budget) {
    check(reader.prepare_bootstrap(),"prepare authenticated bootstrap");
    for(unsigned i=0;i<1000000;++i){const auto step=reader.advance_bootstrap(budget);bounded(step.cost,budget);
      if(step.status==ResourceReadStatus::Ready)return;
      check(step.status==ResourceReadStatus::Pending,"authenticated bootstrap failed");
    }
    throw std::runtime_error("bootstrap stalled");
  };
  const auto expected=fixture::make_small_input(false);
  for(const auto budget:{WorkBudget{1,1,1},WorkBudget{1,17,3},WorkBudget{1,1024,512}}) {
    auto sd=std::make_shared<Memory>(f.built.shared_bundle),si=std::make_shared<Memory>(shared_index);
    auto md=std::make_shared<Memory>(f.built.scheme_bundles.at("full_pinyin")),mi=std::make_shared<Memory>(scheme_index);
    auto sr=std::make_shared<VerifiedResourceReader>(sd,si,publication.resources[0].spec,VerifiedResourceConfig{4,3,17});
    auto mr=std::make_shared<VerifiedResourceReader>(md,mi,publication.resources[1].spec,VerifiedResourceConfig{4,3,17});
    bootstrap(*sr,budget);bootstrap(*mr,budget);
    PagedSharedResource shared;PagedSchemeIndex scheme;std::string error;
    const auto before=sd->calls+si->calls+md->calls+mi->calls;
    check(shared.open_verified(sr,9,{1,17},&error),"shared fast open: "+error);
    check(scheme.open_verified(mr,shared,{1,17},&error),"scheme fast open: "+error);
    TextAccessCheck admission;check(admission.begin(shared,64,128)==TextAccessStatus::Pending,"admission begin");
    const auto admitted=admission.advance(budget);bounded(admitted.cost,budget);
    check(admitted.status==TextAccessStatus::Ready&&admitted.cost.read_calls==0&&admitted.cost.read_bytes==0,"fast admission rescanned resource");
    check(admission.certificate().current(shared,64,128),"fast admission certificate not current");
    TextAccessCheck too_small;check(too_small.begin(shared,63,128)==TextAccessStatus::Pending,"small admission begin");
    const auto denied=too_small.advance(budget);bounded(denied.cost,budget);
    check(denied.status==TextAccessStatus::Incompatible&&denied.cost.read_calls==0,"capacity mismatch reused certificate");
    check(sd->calls+si->calls+md->calls+mi->calls==before,"activation/admission hid extra resource reads");
    check(!sr->snapshot().whole_file_checked&&!mr->snapshot().whole_file_checked,"fast activation claimed whole-file coverage");
    for(RuntimeLexemeRef ref=1;ref<=7;++ref) {
      SharedReadCursor cursor;check(shared.begin_read(SharedRecordKind::Lexeme,ref,&cursor)==SharedReadStatus::InProgress,"fast lexeme start");
      bool ready=false;
      for(unsigned i=0;i<1000000;++i){const auto step=shared.advance_read(&cursor,budget);bounded(step.cost,budget);
        if(step.status==SharedReadStatus::Ready){check(step.lexeme.identity.stable_key==expected.lexemes[ref-1].stable_key,"fast key mismatch");ready=true;break;}
        check(step.status==SharedReadStatus::InProgress,"fast metadata failed");
      }
      check(ready,"fast metadata stalled");
    }
    auto enumerate=[&](PagedSchemeIndex& owner,ExactQueryCursor& cursor) {
      std::size_t hits=0;RuntimeLexemeRef previous=0;bool done=false;
      for(unsigned i=0;i<1000000;++i){const auto step=owner.advance_exact_query(&cursor,budget,3);bounded(step.cost,budget);
        for(const auto& hit:step.batch.hits){check(hit.runtime_ref>=previous,"same-code ref order changed");previous=hit.runtime_ref;++hits;}
        if(step.status==ExactQueryStepStatus::Complete){done=true;break;}
        check(step.status==ExactQueryStepStatus::InProgress||step.status==ExactQueryStepStatus::HitsAvailable,"fast query failed");
      }
      check(done&&hits==56,"bounded fast query omitted raw paths");
    };
    ExactQueryCursor old;check(scheme.begin_exact_query("same",9,&old,&error),"fast query start");enumerate(scheme,old);
    const auto context=scheme.query_context(old);
    auto fresh=std::make_shared<VerifiedResourceReader>(sd,si,publication.resources[0].spec,VerifiedResourceConfig{4,3,17});
    bootstrap(*fresh,budget);
    SharedReadCursor old_read;shared.begin_read(SharedRecordKind::Lexeme,1,&old_read);
    shared.advance_read(&old_read,budget);
    check(shared.open_verified(fresh,9,{1,17},&error),"same-epoch shared reactivation");
    check(!scheme.ready()&&!scheme.query_is_current(context),"old scheme survived new shared activation");
    check(shared.advance_read(&old_read,budget).status==SharedReadStatus::Stale,"old shared read survived activation");
    SharedReadCursor new_read;shared.begin_read(SharedRecordKind::Lexeme,2,&new_read);shared.cancel_read(&old_read);
    bool new_ready=false;
    for(unsigned i=0;i<1000000;++i){const auto step=shared.advance_read(&new_read,budget);bounded(step.cost,budget);
      if(step.status==SharedReadStatus::Ready){check(step.lexeme.identity.stable_key==expected.lexemes[1].stable_key,"old cleanup damaged new read");new_ready=true;break;}
      check(step.status==SharedReadStatus::InProgress,"new shared read was cancelled");
    }
    check(new_ready,"new shared read stalled");
    auto bad_spec=publication.resources[1].spec;bad_spec.required_shared_sha256[0]^=1;
    auto bad=std::make_shared<VerifiedResourceReader>(md,mi,bad_spec,VerifiedResourceConfig{4,3,17});bootstrap(*bad,budget);
    PagedSchemeIndex wrong;check(!wrong.open_verified(bad,shared,{1,17},&error)&&!wrong.ready(),"wrong exact shared SHA accepted");
    auto next=std::make_shared<VerifiedResourceReader>(md,mi,publication.resources[1].spec,VerifiedResourceConfig{4,3,17});bootstrap(*next,budget);
    PagedSchemeIndex replacement;check(replacement.open_verified(next,shared,{1,17},&error),"new scheme activation");
    ExactQueryCursor current;check(replacement.begin_exact_query("same",9,&current,&error),"new query start");
    replacement.cancel_query(&old);enumerate(replacement,current);
    fresh->deactivate();check(!shared.ready()&&!replacement.ready(),"fatal/deactivated authenticated reader remained ready");
  }
  // Vary the budget DURING the same pending request, not only between runs.
  // This selector page straddles authentication blocks; the initial bootstrap
  // has only block zero. A larger next tick must resume the originally captured
  // lower read span instead of replacing it with a newly sized request.
  {
    const std::array<WorkBudget,3> budgets{{{1,1,1},{1,17,3},{1,1024,512}}};
    std::size_t tick=0;
    auto sd=std::make_shared<Memory>(f.built.shared_bundle),si=std::make_shared<Memory>(shared_index);
    auto md=std::make_shared<Memory>(f.built.scheme_bundles.at("full_pinyin")),mi=std::make_shared<Memory>(scheme_index);
    auto sr=std::make_shared<VerifiedResourceReader>(sd,si,publication.resources[0].spec,VerifiedResourceConfig{3,2,1024});
    auto mr=std::make_shared<VerifiedResourceReader>(md,mi,publication.resources[1].spec,VerifiedResourceConfig{3,2,1024});
    bootstrap(*sr,budgets[2]);bootstrap(*mr,budgets[2]);
    PagedSharedResource shared;PagedSchemeIndex scheme;std::string error;
    check(shared.open_verified(sr,9,{1,1024},&error),"mixed-budget shared activation");
    check(scheme.open_verified(mr,shared,{1,1024},&error),"mixed-budget scheme activation");
    const auto selector_page=shared.info().sections[2].offset;
    check(shared.info().sections[2].count && selector_page/kVerifiedBlockBytes!=
          (selector_page+shared.info().page_bytes-1)/kVerifiedBlockBytes,"fixture must cross a shared authentication block");
    const auto selector_record=shared_address(f.built.shared_bundle,2,0);
    const auto loads_before=sr->snapshot().authenticated_loads;
    SharedReadCursor read_cursor;
    check(shared.begin_read(SharedRecordKind::Selector,0,&read_cursor)==SharedReadStatus::InProgress,"mixed-budget shared begin");
    bool ready=false;unsigned pending=0;
    for(unsigned i=0;i<1000000;++i) {
      const auto budget=budgets[tick++%budgets.size()];
      const auto calls=sd->calls+si->calls,bytes=sd->requested+si->requested;
      const auto step=shared.advance_read(&read_cursor,budget);bounded(step.cost,budget);
      check(step.cost.read_calls==sd->calls+si->calls-calls && step.cost.read_bytes==sd->requested+si->requested-bytes,
            "mixed-budget shared physical requests were not metered");
      if(step.status==SharedReadStatus::Ready) {
        check(shared.result_is_current(step) && step.selector.name==u32(f.built.shared_bundle,selector_record) &&
              step.selector.selection_basis==u32(f.built.shared_bundle,selector_record+4),"mixed-budget selector differs from final bytes");
        ready=true;break;
      }
      check(step.status==SharedReadStatus::InProgress,"mixed-budget shared continuation failed");++pending;
    }
    check(ready && pending>=3 && sr->snapshot().authenticated_loads>loads_before,
          "mixed-budget shared operation did not complete its cold cross-block read");

    // Reference uses the original complete-validation path, with fixed budgets.
    // Only this Host test collects every raw tuple; runtime batching stays bounded.
    PagedSchemeIndex reference;
    check(reference.open(std::make_shared<Memory>(f.built.scheme_bundles.at("full_pinyin")),fixture::kRegistryNamespace,
                         f.built.manifest.shared_bundle_id,7,{1,1024},&error),"mixed-budget reference open");
    bool validated=false;
    for(unsigned i=0;i<1000000;++i) {
      const auto step=reference.advance_validation(budgets[2]);bounded(step.cost,budgets[2]);
      if(step.status==ValidationStepStatus::Ready){validated=true;break;}
      check(step.status==ValidationStepStatus::InProgress,"mixed-budget reference validation");
    }
    check(validated,"mixed-budget reference validation stalled");
    ExactQueryCursor reference_cursor;check(reference.begin_exact_query("same",9,&reference_cursor,&error),"mixed-budget reference query");
    std::vector<RawQueryHit> expected_hits,actual_hits;bool reference_done=false;
    for(unsigned i=0;i<1000000;++i) {
      const auto step=reference.advance_exact_query(&reference_cursor,budgets[2],3);bounded(step.cost,budgets[2]);
      expected_hits.insert(expected_hits.end(),step.batch.hits.begin(),step.batch.hits.end());
      if(step.status==ExactQueryStepStatus::Complete){reference_done=true;break;}
      check(step.status==ExactQueryStepStatus::InProgress||step.status==ExactQueryStepStatus::HitsAvailable,"mixed-budget reference enumeration");
    }
    check(reference_done&&expected_hits.size()==56,"mixed-budget reference lost raw paths");
    ExactQueryCursor query;check(scheme.begin_exact_query("same",9,&query,&error),"mixed-budget exact query begin");
    const auto query_loads_before=mr->snapshot().authenticated_loads;
    tick=0;bool complete=false;pending=0;
    for(unsigned i=0;i<1000000;++i) {
      const auto budget=budgets[tick++%budgets.size()];
      const auto calls=md->calls+mi->calls,bytes=md->requested+mi->requested;
      const auto step=scheme.advance_exact_query(&query,budget,3);bounded(step.cost,budget);
      check(step.cost.read_calls==md->calls+mi->calls-calls && step.cost.read_bytes==md->requested+mi->requested-bytes,
            "mixed-budget query physical requests were not metered");
      check(step.batch.hits.size()<=3,"mixed-budget query batch grew beyond capacity");
      actual_hits.insert(actual_hits.end(),step.batch.hits.begin(),step.batch.hits.end());
      if(step.status==ExactQueryStepStatus::Complete){complete=true;break;}
      check(step.status==ExactQueryStepStatus::InProgress||step.status==ExactQueryStepStatus::HitsAvailable,"mixed-budget exact query continuation");
      if(step.status==ExactQueryStepStatus::InProgress)++pending;
    }
    check(complete && pending>=3 && actual_hits.size()==expected_hits.size() &&
          mr->snapshot().authenticated_loads>query_loads_before,"mixed-budget query stalled, avoided the cold block, or lost raw paths");
    for(std::size_t i=0;i<expected_hits.size();++i)
      check(actual_hits[i].runtime_ref==expected_hits[i].runtime_ref && actual_hits[i].scheme_weight==expected_hits[i].scheme_weight &&
            actual_hits[i].mapping_ordinal==expected_hits[i].mapping_ordinal,"mixed-budget raw tuple differs from complete-validation reference");
  }
  std::cout<<"PASS V5 public fast owners, zero-I/O admission, fixed/alternating budgets, raw coverage, same-epoch reactivation and old cleanup\n";
}
} // namespace
int main(int argc,char** argv) {
  try {
    check(argc==2,"usage: hanime_publisher_tests SCRATCH_PARENT");fs::create_directories(argv[1]);
    const auto suffix=std::chrono::steady_clock::now().time_since_epoch().count();
    Fixture fixture(fs::path(argv[1])/("publisher_"+std::to_string(suffix)));
    successes(fixture);shared_semantics(fixture);scheme_semantics(fixture);binding_capacity(fixture);fast_owners(fixture);
    std::cout<<"PUBLISHER 5/5 groups passed\n";
    return 0;
  } catch(const std::exception& error){std::cerr<<"FAIL publisher: "<<error.what()<<'\n';return 1;}
}
