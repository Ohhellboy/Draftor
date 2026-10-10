#include "fixture.hpp"
#include <hanime/host_resource_file.hpp>
#include <hanime_wubi/wubi.hpp>

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hanime;
namespace wb = hanime::wubi;
namespace fx = demo_fixture;
constexpr WorkBudget budget{1, 17, 3};
constexpr std::size_t max_steps = 100'000;

void require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}
void check_cost(WorkCost cost) {
  require(cost.read_calls <= budget.max_read_calls && cost.read_bytes <= budget.max_read_bytes &&
          cost.work_items <= budget.max_work_items, "WorkBudget exceeded");
}
template<class Advance> void validate(Advance advance) {
  for (std::size_t n = 0; n < max_steps; ++n) {
    const auto step = advance();
    check_cost(step.cost);
    if (step.status == ValidationStepStatus::Ready) return;
    require(step.status == ValidationStepStatus::InProgress, "Validation failed: " + step.error);
  }
  throw std::runtime_error("Resource validation stalled");
}

struct Resources {
  HostResourceMetrics metrics; // Readers borrow this; owners outlive every Host.
  PagedSharedResource shared;
  PagedSchemeIndex scheme;
  explicit Resources(const std::filesystem::path& directory) {
    auto reader = std::make_shared<HostResourceFile>(
        (directory / "shared.hns").string(), budget.max_read_bytes, metrics);
    require(reader->valid(), "Cannot open fixture shared.hns");
    std::string error;
    // Discover the immutable bundle ID, then require that ID for the scheme.
    const bool opened = shared.open(reader, fx::registry, 0, 1, {1, 17}, &error);
    require(opened, "Shared header: " + error);
    const auto cursor = shared.validation_cursor();
    validate([&] { return shared.advance_validation(cursor, budget); });
    reader = std::make_shared<HostResourceFile>(
        (directory / "wubi86.hnm").string(), budget.max_read_bytes, metrics);
    require(reader->valid(), "Cannot open fixture wubi86.hnm");
    const auto bundle = shared.info().shared_bundle_id;
    const auto refs = shared.info().sections[0].count;
    const bool paired = scheme.open(reader, fx::registry, bundle, refs, {1, 17}, &error);
    require(paired, "Scheme pairing: " + error);
    validate([&] { return scheme.advance_validation(budget); });
    require(scheme.info().scheme_id == "wubi86", "Wrong scheme ID");
    PagedSchemeIndex wrong_pair;
    require(!wrong_pair.open(reader, fx::registry, bundle ^ SharedBundleId{1}, refs, {1, 17}, &error),
            "Mismatched shared bundle accepted");
    require(!wrong_pair.open(reader, fx::registry ^ RegistryNamespaceId{1}, bundle, refs, {1, 17}, &error),
            "Mismatched namespace accepted");
    std::cout << "fixture: artificial shared + wubi86 resources validated and paired\n"
              << "registry_namespace=0x" << std::hex << fx::registry << std::dec
              << "; wrong bundle/namespace rejected\n";
  }
};

// Example-only disabled source. A fixed owner/namespace lets the binding's
// Exact query initialize; Host immediately selects BaseOnly before any count
// lookup. Unavailable never pretends to mean a learned count of zero.
class NoLearning final : public CommitCountSource {
 public:
  NoLearning() { state.owner = std::make_shared<CountOwnerIdentity>(); state.registry_namespace = fx::registry; }
  StateView view() const override { return state; }
  bool view_is_current(const StateView& v) const override {
    return v.owner == state.owner && v.registry_namespace == state.registry_namespace &&
           v.count_revision == state.count_revision && v.availability_revision == state.availability_revision;
  }
  CountTicket begin_count(const StateView&, const StableLexemeKey&) override { ++lookups; return {}; }
  CountResult advance_count(const CountTicket& ticket, const WorkBudget&) override {
    return {CountStatus::Unavailable, ticket, 0, {}};
  }
  void cancel_count(const CountTicket&) override {}
  LearningStatus notify_committed(const CommitHandle&, RegistryNamespaceId ns,
                                  const StableLexemeKey& key) noexcept override {
    ++notifications; last_namespace = ns; last_key = key;
    return LearningStatus::PausedUnavailable; // No user state or persistence.
  }
  std::size_t lookups{0}, notifications{0};
  RegistryNamespaceId last_namespace{0};
  StableLexemeKey last_key;
 private:
  StateView state;
};

