#include "hanime/paged_shared_resource.hpp"
#include "hanime/small_fixture.hpp"
#include "hanime/file_user_state_persistence.hpp"
#include "streaming_host_driver.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
using namespace hanime;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
class Reader final : public ResourceReader {
 public:
  explicit Reader(std::vector<std::uint8_t> b) : bytes(std::move(b)) {}
  std::size_t size() const override { return bytes.size(); }
  bool read(std::size_t o, void* p, std::size_t n) const override {
    ++calls; requested += n; largest = std::max(largest, n);
    if (fail || o > bytes.size() || n > bytes.size() - o) return false;
    std::memcpy(p, bytes.data() + o, n); return true;
  }
  std::vector<std::uint8_t> bytes;
  mutable std::size_t calls{0}, requested{0}, largest{0}; bool fail{false};
};
std::uint32_t get32(const std::vector<std::uint8_t>& b, std::size_t o) {
  return std::uint32_t(b.at(o)) | (std::uint32_t(b.at(o+1))<<8) |
      (std::uint32_t(b.at(o+2))<<16) | (std::uint32_t(b.at(o+3))<<24);
}
std::uint64_t get64(const std::vector<std::uint8_t>& b, std::size_t o) { return get32(b,o) | (std::uint64_t(get32(b,o+4))<<32); }
void put32(std::vector<std::uint8_t>& b, std::size_t o, std::uint32_t v) {
  for (unsigned i=0;i<4;++i) b.at(o+i)=static_cast<std::uint8_t>(v>>(8U*i));
}
void put64(std::vector<std::uint8_t>& b, std::size_t o, std::uint64_t v) {
  put32(b,o,static_cast<std::uint32_t>(v));put32(b,o+4,static_cast<std::uint32_t>(v>>32));
}
std::size_t address(const std::vector<std::uint8_t>& b, std::size_t section, std::uint32_t index) {
  const auto d=64+24*section; const auto page=get32(b,56), width=get32(b,d+12), per=(page-16)/width;
  return static_cast<std::size_t>(get64(b,d)) + std::size_t(index/per)*page + 16+(index%per)*width;
}
void rehash(std::vector<std::uint8_t>& b) {
  constexpr std::uint64_t prime=1099511628211ULL;
  std::uint64_t hash=14695981039346656037ULL, id=hash;
  for (std::size_t i=8;i<16;++i) id=(id^b[i])*prime;
  for (std::size_t i=40;i<b.size();++i) {hash=(hash^b[i])*prime; id=(id^b[i])*prime;}
  put64(b,16,id);put64(b,24,hash);
}
void sum(WorkCost& a,const WorkCost& b) {a.read_calls+=b.read_calls;a.read_bytes+=b.read_bytes;a.work_items+=b.work_items;}
void check_cost(const WorkCost& c, WorkBudget b, const Reader& r, std::size_t calls, std::size_t bytes) {
  require(c.read_calls<=b.max_read_calls && c.read_bytes<=b.max_read_bytes && c.work_items<=b.max_work_items,"budget exceeded");
  require(c.read_calls==r.calls-calls && c.read_bytes==r.requested-bytes,"unmetered shared I/O");
}
WorkCost validate(PagedSharedResource& shared, const Reader& reader, WorkBudget b={2,73,19}) {
  WorkCost total;
  const auto ticket=shared.validation_cursor();
  for (std::size_t guard=0;guard<5'000'000;++guard) {
    const auto calls=reader.calls, bytes=reader.requested;
    const auto step=shared.advance_validation(ticket,b);check_cost(step.cost,b,reader,calls,bytes);sum(total,step.cost);
    if(step.status==ValidationStepStatus::Ready)return total;
    require(step.status==ValidationStepStatus::InProgress,"validation: "+step.error);
  }
  throw std::runtime_error("validation did not make progress");
}
std::shared_ptr<Reader> open(PagedSharedResource& s,const BuiltResources& built,
    std::size_t cache=1,std::size_t single=17,MountEpoch epoch=9) {
  auto r=std::make_shared<Reader>(built.shared_bundle);std::string error;
  require(s.open(r,built.manifest.registry_namespace,built.manifest.shared_bundle_id,epoch,{cache,single},&error),error);
  require(!s.ready() && r->requested==200 && r->largest<=single,"open hid a table scan");
  return r;
}
SharedReadResult read(PagedSharedResource& s,const Reader& reader,SharedRecordKind kind,std::uint32_t ref,
                       WorkBudget b={1,7,1},WorkCost* total=nullptr) {
  SharedReadCursor cursor;
  require(s.begin_read(kind,ref,&cursor)==SharedReadStatus::InProgress,"begin read failed");
  for(std::size_t guard=0;guard<100000;++guard) {
    const auto calls=reader.calls,bytes=reader.requested;auto step=s.advance_read(&cursor,b);
    check_cost(step.cost,b,reader,calls,bytes);if(total)sum(*total,step.cost);
    if(step.status==SharedReadStatus::Ready){require(s.result_is_current(step),"result not current");return step;}
    require(step.status==SharedReadStatus::InProgress,"read failed: "+step.error);
  }
  throw std::runtime_error("read did not make progress");
}
BuiltResources build(ResourceBuildInput input=fixture::make_small_input(false),std::uint32_t page=96,
                     std::uint32_t scheme_page=96) {
  ResourceBuildOptions options;options.shared_page_bytes=page;options.scheme_page_bytes=scheme_page;
  auto result=ResourceBuilder().build(input,options);require(result.ok,"build: "+result.error);return std::move(result.resources);
}

