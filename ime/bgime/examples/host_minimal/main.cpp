#include <hanime/candidate_session.hpp>
#include <hanime/host_resource_file.hpp>
#include <hanime/small_fixture.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace hanime;
constexpr MountEpoch epoch = 1;
constexpr WorkBudget budget{1, 17, 3};
constexpr std::size_t max_steps = 100'000; // Host guard against a stalled demo.
constexpr SessionConfig session_config{4, 1, 1, 64, 64, 32};

void require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}

template<class Step> Step checked(Step step) {
  require(step.cost.read_calls <= budget.max_read_calls &&
          step.cost.read_bytes <= budget.max_read_bytes &&
          step.cost.work_items <= budget.max_work_items, "Core exceeded a work budget");
  return step;
}

template<class Advance> void validate(Advance advance) {
  for (std::size_t n = 0; n < max_steps; ++n) {
    const auto step = checked(advance());
    if (step.status == ValidationStepStatus::Ready) return;
    require(step.status == ValidationStepStatus::InProgress,
            "Resource validation failed: " + step.error);
  }
  throw std::runtime_error("Resource validation stalled");
}

// Example-local adapter, not a learning implementation. Like the BaseOnly and
// rejected-learning cases in Core's session tests, it never supplies user counts
// or records a commit. Unavailable is deliberately not reported as a zero count.
class NoLearning final : public CommitCountSource {
 public:
  mutable std::size_t count_calls{0};
  std::size_t notifications{0};
  StateView view() const override { ++count_calls; return {}; }
  bool view_is_current(const StateView&) const override { ++count_calls; return false; }
  CountTicket begin_count(const StateView&, const StableLexemeKey&) override {
    ++count_calls;
    return {};
  }
  CountResult advance_count(const CountTicket& ticket, const WorkBudget&) override {
    ++count_calls;
    return {CountStatus::Unavailable, ticket, 0, {}};
  }
  void cancel_count(const CountTicket&) override { ++count_calls; }
  LearningStatus notify_committed(const CommitHandle&, RegistryNamespaceId,
                                  const StableLexemeKey&) noexcept override {
    ++notifications;
    return LearningStatus::PausedUnavailable;
  }
};

// This is the actual text destination. It owns a complete copy before accepting.
class DemoDocument final : public TextCommitSink {
 public:
  std::array<char, 64> bytes{};
  std::size_t used{0}, calls{0};
  bool commit(std::string_view text) override {
    ++calls;
    if (text.size() > bytes.size() - used) return false; // No text was inserted.
    if (!text.empty()) std::memcpy(bytes.data() + used, text.data(), text.size());
    used += text.size();
    return true;
  }
  std::string_view text() const { return {bytes.data(), used}; }
};

struct Resources {
  // Readers retain references to metrics; resources outlive the session.
  HostResourceMetrics metrics;
  PagedSharedResource shared;
  PagedSchemeIndex full, xiaohe;

  explicit Resources(const std::filesystem::path& directory) {
    auto reader = std::make_shared<HostResourceFile>(
        (directory / "shared.hns").string(), budget.max_read_bytes, metrics);
    require(reader->valid(), "Cannot open fixture shared.hns");
    std::string error;
    // Zero discovers this immutable bundle's ID; every scheme is then paired
    // with that ID and the fixture's explicitly isolated registry namespace.
    const bool shared_open = shared.open(reader, fixture::kRegistryNamespace, 0,
                                         epoch, {1, 17}, &error);
    require(shared_open, "Shared header: " + error);
    const auto validation = shared.validation_cursor();
    validate([&] { return shared.advance_validation(validation, budget); });

    for (const auto& item : {std::make_pair("full_pinyin", &full),
                             std::make_pair("xiaohe", &xiaohe)}) {
      reader = std::make_shared<HostResourceFile>(
          (directory / (std::string(item.first) + ".hnm")).string(),
          budget.max_read_bytes, metrics);
      require(reader->valid(), "Cannot open fixture scheme " + std::string(item.first));
      const bool scheme_open = item.second->open(reader, fixture::kRegistryNamespace,
                  shared.info().shared_bundle_id, shared.info().sections[0].count,
                  {1, 17}, &error);
      require(scheme_open, "Scheme pairing: " + error);
      validate([&] { return item.second->advance_validation(budget); });
      require(item.second->info().scheme_id == item.first, "Unexpected scheme ID");

      PagedSchemeIndex wrong_pair;
      require(!wrong_pair.open(reader, fixture::kRegistryNamespace,
                  shared.info().shared_bundle_id ^ SharedBundleId{1},
                  shared.info().sections[0].count, {1, 17}, &error),
              "A mismatched shared bundle was accepted");
    }
    std::cout << "fixture: shared + full_pinyin + xiaohe fully validated\n"
              << "registry_namespace=0x" << std::hex << fixture::kRegistryNamespace
              << std::dec << "; mismatched bundle pairs rejected\n";
  }
};

