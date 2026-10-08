#include "hanime/host_resource_file.hpp"
#include "hanime/host_state_files.hpp"
#include "hanime/bounded_user_state.hpp"
#include "rank_allocation_probe.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace hanime;
namespace {
constexpr RegistryNamespaceId trial_ns=0x54465249414c0001ULL;
constexpr WorkBudget budget{1,17,3};
constexpr StateBudget state_budget{1,1,17,17,1,1};
constexpr SessionConfig config{2,1,1,16,64,32};
constexpr const char* ren="SLK1:8c816cf96715d449d21f7bd8ff7335bd39b9f6ddbecae776649bbf3bdbd7b0db";
constexpr const char* machine="SLK1:e22fdc586736aa52e93e025cfb26125da5ee4e4fd815bc35f9bb094a714e3d8a";
constexpr const char* ji="SLK1:6bc1aee0c170ce1a5d1226e59b18a99576e0840312d846f24db100378ea8c005";
constexpr const char* trad_ji="SLK1:159fb5ec3b2c2cccd7382dcf05e498483017095295fb915f46a052661ead1705";
constexpr const char* fa="SLK1:4e80b13749e7e0531d64ed53193ac754a7f65ac553806fbe7a040afa549aabb9";
constexpr const char* trad_fa="SLK1:d56bdff649f2307861300e96a7c458ae4e977b25b9a206d0948e0f0580cce457";
constexpr const char* hou="SLK1:3819e0a45d35a7d441314d42ca3ca4f3e824caa4e723d016df18cfc60a4b44cc";
constexpr const char* cai="SLK1:dcc30828b33fc9d6d31983255f482919b35d9b1d9b8ae7e18bfd2f0b506a23a9";
constexpr const char* noodles="SLK1:3a0cf9f6297e95741a7a109bcdb8529544d956910d73036d495a6338a06881b4";
constexpr const char* shi="SLK1:3da586e699dd499839e5febcc599199fdf81873e7d271eee13e3bf4ec3d68913";
constexpr const char* trad_shi="SLK1:52a02b7d8c3e37b29d9eb66bdceb4220f55071ba324cfe786d97f3d2504e3656";
void check(bool ok,const char* reason){if(!ok) throw std::runtime_error(reason);}
StableLexemeKey key(const char* s){StableLexemeKey k;check(parse_slk1(s,&k),"invalid expected key");return k;}
bool pending(SessionStatus s){return s==SessionStatus::Pending || s==SessionStatus::WaitingState;}
struct FixedSink final : TextCommitSink {
  std::array<char,16> last{}; std::size_t size{0},calls{0};
  bool commit(std::string_view text) override {
    check(text.size()<=last.size(),"sink capacity");
    std::memcpy(last.data(),text.data(),text.size()); size=text.size(); ++calls; return true;
  }
};
struct View {
  RankItem item; OutputStatus status{OutputStatus::Pending};
  std::array<char,16> text{}; std::size_t size{0}; std::uint32_t payload{0};
  bool more{false}; ScoringMode mode{ScoringMode::Exact};
  std::string_view value() const {return {text.data(),size};}
};
struct Sample {
  const char* stage{nullptr};
  std::uint64_t resource_reads{0},resource_bytes{0},resource_controls{0},resource_ns{0};
  StateCost state; std::uint64_t state_ns{0},wall_ns{0},work{0},steps{0};
};
struct Audit {
  std::array<Sample,96> stages{}; std::size_t size{0};
  HostResourceMetrics resource;
  std::uint64_t work{0},steps{0};
  SessionMemoryUsage session_memory;
  SharedMemoryUsage shared_memory;
  UserStateMemory state_memory; StateV3Memory file_memory;
  std::size_t scheme_resident{0};
  WorkCost shared_open;
  std::size_t scheme_initial_reads{0},scheme_initial_bytes{0};
  std::size_t shared_size{0},scheme_size{0};
  std::uint64_t notification_work{0};
};
struct Chain {
  Audit& audit;
  HostStateFiles files;
  StateV3 disk;
  BoundedUserState counts;
  PagedSharedResource shared;
  PagedSchemeIndex scheme;
  std::shared_ptr<HostResourceFile> shared_file,scheme_file;
  FixedSink sink;
  std::unique_ptr<CandidateSession> session;
  Chain(Audit& a,std::string_view state_a,std::string_view state_b):audit(a),files(state_a,state_b),
      disk(files,{trial_ns,0x545249414c3246ULL,17,8,16*1024*1024}),counts(disk,trial_ns,{4,64,8}) {}
  template<class F> void stage(const char* label,F fn) {
    check(audit.size<audit.stages.size(),"bounded measurement slots full");
    auto& s=audit.stages[audit.size++]; s.stage=label;
    const auto r=audit.resource; const auto f=files.metrics;
    const auto before_work=audit.work,before_steps=audit.steps;
    const auto start=std::chrono::steady_clock::now();
    try {fn();} catch(const std::exception& e) {throw std::runtime_error(std::string(label)+": "+e.what());}
    s.wall_ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
    s.resource_reads=audit.resource.read_calls-r.read_calls; s.resource_bytes=audit.resource.requested_bytes-r.requested_bytes;
    s.resource_controls=audit.resource.control_calls-r.control_calls; s.resource_ns=audit.resource.calls_ns-r.calls_ns;
    const auto& now=files.metrics.requests; const auto& old=f.requests;
    s.state={now.read_calls-old.read_calls,now.write_calls-old.write_calls,now.read_bytes-old.read_bytes,
             now.write_bytes-old.write_bytes,now.control_calls-old.control_calls,0};
    s.state_ns=files.metrics.calls_ns-f.calls_ns; s.work=audit.work-before_work; s.steps=audit.steps-before_steps;
  }
  template<class F> auto resource_step(F fn) {
    auto r=audit.resource; auto f=files.metrics.requests;
    auto result=fn(); const auto& c=result.cost;
    check(c.read_calls<=budget.max_read_calls && c.read_bytes<=budget.max_read_bytes && c.work_items<=budget.max_work_items,"front budget exceeded");
    check(audit.resource.read_calls-r.read_calls+files.metrics.requests.read_calls-f.read_calls==c.read_calls &&
          audit.resource.requested_bytes-r.requested_bytes+files.metrics.requests.read_bytes-f.read_bytes==c.read_bytes,"unmetered front read");
    check(audit.resource.control_calls==r.control_calls && files.metrics.requests.control_calls==f.control_calls &&
          files.metrics.requests.write_calls==f.write_calls,"front performed hidden control/write");
    audit.work+=c.work_items; ++audit.steps; return result;
  }
  template<class F> StateStep state_step(F fn) {
    const auto before=files.metrics.requests; auto r=fn(); const auto& c=r.cost; const auto& after=files.metrics.requests;
    check(c.read_calls<=1 && c.write_calls<=1 && c.read_bytes<=17 && c.write_bytes<=17 && c.control_calls<=1 && c.work_items<=1,"state budget exceeded");
    check(c.read_calls+c.write_calls+c.control_calls<=1,"more than one physical state call");
    check(after.read_calls-before.read_calls==c.read_calls && after.write_calls-before.write_calls==c.write_calls &&
          after.read_bytes-before.read_bytes==c.read_bytes && after.write_bytes-before.write_bytes==c.write_bytes &&
          after.control_calls-before.control_calls==c.control_calls,"unmetered state operation");
    audit.work+=c.work_items; ++audit.steps; return r;
  }
  void mount(std::string_view path) {
    shared_file=std::make_shared<HostResourceFile>(path,17,audit.resource);
    check(shared_file->valid(),"resource file open failed");
    std::array<char,18> invalid_read{};
    const auto calls=audit.resource.read_calls;
    check(!shared_file->read(0,invalid_read.data(),18) &&
          !shared_file->read(shared_file->size(),invalid_read.data(),1) &&
          audit.resource.read_calls==calls,"adapter length/range rejection did I/O");
    std::string error;
    check(shared.open(shared_file,trial_ns,0,9,{1,17},&error),"shared fixed open failed");
    audit.shared_open=shared.info().open_cost; audit.shared_size=shared_file->size();
    const auto v=shared.validation_cursor();
    for(unsigned i=0;i<1'000'000;++i) {
      auto r=resource_step([&]{return shared.advance_validation(v,budget);});
      if(r.status==ValidationStepStatus::Ready) {
        check(shared.info().sections[0].count==13,"expected 13 independently listed trial anchors"); return;
      }
      check(r.status==ValidationStepStatus::InProgress,"shared validation error");
    } throw std::runtime_error("shared validation stalled");
  }
  void activate_scheme(std::string_view path) {
    if(session) session->cancel_input(session->input_handle()); // Idle switch only.
    scheme.deactivate();
    if(scheme_file) check(scheme_file->close(),"old scheme close");
    scheme_file=std::make_shared<HostResourceFile>(path,17,audit.resource);
    check(scheme_file->valid(),"scheme file open failed");
    std::string error;
    check(scheme.open(scheme_file,trial_ns,shared.info().shared_bundle_id,13,{1,17},&error),"scheme fixed open failed");
    audit.scheme_initial_reads=scheme.info().initial_read_calls; audit.scheme_initial_bytes=scheme.info().initial_read_bytes;
    audit.scheme_size=scheme_file->size();
    for(unsigned i=0;i<1'000'000;++i) {
      auto r=resource_step([&]{return scheme.advance_validation(budget);});
      if(r.status==ValidationStepStatus::Ready) {
        if(session) check(session->bind_scheme(scheme)==SessionStatus::Ready,"idle scheme binding failed");
        check(audit.resource.open_files==2 && audit.resource.peak_open_files==2,"more than shared+active scheme opened");return;
      }
      check(r.status==ValidationStepStatus::InProgress,"scheme validation error");
    } throw std::runtime_error("scheme validation stalled");
  }
  void open_state(bool create) {
    check(disk.begin_open(create)==StateStatus::Pending,"state begin open");
    for(unsigned i=0;i<1'000'000;++i) {
      auto r=state_step([&]{return disk.advance_open(state_budget);});
      if(r.status==StateStatus::Ready) return;
      check(r.status==StateStatus::Pending,"state open not healthy");
    } throw std::runtime_error("state open stalled");
  }
  void admit() {
    session=std::make_unique<CandidateSession>(scheme,shared,counts,sink,config);
    check(session->valid() && session->begin_text_access()==TextAccessStatus::Pending,"text access begin");
    for(unsigned i=0;i<1'000'000;++i) {
      auto r=resource_step([&]{return session->advance_text_access(budget);});
      if(r.status==TextAccessStatus::Ready) return;
      check(r.status==TextAccessStatus::Pending,"text access failed");
    } throw std::runtime_error("admission stalled");
  }
  SessionStatus finish() {
    for(unsigned i=0;i<1'000'000;++i) {
      auto r=resource_step([&]{return session->tick(budget);});
      if(!pending(r.status)) return r.status;
    } throw std::runtime_error("session stalled");
  }
  View view() {
    View v;
    check(session->present([&](const CandidateViewPage& page){
      check(page.size==1,"Q=1 result limit");v.item=page.items[0].ranked;v.status=page.items[0].output;
      v.payload=page.items[0].payload;v.more=page.has_more;v.mode=page.mode;
      const auto text=page.text(0);check(text.size()<=v.text.size(),"bounded borrowed text");
      if(!text.empty()) std::memcpy(v.text.data(),text.data(),text.size());
      v.size=text.size();
    })==SessionStatus::Ready,"present not ready");return v;
  }
  void text() {
    check(session->request_text(session->published(),0,1)==SessionStatus::Pending,"request visible text");
    check(finish()==SessionStatus::StagingReady,"visible text failed");
  }
  void first(std::string_view code,std::string_view form="",OutputInvocation invocation=OutputInvocation::Profile,ScoringMode mode=ScoringMode::Exact) {
    session->cancel_input(session->input_handle());
    check(session->set_output({form,invocation})==SessionStatus::Ready,"idle output config");
    check(session->begin_input(code,9,1,0,mode)==SessionStatus::Pending,"begin exact input");
    check(finish()==SessionStatus::StagingReady,"final rank not ready");
    check(session->publish(session->staging())==SessionStatus::Ready,"publish ranked range");
  }
  void range(RangeDirection direction) {
    check(session->request_range(direction,1,0)==SessionStatus::Pending,"request logical range");
    check(finish()==SessionStatus::StagingReady,"range not ready");
    check(session->publish(session->staging())==SessionStatus::Ready,"publish range");
  }
  void find(std::string_view code,const char* expected,std::string_view form="",OutputInvocation invocation=OutputInvocation::Profile) {
    first(code,form,invocation);
    for(unsigned i=0;i<13;++i) {auto v=view();if(v.item.key==key(expected)){text();return;}
      check(v.more,"expected identity not reachable");range(RangeDirection::After);}
    throw std::runtime_error("unbounded candidate traversal");
  }
  std::uint32_t count(const char* expected) {
    auto t=counts.begin_count(counts.view(),key(expected));
    for(unsigned i=0;i<1'000'000;++i) {
      auto r=resource_step([&]{return counts.advance_count(t,budget);});
      if(r.status==CountStatus::Found || r.status==CountStatus::NotFound){counts.cancel_count(t);return r.count;}
      check(r.status==CountStatus::Pending,"count unavailable");
    } throw std::runtime_error("count stalled");
  }
  void flush() {
    for(unsigned i=0;i<1'000'000;++i) {
      auto h=counts.health();if(!h.dirty && !h.intent_pending && !h.frozen_write && !disk.info().maintaining) return;
      auto r=state_step([&]{return counts.advance_state(state_budget).io;});
      // IntentLoad may report Found/NotFound before its following increment step.
      // The owner's bounded health, not one nested lookup result, ends flushing.
      if(r.status!=StateStatus::Pending && r.status!=StateStatus::Ready && r.status!=StateStatus::HostStorageConfirmed &&
          r.status!=StateStatus::Found && r.status!=StateStatus::NotFound)
        throw std::runtime_error("state flush status="+std::to_string(static_cast<int>(r.status)));
    } throw std::runtime_error("flush stalled");
  }
  CommitResult commit(CommitStatus expected=CommitStatus::Accepted) {
    const auto selection=session->selection(0), old_selection=selection;
    const auto before=sink.calls;
    const auto notifications=counts.health().notifications;
    check(session->confirm(selection).status==CommitStatus::Preparing,"confirm did not prepare");
    for(unsigned i=0;i<1'000'000;++i) {
      const auto r=session->commit_result();
      if(r.status!=CommitStatus::Preparing && r.status!=CommitStatus::CallingSink) {
        check(r.status==expected,"commit status unexpected");
        const auto before_duplicate=audit.resource.read_calls;
        check(session->confirm(old_selection).status==expected,"duplicate confirmation changed result");
        check(audit.resource.read_calls==before_duplicate,"duplicate confirmation did I/O");
        check(sink.calls==before+(expected==CommitStatus::Accepted?1:0),"text re-submitted or restricted text accepted");
        check(counts.health().notifications==notifications+(expected==CommitStatus::Accepted?1:0),"learning notification count");
        if(expected==CommitStatus::Accepted) {
          const auto receipt=counts.observe_learning(r.transaction.learning_ticket());
          check(receipt.local_work<=8,"H+4 notification budget");
          audit.notification_work+=receipt.local_work;
          const auto view_before=counts.health().notifications;
          counts.notify_committed(r.transaction,trial_ns,key_from_receipt(r));
          check(counts.health().notifications==view_before,"replayed learning notification");
        }
        return r;
      }
      resource_step([&]{return session->tick(budget);});
    } throw std::runtime_error("commit stalled");
  }
  StableLexemeKey selected_key;
  StableLexemeKey key_from_receipt(const CommitResult&) const {return selected_key;}
  void accept(std::string_view expected) {
    const auto v=view();check(v.value()==expected,"selected output literal mismatch");selected_key=v.item.key;
    commit();check(std::string_view(sink.last.data(),sink.size)==expected,"sink text mismatch");
  }
  void restricted(std::string_view code,const char* k,std::string_view form,OutputInvocation invocation,OutputStatus expected) {
    find(code,k,form,invocation);check(view().status==expected && view().size==0,"restricted output bypassed or silently fell back");
    const auto before=count(k);commit(CommitStatus::OutputRestricted);check(count(k)==before,"restricted output learned");
  }
  void capture_residency() {
    audit.session_memory=session->memory_usage();audit.shared_memory=shared.memory_usage();
    audit.state_memory=counts.memory_usage();audit.file_memory=disk.memory_usage();audit.scheme_resident=scheme.resident_page_bytes();
  }
  void close() {
    if(shared.ready()) capture_residency();
    session->cancel_input(session->input_handle());
    session.reset(); counts.begin_close();
    for(unsigned i=0;i<1'000'000;++i) {
      auto r=state_step([&]{return counts.advance_close(state_budget).io;});
      if(r.status==StateStatus::ClosedSaved) {
        scheme.deactivate();shared.deactivate();check(scheme_file->close() && shared_file->close(),"resource close failed");return;
      }
      check(r.status==StateStatus::Pending || r.status==StateStatus::HostStorageConfirmed || r.status==StateStatus::Ready ||
            r.status==StateStatus::Found || r.status==StateStatus::NotFound,"close not saved");
    } throw std::runtime_error("close stalled");
  }
};
void print(const Audit& a,rank_probe::Statistics allocation) {
  for(std::size_t i=0;i<a.size;++i) {
    const auto& s=a.stages[i];
    std::cout<<"STAGE "<<s.stage<<" resource_reads="<<s.resource_reads<<" resource_bytes="<<s.resource_bytes<<" resource_controls="<<s.resource_controls
      <<" state_reads="<<s.state.read_calls<<" state_read_bytes="<<s.state.read_bytes<<" state_writes="<<s.state.write_calls<<" state_write_bytes="<<s.state.write_bytes
      <<" state_controls="<<s.state.control_calls<<" work="<<s.work<<" steps="<<s.steps<<" resource_ns="<<s.resource_ns<<" state_ns="<<s.state_ns<<" wall_ns="<<s.wall_ns<<'\n';
  }
  std::cout<<"MEMORY full_runtime_default_new_peak="<<allocation.peak<<" after_destruction="<<allocation.current<<" allocations="<<allocation.allocations
    <<" probe_supported="<<allocation.supported<<" Chain_stack_object="<<sizeof(Chain)<<" Audit_test_stack="<<sizeof(Audit)
    <<" HostResourceFile="<<sizeof(HostResourceFile)<<" HostStateFiles="<<sizeof(HostStateFiles)
    <<" state_impl="<<a.state_memory.impl<<" state_slots="<<a.state_memory.slots<<" state_file_impl="<<a.file_memory.impl
    <<" shared_owner="<<a.shared_memory.owner_fixed_bytes<<" shared_cache="<<a.shared_memory.cache_payload_allocated_bytes
    <<" scheme_resident="<<a.scheme_resident<<" session_impl="<<a.session_memory.impl_bytes
    <<" session_snapshots="<<a.session_memory.snapshot_bytes<<" session_text="<<a.session_memory.text_pool_bytes
    <<" session_commit="<<a.session_memory.commit_pool_bytes<<" peak_open_resource_files="<<a.resource.peak_open_files<<'\n';
  std::cout<<"TOTAL_RESOURCE calls="<<a.resource.read_calls<<" requested_bytes="<<a.resource.requested_bytes
    <<" delivered_bytes="<<a.resource.delivered_bytes<<" control_calls="<<a.resource.control_calls
    <<" max_call_ns="<<a.resource.max_call_ns<<" learning_notification_work="<<a.notification_work<<'\n';
  std::cout<<"FILES shared="<<a.shared_size<<" scheme="<<a.scheme_size<<" fixed_shared_open_reads="<<a.shared_open.read_calls
    <<" fixed_shared_open_bytes="<<a.shared_open.read_bytes<<" fixed_scheme_open_reads="<<a.scheme_initial_reads<<" fixed_scheme_open_bytes="<<a.scheme_initial_bytes<<'\n';
}
}