void keys_and_reorder() {
  auto a=fixture::kSameTextA,b=fixture::kSameTextB;
  require(std::equal(a.bytes.begin(),a.bytes.begin()+8,b.bytes.begin()) && a!=b,"fixture lacks equal-prefix keys");
  StableLexemeKey parsed;
  require(parse_slk1(format_slk1(a),&parsed) && parsed==a,"key round trip");
  require(!parse_slk1(format_slk1(a).substr(0,68),&parsed) && parsed==a,"invalid key overwrote output");
  auto uppercase=format_slk1(a);std::transform(uppercase.begin()+5,uppercase.end(),uppercase.begin()+5,[](char c){return c>='a'&&c<='f'?char(c-'a'+'A'):c;});
  require(parse_slk1(uppercase,&parsed)&&parsed==a,"hex case changed identity");
  auto input=fixture::make_small_input(false);input.lexemes[1].stable_key=input.lexemes[0].stable_key;
  require(!ResourceBuilder().build(input).ok,"builder admitted duplicate full key");
  const auto ba=build(),bb=build(fixture::make_small_input(true));
  require(ba.manifest.shared_bundle_id!=bb.manifest.shared_bundle_id,"physical reorder lost binding");
  PagedSharedResource sa,sb;auto ra=open(sa,ba),rb=open(sb,bb);validate(sa,*ra);validate(sb,*rb);
  for(std::uint32_t i=1;i<=7;++i){const auto x=read(sa,*ra,SharedRecordKind::Lexeme,i),y=read(sb,*rb,SharedRecordKind::Lexeme,8-i);
    require(x.lexeme.identity.stable_key==y.lexeme.identity.stable_key,"reorder changed full key");}
  const auto x=read(sa,*ra,SharedRecordKind::Lexeme,4),y=read(sa,*ra,SharedRecordKind::Lexeme,5);
  require(x.lexeme.default_payload==y.lexeme.default_payload && x.lexeme.identity.stable_key!=y.lexeme.identity.stable_key,"payload sharing merged identity");
}
void pages_and_budget() {
  for(auto page:{68U,96U,256U})for(auto cache:{1U,3U}) {
    const auto built=build(fixture::make_small_input(false),page);PagedSharedResource shared;auto reader=open(shared,built,cache,1);
    SharedReadCursor c;require(shared.begin_read(SharedRecordKind::Lexeme,1,&c)==SharedReadStatus::NotReady,"unvalidated resource active");
    const auto calls=reader->calls;require(shared.advance_validation(shared.validation_cursor(),{0,1,1}).status==ValidationStepStatus::InvalidBudget&&reader->calls==calls,"invalid validation budget did work");
    validate(shared,*reader,{1,1,1});
    const auto input=fixture::make_small_input(false);
    for(std::uint32_t i=1;i<=7;++i) {
      const auto got=read(shared,*reader,SharedRecordKind::Lexeme,i,{1,1,1});
      require(got.lexeme.identity.stable_key==input.lexemes[i-1].stable_key &&
          got.lexeme.identity.kind==input.lexemes[i-1].kind && got.lexeme.identity.base_weight==input.lexemes[i-1].base_weight &&
          got.string_descriptor.length==input.lexemes[i-1].default_output_utf8.size(),"metadata mismatch");
    }
    for(std::uint32_t i=0;i<shared.info().sections[1].count;++i)read(shared,*reader,SharedRecordKind::Relation,i,{1,1,1});
    for(std::uint32_t i=0;i<shared.info().sections[2].count;++i)read(shared,*reader,SharedRecordKind::Selector,i,{1,1,1});
    for(std::uint32_t i=0;i<shared.info().sections[3].count;++i)read(shared,*reader,SharedRecordKind::StringDescriptor,i,{1,1,1});
    require(shared.begin_read(SharedRecordKind::Lexeme,0,&c)==SharedReadStatus::InvalidReference &&
        shared.begin_read(SharedRecordKind::Lexeme,8,&c)==SharedReadStatus::InvalidReference &&
        shared.begin_read(SharedRecordKind::Relation,0xffffffffU,&c)==SharedReadStatus::InvalidReference,"invalid refs accepted");
    shared.begin_read(SharedRecordKind::Lexeme,1,&c);
    require(shared.advance_read(&c,{1,0,1}).status==SharedReadStatus::InvalidBudget,"invalid read budget accepted");
    const auto m=shared.memory_usage();require(m.cache_payload_allocated_bytes==cache*page && m.valid_cached_pages<=cache,"cache exceeded allocation bound");
  }
  PagedSharedResource broken;const auto built=build();auto reader=open(broken,built);reader->fail=true;
  require(broken.advance_validation(broken.validation_cursor(),{1,10,10}).status==ValidationStepStatus::Error && !broken.ready(),"read failure activated package");
  require(broken.memory_usage().cache_payload_allocated_bytes==0,"failed validation retained cache");
  PagedSharedResource runtime_failure;auto failing=open(runtime_failure,built);validate(runtime_failure,*failing);
  SharedReadCursor active;runtime_failure.begin_read(SharedRecordKind::Lexeme,1,&active);failing->fail=true;
  require(runtime_failure.advance_read(&active,{1,17,1}).status==SharedReadStatus::Error &&
          runtime_failure.ready(),"runtime I/O failure invalidated immutable activation");
  require(runtime_failure.advance_read(&active,{1,17,1}).status==SharedReadStatus::Stale,"failed operation auto-retried");
  failing->fail=false;
  read(runtime_failure,*failing,SharedRecordKind::Lexeme,1,{1,17,1});
}
void rejects_bad_package() {
  const auto built=build();
  using Mutation=std::function<void(std::vector<std::uint8_t>&)>;
  const std::vector<std::pair<std::string,Mutation>> cases={
    {"duplicate-real-full-key",[](auto& b){auto p=address(b,0,0),q=address(b,0,1);std::copy_n(b.begin()+p,32,b.begin()+q);}},
    {"duplicate-aux-reference",[](auto& b){put32(b,address(b,4,0),get32(b,address(b,4,1)));}},
    {"aux-out-of-range",[](auto& b){put32(b,address(b,4,0),8);}},
    {"aux-missing-item",[](auto& b){put32(b,64+24*4+8,6);}},
    {"aux-order",[](auto& b){put32(b,address(b,4,0),7);put32(b,address(b,4,6),1);}},
    {"wrong-key-width",[](auto& b){put32(b,52,8);}},
    {"wrong-key-format",[](auto& b){put32(b,48,9);}},
    {"namespace",[](auto& b){put64(b,8,77);}},
    {"section-overlap",[](auto& b){put64(b,64+24,200);}},
    {"default-span-ref",[](auto& b){put32(b,address(b,0,0)+36,0xffffffffU);}},
    {"relation-owner-range",[](auto& b){put32(b,address(b,0,1)+40,0);}},
    {"condition-kind",[](auto& b){b.at(address(b,1,0)+12)=9;}},
    {"missing-condition",[](auto& b){b.at(address(b,1,0)+12)=1;}},
    {"bad-permission",[](auto& b){b.at(address(b,1,0)+13)=8;}},
    {"bad-admission",[](auto& b){b.at(address(b,1,0)+14)=2;}},
    {"ready-without-target",[](auto& b){put32(b,address(b,1,0)+4,0xffffffffU);}},
    {"selector-reference",[](auto& b){put32(b,address(b,2,0),0xffffffffU);}},
    {"string-range",[](auto& b){put32(b,address(b,3,0)+4,0xffffffffU);}},
    {"utf8-invalid",[](auto& b){b.at(static_cast<std::size_t>(get64(b,184)))=0xf5;}},
    {"page-ordinal",[](auto& b){put32(b,208,9);}},
  };
  for(const auto& [name,change]:cases) {
    auto bytes=built.shared_bundle;change(bytes);rehash(bytes);
    auto reader=std::make_shared<Reader>(std::move(bytes));PagedSharedResource shared;std::string error;
    if(shared.open(reader,fixture::kRegistryNamespace,0,9,{1,3},&error)) {
      ValidationStepResult step;
      for(std::size_t guard=0;guard<100000;++guard){auto calls=reader->calls,n=reader->requested;step=shared.advance_validation(shared.validation_cursor(),{1,3,2});check_cost(step.cost,{1,3,2},*reader,calls,n);if(step.status!=ValidationStepStatus::InProgress)break;}
      require(step.status==ValidationStepStatus::Error,name+" was not rejected");
      require(step.error.find("checksum")==std::string::npos,name+" only checked checksum");
    }
    require(!shared.ready(),name+" activated");SharedReadCursor cursor;
    require(shared.begin_read(SharedRecordKind::Lexeme,1,&cursor)==SharedReadStatus::NotReady,name+" allowed read");
  }
  for(auto truncated:{true,false}) {
    auto bytes=built.shared_bundle;if(truncated)bytes.pop_back();else bytes.back()^=1;
    auto reader=std::make_shared<Reader>(bytes);PagedSharedResource shared;std::string error;
    if(shared.open(reader,fixture::kRegistryNamespace,0,9,{1,17},&error)) {
      bool failed=false;for(unsigned i=0;i<100000;++i){auto step=shared.advance_validation(shared.validation_cursor(),{2,256,256});if(step.status==ValidationStepStatus::Error){failed=true;break;}require(step.status!=ValidationStepStatus::Ready,"corruption activated");}require(failed,"corruption did not fail");
    }
  }
  PagedSharedResource mismatched;std::string error;
  require(!mismatched.open(std::make_shared<Reader>(built.shared_bundle),fixture::kRegistryNamespace,built.manifest.shared_bundle_id^1,9,{1,17},&error),"bundle binding mismatch admitted");
  std::cout<<"{\"event\":\"SHARED_BAD_PACKAGES\",\"rehash_semantic_cases\":"<<cases.size()<<",\"truncation_checksum_binding_cases\":3}\n";
}
SharedReadResult finish(PagedSharedResource& s,SharedReadCursor& cursor) {
  for(unsigned i=0;i<10000;++i){auto r=s.advance_read(&cursor,{1,3,1});if(r.status==SharedReadStatus::Ready)return r;require(r.status==SharedReadStatus::InProgress,"cursor lost progress");}
  throw std::runtime_error("cursor stalled");
}
void lifecycle() {
  const auto built=build();PagedSharedResource a,b;auto ra=open(a,built),rb=open(b,built);
  const auto va=a.validation_cursor(),vb=b.validation_cursor();const auto before=rb->calls;
  require(b.advance_validation(va,{1,1,1}).status==ValidationStepStatus::Stale&&rb->calls==before,"foreign validation advanced target");
  b.cancel_validation(va);validate(a,*ra);validate(b,*rb);b.cancel_validation(vb);
  require(b.ready(),"finished validation cancel deactivated ready queries");
  SharedReadCursor old,current;a.begin_read(SharedRecordKind::Lexeme,1,&old);auto old_result=finish(a,old);
  b.begin_read(SharedRecordKind::Lexeme,2,&current);
  require(!b.result_is_current(old_result)&&b.advance_read(&old,{1,1,1}).status==SharedReadStatus::Stale,"foreign work accepted");
  b.cancel_read(&old);require(a.advance_read(&old,{1,100,10}).status==SharedReadStatus::Ready,"foreign cancel changed original owner cursor");
  require(finish(b,current).lexeme.identity.stable_key==fixture::kDefaultOnly,"foreign cancel damaged new work");
  a.deactivate();require(!b.result_is_current(old_result),"deactivated identity resurrected");
  b.cancel_read(&current);require(b.advance_read(&current,{1,1,1}).status==SharedReadStatus::Cancelled,"active cancel failed");
  SharedReadCursor newer;b.begin_read(SharedRecordKind::Lexeme,3,&newer);b.cancel_read(&current);finish(b,newer);
  auto before_reopen=finish(b,newer);rb=open(b,built,1,17,9);
  require(b.advance_validation(vb,{1,1,1}).status==ValidationStepStatus::Stale,"reopen accepted old validation");
  b.cancel_validation(vb);validate(b,*rb);
  b.begin_read(SharedRecordKind::Lexeme,4,&current);b.cancel_read(&newer);
  require(!b.result_is_current(before_reopen)&&b.advance_read(&newer,{1,1,1}).status==SharedReadStatus::Stale,"reopen reused identity");finish(b,current);
  alignas(PagedSharedResource) unsigned char storage[sizeof(PagedSharedResource)];
  auto* placed=new(storage)PagedSharedResource;auto r=open(*placed,built);validate(*placed,*r);
  const auto saved_validation=placed->validation_cursor();
  SharedReadCursor saved;placed->begin_read(SharedRecordKind::Lexeme,1,&saved);auto saved_result=finish(*placed,saved);placed->~PagedSharedResource();
  placed=new(storage)PagedSharedResource;r=open(*placed,built);validate(*placed,*r);
  require(placed->advance_validation(saved_validation,{1,1,1}).status==ValidationStepStatus::Stale,"same-address validation revived");
  SharedReadCursor fresh;placed->begin_read(SharedRecordKind::Lexeme,7,&fresh);
  require(!placed->result_is_current(saved_result),"same-address identity reused");placed->cancel_read(&saved);finish(*placed,fresh);placed->~PagedSharedResource();
  PagedSharedResource changed;auto rc=open(changed,built,1,17,10);validate(changed,*rc);
  require(!changed.result_is_current(saved_result),"interleaved mount accepted old result");
  PagedSharedResource cancelled;auto cancelled_reader=open(cancelled,built);const auto ticket=cancelled.validation_cursor();
  cancelled.cancel_validation(ticket);require(!cancelled.ready()&&cancelled.memory_usage().cache_payload_allocated_bytes==0&&
      cancelled.advance_validation(ticket,{1,1,1}).status==ValidationStepStatus::Stale,"active validation cancel failed");
}