void admit_text(CandidateSession& session) {
  require(session.valid(), "Invalid session configuration");
  require(session.begin_text_access() == TextAccessStatus::Pending, "Text admission refused");
  for (std::size_t n = 0; n < max_steps; ++n) {
    const auto step = checked(session.advance_text_access(budget));
    if (step.status == TextAccessStatus::Ready) return;
    require(step.status == TextAccessStatus::Pending, "Text admission failed");
  }
  throw std::runtime_error("Text admission stalled");
}

void finish_range(CandidateSession& session) {
  for (std::size_t n = 0; n < max_steps; ++n) {
    const auto step = checked(session.tick(budget));
    if (step.status == SessionStatus::StagingReady) return;
    require(step.status == SessionStatus::Pending, "Candidate range failed, status=" +
            std::to_string(static_cast<int>(step.status)));
  }
  throw std::runtime_error("Candidate range stalled");
}

void publish_range(CandidateSession& session) {
  finish_range(session);
  require(session.publish(session.staging()) == SessionStatus::Ready, "Publish failed");
}

struct Candidate {
  RankItem ranked;
  std::string text;
  OutputStatus output{OutputStatus::Pending};
  bool more{false};
};

Candidate display(CandidateSession& session) {
  Candidate copy;
  require(session.present([&](const CandidateViewPage& page) {
    require(page.size == 1 && page.mode == ScoringMode::BaseOnly && !page.order_stale,
            "Expected one current BaseOnly candidate");
    copy = {page.items[0].ranked, std::string(page.text(0)), page.items[0].output, page.has_more};
  }) == SessionStatus::Ready, "Candidate view expired");
  return copy; // Borrowed Core buffers never escape the present callback.
}

Candidate query(CandidateSession& session, std::string_view code) {
  require(session.begin_input(code, epoch, 1, 0, ScoringMode::BaseOnly) ==
          SessionStatus::Pending, "Query did not begin");
  publish_range(session);
  const auto identity = display(session);
  require(identity.output == OutputStatus::Pending && identity.text.empty(),
          "Expected an identity-only page before requesting text");
  require(session.request_text(session.published(), 0, 1) == SessionStatus::Pending,
          "Candidate text request failed");
  finish_range(session);
  const auto candidate = display(session);
  require(output_has_text(candidate.output) && is_valid_utf8(candidate.text) &&
          candidate.ranked.key == identity.ranked.key, "Candidate text/identity mismatch");
  return candidate;
}

void submit(CandidateSession& session, DemoDocument& document, const Candidate& candidate) {
  const auto calls = document.calls, used = document.used;
  const auto selection = session.selection(0);
  require(session.confirm(selection).status == CommitStatus::Preparing, "Selection refused");
  for (std::size_t n = 0; ; ++n) {
    require(n < max_steps, "Commit stalled");
    const auto result = session.commit_result();
    if (result.status == CommitStatus::Accepted) {
      require(result.learning == LearningStatus::PausedUnavailable, "Unexpected learning status");
      break;
    }
    require(result.status == CommitStatus::Preparing, "Commit was not accepted, status=" +
            std::to_string(static_cast<int>(result.status)));
    checked(session.tick(budget));
  }
  require(document.calls == calls + 1 && document.text().substr(used) == candidate.text,
          "Sink did not receive the selected text exactly once");
  require(session.confirm(selection).status == CommitStatus::Accepted &&
          document.calls == calls + 1, "Repeated confirmation called the sink again");
  std::cout << "commit=Accepted text=\"" << document.text().substr(used)
            << "\" sink_calls=" << document.calls << "; duplicate confirm: no extra call\n";
}

