// Actual-size file-backed verification. Reference files are test-only streams;
// the linked runtime uses public paged/session/state APIs, never Host decode.
#include <hanime/host_resource_file.hpp>
#include <hanime/host_state_files.hpp>
#include <hanime/bounded_user_state.hpp>
#include "rank_allocation_probe.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace hanime;
namespace {
constexpr RegistryNamespaceId ns=0x48414e494d450001ULL;
constexpr MountEpoch epoch=19;
constexpr WorkBudget budget{1,4096,256};
constexpr StateBudget back_budget{1,1,256,256,1,64};
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
StableLexemeKey key(const std::string& s) { StableLexemeKey k;check(parse_slk1(s,&k),"oracle full key");return k; }
std::string unhex(const std::string& s) {
  std::string out;
  for(std::size_t i=0;i<s.size();i+=2)out.push_back(static_cast<char>(std::stoul(s.substr(i,2),nullptr,16)));
  return out;
}
struct Sink final:TextCommitSink {
  std::array<char,256> text{};std::size_t size{0},calls{0};
  bool commit(std::string_view s) override {
    ++calls;if(s.size()>text.size()-size)return false;
    if(!s.empty())std::memcpy(text.data()+size,s.data(),s.size());
    size+=s.size();return true;
  }
};
struct App {
  HostResourceMetrics metrics;
  HostStateFiles files;
  StateV3 disk;
  BoundedUserState counts;
  PagedSharedResource shared;
  PagedSchemeIndex scheme;
  Sink sink;
  std::unique_ptr<CandidateSession> session;
  std::uint64_t steps{0},previous_bytes{0},previous_reads{0},previous_steps{0};
  explicit App(const std::filesystem::path& p):files((p/"a.state").string(),(p/"b.state").string()),
    disk(files,{ns,0x50524f44434c0001ULL,256,8,16*1024*1024}),counts(disk,ns,{4,64,8}) {}
  template<class F> auto front(F f) {
    auto r=f();check(r.cost.read_calls<=1 && r.cost.read_bytes<=4096 && r.cost.work_items<=256,"front budget");
    check(++steps<500'000'000ULL,"test progress cap, not a device deadline");return r;
  }
  template<class F> auto back(F f) {
    auto r=f();const auto& c=r.cost;
    check(c.read_calls+c.write_calls+c.control_calls<=1 && c.read_bytes<=256 && c.write_bytes<=256 && c.work_items<=64,"state budget");return r;
  }
  void mark(const char* stage) {
    std::cout<<"COST "<<stage<<" reads="<<metrics.read_calls-previous_reads<<" bytes="<<metrics.requested_bytes-previous_bytes
      <<" steps="<<steps-previous_steps<<" state_read="<<files.metrics.requests.read_bytes<<'\n'<<std::flush;
    previous_reads=metrics.read_calls;previous_bytes=metrics.requested_bytes;previous_steps=steps;
  }
  void mount_shared(const std::filesystem::path& p) {
    std::string error;auto r=std::make_shared<HostResourceFile>((p/"shared.hns").string(),4096,metrics);
    check(r->valid(),"shared file");check(shared.open(r,ns,0,epoch,{4,4096},&error),error.c_str());mark("shared_fixed_open");
    auto c=shared.validation_cursor();
    while(!shared.ready()) {auto s=front([&]{return shared.advance_validation(c,budget);});
      check(s.status==ValidationStepStatus::InProgress||s.status==ValidationStepStatus::Ready,"shared validation");}
    check(shared.info().sections[0].count==242793,"all registered records retained");mark("shared_validation");
  }
  void mount_scheme(const std::filesystem::path& p,const std::string& name) {
    check(!session,"idle switch");scheme.deactivate();
    std::string error;auto r=std::make_shared<HostResourceFile>((p/(name+".hnm")).string(),4096,metrics);
    check(r->valid(),"scheme file");check(scheme.open(r,ns,shared.info().shared_bundle_id,242793,{2,4096},&error),error.c_str());
    mark("scheme_fixed_open");
    while(!scheme.ready()) {auto s=front([&]{return scheme.advance_validation(budget);});
      check(s.status==ValidationStepStatus::InProgress||s.status==ValidationStepStatus::Ready,"scheme validation");}
    check(metrics.peak_open_files<=2,"only shared and active scheme open");
    std::cout<<"SCHEME "<<name<<" mappings="<<scheme.info().mapping_count<<" depth="<<scheme.info().tree_depth<<" bytes="<<scheme.info().file_bytes<<'\n';
    mark("scheme_validation");
  }
  SharedLexemeRecord metadata(RuntimeLexemeRef ref) {
    SharedReadCursor c;check(shared.begin_read(SharedRecordKind::Lexeme,ref,&c)==SharedReadStatus::InProgress,"metadata begin");
    for(;;) {auto r=front([&]{return shared.advance_read(&c,budget);});
      if(r.status==SharedReadStatus::Ready) {check(shared.result_is_current(r),"metadata identity");shared.cancel_read(&c);return r.lexeme;}
      check(r.status==SharedReadStatus::InProgress,"metadata status");}
  }
  void all_lexemes(const std::filesystem::path& oracle) {
    std::ifstream f(oracle/"lexemes.txt");std::string k,text;RuntimeLexemeRef ref;std::int32_t weight;std::size_t n=0;
    while(f>>ref>>k>>weight>>text) {
      auto r=metadata(ref);check(r.identity.stable_key==key(k)&&r.identity.base_weight==weight,"complete identity/weight conversion");
      std::array<char,64> out{};SharedTextCursor c;
      check(shared.begin_payload_read(shared.activation_handle(),r.default_payload,out.data(),out.size(),&c)==SharedReadStatus::InProgress,"default text begin");
      for(;;) {auto s=front([&]{return shared.advance_payload_read(&c,budget);});
        if(s.status==SharedReadStatus::Ready) {check(s.text==unhex(text),"original UTF-8 payload");break;}
        check(s.status==SharedReadStatus::InProgress,"payload status");}
      shared.cancel_payload_read(&c);++n;
    }
    check(f.eof()&&n==242793,"all shared oracle rows");mark("all_keys_weights_default_text");std::cout<<"PASS T1 all 242793 keys/weights/default UTF-8 texts\n";
  }
  void raw_all(const std::filesystem::path& oracle,const std::string& name) {
    std::ifstream f(oracle/(name+"_raw.txt"));std::string code,k;RuntimeLexemeRef ref=0;std::int32_t w=0;std::uint32_t ordinal=0;
    auto next=[&]{return bool(f>>code>>ref>>w>>ordinal>>k);};bool have=next();std::size_t n=0,queries=0;
    const auto activation=shared.activation_handle();
    while(have) {
      const auto current=code;ExactQueryCursor cursor;std::string error;
      check(scheme.begin_exact_query(current,epoch,&cursor,&error),"raw start");++queries;
      for(;;) {
        auto s=front([&]{return scheme.advance_exact_query(&cursor,budget,16);});
        check(scheme.batch_is_current(s.batch)&&shared.activation_is_current(activation),"query lifetime");
        check(s.batch.hits.size()<=16,"bounded raw batch");
        for(const auto& h:s.batch.hits) {
          check(have&&code==current&&h.runtime_ref==ref&&h.scheme_weight==w&&h.mapping_ordinal==ordinal,"raw complete reference mismatch");
          check(metadata(ref).identity.stable_key==key(k),"raw short ref full key");++n;have=next();
        }
        if(s.status==ExactQueryStepStatus::Complete||s.status==ExactQueryStepStatus::NoResults)break;
        check(s.status==ExactQueryStepStatus::InProgress||s.status==ExactQueryStepStatus::HitsAvailable,"raw step error");
      }
      check(!have||code!=current,"omitted raw suffix");scheme.cancel_query(&cursor);
    }
    check(f.eof()&&n==scheme.info().mapping_count,"all mapping count");
    std::cout<<"PASS T2 "<<name<<" raw="<<n<<" codes="<<queries<<'\n';mark("all_raw_mappings");
  }
  void state_open(bool create) {
    check(disk.begin_open(create)==StateStatus::Pending,"state begin");
    while(!disk.ready()){auto s=back([&]{return disk.advance_open(back_budget);});
      check(s.status==StateStatus::Pending||s.status==StateStatus::Ready,"state open");}
  }
  void output_checks(const std::filesystem::path& oracle) {
    std::ifstream f(oracle/"outputs.txt");RuntimeLexemeRef ref;std::string k,form,inv,expected,text;
    std::uint32_t same_payload=kNoSharedString;unsigned n=0;
    while(f>>ref>>k>>form>>inv>>expected>>text){
      std::array<char,128> names{};std::array<char,64> out{};OutputReader reader;
      check(reader.begin(shared,shared.activation_handle(),ref,key(k),{form,inv=="E"?OutputInvocation::Explicit:OutputInvocation::Profile},
        names.data(),names.size(),out.data(),out.size())==OutputStatus::Pending,"output begin");
      OutputStep s;do{s=front([&]{return reader.advance(budget);});}while(s.status==OutputStatus::Pending);
      OutputStatus want=OutputStatus::InvalidRequest;
      if(expected=="Exact")want=OutputStatus::Exact;
      if(expected=="Blocked")want=OutputStatus::Blocked;
      if(expected=="Condition")want=OutputStatus::ConditionUndetermined;
      if(expected=="Multiple")want=OutputStatus::MultipleRelations;
      if(expected=="Denied")want=OutputStatus::InvocationDenied;
      if(expected=="Fallback")want=OutputStatus::FallbackDefault;
      if(s.status!=want)std::cerr<<"output "<<form<<" expected="<<expected<<" got="<<int(s.status)<<'\n';
      check(s.status==want&&s.text==(text=="-"?"":unhex(text)),"output policy/UTF-8");
      if(form=="m6-traditional"){
        if(same_payload==kNoSharedString)same_payload=s.payload;
        else check(same_payload==s.payload,"same text payload was not shared");
      }
      reader.cancel();++n;
    }
    check(f.eof()&&n>=9,"output cases");std::cout<<"PASS T7 conditions/permissions/Blocked/missing fallback/shared payload cases="<<n<<'\n';mark("generic_output_checks");
  }
  std::uint32_t count(const StableLexemeKey& k) {
    auto t=counts.begin_count(counts.view(),k);
    for(;;){auto r=front([&]{return counts.advance_count(t,budget);});
      if(r.status==CountStatus::Found||r.status==CountStatus::NotFound){counts.cancel_count(t);return r.count;}
      check(r.status==CountStatus::Pending,"count unavailable");}
  }
  void rank_all(const std::filesystem::path& oracle,const std::string& name) {
    std::ifstream f(oracle/(name+"_rank.txt"));std::string code;std::size_t n,total=0,queries=0;
    while(f>>code>>n) {
      // Independent full-reference vector is test-only and excluded from the
      // runtime memory formula. W stays 8 regardless of this candidate count.
      std::vector<RankItem> expected(n);std::string k;
      for(auto& item:expected){check(bool(f>>k>>item.ref>>item.score>>item.path_count),"rank oracle");item.key=key(k);}
      std::array<RankItem,8> workspace{},result{};
      ExactRanker rank(scheme,shared,counts,workspace.data(),8,16,64);
      check(rank.begin_input(code,epoch)==RankStatus::InProgress,"rank begin");
      RankBoundary boundary;std::size_t pos=0;RankBoundary second_first;bool second=false;
      while(pos<n){RankTicket ticket;auto direction=pos?RangeDirection::After:RangeDirection::First;
        check(rank.request_range(direction,pos?&boundary:nullptr,8,result.data(),8,&ticket)==RankStatus::InProgress,"rank range");
        for(;;){auto r=front([&]{return rank.advance(ticket,budget);});if(r.status==RankStatus::Ready)break;
          check(r.status==RankStatus::InProgress||r.status==RankStatus::WaitingState,"rank not ready");}
        auto v=rank.result(ticket);check(v.size>0&&v.size<=8&&pos+v.size<=n,"rank bounded result");
        for(std::size_t i=0;i<v.size;++i){const auto& a=v.items[i];const auto& b=expected[pos+i];
          check(a.key==b.key&&a.ref==b.ref&&a.score==b.score&&a.path_count==b.path_count,"rank full reference");}
        if(pos==8){second_first=rank.boundary(ticket,0);second=true;}
        pos+=v.size;check(v.has_more==(pos<n),"rank continuation");boundary=rank.boundary(ticket,v.size-1);rank.release(ticket);
      }
      if(second){RankTicket t;check(rank.request_range(RangeDirection::Before,&second_first,8,result.data(),8,&t)==RankStatus::InProgress,"Before begin");
        for(;;){auto r=front([&]{return rank.advance(t,budget);});if(r.status==RankStatus::Ready)break;
          check(r.status==RankStatus::InProgress||r.status==RankStatus::WaitingState,"Before advance");}
        auto v=rank.result(t);check(v.size==8,"Before size");for(std::size_t i=0;i<8;++i)check(v.items[i].key==expected[i].key,"Before identity");rank.release(t);}
      total+=n;++queries;
    }
    check(f.eof()&&queries>5,"rank oracle complete");
    std::cout<<"PASS T3 "<<name<<" queries="<<queries<<" all_ranked_items="<<total<<" W=8 After/Before\n";mark("full_range_rank_rescans");
  }
  SessionStatus tick() {
    auto s=front([&]{return session->tick(budget);});counts.foreground_advanced();
    if (counts.background_due()) {
      back([&]{return counts.advance_state(back_budget).io;});
    }
    return s.status;
  }
  void create_session() {
    session=std::make_unique<CandidateSession>(scheme,shared,counts,sink,SessionConfig{8,8,16,64,128,64});
    check(session->valid()&&session->begin_text_access()==TextAccessStatus::Pending,"session begin");
    while(!session->text_access_ready()){auto s=front([&]{return session->advance_text_access(budget);});
      check(s.status==TextAccessStatus::Pending||s.status==TextAccessStatus::Ready,"all-resource text capacity");}
    mark("text_capacity_admission");
    auto m=session->memory_usage();auto sm=shared.memory_usage();
    std::cout<<"MEMORY shared_owner="<<sm.owner_fixed_bytes<<" shared_cache="<<sm.cache_payload_allocated_bytes
      <<" cache_metadata="<<sm.cache_metadata_allocated_bytes<<" scheme_pages="<<scheme.resident_page_bytes()
      <<" session_impl="<<m.impl_bytes<<" workspace="<<m.rank_work_bytes<<" result="<<m.rank_result_bytes
      <<" snapshots="<<m.snapshot_bytes<<" texts="<<m.text_pool_bytes<<" commit="<<m.commit_pool_bytes
      <<" names="<<m.name_bytes<<" H=4\n";
  }
  void publish() {for(;;){auto s=tick();if(s==SessionStatus::StagingReady)break;
      check(s==SessionStatus::Pending||s==SessionStatus::WaitingState,"session range");}
    check(session->publish(session->staging())==SessionStatus::Ready,"publish");}
  void choose(const std::string& code,const StableLexemeKey& wanted,const std::string& text,const std::string& form="") {
    session->cancel_input(session->input_handle());
    check(session->set_output({form,OutputInvocation::Explicit})==SessionStatus::Ready,"form configuration");
    check(session->begin_input(code,epoch,8,0)==SessionStatus::Pending,"session query");
    std::size_t slot=0;bool found=false,more=false;
    while(!found){publish();check(session->present([&](const CandidateViewPage& v){more=v.has_more;
      for(std::size_t i=0;i<v.size;++i){check(v.text(i).empty(),"identity before text");if(v.items[i].ranked.key==wanted){slot=i;found=true;}}})==SessionStatus::Ready,"view");
      if(!found){check(more,"identity candidate not reachable");check(session->request_range(RangeDirection::After,8,0)==SessionStatus::Pending,"next page");}}
    check(session->request_text(session->published(),slot,1)==SessionStatus::Pending,"visible request");
    for(;;){auto s=tick();if(s==SessionStatus::StagingReady)break;check(s==SessionStatus::Pending,"text step");}
    check(session->present([&](const CandidateViewPage& v){check(v.text(slot)==text,"selected generic text");})==SessionStatus::Ready,"text view");
    auto selected=session->selection(slot);const auto calls=sink.calls;
    check(session->confirm(selected).status==CommitStatus::Preparing,"confirm");
    CommitResult r;
    do{tick();r=session->commit_result();}while(r.status==CommitStatus::Preparing||r.status==CommitStatus::CallingSink);
    check(r.status==CommitStatus::Accepted&&sink.calls==calls+1,"actual accepted text once");
    check(session->confirm(selected).status==CommitStatus::Accepted&&sink.calls==calls+1,"old confirmation no second text");
    auto t=r.transaction.learning_ticket();
    while(counts.observe_learning(t).status==LearningStatus::Pending)back([&]{return counts.advance_state(back_budget).io;});
    check(counts.observe_learning(t).status==LearningStatus::RecordedDirty,"recorded learning");
    check(counts.notify_committed(r.transaction,ns,wanted)==LearningStatus::RecordedDirty,"notification idempotence");
  }
  void close() {
    session.reset();counts.begin_close();
    for(;;){auto s=back([&]{return counts.advance_close(back_budget).io;});if(s.status==StateStatus::ClosedSaved)break;
      check(s.status==StateStatus::Pending||s.status==StateStatus::Ready||s.status==StateStatus::HostStorageConfirmed||s.status==StateStatus::Found||s.status==StateStatus::NotFound,"close failed");}
    mark("close");std::cout<<"TOTAL resource_bytes="<<metrics.requested_bytes<<" reads="<<metrics.read_calls<<" max_host_read_ns="<<metrics.max_call_ns<<" peak_files="<<metrics.peak_open_files<<'\n';
    scheme.deactivate();shared.deactivate();
  }
};
const auto ren=key("SLK1:8c816cf96715d449d21f7bd8ff7335bd39b9f6ddbecae776649bbf3bdbd7b0db");
const auto machine=key("SLK1:e22fdc586736aa52e93e025cfb26125da5ee4e4fd815bc35f9bb094a714e3d8a");
const auto shi=key("SLK1:3da586e699dd499839e5febcc599199fdf81873e7d271eee13e3bf4ec3d68913");
const auto shi2=key("SLK1:52a02b7d8c3e37b29d9eb66bdceb4220f55071ba324cfe786d97f3d2504e3656");
}
int main(int argc,char** argv) {
  try{
    check(argc==5,"usage: production_host demo|verify RESOURCES ORACLE NEW_OR_EXISTING_STATE_DIR");
    const bool create=std::string(argv[1])=="demo";check(create||std::string(argv[1])=="verify","mode");
    std::filesystem::path resources=argv[2],oracle=argv[3],state=argv[4];
    if(create)check(std::filesystem::create_directory(state),"requires NEW state directory");
    else check(std::filesystem::is_directory(state),"requires existing test state");
    rank_probe::start();
    {rank_probe::Capture capture;App app(state);app.mount_shared(resources);app.state_open(create);
      app.all_lexemes(oracle);app.output_checks(oracle);app.mount_scheme(resources,"full_pinyin");
      if(create){app.raw_all(oracle,"full_pinyin");app.rank_all(oracle,"full_pinyin");
        app.mount_scheme(resources,"xiaohe");app.raw_all(oracle,"xiaohe");app.rank_all(oracle,"xiaohe");
        app.mount_scheme(resources,"full_pinyin");
        app.create_session();app.choose("ren",ren,"人");app.choose("jiqi",machine,"机器");app.choose("jiqi",machine,"機器","relation:R004");
        app.choose("shi",shi,"時","m6-traditional");app.choose("shi",shi2,"時","m6-traditional");
        check(app.count(shi)==1&&app.count(shi2)==1&&app.count(machine)==2,"same text distinct learning / multiple forms same identity");
        std::cout<<"PASS T4 generic forms, same text distinct selections/counts, actual commits\n";
        app.mark("five_full_pinyin_commits");
        std::cout<<"MEMORY post_query_shared_cache="<<app.shared.memory_usage().cache_payload_allocated_bytes
          <<" post_query_scheme_pages="<<app.scheme.resident_page_bytes()<<'\n';
        app.session.reset();app.mount_scheme(resources,"xiaohe");
        // Fresh exact scores now contain learned counts, so full reference
        // ranking was deliberately checked before learning, on full pinyin.
        app.create_session();app.choose("rf",ren,"人");check(app.count(ren)==2,"cross-scheme shared count");
        std::cout<<"PASS T5 independent schemes / shared learning\n";
      }else{
        check(app.count(ren)==2&&app.count(machine)==2&&app.count(shi)==1&&app.count(shi2)==1,"cross-process full-key persistence");
        std::cout<<"PASS T6 second process / reversed all refs / four exact learning keys\n";
      }
      app.close();}
    auto p=rank_probe::stop();
    std::cout<<"PROBE supported="<<p.supported<<" current="<<p.current<<" peak="<<p.peak<<" allocations="<<p.allocations
      <<" includes_test_oracle_and_log_allocations_not_all_malloc_or_stack\n";
    check(!p.supported||p.current==0,"tracked test scope allocations remain");
  }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