ResourceBuildInput semantics() {
  auto input=fixture::make_small_input(false);input.lexemes.resize(7);
  input.source_name="explicit-semantic-test-not-real-import";
  const std::vector<std::string> text={"机","发","后","才","机器","未就绪","𨿍"};
  for(std::size_t i=0;i<7;++i){auto& l=input.lexemes[i];l.default_output_utf8=text[i];l.output_relations.clear();l.kind=i==4||i==5?LexemeKind::Word:LexemeKind::Character;}
  input.lexemes[0].output_relations={{"profile","機","","nonempty source explanation is not a predicate",false}};
  input.lexemes[1].output_relations={{"profile","發","non-hair sense","R005 semantic fixture",false,ConditionKind::WordOrSense},{"profile","髮","hair sense","R006 semantic fixture",false,ConditionKind::WordOrSense}};
  input.lexemes[2].output_relations={{"profile","后","empress/spouse/surname","R011 semantic fixture",false,ConditionKind::WordOrSense}};
  input.lexemes[3].output_relations={{"expanded","纔","only/just","R036 semantic fixture",false,ConditionKind::WordOrSense,6}};
  input.lexemes[4].output_relations={{"profile","機器","","word attachment fixture",true}};
  input.lexemes[5].output_relations={{"profile","","","identity approval pending fixture",false,ConditionKind::None,3,OutputAdmission::Blocked}};
  return input;
}
void outputs() {
  const auto input=semantics();const auto built=build(input,68);PagedSharedResource paged;auto reader=open(paged,built,1,1);validate(paged,*reader,{1,1,1});
  std::uint32_t relation=0;
  for(const auto& l:input.lexemes)for(const auto& expected:l.output_relations){const auto r=read(paged,*reader,SharedRecordKind::Relation,relation++);require(r.relation.condition_kind==expected.condition_kind&&r.relation.availability==expected.availability&&r.relation.admission==expected.admission,"typed relation roundtrip");}
  const auto supplement=read(paged,*reader,SharedRecordKind::Lexeme,7);
  require(supplement.string_descriptor.length==4,"supplementary char split");
  std::vector<std::shared_ptr<const ResourceReader>> schemes;
  for(const auto& entry:built.scheme_bundles)schemes.push_back(std::make_shared<Reader>(entry.second));
  MountedResourceSet host;std::string error;require(host.mount(reader,schemes,&error),error);
  require(host.resolve_output(1,"profile").text_utf8=="機","R002 source note created condition");
  require(host.resolve_output(2,"profile").status==OutputResolutionStatus::MultipleFormsUnsupported,"R005/6 silently selected");
  require(host.resolve_output(3,"profile").status==OutputResolutionStatus::ConditionUndetermined&&host.resolve_output(3,"default").text_utf8=="后","R011 blocked source preserve");
  require(host.resolve_output(4,"expanded").status==OutputResolutionStatus::NotAvailable&&host.resolve_output(4,"expanded",OutputInvocation::Expanded).status==OutputResolutionStatus::ConditionUndetermined,"R036 permission satisfied condition");
  require(host.resolve_output(5,"profile").text_utf8=="機器","word output changed");
  require(host.resolve_output(6,"profile").status==OutputResolutionStatus::OutputNotReady&&host.resolve_output(6,"missing").status==OutputResolutionStatus::FallbackDefault,"Blocked became absent fallback");
}

