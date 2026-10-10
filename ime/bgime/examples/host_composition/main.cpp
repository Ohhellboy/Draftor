#include <hanime/host_resource_file.hpp>
#include <hanime/small_fixture.hpp>
#include <hanime_extension/submission.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace hanime;
namespace ex = hanime_extension;
constexpr MountEpoch epoch = 1;
constexpr WorkBudget budget{1, 17, 3};
constexpr std::size_t max_steps = 100'000; // Host guard against a stalled operation.

void require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}

void check_cost(const WorkCost& cost, std::size_t local_work = 0) {
  require(cost.read_calls <= budget.max_read_calls &&
          cost.read_bytes <= budget.max_read_bytes &&
          cost.work_items + local_work <= budget.max_work_items,
          "A tick exceeded its work budget");
}

template<class Advance> void validate(Advance advance) {
  for (std::size_t n = 0; n < max_steps; ++n) {
    const auto step = advance();
    check_cost(step.cost);
    if (step.status == ValidationStepStatus::Ready) return;
    require(step.status == ValidationStepStatus::InProgress,
            "Resource validation failed: " + step.error);
  }
  throw std::runtime_error("Resource validation stalled");
}

struct Resources {
  // File readers borrow metrics; all resources outlive Composition/Submission.
  HostResourceMetrics metrics;
  PagedSharedResource shared;
  PagedSchemeIndex full, xiaohe;

  explicit Resources(const std::filesystem::path& directory) {
    auto reader = std::make_shared<HostResourceFile>(
        (directory / "shared.hns").string(), budget.max_read_bytes, metrics);
    require(reader->valid(), "Cannot open fixture shared.hns");
    std::string error;
    // Discover this bundle ID, then pair both schemes with the validated ID.
    const bool opened = shared.open(reader, fixture::kRegistryNamespace, 0,
                                    epoch, {1, 17}, &error);
    require(opened, "Shared header: " + error);
    const auto cursor = shared.validation_cursor();
    validate([&] { return shared.advance_validation(cursor, budget); });
    for (const auto& item : {std::make_pair("full_pinyin", &full),
                             std::make_pair("xiaohe", &xiaohe)}) {
      reader = std::make_shared<HostResourceFile>(
          (directory / (std::string(item.first) + ".hnm")).string(),
          budget.max_read_bytes, metrics);
      require(reader->valid(), "Cannot open fixture scheme " + std::string(item.first));
      const bool paired = item.second->open(reader, fixture::kRegistryNamespace,
          shared.info().shared_bundle_id, shared.info().sections[0].count, {1, 17}, &error);
      require(paired, "Scheme pairing: " + error);
      validate([&] { return item.second->advance_validation(budget); });
      require(item.second->info().scheme_id == item.first, "Unexpected scheme ID");
    }
    std::cout << "fixture: shared + full_pinyin + xiaohe validated and paired\n"
              << "registry_namespace=0x" << std::hex << fixture::kRegistryNamespace
              << std::dec << " (isolated test data)\n";
  }
};

class DemoDocument final : public TextCommitSink {
 public:
  bool commit(std::string_view text) override {
    ++calls;
    if (text.size() > bytes.size() - used) return false;
    if (!text.empty()) std::memcpy(bytes.data() + used, text.data(), text.size());
    used += text.size(); // Own the complete UTF-8 text before accepting it.
    return true;
  }
  std::string_view text() const { return {bytes.data(), used}; }
  std::size_t calls{0};
 private:
  std::array<char, 64> bytes{};
  std::size_t used{0};
};

ex::Status finish(ex::Composition& input, ex::Status status) {
  const auto operation = input.operation();
  for (std::size_t n = 0; status == ex::Status::Working; ++n) {
    require(n < max_steps, "Extension operation stalled");
    const auto step = input.tick(operation, budget);
    check_cost(step.core_cost, step.local_work);
    status = step.status;
  }
  return status;
}