void paging_checks(CandidateSession& session, DemoDocument& document) {
  const auto calls = document.calls, used = document.used;
  const auto first = query(session, "tongwen");
  require(first.more, "Fixture should contain a second tongwen identity");
  const auto old_selection = session.selection(0);
  require(session.request_range(RangeDirection::After, 1, 1) == SessionStatus::Pending,
          "Next page refused");
  publish_range(session);
  const auto second = display(session);
  require(first.ranked.key != second.ranked.key && first.text == u8"同文" &&
          second.text == first.text && !second.more, "Paging merged or lost fixture identities");
  require(session.confirm(old_selection).status == CommitStatus::Stale,
          "An expired snapshot was allowed to commit");
  require(session.confirm(session.selection(1)).status == CommitStatus::Stale,
          "An out-of-range selection was allowed to commit");
  require(session.request_range(RangeDirection::Before, 1, 1) == SessionStatus::Pending,
          "Previous page refused");
  publish_range(session);
  require(display(session).ranked.key == first.ranked.key, "Previous page lost its identity");
  session.cancel_input(session.input_handle());
  require(checked(session.tick(budget)).status == SessionStatus::Cancelled, "Cancel failed");
  require(document.calls == calls && document.used == used, "Negative checks changed the document");
  std::cout << "paging: tongwen First/After/Before, two distinct identities with text=\""
            << first.text << "\"\n"
            << "expired snapshot + invalid slot: Stale, no sink calls\n";
}
} // namespace

int main(int argc, char** argv) {
  try {
    require(argc == 2, "usage: bgime_host_minimal FIXTURE_DIRECTORY");
    Resources resources(argv[1]);
    NoLearning counts;
    DemoDocument document;
    CandidateSession session(resources.full, resources.shared, counts, document, session_config);
    admit_text(session);
    StableLexemeKey first_key;
    for (const auto& item : {std::make_pair("full_pinyin", "yuanxingji"),
                             std::make_pair("xiaohe", "yrxkji")}) {
      if (std::string_view(item.first) == "xiaohe")
        require(session.bind_scheme(resources.xiaohe) == SessionStatus::Ready, "Scheme switch failed");
      const auto candidate = query(session, item.second);
      // Expected literals are assertions only; display and sink text come from Core.
      require(candidate.text == u8"原型机" && candidate.ranked.key == fixture::kPrototype &&
              !candidate.more, "Unexpected small-fixture query result");
      std::cout << "scheme=" << item.first << " code=" << item.second
                << " candidate[0]=\"" << candidate.text << "\" ref=" << candidate.ranked.ref
                << " paths=" << candidate.ranked.path_count << '\n'
                << "key=" << format_slk1(candidate.ranked.key) << '\n';
      if (std::string_view(item.first) == "full_pinyin") first_key = candidate.ranked.key;
      else require(candidate.ranked.key == first_key, "Cross-scheme stable identity changed");
      submit(session, document, candidate);
    }
    require(session.bind_scheme(resources.full) == SessionStatus::Ready, "Full Pinyin rebind failed");
    paging_checks(session, document);
    require(counts.count_calls == 0 && counts.notifications == 2, "Unexpected count-source activity");
    require(document.calls == 2 && session.costs().sink_calls == 2 &&
            document.text() == u8"原型机原型机" && is_valid_utf8(document.text()),
            "Final UTF-8 document mismatch");
    std::cout << "same stable identity across schemes: yes\n"
              << "learning: unavailable, count-source reads=0, no user state\n"
              << "final UTF-8 document=\"" << document.text() << "\" sink_calls=" << document.calls << '\n'
              << "demo checks: OK\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "demo failed: " << error.what() << '\n';
    return 1;
  }
}