class DemoDocument final : public TextCommitSink {
 public:
  bool commit(std::string_view text) override {
    ++calls;
    if (text.size() > bytes.size() - used) return false;
    if (!text.empty()) std::memcpy(bytes.data() + used, text.data(), text.size());
    used += text.size();
    return true;
  }
  std::string_view text() const { return {bytes.data(), used}; }
  std::size_t calls{0};
 private:
  std::array<char, 64> bytes{};
  std::size_t used{0};
};

struct Item { RankItem ranked; std::string text; OutputStatus output; };
struct Page { std::vector<Item> items; bool more{false}; };

struct Host {
  NoLearning counts;
  DemoDocument document;
  wb::Wubi86Decoder decoder;
  CandidateSession session;
  wb::DirectQueryBinding binding; // Destroy before session, decoder and resources.
  const std::size_t page_size;

  explicit Host(Resources& r, std::size_t page = 1, OutputRequest output = {})
      : session(r.scheme, r.shared, counts, document, {4, 2, 1, 64, 64, 4}),
        binding(decoder, session, r.scheme, r.shared, page), page_size(page) {
    require(session.valid(), "Invalid session configuration");
    require(session.begin_text_access() == TextAccessStatus::Pending, "Text admission refused");
    for (std::size_t n = 0; ; ++n) {
      require(n < max_steps, "Text admission stalled");
      const auto step = session.advance_text_access(budget);
      check_cost(step.cost);
      if (step.status == TextAccessStatus::Ready) break;
      require(step.status == TextAccessStatus::Pending, "Text admission failed");
    }
    require(session.set_output(output) == SessionStatus::Ready, "Output policy refused");
    require(binding.activate() == SessionStatus::Ready, "Wubi activation refused");
  }
  wb::Step tick() {
    const auto step = binding.tick(budget);
    check_cost(step.shared.cost);
    require(counts.lookups == 0, "Unexpected user-count lookup");
    return step;
  }
  void start_base_only() {
    // The first tick initializes the binding's logical input, with no resource
    // reads/count queries. Restart the SAME input through Core's public ranking
    // API. Do not call begin_input or change bindings behind DirectQueryBinding.
    const auto step = tick();
    require(step.shared.status == SessionStatus::Pending && step.shared.cost.read_calls == 0 &&
            step.request == wb::CommitReason::None, "Unexpected input initialization");
    require(session.restart_first(page_size, page_size, ScoringMode::BaseOnly) == SessionStatus::Pending,
            "BaseOnly range restart refused");
  }
  void append(char key) {
    require(binding.append(key) == wb::EditStatus::Changed, "Key insertion refused");
    start_base_only();
  }
  void backspace() {
    require(binding.backspace() == wb::EditStatus::Changed, "Backspace refused");
    if (!decoder.plan().code.empty()) start_base_only();
  }
  wb::Step finish() {
    for (std::size_t n = 0; n < max_steps; ++n) {
      const auto step = tick();
      if (step.shared.status != SessionStatus::Pending) return step;
      require(step.request == wb::CommitReason::None, "Premature automatic request");
    }
    throw std::runtime_error("Wubi query stalled");
  }
  Page page() {
    Page result;
    require(session.present([&](const CandidateViewPage& view) {
      require(view.mode == ScoringMode::BaseOnly && !view.order_stale, "Non-current BaseOnly page");
      result.more = view.has_more;
      for (std::size_t i = 0; i < view.size; ++i)
        result.items.push_back({view.items[i].ranked, std::string(view.text(i)), view.items[i].output});
    }) == SessionStatus::Ready, "Candidate page unavailable");
    return result; // No borrowed Core text escapes its callback.
  }
  void show(const wb::Step& step) {
    std::cout << "code=" << decoder.plan().code << " full=" << decoder.plan().full_code;
    if (step.shared.status == SessionStatus::Ready) {
      const auto p = page();
      std::cout << " candidates=" << p.items.size() << " has_more=" << p.more;
      for (const auto& item : p.items)
        std::cout << " text=\"" << item.text << "\" ref=" << item.ranked.ref;
    } else {
      require(step.shared.status == SessionStatus::NoMatches, "Unexpected query status");
      require(session.present([](const CandidateViewPage&) {}) == SessionStatus::Stale,
              "No-match input retained an old candidate page");
      std::cout << " NoMatches";
    }
    std::cout << " request=" << (step.request == wb::CommitReason::AutoUniqueFullCode ? "AutoUniqueFullCode" : "None")
              << " sink_calls=" << document.calls << '\n';
  }
  wb::Step type(std::string_view keys) {
    require(decoder.plan().code.empty(), "Type expects an empty decoder");
    wb::Step last;
    for (std::size_t i = 0; i < keys.size(); ++i) {
      append(keys[i]);
      require(decoder.plan().code == keys.substr(0, i + 1) && decoder.plan().full_code == (i == 3),
              "Decoder length/full-code mismatch");
      last = finish();
      require(last.code_revision == decoder.plan().revision, "Query used an old code revision");
      if (i < 3) require(last.request == wb::CommitReason::None, "Short code requested auto commit");
      show(last);
    }
    return last;
  }
  void no_writes() const {
    require(document.calls == 0 && document.text().empty() && counts.notifications == 0 && counts.lookups == 0,
            "A non-commit path wrote text or accessed learning");
  }
  void commit(const SelectionHandle& selection, wb::CommitReason reason, const Item& selected) {
    require(output_has_text(selected.output) && is_valid_utf8(selected.text), "Selected text is not usable");
    const auto calls = document.calls, notices = counts.notifications;
    require(binding.confirm(selection, reason).status == CommitStatus::Preparing, "Selection not admitted");
    require(binding.append('a') == wb::EditStatus::Busy, "Commit failed to reserve input");
    for (std::size_t n = 0; session.commit_result().status == CommitStatus::Preparing; ++n) {
      require(n < max_steps, "Commit stalled");
      require(tick().request == wb::CommitReason::None, "Commit generated a second auto request");
    }
    const auto result = session.commit_result();
    require(result.status == CommitStatus::Accepted && result.learning == LearningStatus::PausedUnavailable &&
            document.calls == calls + 1 && session.costs().sink_calls == document.calls &&
            document.text() == selected.text && counts.notifications == notices + 1 &&
            counts.last_key == selected.ranked.key && counts.last_namespace == fx::registry &&
            decoder.plan().code.empty(), "Accepted commit text/identity/count mismatch");
    require(binding.confirm(selection, reason).status == CommitStatus::Accepted &&
            document.calls == calls + 1 && counts.notifications == notices + 1,
            "Duplicate confirmation repeated text or learning notification");
    for (unsigned i = 0; i < 3; ++i)
      require(tick().request == wb::CommitReason::None && document.calls == calls + 1, "Idle replay");
    std::cout << "commit=" << (reason == wb::CommitReason::Explicit ? "Explicit" : "AutoUniqueFullCode (Host confirmed)")
              << " Accepted UTF-8=\"" << document.text() << "\" sink_calls=" << document.calls
              << "; duplicate confirm: no extra call\nkey=" << format_slk1(selected.ranked.key) << '\n';
  }
};

