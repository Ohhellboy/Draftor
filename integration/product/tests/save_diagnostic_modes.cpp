#include "src/hanime_document/save_diagnostic.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
namespace sd = hanime::documents::save_diag;
namespace doc = hanime::documents;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 86; } } while (0)
#if HANIME_SAVE_SUBSTAGE_DIAGNOSTIC
static unsigned ticks = 0;
static std::uint32_t clockTick() { return ++ticks * 10; }
#endif
int main() {
  std::printf("enabled=%d recorder_bytes=%zu phase_bytes=%zu scope_bytes=%zu\n",
              sd::kEnabled, sizeof(sd::Recorder),sizeof(sd::Phase),sizeof(sd::Scope));
  unsigned effects=0, checks=0;
  auto result=sd::measured(sd::Op::Write,7,[&]{++effects;return std::make_unique<int>(37);},
      [&](const auto& p){++checks;return *p==37 ? 1 : 0;});
  CHECK(result && *result==37 && effects==1 && checks==1);
  sd::measuredVoid(sd::Op::Close,[&]{++effects;});
  CHECK(effects==2);
  CHECK(sd::measuredStatus(sd::Op::Write,[]{return doc::Status::IoError;})==doc::Status::IoError);
  char line[1024]="untouched";
  doc::DocumentId id{};
#if HANIME_SAVE_SUBSTAGE_DIAGNOSTIC
  sd::recorder.clock=clockTick;
  CHECK(sd::recorder.start(73));
  CHECK(!sd::recorder.start(74));
  for (unsigned i=0;i<sd::kTransactions;++i) {
    CHECK(sd::recorder.begin(sd::Kind::BodyAuto,sd::Trigger::Idle,id,9,11));
    CHECK(!sd::recorder.begin(sd::Kind::BodyAuto,sd::Trigger::Idle,id,9,11));
    { sd::Scope span(sd::Op::Write,7); span.result(0); }
    { sd::Phase position(1); sd::Scope span(sd::Op::Flush); span.result(1); }
    doc::OperationResult completion{}; completion.status=doc::Status::Ok;
    completion.body_committed=true;
    sd::recorder.finish(completion,17,17,true,false,false,23,23);
    const auto& tx=sd::recorder.at(i);
    CHECK(tx.complete && tx.body_committed && tx.completion_revision==17);
    CHECK(tx.stats[0][unsigned(sd::Op::Write)].failures==1);
    CHECK(tx.stats[1][unsigned(sd::Op::Flush)].count==1);
    CHECK(tx.position_revision==23 && tx.completion_position_revision==23);
  }
  CHECK(!sd::recorder.active() && sd::recorder.pending());
  CHECK(!sd::recorder.nextLine(line,sizeof(line),false));
  CHECK(std::strcmp(line,"untouched")==0);
  CHECK(!sd::recorder.nextLine(line,8,true));
  unsigned rows=0; bool summary=false;
  while(sd::recorder.nextLine(line,sizeof(line),true)) {
    CHECK(++rows<500);
    if(std::strstr(line,"[SAVE_SUMMARY]")) {
      summary=true;
      CHECK(std::strstr(line,"count=4") && std::strstr(line,"dropped=4") &&
            std::strstr(line,"termination=BUFFER_FULL"));
    }
  }
  CHECK(summary && !sd::recorder.pending() && sd::recorder.canStart());
  CHECK(sd::recorder.start(74) && sd::recorder.stop());
  CHECK(sd::recorder.nextLine(line,sizeof(line),true));
  CHECK(std::strstr(line,"termination=USER_STOP"));
#else
  CHECK(std::is_empty_v<sd::Recorder> && std::is_empty_v<sd::Scope> && std::is_empty_v<sd::Phase>);
  CHECK(!sd::recorder.start(73) && !sd::recorder.active() && !sd::recorder.pending());
  CHECK(!sd::recorder.begin(sd::Kind::BodyAuto,sd::Trigger::Idle,id,9,11));
  { sd::Phase position(1); sd::Scope span(sd::Op::Flush); span.result(1); }
  CHECK(!sd::recorder.stop() && !sd::recorder.nextLine(line,sizeof(line),true));
  CHECK(std::strcmp(line,"untouched")==0);
#endif
  std::puts("save observer mode contract PASS");
}