ex::Status query(ex::Composition& input) {
  const auto status = finish(input, input.query_next());
  require(status == ex::Status::PageReady || status == ex::Status::NoPrefixMatch ||
          status == ex::Status::Unresolved || status == ex::Status::NoMappingForPrefix,
          std::string("Planning/query failed: ") + ex::name(status));
  require(!input.planning_report().limited, "Planning hit a configured limit");
  return status;
}

void type_keys(ex::Composition& input, std::string_view keys, bool trace) {
  std::size_t metadata_queries = 0;
  for (std::size_t i = 0; i < keys.size(); ++i) {
    const auto before = input.raw_input();
    const auto old_stamp = before.stamp;
    // The Host only delivers one character. Extension owns encoding/planning.
    require(input.edit_raw(before.raw.size(), before.raw.size(), keys.substr(i, 1)) ==
            ex::Status::Ready, "Key insertion failed");
    require(!input.input_is_current(old_stamp), "Edit retained an old input stamp");
    require(input.raw_input().raw == keys.substr(0, i + 1), "Raw input mismatch");
    const auto status = query(input);
    const auto report = input.planning_report();
    metadata_queries += report.queries;
    if (trace) std::cout << "key='" << keys[i] << "' raw=" << input.raw_input().raw
                         << " -> " << ex::name(status) << '\n';
    if (input.raw_input().rules == ex::SchemeRules::R1Xiaohe && i == 4) {
      require(report.valid && report.incomplete_tail, "Odd Xiaohe tail was not incomplete");
      if (trace) std::cout << "  planner: odd trailing key is incomplete\n";
    }
  }
  require(metadata_queries > 0, "No planner metadata query was executed");
  if (!trace) std::cout << "typed " << keys.size() << " individual keys; query_next after each; raw="
                        << input.raw_input().raw << '\n';
}

void inspect_plan(ex::Composition& input, std::size_t begin, std::size_t first_end,
                  std::size_t covered_end, std::size_t words) {
  const auto p = input.planning_report();
  require(input.status() == ex::Status::PageReady && input.plan_is_current(p) &&
          !p.direct_range && !p.limited && !p.incomplete_tail && !p.unknown_tail &&
          p.begin == begin && p.end == first_end && p.covered_end == covered_end &&
          p.syllables == words * 3 && p.words == words,
          "Unexpected multi-syllable planning result");
  std::cout << "plan: syllables=" << p.syllables << " words=" << p.words
            << " next=[" << p.begin << ',' << p.end << ") covered_end=" << p.covered_end
            << " direct_range=false queries=" << p.queries << " cache_hits=" << p.cache_hits << '\n';
}

struct Candidate {
  ex::Selection selection;
  StableLexemeKey key;
  std::string text;
};

Candidate candidate(ex::Composition& input) {
  const auto page = input.page();
  require(page.size == 1 && !page.has_more, "Expected one fixture candidate");
  const auto item = page.items[0];
  Candidate result{input.selection(0), item.key, {}};
  require(finish(input, input.read_text(result.selection)) == ex::Status::Ready,
          "Candidate text access failed");
  result.text = input.last_text(); // Copy the borrowed text before another operation.
  // Literals are expectations only; all displayed/committed text comes from Core.
  require(result.text == u8"原型机" && is_valid_utf8(result.text) &&
          result.key == fixture::kPrototype, "Unexpected fixture text or identity");
  std::cout << "candidate[0]=\"" << result.text << "\" span=[" << page.begin << ',' << page.end
            << ") ref=" << item.ref << " paths=" << item.path_count << '\n'
            << "key=" << format_slk1(result.key) << '\n';
  return result;
}