void editing_and_explicit(Resources& resources) {
  std::cout << "\n[editing and explicit selection]\n";
  Host h(resources);
  const auto request = h.type("abcd");
  require(request.request == wb::CommitReason::AutoUniqueFullCode, "Unique four-code request missing");
  const auto revision = h.decoder.plan().revision;
  require(h.binding.append('e') == wb::EditStatus::Full &&
          h.binding.append('z') == wb::EditStatus::InvalidCode &&
          h.binding.append('1') == wb::EditStatus::InvalidCode &&
          h.decoder.plan().revision == revision && h.decoder.plan().code == "abcd", "Rejected edit changed code");
  h.no_writes();
  h.backspace();
  require(h.binding.confirm(request.selection, request.request).status == CommitStatus::Stale,
          "Old auto selection survived Backspace");
  auto step = h.finish();
  require(step.request == wb::CommitReason::None && h.decoder.plan().code == "abc" &&
          h.page().items.at(0).text == u8"三", "Backspace query mismatch");
  h.append('D'); // The existing decoder folds uppercase ASCII.
  step = h.finish();
  require(h.decoder.plan().code == "abcd" && step.request == wb::CommitReason::AutoUniqueFullCode &&
          h.binding.confirm(request.selection, request.request).status == CommitStatus::Stale,
          "Retyping revived the previous selection");
  require(h.binding.cancel() == wb::EditStatus::Changed && h.decoder.plan().code.empty() &&
          !h.decoder.plan().full_code && h.tick().shared.status == SessionStatus::Cancelled,
          "Cancel did not clear the decoder/query");
  require(h.binding.backspace() == wb::EditStatus::Unchanged && h.binding.cancel() == wb::EditStatus::Unchanged,
          "Empty edit should be unchanged");
  h.no_writes();
  std::cout << "limit=4; fifth key=Full; z/digit=InvalidCode; Backspace/retype/cancel: no writes, old selection=Stale\n";
  h.type("ab");
  const auto chosen = h.page().items.at(0);
  require(chosen.text == u8"二" && chosen.ranked.key == fx::two, "Wrong explicit candidate");
  require(h.binding.confirm(h.session.selection(1)).status == CommitStatus::Stale &&
          h.binding.confirm({}).status == CommitStatus::Stale, "Invalid selection admitted");
  h.commit(h.session.selection(0), wb::CommitReason::Explicit, chosen);
}

