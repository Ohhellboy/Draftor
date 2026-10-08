#include "hanime/member_eligibility.hpp"
#include "hanime/candidate_session.hpp"
#include "hanime/composition_commit.hpp"
#include "release_pin.hpp"
#include "allocation_probe.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using namespace hanime;
namespace fs=std::filesystem;
void require(bool ok,const std::string& why) {if(!ok)throw std::runtime_error(why);}
struct Io {std::uint64_t calls{0},bytes{0},seeks{0},opens{0},max_read{0};};
Io operator-(Io a,const Io& b) {a.calls-=b.calls;a.bytes-=b.bytes;a.seeks-=b.seeks;a.opens-=b.opens;return a;}
class File:public ResourceReader {
 public:
  File(const fs::path& path,Io& io):meter(io) {
    fd=::open(path.c_str(),O_RDONLY);require(fd>=0,"open "+path.string());
    struct stat s{};require(!::fstat(fd,&s)&&s.st_size>=0,"fstat");n=static_cast<std::size_t>(s.st_size);++meter.opens;
  }
  ~File() override {if(fd>=0)::close(fd);}
  std::size_t size() const override {return n;}
  bool read(std::size_t at,void* to,std::size_t count) const override {
    if(!count)return true;
    if(!to||count>1024||at>n||count>n-at)return false;
    ++meter.calls;meter.bytes+=count;meter.max_read=std::max<std::uint64_t>(meter.max_read,count);
    if(!positioned||at!=next)++meter.seeks;
    next=at+count;positioned=true;
    return ::pread(fd,to,count,static_cast<off_t>(at))==static_cast<ssize_t>(count);
  }
 private: Io& meter;int fd{-1};std::size_t n{0};mutable bool positioned{false};mutable std::size_t next{0};
};
struct DisabledCounts:CommitCountSource {
  mutable unsigned reads{0};unsigned notifications{0};
  StateView view() const override {++reads;throw std::runtime_error("Disabled count view");}
  bool view_is_current(const StateView&) const override {++reads;throw std::runtime_error("Disabled count check");}
  CountTicket begin_count(const StateView&,const StableLexemeKey&) override {throw std::runtime_error("Disabled count begin");}
  CountResult advance_count(const CountTicket&,const WorkBudget&) override {throw std::runtime_error("Disabled count advance");}
  void cancel_count(const CountTicket&) override {throw std::runtime_error("Disabled count cancel");}
  LearningStatus notify_committed(const CommitHandle&,RegistryNamespaceId,const StableLexemeKey&) noexcept override {
    ++notifications;return LearningStatus::PausedClosed; // Old single-item compatibility test only.
  }
};
struct Document:TextCommitSink {
  std::array<char,512> body{};std::size_t size{0},calls{0};
  bool commit(std::string_view text) override {
    ++calls;if(text.size()>body.size()-size)return false;
    std::copy(text.begin(),text.end(),body.begin()+size);size+=text.size();return true;
  }
};
constexpr WorkBudget budget{1,1024,1024};
struct Totals {std::uint64_t steps{0},work{0},max_tick_ns{0};};
void count(Totals& c,const WorkCost& cost,std::chrono::steady_clock::time_point start) {
  require(cost.read_calls<=budget.max_read_calls&&cost.read_bytes<=budget.max_read_bytes&&cost.work_items<=budget.max_work_items,"public advance exceeded budget");
  ++c.steps;c.work+=cost.work_items;
  const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
  c.max_tick_ns=std::max(c.max_tick_ns,static_cast<std::uint64_t>(ns));
}
struct Mark {
  Io all;
  std::array<std::uint64_t,3> data{};
  std::uint64_t proof{0},steps{0},work{0},logical{0},logical_bytes{0},hits{0};
};
struct Runtime {
  std::array<Io,6> io{};
  std::array<std::shared_ptr<VerifiedResourceReader>,3> readers;
  PagedSharedResource shared;PagedSchemeIndex scheme;
  std::shared_ptr<BaseOrderIndex> order;
  DisabledCounts counts;Document doc;Totals totals;
  bool fast;std::string id;
  Mark mark() const {
    Mark m;for(const auto& i:io){m.all.calls+=i.calls;m.all.bytes+=i.bytes;m.all.seeks+=i.seeks;m.all.opens+=i.opens;m.all.max_read=std::max(m.all.max_read,i.max_read);}
    for(unsigned i=1;i<6;i+=2)m.proof+=io[i].bytes;
    for(unsigned i=0;i<3;++i)m.data[i]=io[2*i].bytes;
    m.steps=totals.steps;m.work=totals.work;
    for(const auto& r:readers)if(r){const auto s=r->snapshot();m.logical+=s.logical_requests+s.cache_only_requests;m.logical_bytes+=s.logical_bytes+s.cache_only_bytes;m.hits+=s.data_cache_hits;}
    return m;
  }
  double report(const std::string& tag,const Mark& before,std::uint64_t limit=0,double seconds=0) {
    const auto after=mark();const auto d=after.all-before.all;
    const auto steps=after.steps-before.steps,work=after.work-before.work;
    const double model=d.bytes/85672.0+d.seeks*.007+d.opens*.028+steps*.00005+work/3000000.0;
    const bool pass=(!limit||d.bytes<=limit)&&(!seconds||model<=seconds);
    std::cout<<"COST mode="<<(fast?"ordered":"scan")<<" scheme="<<id<<" tag="<<tag<<" bytes="<<d.bytes<<" calls="<<d.calls
      <<" proof_bytes="<<after.proof-before.proof<<" seeks="<<d.seeks<<" max_request="<<after.all.max_read
      <<" shared_data_bytes="<<after.data[0]-before.data[0]<<" scheme_data_bytes="<<after.data[1]-before.data[1]
      <<" order_data_bytes="<<after.data[2]-before.data[2]
      <<" logical_calls="<<after.logical-before.logical<<" logical_bytes="<<after.logical_bytes-before.logical_bytes
      <<" cache_hits="<<after.hits-before.hits<<" steps="<<steps<<" work="<<work<<" model_s="<<model
      <<" tick_max_ns_cumulative="<<totals.max_tick_ns<<" gate="<<(pass?"PASS":"FAIL")<<'\n';
    require(pass,"absolute gate failed: "+tag);return model;
  }
  Runtime(const fs::path& resource,const fs::path& publication,bool ordered,std::string name):fast(ordered),id(std::move(name)) {
    const auto s=id=="xiaohe"?hanime_release::xiaohe:hanime_release::full_pinyin;
    const auto pin=id=="xiaohe"?hanime_release::xiaohe_base:hanime_release::full_pinyin_base;
    const auto initial=mark();
    open(0,resource/"shared.hns",publication/"shared.hnv",hanime_release::shared,{16,8,1024});
    std::string error;require(shared.open_verified(readers[0],9,{2,1024,fast},&error),error);
    open(1,resource/(id+".hnm"),publication/(id+".hnv"),s,{16,8,1024});
    require(scheme.open_verified(readers[1],shared,{2,1024},&error),error);
    report("primary_startup",initial,id=="xiaohe"?14376:16384);
    if(fast) {
      const auto before=mark();open(2,publication/(id+"_base.hbo"),publication/(id+"_base.hnv"),pin,{8,4,1024});
      order=std::make_shared<BaseOrderIndex>(readers[2],scheme,shared);require(scheme.attach_base_order(order),"attach preordered");
      report("derived_open",before,4096);
      auto m=readers[2]->memory_usage();BaseOrderCursor cursor;
      const auto shared_memory=shared.memory_usage();
      std::cout<<"MEMORY scheme="<<id<<" derived_owner="<<order->memory_bytes()<<" cursor="<<cursor.memory_bytes()
        <<" authenticated_fixed="<<m.owner_fixed_bytes<<" data="<<m.data_payload_bytes<<" proof="<<m.index_payload_bytes
        <<" metadata="<<m.data_metadata_bytes+m.index_metadata_bytes
        <<" shared_fixed="<<shared_memory.owner_fixed_bytes<<" shared_cache="<<shared_memory.cache_payload_allocated_bytes
        <<" shared_cache_metadata="<<shared_memory.cache_metadata_allocated_bytes
        <<" member_request="<<sizeof(MemberRequest)<<" output_reader="<<sizeof(OutputReader)
        <<" source_count_independent=1\n";
    }
  }
  void open(unsigned n,const fs::path& data,const fs::path& index,const ResourceReleaseSpec& pin,VerifiedResourceConfig cfg) {
    readers[n]=std::make_shared<VerifiedResourceReader>(std::make_shared<File>(data,io[2*n]),std::make_shared<File>(index,io[2*n+1]),pin,cfg);
    require(readers[n]->prepare_bootstrap(),readers[n]->error());
    for(unsigned guard=0;!readers[n]->bootstrap_ready()&&guard<100000;++guard) {
      const auto start=std::chrono::steady_clock::now();auto s=readers[n]->advance_bootstrap(budget);count(totals,s.cost,start);
      require(s.status!=ResourceReadStatus::Error,readers[n]->error());
    }
    require(readers[n]->bootstrap_ready(),"bootstrap timeout");
  }
  void switch_to_pinyin(const fs::path& resource,const fs::path& publication) {
    const auto activation=shared.activation_handle();const auto before=mark();
    scheme.deactivate();order.reset();readers[1].reset();readers[2].reset();id="full_pinyin";
    open(1,resource/"full_pinyin.hnm",publication/"full_pinyin.hnv",hanime_release::full_pinyin,{16,8,1024});
    std::string error;require(scheme.open_verified(readers[1],shared,{2,1024},&error),error);
    open(2,publication/"full_pinyin_base.hbo",publication/"full_pinyin_base.hnv",hanime_release::full_pinyin_base,{8,4,1024});
    order=std::make_shared<BaseOrderIndex>(readers[2],scheme,shared);require(scheme.attach_base_order(order),"switch order");
    require(shared.activation_is_current(activation)&&mark().data[0]==before.data[0],"switch reinitialized shared");
    report("switch_scheme_only",before,14376,2.0);
  }
  std::string text(const RankItem& item,OutputRequest request={}) {
    OutputReader output;std::array<char,128> names{};std::array<char,64> out{};
    require(output.begin(shared,shared.activation_handle(),item.ref,item.key,request,names.data(),names.size(),out.data(),out.size())==OutputStatus::Pending,"text begin");
    for(unsigned guard=0;guard<100000;++guard) {
      auto start=std::chrono::steady_clock::now();auto step=output.advance(budget);count(totals,step.cost,start);
      if(step.status!=OutputStatus::Pending){require(output_has_text(step.status),"default text error");return std::string(step.text);}
    }
    throw std::runtime_error("text timeout");
  }
};
struct Entry {RankItem item;std::string text;};
bool equal(const Entry& a,const Entry& b) {
  return a.item.key==b.item.key&&a.item.ref==b.item.ref&&a.item.score==b.item.score&&a.item.scheme_weight==b.item.scheme_weight&&
    a.item.path_count==b.item.path_count&&a.item.representative_ordinal==b.item.representative_ordinal&&a.text==b.text;
}
// Full vector is a TEST ORACLE only. Runtime ranker owns fixed W=6 and Q<=6.
std::vector<Entry> enumerate(Runtime& h,const std::string& code) {
  std::array<RankItem,6> work{},out{};ExactRanker rank(h.scheme,h.shared,h.counts,work.data(),6,3);
  require(rank.begin_input(code,9,{},ScoringMode::BaseOnly)==RankStatus::InProgress,"input");
  const auto memory=rank.memory_usage();
  std::cout<<"RANK_MEMORY impl="<<memory.impl_bytes<<" cursor_total="<<memory.ordered_cursor_bytes
    <<" code_capacity="<<memory.captured_code_capacity<<" lifetime="<<memory.lifetime_bytes
    <<" raw_code="<<memory.raw.code_capacity_bytes<<" boundary="<<memory.boundary_bytes<<'\n';
  std::vector<Entry> result;RankBoundary anchor;RangeDirection direction=RangeDirection::First;unsigned page=0;
  auto next=[&](RangeDirection dir,const RankBoundary* boundary) {
    RankTicket t;require(rank.request_range(dir,boundary,6,out.data(),6,&t)==RankStatus::InProgress,"range request");
    for(unsigned n=0;n<20000000;++n) {
      const auto start=std::chrono::steady_clock::now();auto step=rank.advance(t,budget);count(h.totals,step.cost,start);
      if(step.status==RankStatus::Ready||step.status==RankStatus::NoMatches||step.status==RankStatus::RangeExhausted)return t;
      require(step.status==RankStatus::InProgress,"rank terminated unexpectedly");
    }
    throw std::runtime_error("rank timeout");
  };
  for (;;) {
    const auto start=h.mark();auto t=next(direction,direction==RangeDirection::First?nullptr:&anchor);auto v=rank.result(t);
    h.report(code+"_rank"+std::to_string(page),start);
    const auto text_start=h.mark();
    std::vector<Entry> current;for(std::size_t i=0;i<v.size;++i)current.push_back({v.items[i],h.text(v.items[i])});
    h.report(code+"_text"+std::to_string(page),text_start);
    if(v.size)anchor=rank.boundary(t,v.size-1);
    const auto more=v.has_more;rank.release(t);result.insert(result.end(),current.begin(),current.end());
    h.report(code+"_page"+std::to_string(page),start,h.fast?(page?65536:131072):0,h.fast?(page?1.5:2.5):0);
    ++page;if(!more)break;direction=RangeDirection::After;require(page<1000,"finite coverage");
  }
  if(!result.empty()) {
    const auto before=h.mark();auto t=next(RangeDirection::Before,&anchor);auto v=rank.result(t);
    const auto n=std::min<std::size_t>(6,result.size()-1);require(v.size==n,"Before size");
    for(std::size_t i=0;i<n;++i)require(equal({v.items[i],h.text(v.items[i])},result[result.size()-1-n+i]),"Before identity/text/order");
    rank.release(t);h.report(code+"_before",before,h.fast?65536:0,h.fast?1.5:0);
  }
  require(!h.counts.reads,"Disabled accessed count source");
  std::size_t max_text=0;for(const auto& e:result)max_text=std::max(max_text,e.text.size());
  std::cout<<"ENUM scheme="<<h.id<<" code="<<code<<" candidates="<<result.size()<<" ranges="<<page<<" max_utf8_bytes="<<max_text<<'\n';
  return result;
}
SessionStatus ready(Runtime& h,CandidateSession& session) {
  for(unsigned i=0;i<1000000;++i) {
    auto start=std::chrono::steady_clock::now();auto step=session.tick(budget);count(h.totals,step.cost,start);
    if(step.status!=SessionStatus::Pending)return step.status;
  }
  throw std::runtime_error("session timeout");
}
void prefetch(Runtime& h,bool enabled) {
  CandidateSession session(h.scheme,h.shared,h.counts,h.doc,{6,6,3,64,128,128});
  require(session.begin_text_access()==TextAccessStatus::Pending,"access begin");
  require(session.advance_text_access(budget).status==TextAccessStatus::Ready,"reuse text admission");
  require(session.begin_input("ui",9,6,6,ScoringMode::BaseOnly)==SessionStatus::Pending,"session begin");
  require(ready(h,session)==SessionStatus::StagingReady,"first staging");
  require(session.publish(session.staging())==SessionStatus::Ready,"first publish");
  StableLexemeKey key;std::string first;
  session.present([&](const CandidateViewPage& v){key=v.items[0].ranked.key;first=std::string(v.text(0));});
  const auto selection=session.selection(0);const auto before=h.mark();
  if(enabled) {
    require(session.request_range(RangeDirection::After,6,6)==SessionStatus::Pending,"hidden request");
    require(ready(h,session)==SessionStatus::StagingReady,"hidden ready");
    session.present([&](const CandidateViewPage& v){require(v.items[0].ranked.key==key&&v.text(0)==first,"prefetch replaced visible frame");});
    h.report("hidden_prepare",before,65536,1.5);
  }
  const auto select_start=h.mark();const auto calls=h.doc.calls;
  auto commit=session.confirm(selection);require(commit.status==CommitStatus::Preparing,"visible selection rejected by hidden work");
  for(unsigned i=0;i<100000&&session.commit_result().status==CommitStatus::Preparing;++i)ready(h,session);
  require(session.commit_result().status==CommitStatus::Accepted&&h.doc.calls==calls+1,"single text accepted");
  require(session.confirm(selection).status!=CommitStatus::Preparing&&h.doc.calls==calls+1,"duplicate accepted");
  h.report(enabled?"selection_while_prefetched":"selection_no_prefetch",select_start,32768,.75);
  session.cancel_input(session.input_handle());
  const auto mem=session.memory_usage();
  std::cout<<"PREFETCH enabled="<<enabled<<" visible_not_advanced=1 old_selection_accepted=1 same_config=1 snapshot="<<mem.snapshot_bytes
    <<" text_pools="<<mem.text_pool_bytes<<" rank_work="<<mem.rank_work_bytes<<" commit_text="<<mem.commit_pool_bytes
    <<" session_impl="<<mem.impl_bytes<<" render_callback_reads=0\n";
}
void navigation(Runtime& h,bool enabled) {
  CandidateSession s(h.scheme,h.shared,h.counts,h.doc,{6,6,3,64,128,128});
  require(s.begin_text_access()==TextAccessStatus::Pending&&s.advance_text_access(budget).status==TextAccessStatus::Ready,"navigation admission");
  const auto first=h.mark();
  require(s.begin_input("ui",9,6,6,ScoringMode::BaseOnly)==SessionStatus::Pending,"navigation input");
  require(ready(h,s)==SessionStatus::StagingReady&&s.publish(s.staging())==SessionStatus::Ready,"navigation first");
  h.report("nav_first6",first,131072,2.5);
  std::array<StableLexemeKey,5> starts;
  s.present([&](const CandidateViewPage& v){starts[0]=v.items[0].ranked.key;});
  for(unsigned page=1;page<=4;++page) {
    const auto need=h.mark();
    require(s.request_range(RangeDirection::After,6,6)==SessionStatus::Pending,"nav next");
    // Each tick is one scheduler opportunity. No loop in a drawing callback.
    require(ready(h,s)==SessionStatus::StagingReady,"nav staged");
    s.present([&](const CandidateViewPage& v){require(v.items[0].ranked.key==starts[page-1],"hidden result advanced visible position");});
    if(enabled)h.report("nav_background_page"+std::to_string(page),need,65536,1.5);
    const auto handoff=h.mark();require(s.publish(s.staging())==SessionStatus::Ready,"nav handoff");
    if(enabled) {
      require(h.mark().all.bytes==handoff.all.bytes,"prepared handoff read data");
      h.report("nav_prefetch_hit",handoff,1,.01);
    } else h.report("nav_explicit_miss_page"+std::to_string(page),need,65536,1.5);
    s.present([&](const CandidateViewPage& v){starts[page]=v.items[0].ranked.key;});
  }
  const auto reverse=h.mark();require(s.request_range(RangeDirection::Before,6,6)==SessionStatus::Pending,"direction reverse");
  require(ready(h,s)==SessionStatus::StagingReady&&s.publish(s.staging())==SessionStatus::Ready,"reverse publication");
  s.present([&](const CandidateViewPage& v){require(v.items[0].ranked.key==starts[3],"Before returned wrong frame");});
  h.report("nav_reverse",reverse,65536,1.5);
  require(s.request_range(RangeDirection::After,6,6)==SessionStatus::Pending,"cancel next");
  const auto old=s.input_handle();s.tick(budget);s.cancel_input(old);
  require(s.begin_input("le",9,6,6,ScoringMode::BaseOnly)==SessionStatus::Pending,"edit after prefetch cancel");
  s.cancel_input(old); // Cannot cancel new input, even before its first tick.
  require(ready(h,s)==SessionStatus::StagingReady&&s.publish(s.staging())==SessionStatus::Ready,"new input survived old cancel");
  s.present([&](const CandidateViewPage& v){require(v.text(0)=="了","edit selected stale ui candidate");});
  std::cout<<"NAV PASS prefetch="<<enabled<<" identical_capacity=1 forward=4 reverse=1 cancel_edit=1 no_layout_claim=1\n";
}
void k1(Runtime& h,const std::array<Entry,3>& selected) {
  std::array<char,128> names{};std::array<char,64> text{};
  MemberEligibility member(*h.order,h.shared,names.data(),names.size(),text.data(),text.size());
  const std::array<std::string,3> codes=h.id=="xiaohe"?std::array<std::string,3>{"wj","dj","le"}:std::array<std::string,3>{"wan","dan","le"};
  std::array<MemberTicket,3> tickets;std::array<MemberRequest,3> requests;
  const auto before=h.mark();std::uint32_t pos=0,total=0;
  for(const auto& c:codes)total+=static_cast<std::uint32_t>(c.size());
  for(unsigned i=0;i<3;++i) {
    MemberRequest request{h.shared.activation_handle(),codes[i],selected[i].item.ref,selected[i].item.key,7,4,pos,pos+static_cast<std::uint32_t>(codes[i].size()),total,{}};
    requests[i]=request;
    pos=request.end;require(member.begin(request,&tickets[i])==MemberStatus::Pending,"K1 member start");
    for(unsigned guard=0;;++guard) {
      require(guard<100000,"member timeout");auto start=std::chrono::steady_clock::now();auto step=member.advance(tickets[i],budget);count(h.totals,step.cost,start);
      if(step.status==MemberStatus::Eligible)break;
      require(step.status==MemberStatus::Pending,"K1 member invalid");
    }
  }
  for(unsigned i=0;i<3;++i) {
    auto changed=requests[i];++changed.selection_revision;
    require(member.eligible(tickets[i],requests[i])&&!member.eligible(tickets[i],changed),"mixed selection qualification");
  }
  h.report("K1_three_members",before,131072,2.5);
  // Independent final editor model; no learning owner, file or queue.
  CompositionCommitter committer(h.shared,h.doc,{3,128,64,192,CompositionLearningPolicy::Disabled});
  std::array<CompositionMember,3> members;pos=0;
  for(unsigned i=0;i<3;++i){members[i]={selected[i].item.ref,selected[i].item.key,{pos,pos+codes[i].size()},{}};pos+=static_cast<std::uint32_t>(codes[i].size());}
  const auto start=h.mark();auto prepared=committer.prepare(h.shared.activation_handle(),{7,4,{0,total}},members.data(),3);
  const auto body_before=h.doc.size,calls=h.doc.calls;
  for(unsigned guard=0;prepared.status==CompositionStatus::Preparing;++guard) {
    require(guard<100000,"commit timeout");auto time=std::chrono::steady_clock::now();auto step=committer.advance(prepared.transaction,budget);count(h.totals,step.cost,time);prepared=step.result;
    require(h.doc.size==body_before,"preparation modified body");
  }
  require(prepared.status==CompositionStatus::Ready,"composition ready");
  committer.prepared(prepared.transaction,[&](const PreparedCompositionView& v){require(v.text=="玩蛋了","K1 text/identities changed");});
  h.report("K1_independent_prepare",start);
  const auto confirm=h.mark();require(committer.confirm(prepared.transaction).status==CompositionStatus::Accepted,"composition accept");
  require(committer.confirm(prepared.transaction).status==CompositionStatus::Accepted&&h.doc.calls==calls+1,"duplicate composition confirm");
  h.report("K1_confirm",confirm,1,.01);require(!h.counts.reads,"K1 used count source");
  std::cout<<"K1 PASS qualification_bytes_exclude_test_lookup=1 frozen_BasicHost_not_adapted=1 actual_DeviceIme_key_path=OPEN member_fixed="<<member.memory_bytes()
    <<" proof_payload_each="<<MemberEligibility::ticket_payload_bytes()<<" retained_proofs=3 proof_control_blocks_unmeasured=1\n";
}
}
int main(int argc,char** argv) {
  try {
    require(argc==3||(argc==4&&std::string_view(argv[3])=="--bounded"),"usage: query_consumer ORIGINAL_RESOURCE_DIR BASE_ORDER_PUBLICATION_DIR [--bounded]");
    if(argc==4) {
      // Fixed selections from the independently reviewed SRP2 event sequence.
      // This mode has no full-result oracle, no original scanning runtime.
      std::array<Entry,3> selected;
      const std::array<const char*,3> keys={
        "SLK1:bb7b9273cf5d3df5f97e93f7336bbf898ed21966eb5c635cc87adb0d37cbe9c7",
        "SLK1:c4b280f48b139bc5ffb15ebd48b7ee52dcee13ddf3ed7a8459ebbdaa93e492b7",
        "SLK1:74616ecfc09179cb6013b9ae5720231c62f55daa2036843048c7d028fe05f3e8"};
      const std::array<RuntimeLexemeRef,3> refs={177730,186390,110039};
      for(unsigned i=0;i<3;++i){selected[i].item.ref=refs[i];require(parse_slk1(keys[i],&selected[i].item.key),"fixed SRP2 key");}
      for(const auto& id:{std::string("xiaohe"),std::string("full_pinyin")}) {
        query_probe::start();
        {Runtime h(argv[1],argv[2],true,id);k1(h,selected);}
        auto m=query_probe::stop();require(!m.current,"bounded K1 retained owned allocation");
        std::cout<<"ALLOCATION case=K1 scheme="<<id<<" current="<<m.current<<" peak="<<m.peak<<" calls="<<m.allocations<<" enabled="<<m.enabled<<'\n';
      }
      for(bool enabled:{false,true}) {
        query_probe::start();
        {Runtime h(argv[1],argv[2],true,"xiaohe");navigation(h,enabled);}
        auto m=query_probe::stop();require(!m.current,"bounded navigation retained owned allocation");
        std::cout<<"ALLOCATION case=navigation prefetch="<<enabled<<" current="<<m.current<<" peak="<<m.peak<<" calls="<<m.allocations<<" enabled="<<m.enabled<<'\n';
        {Runtime h(argv[1],argv[2],true,"xiaohe");prefetch(h,enabled);}
      }
      {
        Runtime h(argv[1],argv[2],true,"xiaohe");h.switch_to_pinyin(argv[1],argv[2]);
        CandidateSession session(h.scheme,h.shared,h.counts,h.doc,{6,6,3,64,128,128});
        require(session.begin_text_access()==TextAccessStatus::Pending&&session.advance_text_access(budget).status==TextAccessStatus::Ready,"switch admission");
        const auto before=h.mark();
        require(session.begin_input("wandan",9,6,6,ScoringMode::BaseOnly)==SessionStatus::Pending,"switch query");
        require(ready(h,session)==SessionStatus::StagingReady&&session.publish(session.staging())==SessionStatus::Ready,"switch first");
        session.present([&](const CandidateViewPage& v){require(v.text(0)=="完蛋","scheme switch changed word");});
        h.report("switch_first_candidate",before,131072,2.5);
      }
      std::cout<<"PASS bounded public consumer; no full-result vector exercised; test logging included, not total malloc/stack/device peak\n";
      return 0;
    }
    for(const auto& id:{std::string("xiaohe"),std::string("full_pinyin")}) {
      Runtime scan(argv[1],argv[2],false,id),fast(argv[1],argv[2],true,id);
      std::array<Entry,3> selected;
      const std::array<std::string,5> codes=id=="xiaohe"?std::array<std::string,5>{"ui","wj","dj","le","wjdjle"}:std::array<std::string,5>{"shi","wan","dan","le","yuanxingji"};
      for(const auto& code:codes) {
        const auto a=enumerate(scan,code),b=enumerate(fast,code);require(a.size()==b.size(),"full range size differs");
        for(std::size_t i=0;i<a.size();++i)require(equal(a[i],b[i]),"full rank/key/text differs");
        // Exact public choices in SRP2: ordinal0 玩, ordinal3 蛋, ordinal0 了.
        // Enumeration is oracle setup, NOT charged as a selection/confirmation.
        if(code==codes[1]){require(a.size()>0&&a[0].text=="玩","SRP2 玩 ordinal");selected[0]=a[0];}
        if(code==codes[2]){require(a.size()>3&&a[3].text=="蛋","SRP2 蛋 ordinal");selected[1]=a[3];}
        if(code==codes[3]){require(a.size()>0&&a[0].text=="了","SRP2 了 ordinal");selected[2]=a[0];}
        if(code=="ui")require(b.size()==222,"ui candidate coverage changed");
      }
      const std::array<std::string,3> extra=id=="xiaohe"?
          std::array<std::string,3>{"yi","wjdj","aakesdhasakezuzivixm"}:
          std::array<std::string,3>{"yi","wandan","qiandongnanmiaozudongzuzizhizhou"};
      for(const auto& code:extra) {
        const auto a=enumerate(scan,code),b=enumerate(fast,code);require(a.size()==b.size()&&!a.empty(),"additional production coverage");
        for(std::size_t i=0;i<a.size();++i)require(equal(a[i],b[i]),"long/dense production mismatch");
        require(code!="yi"||a.size()==434,"high-collision set changed");
        require(code.size()<10||a.front().text.size()>18,"long-word probe did not exercise long text");
      }
      require(enumerate(fast,"zzzzzznotacode").empty(),"NoMatches");
      if(id=="xiaohe")for(bool enabled:{false,true}) {
        {Runtime policy(argv[1],argv[2],true,id);prefetch(policy,enabled);}
        {Runtime policy(argv[1],argv[2],true,id);navigation(policy,enabled);}
      }
      for(const auto& e:selected)std::cout<<"SRP2_MEMBER text="<<e.text<<" ref="<<e.item.ref<<" key="<<format_slk1(e.item.key)<<'\n';
      {Runtime cold(argv[1],argv[2],true,id);k1(cold,selected);}
    }
    std::cout<<"PASS public consumer; derived ordering/member/prefetch Host only; S3 and extension integration OPEN\n";
  } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