void select(ex::Composition& input, const Candidate& choice, std::size_t members,
            std::size_t focus, std::string_view expected) {
  const auto old_plan = input.planning_report();
  require(finish(input, input.select(choice.selection)) == ex::Status::Ready,
          "Candidate selection failed");
  const auto s = input.snapshot();
  require(s.member_count == members && s.focus == focus && s.selected_text == expected &&
          !input.plan_is_current(old_plan), "Selected composition mismatch");
  std::size_t previous_end = 0;
  for (std::size_t i = 0; i < s.member_count; ++i) {
    const auto& m = s.members[i];
    require(m.valid && m.key == fixture::kPrototype && m.begin == previous_end && m.end > m.begin,
            "Selected member identity/span mismatch");
    previous_end = m.end;
  }
  const bool all = s.focus == s.raw.size();
  require(s.state == (all ? ex::CompositionState::AllSelected : ex::CompositionState::PartialSelected),
          "Unexpected composition state");
  std::cout << "selected: members=" << s.member_count << " text=\"" << s.selected_text
            << "\" remaining_raw=\"" << s.raw.substr(s.focus) << "\" state="
            << (all ? "AllSelected" : "PartialSelected") << '\n';
}

void submit(ex::Composition& input, ex::Submission& submission, DemoDocument& document,
            std::size_t members, std::string_view expected) {
  require(document.calls == 0 && input.snapshot().state == ex::CompositionState::AllSelected,
          "Submission precondition failed");
  auto result = submission.prepare();
  require(result.core.status == CompositionStatus::Preparing, "Submission did not prepare");
  const auto transaction = result.transaction;
  const auto raw_size = input.raw_input().raw.size();
  require(input.edit_raw(raw_size - 1, raw_size, "") == ex::Status::Busy,
          "Submission failed to reserve the input");
  for (std::size_t n = 0; result.core.status == CompositionStatus::Preparing; ++n) {
    require(n < max_steps, "Submission preparation stalled");
    const auto step = submission.advance(transaction, budget);
    check_cost(step.cost);
    result = step.result;
  }
  require(result.core.status == CompositionStatus::Ready, "Submission was not ready");
  bool inspected = false;
  require(submission.prepared(transaction, [&](const PreparedCompositionView& view) {
    require(view.size == members && view.text == expected && is_valid_utf8(view.text),
            "Prepared UTF-8 composition mismatch");
    inspected = true;
  }) == CompositionStatus::Ready && inspected, "Prepared text was unavailable");
  result = submission.confirm(transaction);
  require(result.core.status == CompositionStatus::Accepted &&
          result.core.learning == CompositionLearning::DisabledByConfiguration &&
          document.calls == 1 && submission.costs().sink_calls == 1 && document.text() == expected,
          "Submission did not deliver the complete text exactly once");
  require(submission.confirm(transaction).core.status == CompositionStatus::Accepted &&
          document.calls == 1 && submission.costs().sink_calls == 1,
          "Repeated confirmation wrote text twice");
  require(input.snapshot().state == ex::CompositionState::Idle && input.raw_input().raw.empty() &&
          input.count_begin_calls() == 0, "Accepted input did not finish without count lookups");
  std::cout << "submission: Preparing -> Ready -> Accepted; UTF-8=\"" << document.text()
            << "\" sink_calls=" << document.calls << '\n'
            << "duplicate confirm: Accepted, sink_calls=1; input=Idle; learning=DisabledByConfiguration\n";
}