void unique_request(Resources& resources) {
  std::cout << "\n[unique identity: request first, Host confirmation later]\n";
  Host h(resources);
  const auto request = h.type("abcd");
  const auto p = h.page();
  require(request.request == wb::CommitReason::AutoUniqueFullCode && p.items.size() == 1 && !p.more &&
          p.items[0].ranked.key == fx::four && p.items[0].ranked.path_count == 2 &&
          p.items[0].text == u8"四" && output_has_text(p.items[0].output), "Uniqueness evidence mismatch");
  for (unsigned i = 0; i < 3; ++i) require(h.tick().request == wb::CommitReason::None, "Repeated auto request");
  h.no_writes();
  std::cout << "two mapping paths -> one identity; valid text; request alone leaves sink_calls=0\n";
  h.commit(request.selection, request.request, p.items[0]);
}

void multiple_candidates(Resources& resources) {
  std::cout << "\n[same text, distinct identities; one-slot pages]\n";
  Host h(resources);
  require(h.type("bbbb").request == wb::CommitReason::None, "Multi-candidate auto request");
  const auto first = h.page();
  const auto old_selection = h.session.selection(0);
  require(first.items.size() == 1 && first.more && first.items[0].ranked.key == fx::same_a,
          "Missing next-page evidence");
  require(h.session.request_range(RangeDirection::After, 1, 1) == SessionStatus::Pending, "Next page refused");
  const auto step = h.finish();
  h.show(step);
  const auto second = h.page();
  require(step.request == wb::CommitReason::None && second.items.size() == 1 && !second.more &&
          second.items[0].ranked.key == fx::same_b && second.items[0].ranked.key != first.items[0].ranked.key &&
          first.items[0].text == u8"同文" && second.items[0].text == first.items[0].text,
          "Pagination merged identities or inferred false uniqueness");
  require(h.binding.confirm(old_selection).status == CommitStatus::Stale, "Old page selection admitted");
  h.no_writes();
  std::cout << "first key=" << format_slk1(first.items[0].ranked.key) << '\n';
  h.commit(h.session.selection(0), wb::CommitReason::Explicit, second.items[0]);
  {
    // Separate owner only after the first input has finished; shared resources
    // are serial. Leave explicitly before either binding is destroyed.
    require(h.binding.leave() == wb::EditStatus::Unchanged, "First binding did not leave");
    Host two_slots(resources, 2);
    require(two_slots.type("bbbb").request == wb::CommitReason::None, "Two visible candidates requested auto commit");
    const auto both = two_slots.page();
    require(both.items.size() == 2 && !both.more && both.items[0].ranked.key != both.items[1].ranked.key,
            "Two-slot identity page mismatch");
    require(two_slots.binding.leave() == wb::EditStatus::Changed && two_slots.decoder.plan().code.empty(), "Leave failed");
    two_slots.no_writes();
  }
}

