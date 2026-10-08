#include "hanime/verified_resource.hpp"
#include "hanime/exact_ranker.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace hanime;
constexpr auto block_bytes=kVerifiedBlockBytes;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
std::string hex(const Sha256Digest& d){std::string s;for(auto x:d){s+="0123456789abcdef"[x>>4];s+="0123456789abcdef"[x&15];}return s;}
struct Reader final:ResourceReader {
  std::vector<std::uint8_t> bytes;mutable std::size_t calls=0,total=0;bool live=true,short_read=false;
  explicit Reader(std::vector<std::uint8_t> x):bytes(std::move(x)){}
  std::size_t size()const override{return bytes.size();}
  bool healthy()const override{return live;}
  bool read(std::size_t o,void* p,std::size_t n)const override {
    ++calls;total+=n;if(o>bytes.size()||n>bytes.size()-o)return false;
    if(n)std::memcpy(p,bytes.data()+o,short_read?n/2:n);
    return !short_read;
  }
};
struct Fixture {
  std::shared_ptr<Reader> data,index;ResourceReleaseSpec spec;
  static std::vector<std::uint8_t> synthetic_bytes() {
    std::vector<std::uint8_t> bytes(40*block_bytes+7);for(std::size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<std::uint8_t>(i*17+i/251);
    return bytes;
  }
  Fixture():Fixture(synthetic_bytes(),ResourceReleaseSpec{}){}
  Fixture(std::vector<std::uint8_t> bytes,ResourceReleaseSpec release):spec(release){
    data=std::make_shared<Reader>(std::move(bytes));spec.file_bytes=data->size();
    if(!spec.registry_namespace){spec.registry_namespace=1;spec.shared_bundle_id=2;spec.runtime_ref_count=3;}
    spec.file_sha256=sha256(data->bytes.data(),data->bytes.size());MerkleLayout l;check(merkle_layout(spec.file_bytes,&l),"layout");
    std::vector<std::uint8_t> idx(static_cast<std::size_t>(l.index_bytes));std::vector<Sha256Digest> children;
    for(std::size_t i=0;i<l.data_blocks;++i)children.push_back(merkle_leaf_hash(i,data->bytes.data()+i*block_bytes,std::min<std::size_t>(block_bytes,data->size()-i*block_bytes)));
    for(std::uint32_t level=0;level<l.levels;++level){
      std::vector<Sha256Digest> next;
      for(std::uint32_t page=0;page<l.page_counts[level];++page){auto* p=idx.data()+l.offsets[level]+page*1024;
        for(std::size_t i=0;i<32&&page*32+i<children.size();++i)std::memcpy(p+i*32,children[page*32+i].data(),32);
        next.push_back(merkle_page_hash(level,page,p));
      }children=std::move(next);
    }spec.merkle_root=children.at(0);index=std::make_shared<Reader>(std::move(idx));
  }
};
ResourceReadContext operation() {return {std::make_shared<const int>(0),1};}
ResourceReadStep finish(VerifiedResourceReader& r,Fixture& f,std::size_t offset,void* dest,std::size_t n,WorkBudget b={1,1,1},
                        const ResourceReadContext& context=operation()) {
  for(unsigned guard=0;guard<200000;++guard){const auto calls=f.data->calls+f.index->calls,bytes=f.data->total+f.index->total;
    auto s=r.read_budgeted(offset,dest,n,b,context);
    check(s.cost.read_calls<=b.max_read_calls&&s.cost.read_bytes<=b.max_read_bytes&&s.cost.work_items<=b.max_work_items,"budget");
    check(s.cost.read_calls==f.data->calls+f.index->calls-calls&&s.cost.read_bytes==f.data->total+f.index->total-bytes,"actual cost");
    if(s.status!=ResourceReadStatus::Pending)return s;
  }throw std::runtime_error("stalled");
}
// An instrumented read port exposes ownership, not timing or hidden I/O. It
// lets the ranker reach a real Pending scheme read without a state source.
struct QueuedReader final:ResourceReader {
  Reader raw;mutable ResourceReadContext active;mutable std::size_t released{0};bool suspend=true;
  explicit QueuedReader(std::vector<std::uint8_t> bytes):raw(std::move(bytes)){}
  std::size_t size()const override{return raw.size();}
  bool read(std::size_t o,void* p,std::size_t n)const override{return raw.read(o,p,n);}
  ResourceReadStep read_budgeted(std::size_t o,void* p,std::size_t n,const WorkBudget& b,
                                 const ResourceReadContext& c)const override {
    if(suspend){active=c;return {ResourceReadStatus::Pending,{0,0,b.max_work_items?1U:0U}};}
    const auto s=ResourceReader::read_budgeted(o,p,n,b,c);
    if(s.status!=ResourceReadStatus::Pending)active={};
    return s;
  }
  void cancel_read_budgeted(const ResourceReadContext& c)const override {
    if(c.owner && active.owner==c.owner && active.revision==c.revision){active={};++released;}
  }
};
struct DisabledCounts final:CountSource {
  StateView view()const override{throw std::runtime_error("BaseOnly read state");}
  bool view_is_current(const StateView&)const override{throw std::runtime_error("BaseOnly checked state");}
  CountTicket begin_count(const StateView&,const StableLexemeKey&)override{throw std::runtime_error("BaseOnly started count");}
  CountResult advance_count(const CountTicket&,const WorkBudget&)override{throw std::runtime_error("BaseOnly advanced count");}
  void cancel_count(const CountTicket&)override{throw std::runtime_error("BaseOnly cancelled nonexistent count");}
};
void cleanup_regression() {
  ResourceBuildInput input;input.registry_namespace=91;input.source_name="synthetic-cleanup";
  input.source_version="v1";input.relation_basis="artificial cleanup identity";
  LexemeSourceRecord record;record.stable_key.bytes.back()=1;record.status=RegistryStatus::Approved;
  record.kind=LexemeKind::Character;record.default_output_utf8="人";
  record.identity_definition="synthetic cleanup key";record.provenance="test only";input.lexemes.push_back(record);
  for(unsigned i=0;i<80;++i)input.mappings.push_back({"test","same",record.stable_key,static_cast<std::int32_t>(i)});
  const auto built=ResourceBuilder().build(input,{96,96});check(built.ok,"cleanup fixture build");
  auto validate_shared=[&](PagedSharedResource& shared) {
    std::string error;
    check(shared.open(std::make_shared<Reader>(built.resources.shared_bundle),91,built.resources.manifest.shared_bundle_id,9,{1,17},&error),"cleanup shared open");
    const auto c=shared.validation_cursor();
    for(unsigned i=0;i<100000;++i){const auto s=shared.advance_validation(c,{1,17,3});
      if(s.status==ValidationStepStatus::Ready)return;
      check(s.status==ValidationStepStatus::InProgress,"cleanup shared validation");}
    throw std::runtime_error("cleanup shared validation stalled");
  };
  auto validate_scheme=[&](PagedSchemeIndex& scheme,std::shared_ptr<const ResourceReader> reader) {
    std::string error;
    check(scheme.open(std::move(reader),91,built.resources.manifest.shared_bundle_id,1,{1,17},&error),"cleanup scheme open");
    for(unsigned i=0;i<100000;++i){const auto s=scheme.advance_validation({1,17,3});
      if(s.status==ValidationStepStatus::Ready)return;
      check(s.status==ValidationStepStatus::InProgress,"cleanup scheme validation");}
    throw std::runtime_error("cleanup scheme validation stalled");
  };
  // Real authenticated owner, including shared reactivation at a reused epoch.
  PagedSharedResource shared;validate_shared(shared);
  PagedSchemeIndex scheme;validate_scheme(scheme,std::make_shared<Reader>(built.resources.scheme_bundles.at("test")));
  ResourceReleaseSpec sp;sp.registry_namespace=91;sp.shared_bundle_id=built.resources.manifest.shared_bundle_id;sp.runtime_ref_count=1;
  Fixture sf(built.resources.shared_bundle,sp);sp=sf.spec;
  auto mp=sp;mp.role=ResourceReleaseRole::Scheme;mp.required_shared_sha256=sp.file_sha256;
  std::memcpy(mp.scheme_id.data(),"test",5);mp.bootstrap_root_offset=scheme.info().root_offset;mp.bootstrap_root_bytes=scheme.info().page_bytes;
  Fixture mf(built.resources.scheme_bundles.at("test"),mp);
  auto sr=std::make_shared<VerifiedResourceReader>(sf.data,sf.index,sf.spec);
  auto mr=std::make_shared<VerifiedResourceReader>(mf.data,mf.index,mf.spec);
  auto bootstrap=[](VerifiedResourceReader& reader) {
    check(reader.prepare_bootstrap(),"cleanup bootstrap begin");
    for(unsigned i=0;i<100000;++i){const auto s=reader.advance_bootstrap({1,1024,1024});
      if(s.status==ResourceReadStatus::Ready)return;
      check(s.status==ResourceReadStatus::Pending,"cleanup bootstrap failed");}
    throw std::runtime_error("cleanup bootstrap stalled");
  };
  bootstrap(*sr);bootstrap(*mr);std::string error;
  check(shared.open_verified(sr,9,{1,1024},&error)&&scheme.open_verified(mr,shared,{1,1024},&error),"cleanup fast activation");
  ExactQueryCursor cursor;check(scheme.begin_exact_query("same",9,&cursor,&error),"cleanup query begin");
  const auto context=scheme.query_context(cursor);const auto owners=context.index_identity.use_count();
  {const auto s=scheme.advance_exact_query(&cursor,{1,1024,1},3);check(s.status==ExactQueryStepStatus::InProgress,"cleanup read not pending");}
  check(context.index_identity.use_count()==owners+1,"cleanup pending read did not retain owner");
  auto fresh=std::make_shared<VerifiedResourceReader>(sf.data,sf.index,sf.spec);bootstrap(*fresh);
  check(shared.open_verified(fresh,9,{1,1024},&error),"cleanup shared reactivation");
  {const auto s=scheme.advance_exact_query(&cursor,{1,1024,1},3);check(s.status==ExactQueryStepStatus::Stale,"cleanup old query not stale");}
  scheme.cancel_query(&cursor);
  check(context.index_identity.use_count()==owners,"stale scheme retained authenticated pending read");
  // Ranker cancellation, range handoff, stale terminal, destruction and cleanup
  // after another caller replaces the scheme operation. No learning object.
  for(unsigned scenario=0;scenario<6;++scenario) {
    PagedSharedResource host_shared;validate_shared(host_shared);
    auto queued=std::make_shared<QueuedReader>(built.resources.scheme_bundles.at("test"));
    PagedSchemeIndex host_scheme;validate_scheme(host_scheme,queued);DisabledCounts counts;
    std::array<RankItem,3> workspace{},output{};
    auto ranker=std::make_unique<ExactRanker>(host_scheme,host_shared,counts,workspace.data(),workspace.size(),3);
    check(ranker->begin_input("same",9,{},ScoringMode::BaseOnly)==RankStatus::InProgress,"cleanup rank begin");
    RankTicket ticket;
    check(ranker->request_range(RangeDirection::First,nullptr,3,output.data(),output.size(),&ticket)==RankStatus::InProgress,"cleanup range begin");
    for(unsigned i=0;!queued->active.owner&&i<100;++i)
      check(ranker->advance(ticket,{1,17,3}).status==RankStatus::InProgress,"cleanup rank pending");
    check(bool(queued->active.owner),"ranker never opened its pending read");
    const auto own=queued->active;const auto cancels=queued->released;
    const auto calls=queued->raw.calls;ExactQueryCursor external;
    if(scenario==4 || scenario==5) {
      check(host_scheme.begin_exact_query("same",9,&external,&error),"external query begin");
      {const auto s=host_scheme.advance_exact_query(&external,{1,17,3},3);check(s.status==ExactQueryStepStatus::InProgress,"external read pending");}
      check(queued->active.owner&&queued->active.revision!=own.revision,"external request not replaced");
    }
    const auto before_cleanup=queued->active;const auto before_cancel=queued->released;
    if(scenario==0 || scenario==4)ranker->cancel(ticket);
    else if(scenario==1)check(ranker->abandon_range(ticket),"range handoff rejected");
    else if(scenario==2 || scenario==5) {
      host_shared.deactivate();check(ranker->advance(ticket,{1,17,3}).status==RankStatus::Stale,"rank stale terminal");
    } else ranker.reset();
    check(queued->raw.calls==calls,"cleanup performed resource I/O");
    if(scenario>=4) {
      check(queued->active.owner==before_cleanup.owner&&queued->active.revision==before_cleanup.revision&&
            queued->released==before_cancel,"old rank cleanup cancelled external new read");
      queued->suspend=false;std::size_t hits=0;bool done=false;
      for(unsigned i=0;i<100000;++i){const auto s=host_scheme.advance_exact_query(&external,{1,17,3},3);
        hits+=s.batch.hits.size();if(s.status==ExactQueryStepStatus::Complete){done=true;break;}
        check(s.status==ExactQueryStepStatus::InProgress||s.status==ExactQueryStepStatus::HitsAvailable,"external query harmed");}
      check(done&&hits==80,"external query lost raw mappings");
    } else {
      check(!queued->active.owner&&queued->released==cancels+1,"rank cleanup retained pending scheme read");
      if(ranker){ranker->cancel(ticket);check(queued->released==cancels+1,"rank cleanup not idempotent");}
      if(scenario==1) {
        queued->suspend=false;RankTicket resumed;
        check(ranker->request_range(RangeDirection::First,nullptr,3,output.data(),output.size(),&resumed)==RankStatus::InProgress,
              "read release invalidated range restart");
        bool done=false;
        for(unsigned i=0;i<100000;++i){const auto s=ranker->advance(resumed,{1,17,3});
          if(s.status==RankStatus::Ready){done=true;break;}
          check(s.status==RankStatus::InProgress,"handoff restart failed");}
        const auto result=ranker->result(resumed);
        check(done&&result.size==1&&result.items[0].path_count==80,"handoff restart lost raw paths");
      }
    }
  }
  std::cout<<"PASS V8 stale scheme and rank terminal/cancel/handoff/destructor release; external query preserved\n";
}
int main(){
  check(hex(sha256(nullptr,0))=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","empty SHA");
  check(hex(sha256("abc",3))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","abc SHA");
  const std::string longer="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  check(hex(sha256(longer.data(),longer.size()))=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","long SHA");
  Sha256 million;const std::string chunk(1000,'a');for(unsigned i=0;i<1000;++i)million.update(chunk.data(),chunk.size());
  check(hex(million.final())=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","million SHA");
  // Independent protocol vectors produced with Python hashlib + struct.pack,
  // not by either the publisher or this implementation. FIPS SHA vectors above
  // also exercise both one-block and two-block padding, plus streaming input.
  std::array<std::uint8_t,block_bytes> vector_data{};
  std::array<std::uint8_t,1024> vector_page{};
  for(std::size_t i=0;i<vector_data.size();++i)vector_data[i]=static_cast<std::uint8_t>(i);
  for(std::size_t i=0;i<vector_page.size();++i)vector_page[i]=static_cast<std::uint8_t>(i*3);
  check(hex(merkle_leaf_hash(7,vector_data.data(),vector_data.size()))==
      "3339d4ce78226b9d7e1666a769a363151b6eeaf7f14836615a3a0b1986ba0049","independent leaf domain");
  check(hex(merkle_page_hash(2,5,vector_page.data()))==
      "9eb1b5943aba6577db751471a35b9d8e8c6cec33ab23e5cc5ed6e898bb336136","independent page domain");
  MerkleLayout overflow;
  check(!merkle_layout(0,&overflow)&&!merkle_layout(std::numeric_limits<std::uint64_t>::max(),&overflow),"invalid layout admitted");
  std::cout<<"PASS V1 SHA-256 known vectors and independent Merkle domains\n";
  Fixture f;VerifiedResourceReader r(f.data,f.index,f.spec,{3,1,17});check(r.healthy(),"healthy");
  std::array<std::uint8_t,21> output{};check(!r.read(0,output.data(),1)&&f.data->calls==0&&f.index->calls==0,"cold cache read did IO");
  check(r.prepare_bootstrap(),"bootstrap begin");for(unsigned i=0;i<20000&&!r.bootstrap_ready();++i){auto s=r.advance_bootstrap({1,1,1});check(s.status!=ResourceReadStatus::Error,"bootstrap error");}
  check(r.bootstrap_ready()&&r.read(0,output.data(),21),"boot not ready");
  check(finish(r,f,block_bytes-5,output.data(),21).status==ResourceReadStatus::Ready,"cross block");
  check(std::memcmp(output.data(),f.data->bytes.data()+block_bytes-5,21)==0,"cross block bytes");
  for(std::size_t block=1;block<41;++block){check(finish(r,f,block*block_bytes,output.data(),1,{1,17,3}).status==ResourceReadStatus::Ready,"eviction");check(output[0]==f.data->bytes[block*block_bytes],"eviction bytes");}
  check(r.snapshot().resident_verified<=3&&r.snapshot().resident_verified_index_pages<=1&&!r.snapshot().whole_file_checked,"resident");
  Fixture hot;VerifiedResourceReader lru(hot.data,hot.index,hot.spec,{3,2,1024});
  std::uint8_t hot_byte=0;
  for(std::size_t b=0;b<3;++b)check(finish(lru,hot,b*block_bytes,&hot_byte,1,{1,1024,1024}).status==ResourceReadStatus::Ready,"LRU fill");
  check(lru.read(0,&hot_byte,1),"LRU hot hit");
  check(finish(lru,hot,3*block_bytes,&hot_byte,1,{1,1024,1024}).status==ResourceReadStatus::Ready&&
        lru.read(0,&hot_byte,1)&&!lru.read(block_bytes,&hot_byte,1),"hot data block evicted as FIFO");
  Fixture proof_hot;VerifiedResourceReader proof_lru(proof_hot.data,proof_hot.index,proof_hot.spec,{3,2,1024});
  for(const std::size_t b:{0U,33U,1U})
    check(finish(proof_lru,proof_hot,b*block_bytes,&hot_byte,1,{1,1024,1024}).status==ResourceReadStatus::Ready,"LRU proof fill");
  check(proof_lru.snapshot().authenticated_index_loads==4,"hot proof root was evicted as FIFO");
  std::cout<<"PASS V2 exact tiny budgets, cross-block reads and bounded eviction\n";
  // Cancel/replacement while an old block operation has only progressed partway.
  const auto old_operation=operation();
  for(unsigned i=0;i<7;++i)r.read_budgeted(0,output.data(),1,{1,1,1},old_operation);
  std::uint8_t replacement=0;check(finish(r,f,20*block_bytes,&replacement,1).status==ResourceReadStatus::Ready&&replacement==f.data->bytes[20*block_bytes],"replacement");
  // New activation on the same backing data has independent pending work.
  VerifiedResourceReader next(f.data,f.index,f.spec,{3,1,17});
  std::uint8_t next_value=0;
  const auto next_operation=operation();
  for(unsigned i=0;i<7;++i)next.read_budgeted(25*block_bytes,&next_value,1,{1,1,1},next_operation);
  r.deactivate();
  check(r.read_budgeted(0,output.data(),1,{1,1,1},old_operation).status==ResourceReadStatus::Error,"old activation progressed");
  check(finish(next,f,25*block_bytes,&next_value,1,{1,1,1},next_operation).status==ResourceReadStatus::Ready&&next_value==f.data->bytes[25*block_bytes],"old close hurt new work");
  std::cout<<"PASS V3 cancelled request replacement and old/new activation isolation\n";
  auto mem=next.memory_usage();std::cout<<"MEM fixed="<<mem.owner_fixed_bytes<<" data="<<mem.data_payload_bytes<<" index="<<mem.index_payload_bytes<<" metadata="<<mem.data_metadata_bytes+mem.index_metadata_bytes<<" sha="<<mem.hash_state_bytes<<'\n';
  f.data->live=false;check(!next.healthy()&&!next.read(25*block_bytes,&replacement,1)&&next.snapshot().status==VerifiedResourceStatus::Error,"health latch");f.data->live=true;check(!next.healthy(),"health revived");
  Fixture bad;VerifiedResourceReader rb(bad.data,bad.index,bad.spec);bad.data->bytes[33*block_bytes]^=1;
  replacement=0xa5;
  check(finish(rb,bad,33*block_bytes,&replacement,1,{1,1024,1024}).status==ResourceReadStatus::Error&&!rb.healthy()&&replacement==0xa5,"bad block accepted/copied");
  Fixture proof;VerifiedResourceReader rp(proof.data,proof.index,proof.spec);proof.index->bytes[0]^=1;
  check(finish(rp,proof,0,&replacement,1,{1,1024,1024}).status==ResourceReadStatus::Error&&!rp.healthy(),"bad proof accepted");
  Fixture shorted;VerifiedResourceReader rs(shorted.data,shorted.index,shorted.spec);shorted.data->short_read=true;
  replacement=0xa5;
  check(finish(rs,shorted,0,&replacement,1,{1,1024,1024}).status==ResourceReadStatus::Error&&replacement==0xa5,"short read accepted/copied");
  Fixture cross_bad;VerifiedResourceReader cb(cross_bad.data,cross_bad.index,cross_bad.spec);
  cross_bad.data->bytes[block_bytes]^=1;output.fill(0xa5);
  check(finish(cb,cross_bad,block_bytes-5,output.data(),21,{1,1024,1024}).status==ResourceReadStatus::Error,"cross-block corruption accepted");
  check(std::memcmp(output.data(),cross_bad.data->bytes.data()+block_bytes-5,5)==0,"authenticated prefix incorrect");
  for(std::size_t i=5;i<output.size();++i)check(output[i]==0xa5,"unauthenticated suffix copied");
  std::cout<<"PASS V4 fatal health/data/index/short-read and unpublishable partial output\n";
  for(unsigned variant=0;variant<8;++variant){Fixture wrong;auto pin=wrong.spec;
    if(variant==0)++pin.rule_version;
    else if(variant==1)++pin.resource_format;
    else if(variant==2)++pin.key_format;
    else if(variant==3)--pin.file_bytes;
    else if(variant==4)wrong.index->bytes.pop_back();
    else if(variant==5)pin.merkle_root={};
    else if(variant==6)pin.text_capacity=0;
    else pin.required_shared_sha256=pin.file_sha256;
    VerifiedResourceReader rejected(wrong.data,wrong.index,pin);
    check(!rejected.healthy()&&!rejected.prepare_bootstrap()&&wrong.data->calls==0&&wrong.index->calls==0,"invalid metadata performed I/O/activated");
  }
  std::cout<<"PASS V5 incompatible/missing pin, length, format and capability metadata\n";
  Fixture scheme;scheme.spec.role=ResourceReleaseRole::Scheme;scheme.spec.required_shared_sha256=scheme.spec.file_sha256;std::memcpy(scheme.spec.scheme_id.data(),"xiaohe",7);
  scheme.spec.bootstrap_root_offset=block_bytes*20+block_bytes-6;scheme.spec.bootstrap_root_bytes=4*block_bytes;
  VerifiedResourceReader too_small(scheme.data,scheme.index,scheme.spec,{5,1,1024});
  check(!too_small.healthy(),"undersized root bootstrap cache admitted");
  VerifiedResourceReader sc(scheme.data,scheme.index,scheme.spec,{6,1,1024});check(sc.prepare_bootstrap(),"scheme bootstrap begin");
  for(unsigned i=0;i<10000&&!sc.bootstrap_ready();++i)check(sc.advance_bootstrap({1,1024,1024}).status!=ResourceReadStatus::Error,"scheme bootstrap");
  std::array<std::uint8_t,4*block_bytes> root{};check(sc.bootstrap_ready()&&sc.read(scheme.spec.bootstrap_root_offset,root.data(),root.size()),"root span uncached");
  check(std::memcmp(root.data(),scheme.data->bytes.data()+scheme.spec.bootstrap_root_offset,root.size())==0,"root span bytes");
  sc.deactivate();check(!sc.healthy()&&sc.snapshot().status==VerifiedResourceStatus::Closed,"deactivate");
  std::cout<<"PASS V6 bounded cross-block scheme root bootstrap, capacity and close\n";
  Fixture reused;VerifiedResourceReader reuse(reused.data,reused.index,reused.spec);
  auto old=operation();std::weak_ptr<const void> weak=old.owner;
  output.fill(0);
  check(reuse.read_budgeted(1,output.data(),output.size(),{1,1,1}).status==ResourceReadStatus::Error&&reuse.healthy(),"missing ownership allowed");
  for(unsigned i=0;i<20000&&output[0]!=reused.data->bytes[1];++i)
    check(reuse.read_budgeted(1,output.data(),output.size(),{1,1,1},old).status==ResourceReadStatus::Pending,"old read completed too early");
  check(output[0]==reused.data->bytes[1],"no partial authenticated copy");
  auto newer=operation();output.fill(0);old.owner.reset();
  check(!weak.expired(),"pending operation failed to retain owner identity");
  check(finish(reuse,reused,1,output.data(),output.size(),{1,1,1},newer).status==ResourceReadStatus::Ready&&
        std::memcmp(output.data(),reused.data->bytes.data()+1,output.size())==0,"same buffer new owner resumed old copy cursor");
  check(weak.expired(),"replaced operation retained old owner");
  old=newer;++newer.revision;output.fill(0);
  check(reuse.read_budgeted(1,output.data(),output.size(),{1,1,2},newer).status==ResourceReadStatus::Pending,"revision read should be pending");
  const auto cost_before=reuse.snapshot().cost;
  reuse.cancel_read_budgeted(old);
  check(reuse.snapshot().cost.read_bytes==cost_before.read_bytes&&reuse.snapshot().cost.work_items==cost_before.work_items,"cancel performed work");
  const auto after_cancel=reuse.read_budgeted(1,output.data(),output.size(),{1,1,20},newer);
  check(after_cancel.status==ResourceReadStatus::Ready&&after_cancel.cost.work_items==20&&
        std::memcmp(output.data(),reused.data->bytes.data()+1,output.size())==0,"old revision cancel damaged new partial copy");
  ++newer.revision;
  reuse.read_budgeted(1,output.data(),output.size(),{1,1,1},newer);
  reuse.cancel_read_budgeted(newer);reuse.cancel_read_budgeted(newer);
  check(reuse.healthy(),"own idempotent cancellation damaged release");
  std::cout<<"PASS V7 same-buffer owner/revision reuse and conditional old cleanup\n";
  cleanup_regression();
}