void edit_checks(ex::Composition& input, ex::Submission& submission, DemoDocument& document) {
  const auto stamp = input.raw_input().stamp;
  const auto old_selection = input.selection(0);
  const auto old_plan = input.planning_report();
  const std::string raw(input.raw_input().raw);
  require(input.query_next() == ex::Status::Working, "Edit check query did not start");
  const auto old_operation = input.operation();
  require(input.edit_raw(raw.size() - 1, raw.size(), "") == ex::Status::Busy &&
          input.raw_input().raw == raw, "An active query allowed an edit");
  require(input.cancel(old_operation) == ex::Status::Cancelled, "Query cancellation failed");
  require(input.edit_raw(raw.size() - 1, raw.size(), "") == ex::Status::Ready,
          "Backspace failed after cancellation");
  require(input.raw_input().raw == std::string_view(raw).substr(0, raw.size() - 1) &&
          !input.input_is_current(stamp) && !input.plan_is_current(old_plan),
          "Backspace retained old input/planning authority");
  std::cout << "edit: active query -> Busy; cancel -> Backspace; raw=" << input.raw_input().raw << '\n';
  require(input.select(old_selection) == ex::Status::Stale &&
          submission.prepare().core.status == CompositionStatus::InvalidRequest && document.calls == 0,
          "A stale selection was allowed to submit");
  const auto length = input.raw_input().raw.size();
  require(input.edit_raw(length, length, std::string_view(raw).substr(raw.size() - 1)) ==
          ex::Status::Ready, "Retyping the last key failed");
  const auto start = input.query_next();
  require(start == ex::Status::Working && input.tick(old_operation, budget).status == ex::Status::Stale &&
          input.cancel(old_operation) == ex::Status::Stale, "Old operation retained authority");
  require(finish(input, start) == ex::Status::PageReady &&
          input.select(old_selection) == ex::Status::Stale, "Requery revived an old selection");
  std::cout << "retype='" << raw.back() << "' raw=" << input.raw_input().raw
            << "; old stamp/plan invalid, old operation/selection=Stale, sink_calls=0\n";
}

StableLexemeKey run(Resources& resources, PagedSchemeIndex& scheme, ex::SchemeRules rules,
                    std::string_view code, bool multiple, bool edit) {
  DemoDocument document;
  ex::Composition input(scheme, resources.shared, rules);
  ex::Submission submission(input, resources.shared, document);
  require(input.valid() && submission.valid(), "Invalid Host configuration");
  const std::string keys = multiple ? std::string(code) + std::string(code) : std::string(code);
  std::cout << "\n[" << scheme.info().scheme_id << (multiple ? " / two members" : " / one word") << "]\n";
  type_keys(input, keys, !multiple);
  inspect_plan(input, 0, code.size(), keys.size(), multiple ? 2 : 1);
  if (edit) {
    edit_checks(input, submission, document);
    inspect_plan(input, 0, code.size(), keys.size(), 1);
  }
  const auto first = candidate(input);
  select(input, first, 1, code.size(), u8"原型机");
  if (multiple) {
    require(submission.prepare().core.status == CompositionStatus::InvalidRequest && document.calls == 0,
            "A partial composition was allowed to submit");
    std::cout << "partial submission: InvalidRequest, sink_calls=0\n";
    require(query(input) == ex::Status::PageReady, "Remaining raw query failed");
    inspect_plan(input, code.size(), keys.size(), keys.size(), 1);
    const auto second = candidate(input);
    require(second.key == first.key, "Repeated fixture word changed identity");
    select(input, second, 2, keys.size(), u8"原型机原型机");
  }
  submit(input, submission, document, multiple ? 2 : 1, multiple ? u8"原型机原型机" : u8"原型机");
  return first.key;
}
} // namespace

int main(int argc, char** argv) {
  try {
    require(argc == 2, "usage: bgime_host_composition FIXTURE_DIRECTORY");
    Resources resources(argv[1]);
    const auto full = run(resources, resources.full, ex::SchemeRules::FullPinyin, "yuanxingji", false, true);
    const auto xiaohe = run(resources, resources.xiaohe, ex::SchemeRules::R1Xiaohe, "yrxkji", false, false);
    require(full == xiaohe, "Cross-scheme stable identity mismatch");
    run(resources, resources.full, ex::SchemeRules::FullPinyin, "yuanxingji", true, false);
    run(resources, resources.xiaohe, ex::SchemeRules::R1Xiaohe, "yrxkji", true, false);
    std::cout << "\nsame stable identity across schemes: yes\n"
              << "four compositions accepted, one sink call per composition; demo checks: OK\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "demo failed: " << error.what() << '\n';
    return 1;
  }
}