std::string contents(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
struct Presenter final:CandidatePresenter{bool present(std::uint64_t,const std::vector<CandidateView>&)override{return true;}};
struct Sink final:TextCommitSink{unsigned calls{0};bool commit(std::string_view)override{++calls;return true;}};
void state_compatibility(const std::filesystem::path& dir) {
  std::filesystem::create_directories(dir);const auto path=dir/"incompatible_state.txt";
  const auto built=build();std::vector<std::shared_ptr<const ResourceReader>> schemes;
  for(const auto& s:built.scheme_bundles)schemes.push_back(std::make_shared<Reader>(s.second));
  const std::vector<std::string> files={"HANIME_USER_STATE_V1\n123 4097 9\n",
    "HANIME_USER_STATE_V2 1 SLK1_256\n",
    "HANIME_USER_STATE_V2 "+std::to_string(fixture::kRegistryNamespace)+" WRONG_FORMAT\n"};
  for(const auto& bytes:files) {
    {std::ofstream f(path,std::ios::binary|std::ios::trunc);f<<bytes;}
    FileUserStatePersistence persistence(path);std::uint32_t value=123;
    require(persistence.load(fixture::kRegistryNamespace,fixture::kPrototype,&value)==PersistenceLoadResult::Incompatible&&value==123,"incompatible load became empty state");
    require(!persistence.write(fixture::kRegistryNamespace,fixture::kPrototype,1)&&contents(path)==bytes,"incompatible file overwritten");
    MountedResourceSet resources;std::string error;require(resources.mount(std::make_shared<Reader>(built.shared_bundle),schemes,&error),error);
    UserStateStore state(fixture::kRegistryNamespace,persistence);Presenter presenter;Sink sink;
    ImeSession session(resources,state,presenter,sink,"full_pinyin");
    require(session.query_complete_code("yuanxingji",&error)&&!session.learning_available(),"learning failure blocked text query");
    const auto c=session.current_candidates().at(0);require(!c.learning_available,"candidate hid learning state");
    require(session.confirm(c.handle)==ConfirmResult::CommittedStateLoadError&&sink.calls==1,"learning failure blocked successful commit");
    require(session.confirm(c.handle)==ConfirmResult::RejectedStale&&sink.calls==1&&contents(path)==bytes,"retry overwrote old file or repeated text");
    require(state.dirty_count()==0&&state.retry_dirty()&&contents(path)==bytes,"unrecorded learning was invented");
  }
  std::cout<<"{\"event\":\"STATE_COMPATIBILITY\",\"preserved_files\":3,\"text_commit_once_each\":true}\n";
}

ResourceBuildInput synthetic(std::size_t n) {
  ResourceBuildInput input;input.registry_namespace=0x5354455032420001ULL;
  input.source_name="synthetic-step2b-no-language-evidence";input.source_version="1";
  for(std::size_t i=0;i<n;++i){LexemeSourceRecord l;l.stable_key.bytes[0]=0x53;
    for(unsigned b=0;b<8;++b)l.stable_key.bytes[31-b]=static_cast<std::uint8_t>((std::uint64_t(i)+1)>>(8U*b));
    l.status=RegistryStatus::Approved;l.identity_definition="synthetic "+std::to_string(i);l.provenance="test registry";
    l.default_output_utf8="词"+std::to_string(i);l.base_weight=static_cast<std::int32_t>(i%17);input.lexemes.push_back(l);
    input.mappings.push_back({"full_pinyin","same",l.stable_key,1});input.mappings.push_back({"xiaohe","sx",l.stable_key,2});}
  input.mappings.push_back({"full_pinyin","same",input.lexemes[0].stable_key,3});return input;
}
void scale_and_driver() {
  for(const auto n:{128U,1280U})for(const auto cache:{1U,4U}) {
    const auto input=synthetic(n);const auto built=build(input,256);
    PagedSharedResource shared;auto reader=open(shared,built,cache,17);const auto validation=validate(shared,*reader,{2,73,19});
    WorkCost access;for(const auto ref:{1U,n,2U,n-1,1U,n}){const auto r=read(shared,*reader,SharedRecordKind::Lexeme,ref,{1,7,1},&access);require(r.lexeme.identity.stable_key==input.lexemes[ref-1].stable_key,"scale access identity mismatch");}
    auto scheme_reader=std::make_shared<Reader>(built.scheme_bundles.at("full_pinyin"));PagedSchemeIndex scheme;std::string error;
    require(scheme.open(scheme_reader,input.registry_namespace,built.manifest.shared_bundle_id,n,{1,17},&error),error);
    while(!scheme.ready()){const auto v=scheme.advance_validation({2,73,19});require(v.status==ValidationStepStatus::Ready||v.status==ValidationStepStatus::InProgress,v.error);}
    std::uint64_t sum_refs=0;
    test::StreamingHostDriver driver(scheme,shared,"same",9,{1,17,3},3,[&](const RawQueryHit& h,const SharedReadResult& r){require(r.lexeme.identity.stable_key==input.lexemes[h.runtime_ref-1].stable_key,"batch identity mismatch");sum_refs+=h.runtime_ref;});
    for(std::size_t ticks=0;!driver.tick();++ticks)require(ticks<2'000'000,"driver stalled");
    require(driver.consumed==n+1&&sum_refs==std::uint64_t(n)*(n+1)/2+1&&driver.maximum_retained_hits<=3,"driver truncated duplicate/raw coverage");
    const auto m=shared.memory_usage();require(m.cache_payload_allocated_bytes==cache*256&&m.valid_cached_pages<=cache,"scale caused unbounded cache");
    std::cout<<"{\"event\":\"STEP2B_SYNTHETIC_COST\",\"N\":"<<n<<",\"cache_pages\":"<<cache
      <<",\"shared_file_bytes\":"<<built.shared_bundle.size()<<",\"aux_file_bytes\":"<<built.manifest.uniqueness_sequence_bytes
      <<",\"open_calls\":"<<shared.info().open_cost.read_calls<<",\"open_bytes\":"<<shared.info().open_cost.read_bytes
      <<",\"validation_calls\":"<<validation.read_calls<<",\"validation_bytes\":"<<validation.read_bytes<<",\"validation_work\":"<<validation.work_items
      <<",\"uniqueness_calls\":"<<shared.info().uniqueness_cost.read_calls<<",\"uniqueness_bytes\":"<<shared.info().uniqueness_cost.read_bytes<<",\"uniqueness_work\":"<<shared.info().uniqueness_cost.work_items
      <<",\"six_access_calls\":"<<access.read_calls<<",\"six_access_bytes\":"<<access.read_bytes<<",\"six_access_work\":"<<access.work_items
      <<",\"driver_hits\":"<<driver.consumed<<",\"driver_calls\":"<<driver.cost.read_calls<<",\"driver_bytes\":"<<driver.cost.read_bytes
      <<",\"cache_payload_allocated\":"<<m.cache_payload_allocated_bytes<<",\"cache_metadata_allocated\":"<<m.cache_metadata_allocated_bytes
      <<",\"owner_fixed_bytes\":"<<m.owner_fixed_bytes<<",\"included_stream_scratch\":"<<m.stream_scratch_bytes
      <<",\"cursor_sizeof\":"<<sizeof(SharedReadCursor)<<",\"result_sizeof\":"<<sizeof(SharedReadResult)
      <<",\"raw_hit_sizeof\":"<<sizeof(RawQueryHit)<<",\"driver_retained_hits_max\":"<<driver.maximum_retained_hits
      <<",\"whole_core_heap_stack_peak\":\"not_measured\"}\n";
  }
}
void driver_activation_binding() {
  enum class Point { None, Before, DuringRead, Callback, BetweenBatches, SameAddress, Deactivate };
  struct Case {
    const char* name;
    Point point;
    bool reverse;
    MountEpoch epoch;
    std::size_t batch_capacity;
  };
  const Case cases[] = {
      {"unchanged-pair", Point::None, false, 9, 3},
      {"different-package-reused-epoch", Point::Before, true, 9, 3},
      {"same-package-same-epoch", Point::Before, false, 9, 3},
      {"different-package-new-epoch", Point::Before, true, 10, 3},
      {"during-shared-read", Point::DuringRead, true, 9, 3},
      {"consumer-callback-next-hit", Point::Callback, true, 9, 3},
      {"between-batches", Point::BetweenBatches, true, 9, 1},
      {"same-address-same-package", Point::SameAddress, false, 9, 3},
      {"deactivated-owner", Point::Deactivate, false, 9, 3},
  };
  const auto a = build(), b = build(fixture::make_small_input(true));
  // Both duplicate mappings fit one leaf for the callback-to-next-hit case.
  // Other cases retain the original 96-byte scheme pages.
  const auto callback_a = build(fixture::make_small_input(false), 96, 256);
  const auto callback_b = build(fixture::make_small_input(true), 96, 256);
  std::size_t failures = 0;
  for (const auto& test_case : cases) {
    try {
      const auto& case_a = test_case.point == Point::Callback ? callback_a : a;
      const auto& case_b = test_case.point == Point::Callback ? callback_b : b;
      alignas(PagedSharedResource) unsigned char storage[sizeof(PagedSharedResource)];
      const auto destroy = [](PagedSharedResource* p) { p->~PagedSharedResource(); };
      std::unique_ptr<PagedSharedResource, decltype(destroy)> shared(
          new (storage) PagedSharedResource(), destroy);
      auto shared_reader = open(*shared, case_a);
      validate(*shared, *shared_reader);
      auto scheme_reader = std::make_shared<Reader>(case_a.scheme_bundles.at("full_pinyin"));
      PagedSchemeIndex scheme;
      std::string error;
      require(scheme.open(scheme_reader, case_a.manifest.registry_namespace,
                          case_a.manifest.shared_bundle_id, 7, {1,17}, &error), error);
      while (!scheme.ready()) {
        const auto step = scheme.advance_validation({2,73,19});
        require(step.status == ValidationStepStatus::InProgress ||
                step.status == ValidationStepStatus::Ready, step.error);
      }
      SharedReadCursor protected_read;
      bool replaced = false;
      const auto replace_shared = [&] {
        if (test_case.point == Point::SameAddress) {
          auto* address = shared.release();
          address->~PagedSharedResource();
          shared.reset(new (address) PagedSharedResource());
        }
        if (test_case.point == Point::Deactivate) {
          shared->deactivate();
        } else {
          shared_reader = open(*shared, test_case.reverse ? case_b : case_a, 1, 17, test_case.epoch);
          validate(*shared, *shared_reader);
          require(shared->begin_read(SharedRecordKind::Lexeme,
                  test_case.reverse ? 7 : 1, &protected_read) == SharedReadStatus::InProgress,
                  "cannot start new activation's protected read");
        }
        replaced = true;
      };
      std::vector<StableLexemeKey> delivered;
      std::size_t last_read_calls = shared_reader->calls;
      // Larger work budget in the callback case guarantees both duplicate hits
      // in one batch; the exact review reproduction uses {1,17,3}.
      const WorkBudget budget{1,17,test_case.point == Point::Callback ? 64U : 3U};
      auto driver = std::make_unique<test::StreamingHostDriver>(
          scheme, *shared, "yuanxingji", 9, budget, test_case.batch_capacity,
          [&](const RawQueryHit&, const SharedReadResult& result) {
            delivered.push_back(result.lexeme.identity.stable_key);
            if (test_case.point == Point::Callback && !replaced) replace_shared();
          });
      if (test_case.point == Point::Before || test_case.point == Point::SameAddress ||
          test_case.point == Point::Deactivate) replace_shared();
      bool done = false;
      std::string rejection;
      for (std::size_t ticks = 0; ticks < 100000 && !done && rejection.empty(); ++ticks) {
        const auto before = driver->cost;
        try { done = driver->tick(); }
        catch (const std::runtime_error& e) { rejection = e.what(); }
        require(driver->cost.read_calls - before.read_calls <= budget.max_read_calls &&
                driver->cost.read_bytes - before.read_bytes <= budget.max_read_bytes &&
                driver->cost.work_items - before.work_items <= budget.max_work_items,
                "driver exceeded one-advance tick budget");
        if (!replaced && test_case.point == Point::DuringRead &&
            shared_reader->calls > last_read_calls) {
          require(delivered.empty(), "read-interruption case fired too late");
          replace_shared();
        }
        if (!replaced && test_case.point == Point::BetweenBatches && delivered.size() == 1)
          replace_shared();
      }
      std::cout << "{\"event\":\"DRIVER_BINDING_CASE\",\"case\":\"" << test_case.name
                << "\",\"delivered\":" << delivered.size() << ",\"rejected\":"
                << (rejection.empty() ? "false" : "true") << ",\"last_key\":\""
                << (delivered.empty() ? "" : format_slk1(delivered.back())) << "\"}\n";
      const bool unchanged = test_case.point == Point::None;
      const std::size_t expected_count = unchanged ? 2 :
          (test_case.point == Point::Callback || test_case.point == Point::BetweenBatches ? 1 : 0);
      require(delivered.size() == expected_count, "wrong number delivered after activation change");
      require(driver->consumed == delivered.size(), "valid consumed result was rolled back");
      require(std::all_of(delivered.begin(), delivered.end(),
                          [](const auto& key) { return key == fixture::kPrototype; }),
              "old short reference interpreted by replacement shared resource");
      require(unchanged ? done && rejection.empty() : replaced && !rejection.empty(),
              "old driver did not reject changed activation");
      require(driver->maximum_retained_hits <= test_case.batch_capacity,
              "driver retained too many hits");
      if (test_case.point == Point::Callback)
        require(driver->maximum_retained_hits == 2, "callback test did not cover same-batch handoff");
      if (!unchanged) {
        const auto shared_calls = shared_reader->calls, scheme_calls = scheme_reader->calls;
        try { driver->tick(); } catch (const std::runtime_error&) {}
        driver.reset();  // Finishing old work must not cancel the new owner's work.
        require(shared_reader->calls == shared_calls && scheme_reader->calls == scheme_calls,
                "stale driver performed resource I/O");
        if (test_case.point != Point::Deactivate) {
          SharedReadResult result;
          for (std::size_t ticks = 0; ticks < 100000; ++ticks) {
            result = shared->advance_read(&protected_read, {1,17,3});
            if (result.status != SharedReadStatus::InProgress) break;
          }
          require(shared->result_is_current(result) &&
                  result.lexeme.identity.stable_key == fixture::kPrototype,
                  "old driver damaged new activation's protected read");
          const auto& current = test_case.reverse ? case_b : case_a;
          auto new_scheme_reader = std::make_shared<Reader>(current.scheme_bundles.at("full_pinyin"));
          PagedSchemeIndex new_scheme;
          require(new_scheme.open(new_scheme_reader, current.manifest.registry_namespace,
                  current.manifest.shared_bundle_id, 7, {1,17}, &error), error);
          while (!new_scheme.ready()) {
            const auto step = new_scheme.advance_validation({2,73,19});
            require(step.status == ValidationStepStatus::InProgress ||
                    step.status == ValidationStepStatus::Ready, step.error);
          }
          std::size_t received = 0;
          test::StreamingHostDriver fresh(new_scheme, *shared, "yuanxingji",
              test_case.epoch, {1,17,3}, 3, [&](const auto&, const auto& value) {
                require(value.lexeme.identity.stable_key == fixture::kPrototype, "fresh driver wrong key");
                ++received;
              });
          bool complete = false;
          for (std::size_t ticks = 0; ticks < 100000 && !complete; ++ticks) complete = fresh.tick();
          require(complete && received == 2, "fresh activation lost duplicate paths");
        }
      }
    } catch (const std::exception& e) {
      ++failures;
      std::cout << "{\"event\":\"DRIVER_BINDING_FAILURE\",\"case\":\"" << test_case.name
                << "\",\"message\":\"" << e.what() << "\"}\n";
    }
  }
  // Read-only handle probes must not use a sacrificial read/validation operation
  // or invalidate a real operation on either resource owner.
  PagedSharedResource shared;
  require(!shared.activation_is_current(SharedActivationHandle{}), "default activation is current");
  auto reader = open(shared, a);
  const auto not_ready_handle = shared.activation_handle();
  require(!shared.activation_is_current(not_ready_handle), "unvalidated activation is current");
  validate(shared, *reader);
  const auto handle = shared.activation_handle();
  SharedReadCursor protected_read;
  require(shared.begin_read(SharedRecordKind::Lexeme, 1, &protected_read) ==
          SharedReadStatus::InProgress, "handle probe cannot start protected read");
  const auto calls = reader->calls, bytes = reader->requested;
  require(shared.activation_is_current(handle) &&
          shared.activation_is_current(shared.activation_handle()) &&
          !shared.activation_is_current(not_ready_handle) &&
          handle.mount_epoch() == 9 &&
          handle.registry_namespace() == a.manifest.registry_namespace &&
          handle.shared_bundle_id() == a.manifest.shared_bundle_id,
          "ready activation snapshot fields mismatch");
  require(reader->calls == calls && reader->requested == bytes, "activation probe performed I/O");
  SharedReadResult protected_result;
  do { protected_result = shared.advance_read(&protected_read, {1,17,3}); }
  while (protected_result.status == SharedReadStatus::InProgress);
  require(shared.result_is_current(protected_result), "activation probe invalidated read");

  auto scheme_reader = std::make_shared<Reader>(a.scheme_bundles.at("full_pinyin"));
  PagedSchemeIndex scheme;
  std::string error;
  require(scheme.open(scheme_reader, a.manifest.registry_namespace,
          a.manifest.shared_bundle_id, 7, {1,17}, &error), error);
  while (!scheme.ready()) {
    const auto step = scheme.advance_validation({2,73,19});
    require(step.status == ValidationStepStatus::InProgress ||
            step.status == ValidationStepStatus::Ready, step.error);
  }
  ExactQueryCursor protected_query;
  require(scheme.begin_exact_query("yuanxingji", 9, &protected_query, &error), error);
  bool rejected = false;
  const auto scheme_calls = scheme_reader->calls, shared_calls = reader->calls;
  try {
    test::StreamingHostDriver mismatch(scheme, shared, "yuanxingji", 10, {1,17,3}, 3,
                                      [](const auto&, const auto&) {});
  } catch (const std::runtime_error&) { rejected = true; }
  require(rejected && scheme_reader->calls == scheme_calls && reader->calls == shared_calls &&
          shared.result_is_current(protected_result), "constructor mismatch modified resource work");
  std::size_t hits = 0;
  for (;;) {
    const auto step = scheme.advance_exact_query(&protected_query, {1,17,3}, 3);
    if (step.status == ExactQueryStepStatus::Complete) break;
    require(step.status == ExactQueryStepStatus::InProgress ||
            step.status == ExactQueryStepStatus::HitsAvailable, "constructor mismatch changed query");
    hits += step.batch.hits.size();
  }
  require(hits == 2, "constructor mismatch lost protected query paths");
  std::cout << "{\"event\":\"DRIVER_BINDING_SUMMARY\",\"cases\":9,\"failed\":" << failures
            << ",\"readonly_handle_io\":0,\"constructor_epoch_mismatch_rejected\":true"
            << ",\"activation_handle_sizeof\":" << sizeof(SharedActivationHandle)
            << ",\"driver_sizeof\":" << sizeof(test::StreamingHostDriver) << "}\n";
  require(failures == 0, "driver activation binding regression");
}
}  // namespace
int main(int argc,char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--binding-only") {
    try { driver_activation_binding(); return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  const std::vector<std::pair<std::string,std::function<void()>>> tests={
    {"B1-full-keys-reorder",keys_and_reorder},{"B2-pages-budget",pages_and_budget},
    {"B3-format-uniqueness",rejects_bad_package},{"B4-lifecycle",lifecycle},
    {"B5-output-semantics",outputs},{"B6-state-compatibility",[&]{require(argc==2,"test requires private scratch directory");state_compatibility(argv[1]);}},
    {"B7-synthetic-streaming",scale_and_driver},{"B8-driver-activation-binding",driver_activation_binding}};
  for(const auto& [name,test]:tests)try{test();std::cout<<"{\"test\":\""<<name<<"\",\"status\":\"PASS\"}\n";}
    catch(const std::exception& e){std::cerr<<"{\"test\":\""<<name<<"\",\"status\":\"FAIL\",\"message\":\""<<e.what()<<"\"}\n";return 1;}
  std::cout<<"{\"event\":\"STEP2B_SUMMARY\",\"passed\":8,\"total\":8}\n";return 0;
}