void stale_query_and_exit(Resources& resources) {
  std::cout << "\n[pending query replacement, no match, cancel and leave]\n";
  Host h(resources);
  h.type("abc");
  const auto old = h.session.selection(0);
  h.append('d'); // Start four-code work, but edit before its candidate is ready.
  const auto obsolete_input = h.session.input_handle();
  const auto obsolete_query = resources.scheme.current_query_revision();
  h.backspace();
  require(resources.scheme.current_query_revision() != obsolete_query, "Query revision did not change");
  h.session.cancel_input(obsolete_input); // A stale cancel must not cancel its replacement.
  const auto step = h.finish();
  require(step.shared.status == SessionStatus::Ready && step.request == wb::CommitReason::None &&
          h.decoder.plan().code == "abc" && h.page().items[0].text == u8"三" &&
          h.binding.confirm(old).status == CommitStatus::Stale, "Old query/selection retained authority");
  h.append('c');
  const auto missing = h.finish();
  h.show(missing);
  require(missing.shared.status == SessionStatus::NoMatches && missing.request == wb::CommitReason::None &&
          h.binding.confirm(h.session.selection(0)).status == CommitStatus::Stale, "No-match produced a candidate");
  require(h.binding.cancel() == wb::EditStatus::Changed && h.tick().shared.status == SessionStatus::Cancelled,
          "No-match cancellation failed");
  h.append('a');
  require(h.binding.cancel() == wb::EditStatus::Changed && h.tick().shared.status == SessionStatus::Cancelled,
          "Pending query cancellation failed");
  const auto request = h.type("abcd");
  require(h.binding.leave() == wb::EditStatus::Changed && h.decoder.plan().code.empty() &&
          h.binding.append('a') == wb::EditStatus::Closed &&
          h.binding.confirm(request.selection, request.request).status == CommitStatus::Stale,
          "Leaving allowed a queued request to submit");
  h.no_writes();
  std::cout << "old pending four-code query retired; abcc has no fallback to abc; cancel/leave sink_calls=0\n";
}

void restricted_output(Resources& resources) {
  std::cout << "\n[unique identity with blocked output]\n";
  Host h(resources, 1, {"blocked", OutputInvocation::Explicit});
  const auto result = h.type("abcd");
  const auto p = h.page();
  require(result.request == wb::CommitReason::None && p.items.size() == 1 && !p.more &&
          !output_has_text(p.items[0].output) && p.items[0].text.empty(), "Blocked output requested auto commit");
  require(h.binding.confirm(h.session.selection(0)).status == CommitStatus::Preparing, "Blocked selection capture failed");
  for (std::size_t n = 0; h.session.commit_result().status == CommitStatus::Preparing; ++n) {
    require(n < max_steps, "Restricted commit stalled");
    require(h.tick().request == wb::CommitReason::None, "Blocked commit requested auto");
  }
  require(h.session.commit_result().status == CommitStatus::OutputRestricted, "Core did not enforce output policy");
  require(h.binding.cancel() == wb::EditStatus::Changed, "Restricted input cancel failed");
  h.no_writes();
  std::cout << "output invalid for selected form: request=None; explicit commit=OutputRestricted; sink_calls=0\n";
}

void resource_expiry(Resources& resources) {
  std::cout << "\n[resource activation expires before confirmation]\n";
  Host h(resources);
  const auto request = h.type("abcd");
  resources.scheme.deactivate(); // Local test owner only, after all other cases.
  const auto step = h.tick();
  require(step.shared.status == SessionStatus::Stale && step.request == wb::CommitReason::None &&
          h.binding.confirm(request.selection, request.request).status == CommitStatus::Stale &&
          h.session.present([](const CandidateViewPage&) {}) == SessionStatus::Stale,
          "An expired resource activation authorized display/submission");
  h.no_writes();
  std::cout << "expired query/display/selection=Stale; request=None; sink_calls=0\n";
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--build-fixture") {
      fx::write(argv[2]);
      std::cout << "Built artificial Wubi86 fixture (six identities, seven mapping rows).\n";
      return 0;
    }
    require(argc == 2, "usage: bgime_host_wubi86 FIXTURE_DIRECTORY");
    Resources resources(argv[1]);
    editing_and_explicit(resources);
    unique_request(resources);
    multiple_candidates(resources);
    stale_query_and_exit(resources);
    restricted_output(resources);
    resource_expiry(resources);
    std::cout << "\nlearning unavailable; frequency lookups=0; accepted notifications preserved without recording\n"
              << "three accepted commits in separate documents; UTF-8 and exactly-once checks: OK\n"
              << "public Wubi86 request only; Draftor autoSafe/window/target policy not exercised\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "demo failed: " << error.what() << '\n';
    return 1;
  }
}
