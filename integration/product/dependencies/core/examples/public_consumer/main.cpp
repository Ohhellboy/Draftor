// External application orchestration only. No src/, tests/ or private headers.
#include <hanime/host_resource_file.hpp>
#include <hanime/host_state_files.hpp>
#include <hanime/bounded_user_state.hpp>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace hanime;
namespace {
constexpr RegistryNamespaceId trial_ns = 0x54465249414c0001ULL;
constexpr MountEpoch epoch = 9;
constexpr WorkBudget front_budget{1,17,3};
constexpr StateBudget back_budget{1,1,17,17,1,1};
constexpr SessionConfig session_config{2,1,1,16,64,32};
constexpr unsigned max_steps = 1'000'000; // Demo safety cap, not an engine timeout.
void require(bool value, const char* reason) {
  if (!value) throw std::runtime_error(reason);
}
StableLexemeKey expected_key(const char* value) {
  StableLexemeKey key;
  require(parse_slk1(value,&key),"invalid example identity literal");
  return key;
}
const auto ren_key = expected_key("SLK1:8c816cf96715d449d21f7bd8ff7335bd39b9f6ddbecae776649bbf3bdbd7b0db");
const auto machine_key = expected_key("SLK1:e22fdc586736aa52e93e025cfb26125da5ee4e4fd815bc35f9bb094a714e3d8a");

// This is the example application's actual bounded text destination, not a
// staging sink pretending that some other editor has accepted a combination.
struct DemoDocument final : TextCommitSink {
  std::array<char,64> bytes{};
  std::size_t used{0}, calls{0};
  bool commit(std::string_view text) override {
    ++calls;
    if (text.size()>bytes.size()-used) return false; // Proven no side effect.
    if (!text.empty()) std::memcpy(bytes.data()+used,text.data(),text.size());
    used+=text.size();
    return true; // The whole text is now owned by this document.
  }
};
struct OwnedDisplay {
  StableLexemeKey key;
  std::array<char,16> bytes{};
  std::size_t size{0};
  bool more{false};
  OutputStatus output{OutputStatus::Pending};
};
struct FragmentFact { RawQueryHit path; StableLexemeKey key; };
struct Fragment {
  std::size_t start, end; // Application's byte offsets in explicitly given renjiqi.
  const char* code;
  std::array<FragmentFact,8> facts{}; // Demo storage limit; never silent truncation.
  std::size_t size{0};
};

class Application {
 public:
  HostResourceMetrics resource_metrics; // Must outlive both resource readers.
  HostStateFiles files;
  StateV3 disk;
  BoundedUserState counts;
  PagedSharedResource shared;
  PagedSchemeIndex scheme;
  DemoDocument document;
  std::unique_ptr<CandidateSession> session;
  std::size_t foreground_steps{0}, background_steps{0};