int main(int argc,char** argv) {
  try {
    if(argc!=4) throw std::runtime_error("usage: hanime_real_host write|read RESOURCE_DIR DEDICATED_STATE_DIR");
    const bool writing=std::string_view(argv[1])=="write";
    check(writing || std::string_view(argv[1])=="read","invalid command");
    const std::filesystem::path resources(argv[2]),state_dir(argv[3]);
    // Harness path construction / filesystem directory setup excluded, before capture.
    // Every adapter copies at most 512 B; all owner/token/buffer allocations BELOW captured.
    if(writing) {check(!std::filesystem::exists(state_dir/"a.state") && !std::filesystem::exists(state_dir/"b.state"),"refusing existing learning store");std::filesystem::create_directories(state_dir);}
    const auto shared_path=(resources/"shared.hns").string(),full_path=(resources/"full_pinyin.hnm").string(),xiaohe_path=(resources/"xiaohe.hnm").string();
    const auto state_a=(state_dir/"a.state").string(),state_b=(state_dir/"b.state").string();
    Audit audit;
    rank_probe::start();
    {
      rank_probe::Capture all_runtime;
      Chain h(audit,state_a,state_b);
      h.stage("resource_shared_open_validation",[&]{h.mount(shared_path);});
      h.stage("resource_full_open_validation",[&]{h.activate_scheme(full_path);});
      h.stage("state_open_validation",[&]{h.open_state(writing);});
      h.stage("text_admission",[&]{h.admit();});
      if(writing) {
        h.stage("state_empty_lookup",[&]{check(h.count(ren)==0,"new isolated store not empty");});
        h.stage("first_rank_no_text",[&]{h.first("ren");check(h.view().item.key==key(ren) && h.view().item.ref==1 && h.view().size==0,"real ren key/ref or eager text");});
        h.stage("visible_text",[&]{h.text();check(h.view().value()=="人","real ren output");});
        h.stage("submit_text_and_notify",[&]{h.accept("人");});
        h.stage("learning_writeback",[&]{h.flush();check(h.count(ren)==1,"first learning");});
        h.stage("state_hot_known",[&]{const auto calls=h.files.metrics.requests.read_calls;
          check(h.count(ren)==1 && h.files.metrics.requests.read_calls==calls,"known hot count performed file I/O");});
        h.stage("idle_switch_xiaohe",[&]{h.activate_scheme(xiaohe_path);});
        h.stage("shared_learning_second_scheme",[&]{h.find("rf",ren);check(h.view().item.ref==1 && h.view().item.effective_count==1,"cross scheme count");h.accept("人");h.flush();check(h.count(ren)==2,"shared count increment");});
        h.stage("word_default_optional_output",[&]{h.find("jiqi",machine);h.accept("机器");h.flush();h.find("jiqi",machine,"opencc-s2t-1.4.2");h.accept("機器");h.flush();check(h.count(machine)==2,"word outputs split learning key");});
        h.stage("distinct_ji_anchors",[&]{h.find("ww",ji,"opencc-s2t-1.4.2");h.accept("機");h.flush();h.find("ji",trad_ji);h.accept("機");h.flush();check(h.count(ji)==1 && h.count(trad_ji)==1,"ji identities merged");});
        View first_shi,second_shi;
        h.stage("same_text_first",[&]{h.first("ui","m6-traditional");h.text();first_shi=h.view();check(first_shi.item.key==key(shi) && first_shi.value()=="時" && first_shi.more,"same-text first literal");});
        h.stage("same_text_after",[&]{h.range(RangeDirection::After);h.text();second_shi=h.view();check(second_shi.item.key==key(trad_shi) && second_shi.value()=="時" && !second_shi.more,"same-text after literal");check(first_shi.payload==second_shi.payload && first_shi.item.ref!=second_shi.item.ref,"payload dedup/identity separation");});
        h.stage("same_text_before",[&]{h.range(RangeDirection::Before);h.text();check(h.view().item.key==key(shi),"bounded before reconstruction");});
        h.stage("same_text_independent_learning",[&]{h.accept("時");h.flush();h.find("ui",trad_shi,"m6-traditional");h.accept("時");h.flush();h.find("ui",shi,"m6-traditional");h.accept("時");h.flush();check(h.count(shi)==2 && h.count(trad_shi)==1,"same text shared count incorrectly");});
        h.stage("condition_permission_and_observation",[&]{
          h.restricted("fa",fa,"opencc-s2t-1.4.2",OutputInvocation::Profile,OutputStatus::MultipleRelations);
          h.restricted("fa",fa,"relation:R005",OutputInvocation::Explicit,OutputStatus::ConditionUndetermined);
          h.restricted("hz",hou,"relation:R011",OutputInvocation::Explicit,OutputStatus::ConditionUndetermined);
          h.find("hz",hou,"source-preserve");check(h.view().status==OutputStatus::Exact && h.view().value()=="后","source preserve wrongly condition-gated");
          h.restricted("cd",cai,"relation:R036",OutputInvocation::Profile,OutputStatus::InvocationDenied);
          h.restricted("cd",cai,"relation:R036",OutputInvocation::Expanded,OutputStatus::ConditionUndetermined);
          h.restricted("fa",trad_fa,"opencc-s2t-1.4.2",OutputInvocation::Profile,OutputStatus::Blocked);
          h.restricted("mmtn",noodles,"relation:R028",OutputInvocation::Explicit,OutputStatus::Blocked);
          h.restricted("rf",ren,"product-default-traditional",OutputInvocation::Profile,OutputStatus::Blocked);
          h.find("rf",ren,"not-declared");check(h.view().status==OutputStatus::FallbackDefault && h.view().value()=="人","genuine absent form fallback");
        });
        h.stage("lifecycle_baseonly_and_pending_mapping",[&]{
          h.first("rf","",OutputInvocation::Profile,ScoringMode::BaseOnly);h.text();auto old=h.session->selection(0);
          h.first("jiqi","",OutputInvocation::Profile,ScoringMode::BaseOnly);
          check(h.session->confirm(old).status==CommitStatus::Stale,"old input selected");
          h.text();check(h.view().value()=="机器" && h.view().mode==ScoringMode::BaseOnly,"old work cleared new input or changed BaseOnly");
          h.session->cancel_input(h.session->input_handle());
          check(h.session->begin_input("ee",9,1,0)==SessionStatus::Pending && h.finish()==SessionStatus::NoMatches,"PendingIdentity U+28FCD silently enabled");
          h.activate_scheme(full_path);
          h.find("jiqi",machine,"opencc-s2t-1.4.2");
          check(h.view().item.effective_count==2 && h.view().value()=="機器","full-pinyin word output did not share xiaohe count");
          h.session->cancel_input(h.session->input_handle());
          check(h.session->begin_input("fa",9,1,0)==SessionStatus::Pending && h.finish()==SessionStatus::NoMatches,"Pending full-pinyin mapping guessed");
          h.first("ren");h.text();h.capture_residency();old=h.session->selection(0);h.shared.deactivate();
          check(h.session->confirm(old).status==CommitStatus::Stale,"old resource selection survived deactivation");
        });
      } else {
        h.stage("state_cold_persisted",[&]{const auto calls=h.files.metrics.requests.read_calls;
          check(h.count(ren)==2 && h.files.metrics.requests.read_calls>calls,"cold read did not read persisted count");});
        h.stage("reordered_resource_restart",[&]{h.first("ren");h.text();check(h.view().item.ref==13 && h.view().item.key==key(ren) && h.view().item.effective_count==2 && h.view().value()=="人","reordered ref lost persistent full key");});
        h.stage("all_learned_keys_restart",[&]{check(h.count(machine)==2 && h.count(ji)==1 && h.count(trad_ji)==1 && h.count(shi)==2 && h.count(trad_shi)==1,"restarted counts mismatch");});
        h.stage("restart_idle_switch_and_output",[&]{h.activate_scheme(xiaohe_path);h.find("jiqi",machine,"opencc-s2t-1.4.2");check(h.view().value()=="機器" && h.view().item.ref==12 && h.view().item.effective_count==2,"restart word form/key");});
      }
      h.stage("explicit_close",[&]{h.close();});
      check(audit.resource.open_files==0,"resource descriptors not closed");
    }
    auto allocation=rank_probe::stop();
    check(!allocation.supported || allocation.current==0,"runtime requested-new not released after destruction");
    print(audit,allocation);
    if(writing) std::cout<<"PASS F6 actual offset files / strict budgets / single active scheme\nPASS F7 cross-scheme and word-output shared learning\nPASS F8 same-text distinct identity / First After Before / payload sharing\nPASS F9 runtime conditions / invocation / Blocked / genuine fallback\nPASS F10 lifecycle / BaseOnly / pending identity and mapping\nPASS F11 once-only submit learning / explicit close / whole-path requested-new capture\n";
    else std::cout<<"PASS F12 independent process + reordered refs preserve six full-key counts\n";
  } catch(const std::exception& error){std::cerr<<"FAIL real-resource Host: "<<error.what()<<'\n';return 1;}
}