  explicit Application(const std::filesystem::path& state_dir)
      : files((state_dir/"a.state").string(),(state_dir/"b.state").string()),
        disk(files,{trial_ns,0x41504944454d4f01ULL,17,8,16*1024*1024}),
        counts(disk,trial_ns,{4,64,8}) {
    require(files.valid() && counts.valid(),"invalid Host state configuration");
  }
  template<class F> auto front(F operation) {
    auto result=operation();
    require(result.cost.read_calls<=front_budget.max_read_calls &&
            result.cost.read_bytes<=front_budget.max_read_bytes &&
            result.cost.work_items<=front_budget.max_work_items,"foreground budget exceeded");
    ++foreground_steps;
    return result;
  }
  template<class F> StateStep back(F operation) {
    auto result=operation(); const auto& c=result.cost;
    require(c.read_calls<=1 && c.write_calls<=1 && c.control_calls<=1 &&
            c.read_bytes<=17 && c.write_bytes<=17 && c.work_items<=1 &&
            c.read_calls+c.write_calls+c.control_calls<=1,"background budget exceeded");
    ++background_steps;
    return result;
  }
  void mount(const std::filesystem::path& resources) {
    auto reader=std::make_shared<HostResourceFile>((resources/"shared.hns").string(),17,resource_metrics);
    require(reader->valid(),"shared file open failed"); std::string error;
    require(shared.open(reader,trial_ns,0,epoch,{1,17},&error),"shared fixed header failed");
    auto validation=shared.validation_cursor();
    for (unsigned n=0; !shared.ready(); ++n) {
      require(n<max_steps,"shared validation stalled");
      auto step=front([&]{return shared.advance_validation(validation,front_budget);});
      require(step.status==ValidationStepStatus::InProgress || step.status==ValidationStepStatus::Ready,"shared validation failed");
    }
    reader=std::make_shared<HostResourceFile>((resources/"full_pinyin.hnm").string(),17,resource_metrics);
    require(reader->valid(),"scheme file open failed");
    require(scheme.open(reader,trial_ns,shared.info().shared_bundle_id,
                        shared.info().sections[0].count,{1,17},&error),"scheme pairing failed");
    for (unsigned n=0; !scheme.ready(); ++n) {
      require(n<max_steps,"scheme validation stalled");
      auto step=front([&]{return scheme.advance_validation(front_budget);});
      require(step.status==ValidationStepStatus::InProgress || step.status==ValidationStepStatus::Ready,"scheme validation failed");
    }
    require(scheme.info().scheme_id=="full_pinyin","unexpected scheme");
    std::cout<<"PASS P1 paired files fully validated, epoch="<<epoch<<'\n';
  }
  void open_state(bool create) {
    require(disk.begin_open(create)==StateStatus::Pending,"state open refused");
    for (unsigned n=0; !disk.ready(); ++n) {
      require(n<max_steps,"state open stalled");
      auto step=back([&]{return disk.advance_open(back_budget);});
      require(step.status==StateStatus::Pending || step.status==StateStatus::Ready,"state validation failed");
    }
  }
  void fragments() {
    // Run before creating CandidateSession: it needs the same exclusive ports.
    require(!session,"raw fragments require exclusive query/read ownership");
    const auto activation=shared.activation_handle();
    std::array<Fragment,2> pieces{{{0,3,"ren",{},0},{3,7,"jiqi",{},0}}};
    QueryBatchContext previous;
    for (auto& piece:pieces) {
      ExactQueryCursor cursor; std::string error;
      require(scheme.begin_exact_query(piece.code,epoch,&cursor,&error),"raw begin failed");
      if (previous.index_identity) require(!scheme.query_is_current(previous),"old raw query revived");
      previous=scheme.query_context(cursor);
      bool complete=false;
      for (unsigned n=0; !complete; ++n) {
        require(n<max_steps,"raw query stalled");
        require(shared.activation_is_current(activation),"shared activation changed");
        auto step=front([&]{return scheme.advance_exact_query(&cursor,front_budget,1);});
        require(scheme.batch_is_current(step.batch),"stale raw batch");
        require(step.batch.hits.size()<=1,"raw batch capacity exceeded");
        for (const auto& hit:step.batch.hits) {
          require(piece.size<piece.facts.size(),"demo fragment capacity; not a truncated result");
          require(shared.activation_is_current(activation),"stale before metadata read");
          SharedReadCursor read;
          require(shared.begin_read(SharedRecordKind::Lexeme,hit.runtime_ref,&read)==SharedReadStatus::InProgress,"lexeme begin failed");
          SharedReadResult metadata;
          for (unsigned k=0;;++k) {
            require(k<max_steps,"metadata stalled");
            metadata=front([&]{return shared.advance_read(&read,front_budget);});
            if (metadata.status==SharedReadStatus::Ready) break;
            require(metadata.status==SharedReadStatus::InProgress,"metadata read failed");
          }
          require(shared.activation_is_current(activation) && shared.result_is_current(metadata) &&
                  scheme.batch_is_current(step.batch),"stale before fragment delivery");
          piece.facts[piece.size++]={hit,metadata.lexeme.identity.stable_key};
          shared.cancel_read(&read); // Conditioned on this exact operation.
        }
        complete=step.status==ExactQueryStepStatus::Complete || step.status==ExactQueryStepStatus::NoResults;
        require(complete || step.status==ExactQueryStepStatus::InProgress ||
                step.status==ExactQueryStepStatus::HitsAvailable,"raw query error/cancel/stale");
      }
      scheme.cancel_query(&cursor);
      require(piece.size==1,"this explicit fragment should have one raw path");
    }
    require(shared.activation_is_current(activation),"fragment references expired");
    require(pieces[0].facts[0].key==ren_key && pieces[1].facts[0].key==machine_key,"fragment identity mismatch");
    for (const auto& piece:pieces)
      std::cout<<"FRAGMENT ["<<piece.start<<','<<piece.end<<") code="<<piece.code
               <<" ref="<<piece.facts[0].path.runtime_ref<<" key="<<format_slk1(piece.facts[0].key)<<'\n';
    std::cout<<"PASS P2 two specified fragments retain paths/keys; no splitting or combined submission\n";
  }
  std::uint32_t count(const StableLexemeKey& key) {
    auto ticket=counts.begin_count(counts.view(),key);
    for (unsigned n=0;;++n) {
      require(n<max_steps,"count stalled");
      auto step=front([&]{return counts.advance_count(ticket,front_budget);});
      if (step.status==CountStatus::Found || step.status==CountStatus::NotFound) {
        counts.cancel_count(ticket); return step.count;
      }
      require(step.status==CountStatus::Pending,"state unavailable/stale, not zero");
    }
  }
  void create_session() {
    session=std::make_unique<CandidateSession>(scheme,shared,counts,document,session_config);
    require(session->valid(),"session allocation/configuration failed");
    require(session->begin_text_access()==TextAccessStatus::Pending,"text admission failed");
    for (unsigned n=0;!session->text_access_ready();++n) {
      require(n<max_steps,"text admission stalled");
      auto step=front([&]{return session->advance_text_access(front_budget);});
      require(step.status==TextAccessStatus::Pending || step.status==TextAccessStatus::Ready,"text capacity or read error");
    }
  }
  SessionStatus tick() {
    const auto step=front([&]{return session->tick(front_budget);});
    counts.foreground_advanced();
    // A distinct grant, not extra work hidden inside the session's front budget.
    if (counts.background_due()) back([&]{return counts.advance_state(back_budget).io;});
    return step.status;
  }
  void finish_range() {
    for (unsigned n=0;;++n) {
      require(n<max_steps,"session stalled");
      auto s=tick();
      if (s==SessionStatus::StagingReady) return;
      require(s==SessionStatus::Pending || s==SessionStatus::WaitingState,"range not Ready: handle error/restart externally");
    }
  }
  OwnedDisplay display() {
    OwnedDisplay copy;
    require(session->present([&](const CandidateViewPage& view) {
      require(view.size==1,"example requests Q=1");
      copy.key=view.items[0].ranked.key; copy.output=view.items[0].output; copy.more=view.has_more;
      auto text=view.text(0); require(text.size()<=copy.bytes.size(),"display destination too small");
      if (!text.empty()) std::memcpy(copy.bytes.data(),text.data(),text.size());
      copy.size=text.size(); // No borrowed pointer escapes this callback.
    })==SessionStatus::Ready,"published view expired");
    return copy;
  }
  void publish_range() {
    finish_range(); require(session->publish(session->staging())==SessionStatus::Ready,"publish failed");
  }
  void page_demo() {
    require(session->begin_input("shi",epoch,1,0)==SessionStatus::Pending,"input begin failed");
    publish_range(); const auto first=display();
    require(first.size==0 && first.output==OutputStatus::Pending && first.more,"ranked identity should precede text");
    require(session->request_text(session->published(),0,1)==SessionStatus::Pending,"text request failed");
    finish_range(); require(display().size!=0,"visible text not ready");
    require(session->request_range(RangeDirection::After,1,1)==SessionStatus::Pending,"After failed");
    publish_range(); require(display().key!=first.key,"After duplicated first identity");
    require(session->request_range(RangeDirection::Before,1,1)==SessionStatus::Pending,"Before failed");
    publish_range(); require(display().key==first.key,"Before lost first identity");
    std::cout<<"PASS P3 bounded First/After/Before; text did not advance page boundary\n";
  }
  void submit_once() {
    require(session->begin_input("ren",epoch,1,0)==SessionStatus::Pending,"ren input failed");
    publish_range(); require(display().key==ren_key,"wrong identity");
    const auto selection=session->selection(0);
    require(session->confirm(selection).status==CommitStatus::Preparing,"confirm did not prepare");
    CommitResult result;
    for (unsigned n=0;;++n) {
      require(n<max_steps,"commit stalled"); result=session->commit_result();
      if (result.status!=CommitStatus::Preparing && result.status!=CommitStatus::CallingSink) break;
      tick();
    }
    // Other hosts must handle RetryableNoEffect/UnknownOutcome as documented;
    // never repeat confirm or insertion merely because learning is Pending.
    require(result.status==CommitStatus::Accepted,"document did not accept text");
    require(document.calls==1 && std::string_view(document.bytes.data(),document.used)=="人","text not inserted exactly once");
    require(session->confirm(selection).status==CommitStatus::Accepted && document.calls==1,"duplicate confirm inserted text");
    const auto learning=result.transaction.learning_ticket();
    for (unsigned n=0;counts.observe_learning(learning).status==LearningStatus::Pending;++n) {
      require(n<max_steps,"learning stalled"); back([&]{return counts.advance_state(back_budget).io;});
    }
    require(counts.observe_learning(learning).status==LearningStatus::RecordedDirty,"unexpected learning outcome");
    session.reset(); // Session end does not flush/drop accepted learning.
    require(count(ren_key)==1 && counts.health().notifications==1,"wrong single-key learning count");
    std::cout<<"PASS P4 one Accepted text / one single-key learning receipt; no combined commit\n";
  }
  void close() {
    session.reset(); counts.begin_close();
    for (unsigned n=0;;++n) {
      require(n<max_steps,"close stalled; retain unsaved state");
      const auto result=back([&]{return counts.advance_close(back_budget).io;});
      if (result.status==StateStatus::ClosedSaved) break;
      require(result.status==StateStatus::Pending || result.status==StateStatus::Ready ||
              result.status==StateStatus::HostStorageConfirmed || result.status==StateStatus::Found ||
              result.status==StateStatus::NotFound,"close unsaved: do not claim durable completion");
    }
    scheme.deactivate(); shared.deactivate();
    std::cout<<"PASS P5 ClosedSaved; front_steps="<<foreground_steps<<" back_steps="<<background_steps
             <<" resource_bytes="<<resource_metrics.requested_bytes<<" state_read_bytes="<<files.metrics.requests.read_bytes
             <<" state_write_bytes="<<files.metrics.requests.write_bytes<<'\n';
  }
};
} // namespace

int main(int argc,char** argv) {
  try {
    require(argc==4,"usage: hanime_consumer demo|verify RESOURCE_ORDER_DIR STATE_DIR");
    const std::string mode=argv[1]; const std::filesystem::path resources=argv[2], state_dir=argv[3];
    require(mode=="demo" || mode=="verify","unknown mode");
    if (mode=="demo") require(std::filesystem::create_directory(state_dir),"demo requires a NEW state directory; never overwrite user state");
    else require(std::filesystem::is_directory(state_dir),"verify requires existing demo state");
    Application app(state_dir); app.mount(resources); app.open_state(mode=="demo");
    app.fragments();
    if (mode=="demo") {
      require(app.count(ren_key)==0,"demo state must start empty");
      app.create_session(); app.page_demo(); app.submit_once();
    } else {
      require(app.count(ren_key)==1 && app.count(machine_key)==0,"reordered resource restart changed learning identity");
      std::cout<<"PASS P6 new process / reordered refs / learned ren=1, unsubmitted jiqi=0\n";
    }
    app.close();
    return 0;
  } catch (const std::exception& e) {
    std::cerr<<"FAIL: "<<e.what()<<" (state files retained for inspection; no automatic text retry)\n";
    return 1;
  }
}
