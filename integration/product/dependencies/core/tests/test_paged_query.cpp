#include "hanime/core.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace hanime;

class TestFailure : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw TestFailure(message);
  }
}

class CountingReader final : public ResourceReader {
 public:
  explicit CountingReader(std::vector<std::uint8_t> bytes)
      : bytes_(std::move(bytes)) {}

  std::size_t size() const override { return bytes_.size(); }

  bool read(std::size_t offset, void* destination,
            std::size_t byte_count) const override {
    ++read_calls_;
    read_bytes_ += byte_count;
    max_requested_ = std::max(max_requested_, byte_count);
    if (fail_reads_) {
      return false;
    }
    if (offset > bytes_.size() || byte_count > bytes_.size() - offset) {
      return false;
    }
    if (byte_count != 0U) {
      std::memcpy(destination, bytes_.data() + offset, byte_count);
    }
    return true;
  }

  void clear_cost() const {
    read_calls_ = 0U;
    read_bytes_ = 0U;
    max_requested_ = 0U;
  }

  std::size_t read_calls() const { return read_calls_; }
  std::size_t read_bytes() const { return read_bytes_; }
  std::size_t max_requested() const { return max_requested_; }
  void set_fail_reads(bool value) { fail_reads_ = value; }

 private:
  std::vector<std::uint8_t> bytes_;
  mutable std::size_t read_calls_{0};
  mutable std::size_t read_bytes_{0};
  mutable std::size_t max_requested_{0};
  bool fail_reads_{false};
};

std::string numbered(std::string_view prefix, std::size_t value) {
  std::ostringstream output;
  output << prefix << std::setw(5) << std::setfill('0') << value;
  return output.str();
}

struct SyntheticFixture {
  ResourceBuildInput input;
  BuiltResources built;
  RuntimeLexemeRef maximum_runtime_ref{0};
};

SyntheticFixture build_synthetic(std::size_t lexeme_count,
                                 std::uint32_t page_bytes) {
  expect(lexeme_count >= 64U, "synthetic fixture is unexpectedly small");
  SyntheticFixture fixture;
  fixture.input.registry_namespace = 0x5354455032410001ULL;
  fixture.input.source_name = "step2a-synthetic-paged-index";
  fixture.input.source_version = "2026-09-08.v0.1";
  fixture.input.relation_basis = "synthetic identity-preserving mappings";
  for (std::size_t index = 0; index < lexeme_count; ++index) {
    LexemeSourceRecord lexeme;
    // Synthetic registry definition, outside ResourceBuilder; never a production key.
    lexeme.stable_key.bytes[0] = 0x70;
    for (unsigned b = 0; b < 8; ++b)
      lexeme.stable_key.bytes[31 - b] = static_cast<std::uint8_t>((std::uint64_t(index) + 1) >> (8U * b));
    lexeme.status = RegistryStatus::Approved;
    lexeme.kind = LexemeKind::Word;
    lexeme.identity_definition = numbered("synthetic lexeme ", index);
    lexeme.provenance = numbered("synthetic/source/", index);
    lexeme.base_weight = static_cast<std::int32_t>(index % 101U);
    lexeme.default_output_utf8 = numbered("词", index);
    fixture.input.lexemes.push_back(std::move(lexeme));
  }

  // Insert in deliberately non-sorted order. The first 41 records share one
  // code, and one stable identity has a duplicate mapping path.
  for (std::size_t reverse = lexeme_count; reverse > 0U; --reverse) {
    const std::size_t index = reverse - 1U;
    fixture.input.mappings.push_back(
        {"full_pinyin", numbered("code", index),
         fixture.input.lexemes[index].stable_key,
         static_cast<std::int32_t>(index % 17U)});
  }
  for (std::size_t index = 0; index < 41U; ++index) {
    fixture.input.mappings.push_back(
        {"full_pinyin", "same", fixture.input.lexemes[index].stable_key,
         static_cast<std::int32_t>(200U - index)});
  }
  fixture.input.mappings.push_back(
      {"full_pinyin", "same", fixture.input.lexemes.front().stable_key,
       999});
  fixture.input.mappings.push_back(
      {"full_pinyin", "aaaa", fixture.input.lexemes.front().stable_key, 7});
  fixture.input.mappings.push_back(
      {"full_pinyin", "zzzz", fixture.input.lexemes.back().stable_key, 9});
  for (std::size_t index = 0; index < 10U; ++index) {
    fixture.input.mappings.push_back(
        {"xiaohe", "sx", fixture.input.lexemes[index].stable_key,
         static_cast<std::int32_t>(50U - index)});
  }

  ResourceBuildOptions options;
  options.scheme_page_bytes = page_bytes;
  BuildResult result = ResourceBuilder().build(fixture.input, options);
  expect(result.ok, "synthetic build failed: " + result.error);
  fixture.built = std::move(result.resources);
  fixture.maximum_runtime_ref =
      static_cast<RuntimeLexemeRef>(lexeme_count);
  return fixture;
}

std::vector<RawQueryHit> reference_hits(const SyntheticFixture& fixture,
                                        std::string_view code,
                                        std::string_view scheme_id =
                                            "full_pinyin") {
  std::map<StableLexemeKey, RuntimeLexemeRef> refs;
  for (std::size_t index = 0; index < fixture.input.lexemes.size(); ++index) {
    refs[fixture.input.lexemes[index].stable_key] =
        static_cast<RuntimeLexemeRef>(index + 1U);
  }
  std::vector<RawQueryHit> hits;
  std::uint32_t ordinal = 0U;
  for (const SchemeMappingInput& mapping : fixture.input.mappings) {
    if (mapping.scheme_id == scheme_id) {
      if (mapping.code == code) {
        hits.push_back(
            {refs.at(mapping.stable_key), mapping.scheme_weight, ordinal});
      }
      ++ordinal;
    }
  }
  std::sort(hits.begin(), hits.end(),
            [](const RawQueryHit& left, const RawQueryHit& right) {
              if (left.runtime_ref != right.runtime_ref) {
                return left.runtime_ref < right.runtime_ref;
              }
              if (left.scheme_weight != right.scheme_weight) {
                return left.scheme_weight > right.scheme_weight;
              }
              return left.mapping_ordinal < right.mapping_ordinal;
            });
  return hits;
}

bool same_hits(const std::vector<RawQueryHit>& left,
               const std::vector<RawQueryHit>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (left[index].runtime_ref != right[index].runtime_ref ||
        left[index].scheme_weight != right[index].scheme_weight ||
        left[index].mapping_ordinal != right[index].mapping_ordinal) {
      return false;
    }
  }
  return true;
}

struct ValidationTotals {
  WorkCost cost;
  std::size_t steps{0};
};

ValidationTotals validate(PagedSchemeIndex* index, const WorkBudget& budget,
                           const CountingReader* reader = nullptr) {
  ValidationTotals totals;
  for (std::size_t guard = 0; guard < 2'000'000U; ++guard) {
    const auto calls_before = reader ? reader->read_calls() : 0U;
    const auto bytes_before = reader ? reader->read_bytes() : 0U;
    const ValidationStepResult step = index->advance_validation(budget);
    expect(step.cost.read_calls <= budget.max_read_calls &&
               step.cost.read_bytes <= budget.max_read_bytes &&
               step.cost.work_items <= budget.max_work_items,
           "validation exceeded its step budget");
    if (reader) {
      expect(step.cost.read_calls == reader->read_calls() - calls_before &&
                 step.cost.read_bytes == reader->read_bytes() - bytes_before,
             "validation cost did not account for actual reader requests");
    }
    totals.cost.read_calls += step.cost.read_calls;
    totals.cost.read_bytes += step.cost.read_bytes;
    totals.cost.work_items += step.cost.work_items;
    ++totals.steps;
    if (step.status == ValidationStepStatus::Ready) {
      return totals;
    }
    expect(step.status == ValidationStepStatus::InProgress,
           "validation failed: " + step.error);
  }
  throw TestFailure("validation did not finish within the guard");
}

struct QueryTotals {
  std::vector<RawQueryHit> hits;
  WorkCost cost;
  std::size_t steps{0};
  ExactQueryStepStatus terminal{ExactQueryStepStatus::Error};
};

QueryTotals finish_query(PagedSchemeIndex* index, ExactQueryCursor* cursor,
                         const WorkBudget& budget,
                         std::size_t batch_capacity) {
  QueryTotals totals;
  for (std::size_t guard = 0; guard < 2'000'000U; ++guard) {
    ExactQueryStepResult step =
        index->advance_exact_query(cursor, budget, batch_capacity);
    expect(step.cost.read_calls <= budget.max_read_calls &&
               step.cost.read_bytes <= budget.max_read_bytes &&
               step.cost.work_items <= budget.max_work_items,
           "query exceeded its step budget");
    expect(step.batch.hits.size() <= batch_capacity,
           "query returned an oversized batch");
    totals.cost.read_calls += step.cost.read_calls;
    totals.cost.read_bytes += step.cost.read_bytes;
    totals.cost.work_items += step.cost.work_items;
    ++totals.steps;
    if (step.status == ExactQueryStepStatus::HitsAvailable) {
      totals.hits.insert(totals.hits.end(), step.batch.hits.begin(),
                         step.batch.hits.end());
      continue;
    }
    if (step.status == ExactQueryStepStatus::InProgress) {
      continue;
    }
    expect(step.status == ExactQueryStepStatus::Complete ||
               step.status == ExactQueryStepStatus::NoResults,
           "query failed: " + step.error);
    totals.terminal = step.status;
    return totals;
  }
  throw TestFailure("query did not finish within the guard");
}

struct OpenedIndex {
  std::shared_ptr<CountingReader> reader;
  std::unique_ptr<PagedSchemeIndex> index;
  ValidationTotals validation;
};

OpenedIndex open_index(const SyntheticFixture& fixture,
                       std::size_t cache_capacity,
                       const WorkBudget& validation_budget,
                       std::size_t max_single_read_bytes = 17U,
                       std::string_view scheme_id = "full_pinyin") {
  OpenedIndex opened;
  opened.reader = std::make_shared<CountingReader>(
      fixture.built.scheme_bundles.at(std::string(scheme_id)));
  opened.index = std::make_unique<PagedSchemeIndex>();
  PagedSchemeIndexConfig config;
  config.page_cache_capacity = cache_capacity;
  config.max_single_read_bytes = max_single_read_bytes;
  std::string error;
  expect(opened.index->open(
             opened.reader, fixture.input.registry_namespace,
             fixture.built.manifest.shared_bundle_id,
             fixture.maximum_runtime_ref, config, &error),
         "paged index open failed: " + error);
  expect(!opened.index->ready(),
         "paged index activated before full validation");
  ExactQueryCursor premature;
  expect(!opened.index->begin_exact_query("same", 1U, &premature, &error),
         "query began before validation completed");
  opened.validation = validate(opened.index.get(), validation_budget, opened.reader.get());
  expect(opened.index->ready(), "validated index is not ready");
  return opened;
}

QueryTotals query_code(PagedSchemeIndex* index, std::string code,
                       const WorkBudget& budget,
                       std::size_t batch_capacity) {
  ExactQueryCursor cursor;
  std::string error;
  expect(index->begin_exact_query(std::move(code), 9U, &cursor, &error),
         "query begin failed: " + error);
  return finish_query(index, &cursor, budget, batch_capacity);
}

void test_q1_reference_and_cross_page() {
  const SyntheticFixture fixture = build_synthetic(128U, 96U);
  OpenedIndex opened =
      open_index(fixture, 2U, WorkBudget{2U, 19U, 3U}, 11U);
  const WorkBudget query_budget{1U, 7U, 1U};

  const QueryTotals same =
      query_code(opened.index.get(), "same", query_budget, 3U);
  expect(same.terminal == ExactQueryStepStatus::Complete &&
             same_hits(same.hits, reference_hits(fixture, "same")) &&
             same.hits.size() == 42U,
         "cross-leaf same-code query disagrees with the reference");
  const auto duplicate_count = static_cast<std::size_t>(std::count_if(
      same.hits.begin(), same.hits.end(), [](const RawQueryHit& hit) {
        return hit.runtime_ref == 1U;
      }));
  expect(duplicate_count == 2U,
         "duplicate mapping paths were merged in the raw query layer");

  for (const std::string& code : {std::string("aaaa"),
                                  std::string("code00000"),
                                  std::string("code00127"),
                                  std::string("zzzz")}) {
    const QueryTotals actual =
        query_code(opened.index.get(), code, query_budget, 2U);
    expect(actual.terminal == ExactQueryStepStatus::Complete &&
               same_hits(actual.hits, reference_hits(fixture, code)),
           "boundary-code query disagrees with the reference: " + code);
  }
  const QueryTotals absent =
      query_code(opened.index.get(), "zzzzz", query_budget, 2U);
  expect(absent.terminal == ExactQueryStepStatus::NoResults &&
             absent.hits.empty(),
         "true no-result query did not finish as NoResults");

  OpenedIndex xiaohe = open_index(
      fixture, 1U, WorkBudget{1U, 13U, 2U}, 7U, "xiaohe");
  const QueryTotals xiaohe_hits =
      query_code(xiaohe.index.get(), "sx", query_budget, 2U);
  expect(xiaohe_hits.terminal == ExactQueryStepStatus::Complete &&
             same_hits(xiaohe_hits.hits,
                       reference_hits(fixture, "sx", "xiaohe")),
         "xiaohe did not use the same paged query logic");

  const ValidationStepResult invalid_validation =
      opened.index->advance_validation(WorkBudget{0U, 1U, 1U});
  expect(invalid_validation.status == ValidationStepStatus::InvalidBudget,
         "zero validation budget was not rejected");
  ExactQueryCursor cursor;
  std::string error;
  expect(opened.index->begin_exact_query("same", 9U, &cursor, &error), error);
  const ExactQueryStepResult invalid_query =
      opened.index->advance_exact_query(&cursor, WorkBudget{1U, 0U, 1U}, 1U);
  expect(invalid_query.status == ExactQueryStepStatus::InvalidBudget,
         "zero query budget was not rejected");
}

void test_q2_cache_does_not_change_results() {
  const SyntheticFixture fixture = build_synthetic(128U, 96U);
  OpenedIndex one_page =
      open_index(fixture, 1U, WorkBudget{1U, 13U, 1U}, 5U);
  OpenedIndex four_pages =
      open_index(fixture, 4U, WorkBudget{3U, 31U, 5U}, 19U);
  const QueryTotals one = query_code(
      one_page.index.get(), "same", WorkBudget{1U, 9U, 2U}, 4U);
  const QueryTotals four = query_code(
      four_pages.index.get(), "same", WorkBudget{2U, 37U, 7U}, 9U);
  expect(same_hits(one.hits, four.hits) &&
             same_hits(one.hits, reference_hits(fixture, "same")),
         "cache or batch capacity changed raw query coverage");
  expect(one_page.index->cached_page_count() <= 1U &&
             one_page.index->resident_page_bytes() <= 96U &&
             four_pages.index->cached_page_count() <= 4U &&
             four_pages.index->resident_page_bytes() <= 4U * 96U,
         "page cache exceeded its configured capacity");
  expect(one_page.reader->max_requested() <= 5U &&
             four_pages.reader->max_requested() <= 19U,
         "resource reader received an oversized synchronous request");
}

void test_q3_mount_validation_and_damage() {
  const SyntheticFixture small = build_synthetic(128U, 96U);
  const SyntheticFixture large = build_synthetic(1280U, 96U);
  OpenedIndex small_index =
      open_index(small, 2U, WorkBudget{2U, 23U, 4U}, 13U);
  OpenedIndex large_index =
      open_index(large, 2U, WorkBudget{2U, 23U, 4U}, 13U);
  expect(small_index.index->info().initial_read_calls ==
             large_index.index->info().initial_read_calls &&
             small_index.index->info().initial_read_bytes ==
                 large_index.index->info().initial_read_bytes,
         "fixed open work grew with the record count");
  expect(large_index.index->info().file_bytes >
             small_index.index->info().file_bytes &&
             large_index.index->info().tree_depth >=
                 small_index.index->info().tree_depth,
         "synthetic scale did not grow the paged index");

  PagedSchemeIndexConfig config;
  config.page_cache_capacity = 1U;
  config.max_single_read_bytes = 16U;
  std::string error;
  PagedSchemeIndex mismatch;
  expect(!mismatch.open(
             std::make_shared<CountingReader>(
                 small.built.scheme_bundles.at("full_pinyin")),
             small.input.registry_namespace,
             small.built.manifest.shared_bundle_id + 1U,
             small.maximum_runtime_ref, config, &error) &&
             error.find("another registry or shared bundle") !=
                 std::string::npos,
         "binding mismatch was not rejected during fixed open");

  std::vector<std::uint8_t> truncated =
      small.built.scheme_bundles.at("full_pinyin");
  truncated.pop_back();
  PagedSchemeIndex truncated_index;
  error.clear();
  expect(!truncated_index.open(std::make_shared<CountingReader>(truncated),
                               small.input.registry_namespace,
                               small.built.manifest.shared_bundle_id,
                               small.maximum_runtime_ref, config, &error) &&
             error.find("body size") != std::string::npos,
         "truncated paged index was not rejected");

  std::vector<std::uint8_t> corrupted =
      small.built.scheme_bundles.at("full_pinyin");
  corrupted[corrupted.size() / 2U] ^= 0x5aU;
  PagedSchemeIndex corrupted_index;
  error.clear();
  expect(corrupted_index.open(std::make_shared<CountingReader>(corrupted),
                              small.input.registry_namespace,
                              small.built.manifest.shared_bundle_id,
                              small.maximum_runtime_ref, config, &error),
         "bounded open unexpectedly scanned the corrupted body: " + error);
  ValidationStepStatus final_status = ValidationStepStatus::InProgress;
  for (std::size_t guard = 0; guard < 1'000'000U; ++guard) {
    const ValidationStepResult step =
        corrupted_index.advance_validation(WorkBudget{1U, 17U, 1U});
    final_status = step.status;
    if (final_status != ValidationStepStatus::InProgress) {
      expect(final_status == ValidationStepStatus::Error &&
                 step.error.find("checksum") != std::string::npos,
             "corruption did not end as an explicit checksum error");
      break;
    }
  }
  expect(final_status == ValidationStepStatus::Error &&
             !corrupted_index.ready(),
         "corrupted index was activated");

  OpenedIndex read_failure =
      open_index(small, 1U, WorkBudget{2U, 23U, 4U}, 13U);
  read_failure.reader->set_fail_reads(true);
  ExactQueryCursor failed_cursor;
  error.clear();
  expect(read_failure.index->begin_exact_query("same", 21U, &failed_cursor,
                                               &error),
         error);
  const ExactQueryStepResult failed_step =
      read_failure.index->advance_exact_query(
          &failed_cursor, WorkBudget{1U, 17U, 2U}, 2U);
  expect(failed_step.status == ExactQueryStepStatus::Error &&
             failed_step.cost.read_calls == 1U &&
             failed_step.cost.read_bytes == 13U &&
             failed_step.error.find("read failed") != std::string::npos,
         "query reader failure was not reported as Error");
}

void test_q4_cancel_stale_and_new_query_survives() {
  const SyntheticFixture fixture = build_synthetic(128U, 96U);
  OpenedIndex opened =
      open_index(fixture, 2U, WorkBudget{2U, 29U, 3U}, 13U);
  std::string error;
  ExactQueryCursor old_cursor;
  expect(opened.index->begin_exact_query("same", 15U, &old_cursor, &error),
         error);
  RawQueryBatch old_batch;
  for (std::size_t guard = 0; guard < 10000U; ++guard) {
    ExactQueryStepResult step = opened.index->advance_exact_query(
        &old_cursor, WorkBudget{1U, 11U, 2U}, 1U);
    if (step.status == ExactQueryStepStatus::HitsAvailable) {
      old_batch = std::move(step.batch);
      break;
    }
    expect(step.status == ExactQueryStepStatus::InProgress,
           "old query failed before producing a batch");
  }
  expect(old_batch.hits.size() == 1U &&
             opened.index->batch_is_current(old_batch),
         "old query did not produce a current batch");

  ExactQueryCursor new_cursor;
  expect(opened.index->begin_exact_query("aaaa", 16U, &new_cursor, &error),
         error);
  expect(!opened.index->batch_is_current(old_batch),
         "old batch remained current after new input");
  const ExactQueryStepResult stale = opened.index->advance_exact_query(
      &old_cursor, WorkBudget{1U, 11U, 2U}, 1U);
  expect(stale.status == ExactQueryStepStatus::Stale,
         "old cursor was not rejected as stale");
  const QueryTotals new_result = finish_query(
      opened.index.get(), &new_cursor, WorkBudget{1U, 11U, 2U}, 1U);
  expect(new_result.terminal == ExactQueryStepStatus::Complete &&
             same_hits(new_result.hits, reference_hits(fixture, "aaaa")),
         "late old work damaged the new query");

  ExactQueryCursor cancelled;
  expect(opened.index->begin_exact_query("same", 17U, &cancelled, &error),
         error);
  opened.index->cancel_query(&cancelled);
  const ExactQueryStepResult cancelled_step =
      opened.index->advance_exact_query(
          &cancelled, WorkBudget{1U, 11U, 2U}, 1U);
  expect(cancelled_step.status == ExactQueryStepStatus::Cancelled,
         "cancelled query continued running");

  ExactQueryCursor resource_cursor;
  expect(opened.index->begin_exact_query("same", 18U, &resource_cursor,
                                         &error),
         error);
  ExactQueryStepResult resource_batch;
  for (std::size_t guard = 0; guard < 10000U; ++guard) {
    resource_batch = opened.index->advance_exact_query(
        &resource_cursor, WorkBudget{1U, 11U, 2U}, 1U);
    if (resource_batch.status == ExactQueryStepStatus::HitsAvailable) {
      break;
    }
    expect(resource_batch.status == ExactQueryStepStatus::InProgress,
           "resource-generation query failed before its first batch");
  }
  expect(resource_batch.status == ExactQueryStepStatus::HitsAvailable,
         "resource-generation query produced no batch");
  opened.index->deactivate();
  expect(!opened.index->batch_is_current(resource_batch.batch),
         "old batch survived resource deactivation");
  const ExactQueryStepResult resource_stale =
      opened.index->advance_exact_query(
          &resource_cursor, WorkBudget{1U, 11U, 2U}, 1U);
  expect(resource_stale.status == ExactQueryStepStatus::Stale,
         "old cursor survived resource deactivation");
}

void test_q5_scale_and_cost_report() {
  const SyntheticFixture small = build_synthetic(128U, 96U);
  const SyntheticFixture large = build_synthetic(1280U, 96U);
  OpenedIndex small_index =
      open_index(small, 2U, WorkBudget{2U, 37U, 5U}, 17U);
  OpenedIndex large_index =
      open_index(large, 2U, WorkBudget{2U, 37U, 5U}, 17U);
  small_index.reader->clear_cost();
  large_index.reader->clear_cost();
  const QueryTotals small_query = query_code(
      small_index.index.get(), "same", WorkBudget{2U, 37U, 5U}, 8U);
  const QueryTotals large_query = query_code(
      large_index.index.get(), "same", WorkBudget{2U, 37U, 5U}, 8U);
  expect(same_hits(small_query.hits, reference_hits(small, "same")) &&
             same_hits(large_query.hits, reference_hits(large, "same")),
         "scale query disagrees with the reference");
  expect(sizeof(PagedSchemeIndex) == sizeof(std::unique_ptr<void*>) &&
             small_index.index->resident_page_bytes() <= 2U * 96U &&
             large_index.index->resident_page_bytes() <= 2U * 96U,
         "runtime object or resident page cache grew with table size");

  const auto report = [](std::string_view size_name,
                         const OpenedIndex& opened,
                         const QueryTotals& query) {
    const PagedSchemeIndexInfo& info = opened.index->info();
    std::cout << "{\"event\":\"STEP2A_COST\",\"dataset\":\""
              << size_name << "\",\"synthetic\":true,\"file_bytes\":"
              << info.file_bytes << ",\"mapping_count\":"
              << info.mapping_count << ",\"page_bytes\":"
              << info.page_bytes << ",\"page_count\":" << info.page_count
              << ",\"leaf_pages\":" << info.leaf_page_count
              << ",\"tree_depth\":" << info.tree_depth
              << ",\"fixed_open_read_calls\":"
              << info.initial_read_calls << ",\"fixed_open_read_bytes\":"
              << info.initial_read_bytes
              << ",\"validation_steps\":" << opened.validation.steps
              << ",\"validation_read_calls\":"
              << opened.validation.cost.read_calls
              << ",\"validation_read_bytes\":"
              << opened.validation.cost.read_bytes
              << ",\"validation_work_items\":"
              << opened.validation.cost.work_items
              << ",\"query_steps\":" << query.steps
              << ",\"query_read_calls\":" << query.cost.read_calls
              << ",\"query_read_bytes\":" << query.cost.read_bytes
              << ",\"query_work_items\":" << query.cost.work_items
              << ",\"raw_hits\":" << query.hits.size()
              << ",\"cache_capacity_pages\":2"
              << ",\"resident_page_bytes_upper\":" << 2U * info.page_bytes
              << ",\"cursor_page_bytes_upper\":" << info.page_bytes
              << ",\"validation_page_buffer_bytes_upper\":" << 2U * info.page_bytes
              << ",\"validation_code_logical_bytes_upper\":" << 2U * info.page_bytes
              << ",\"query_context_shallow_bytes\":" << sizeof(QueryBatchContext)
              << ",\"batch_hit_bytes_shallow_upper\":"
              << 8U * sizeof(RawQueryHit)
              << ",\"peak_total_ram\":\"not_measured\"}\n";
  };
  report("N", small_index, small_query);
  report("10N", large_index, large_query);
}

RawQueryBatch first_batch(PagedSchemeIndex* index, ExactQueryCursor* cursor) {
  for (std::size_t guard = 0; guard < 10000U; ++guard) {
    auto step = index->advance_exact_query(cursor, WorkBudget{1U, 7U, 1U}, 1U);
    if (step.status == ExactQueryStepStatus::HitsAvailable) {
      return std::move(step.batch);
    }
    expect(step.status == ExactQueryStepStatus::InProgress,
           "query failed before its first batch");
  }
  throw TestFailure("first batch did not arrive");
}

void test_q6_instance_ownership() {
  const auto fixture = build_synthetic(128U, 96U);
  auto a = open_index(fixture, 1U, WorkBudget{1U, 7U, 1U});
  auto b = open_index(fixture, 1U, WorkBudget{1U, 7U, 1U}, 17U, "xiaohe");
  ExactQueryCursor old_cursor, new_cursor;
  std::string error;
  expect(a.index->begin_exact_query("same", 9U, &old_cursor, &error), error);
  const auto old_batch = first_batch(a.index.get(), &old_cursor);
  a.index->deactivate();
  expect(b.index->begin_exact_query("sx", 9U, &new_cursor, &error), error);
  expect(!b.index->batch_is_current(old_batch),
         "foreign batch accepted after same-MountEpoch scheme replacement");
  const auto revision = b.index->current_query_revision();
  auto foreign = b.index->advance_exact_query(&old_cursor, {1U, 7U, 1U}, 1U);
  expect(foreign.status == ExactQueryStepStatus::Stale &&
             foreign.cost.read_calls == 0U && foreign.cost.work_items == 0U,
         "new instance advanced a foreign cursor");
  b.index->cancel_query(&old_cursor);
  expect(b.index->current_query_revision() == revision &&
             same_hits(finish_query(b.index.get(), &new_cursor,
                                   {1U, 7U, 1U}, 2U).hits,
                       reference_hits(fixture, "sx", "xiaohe")),
         "foreign cancellation damaged the new query");

  // Also prove cancellation through B leaves a still-live A cursor untouched.
  auto live_a = open_index(fixture, 1U, {1U, 7U, 1U});
  ExactQueryCursor live_cursor;
  expect(live_a.index->begin_exact_query("aaaa", 9U, &live_cursor, &error), error);
  expect(b.index->advance_exact_query(&live_cursor, {1U, 7U, 1U}, 1U).status ==
             ExactQueryStepStatus::Stale,
         "foreign active cursor was advanced");
  b.index->cancel_query(&live_cursor);
  expect(same_hits(finish_query(live_a.index.get(), &live_cursor,
                               {1U, 7U, 1U}, 1U).hits,
                   reference_hits(fixture, "aaaa")),
         "foreign cancellation mutated another instance's cursor");

  // Reconstruct exactly the same scheme and package at exactly the same address.
  alignas(PagedSchemeIndex) unsigned char storage[sizeof(PagedSchemeIndex)];
  auto destroy = [](PagedSchemeIndex* index) { index->~PagedSchemeIndex(); };
  std::unique_ptr<PagedSchemeIndex, decltype(destroy)> reused(
      new (storage) PagedSchemeIndex(), destroy);
  const auto reopen = [&]() {
    expect(reused->open(a.reader, fixture.input.registry_namespace,
                        fixture.built.manifest.shared_bundle_id,
                        fixture.maximum_runtime_ref, {1U, 17U}, &error), error);
    validate(reused.get(), {1U, 7U, 1U});
  };
  reopen();
  ExactQueryCursor destroyed_cursor;
  expect(reused->begin_exact_query("same", 9U, &destroyed_cursor, &error), error);
  const auto destroyed_batch = first_batch(reused.get(), &destroyed_cursor);
  reused.reset();
  reused.reset(new (storage) PagedSchemeIndex());
  reopen();
  expect(reused->begin_exact_query("aaaa", 9U, &new_cursor, &error), error);
  expect(!reused->batch_is_current(destroyed_batch),
         "address reuse revived an old batch from the same package");
  reused->cancel_query(&destroyed_cursor);
  expect(reused->advance_exact_query(&destroyed_cursor, {1U, 7U, 1U}, 1U).status ==
             ExactQueryStepStatus::Stale,
         "recreated instance accepted a destroyed instance's cursor");
  expect(same_hits(finish_query(reused.get(), &new_cursor, {1U, 7U, 1U}, 1U).hits,
                   reference_hits(fixture, "aaaa")),
         "destroyed instance cancellation damaged replacement");

  // Re-open the same live instance, then reject old context and stale cancellation.
  expect(reused->begin_exact_query("same", 9U, &old_cursor, &error), error);
  const auto before_reopen = first_batch(reused.get(), &old_cursor);
  reopen();
  expect(reused->begin_exact_query("aaaa", 9U, &new_cursor, &error), error);
  expect(!reused->batch_is_current(before_reopen), "reopen revived old context");
  const auto reopened_revision = reused->current_query_revision();
  reused->cancel_query(&old_cursor);
  expect(reused->current_query_revision() == reopened_revision &&
             reused->advance_exact_query(&old_cursor, {1U, 7U, 1U}, 1U).status ==
                 ExactQueryStepStatus::Stale &&
             same_hits(finish_query(reused.get(), &new_cursor,
                                    {1U, 7U, 1U}, 1U).hits,
                       reference_hits(fixture, "aaaa")),
         "reopen or stale cancellation damaged current query");
}

std::uint64_t read_le(const std::vector<std::uint8_t>& bytes,
                      std::size_t offset, std::size_t count) {
  std::uint64_t value = 0U;
  for (std::size_t i = 0; i < count; ++i) {
    value |= static_cast<std::uint64_t>(bytes.at(offset + i)) << (i * 8U);
  }
  return value;
}

void write_le(std::vector<std::uint8_t>* bytes, std::size_t offset,
              std::size_t count, std::uint64_t value) {
  for (std::size_t i = 0; i < count; ++i) {
    bytes->at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8U));
  }
}

void refresh_body_checksum(std::vector<std::uint8_t>* bytes) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (std::size_t i = 40U; i < bytes->size(); ++i) {
    hash = (hash ^ bytes->at(i)) * 1099511628211ULL;
  }
  write_le(bytes, 24U, 8U, hash);
}

void test_q7_semantic_structure() {
  const auto fixture = build_synthetic(128U, 96U);
  const auto original = fixture.built.scheme_bundles.at("full_pinyin");
  const std::size_t pages = 40U + read_le(original, 72U, 8U);
  const std::size_t root_id = read_le(original, 52U, 4U);
  const std::size_t root = pages + root_id * 96U;
  const std::size_t first_record = root + 16U;
  const std::size_t first_child = first_record + 2U +
                                read_le(original, first_record, 2U);
  const std::size_t second_record = first_child + 4U;
  const std::size_t second_child = second_record + 2U +
                                 read_le(original, second_record, 2U);
  expect(root_id == 66U && read_le(original, first_child, 4U) == 64U &&
             read_le(original, second_child, 4U) == 65U,
         "review reproduction fixture layout changed");

  const std::vector<std::string> variants = {
      "duplicate-root-child", "wrong-fence", "wrong-child-level",
      "omitted-last-subtree", "unordered-fences", "broken-leaf-chain",
      "wrong-root-depth", "wrong-mapping-total"};
  for (const auto& variant : variants) {
    auto bytes = original;
    if (variant == "duplicate-root-child") {
      write_le(&bytes, first_child, 4U, 65U);
    } else if (variant == "wrong-fence") {
      bytes[first_record + 2U] = 'b'; // valid UTF-8, still below next fence
    } else if (variant == "wrong-child-level") {
      bytes[pages + 64U * 96U + 5U] = 1U; // internal but skips a level
    } else if (variant == "omitted-last-subtree") {
      write_le(&bytes, root + 6U, 2U, 1U);
      write_le(&bytes, root + 12U, 4U, second_record - root);
    } else if (variant == "unordered-fences") {
      bytes[first_record + 2U] = '~';
    } else if (variant == "broken-leaf-chain") {
      write_le(&bytes, pages + 8U, 4U, 2U);
    } else if (variant == "wrong-root-depth") {
      bytes[root + 5U] = 4U;
      write_le(&bytes, 56U, 4U, 5U); // self-consistent fixed root header
    } else {
      write_le(&bytes, 60U, 4U, read_le(bytes, 60U, 4U) + 1U);
    }
    refresh_body_checksum(&bytes);
    auto reader = std::make_shared<CountingReader>(std::move(bytes));
    PagedSchemeIndex index;
    std::string error;
    expect(index.open(reader, fixture.input.registry_namespace,
                      fixture.built.manifest.shared_bundle_id,
                      fixture.maximum_runtime_ref, {1U, 7U}, &error),
           variant + ": open must remain bounded: " + error);
    ValidationStepResult step;
    for (std::size_t guard = 0; guard < 100000U; ++guard) {
      reader->clear_cost();
      step = index.advance_validation({1U, 1U, 1U});
      expect(step.cost.read_calls <= 1U && step.cost.read_bytes <= 1U &&
                 step.cost.work_items <= 1U &&
                 step.cost.read_calls == reader->read_calls() &&
                 step.cost.read_bytes == reader->read_bytes(),
             variant + ": structural validation exceeded or hid budgeted I/O");
      if (step.status != ValidationStepStatus::InProgress) {
        break;
      }
    }
    expect(step.status == ValidationStepStatus::Error && !index.ready() &&
               !step.error.empty() && step.error.find("checksum") == std::string::npos,
           variant + ": checksum-correct malformed tree was not rejected");
    ExactQueryCursor cursor;
    expect(!index.begin_exact_query("aaaa", 9U, &cursor, &error),
           variant + ": malformed tree allowed a query");
    std::cout << "{\"event\":\"SEMANTIC_REJECT\",\"variant\":\"" << variant
              << "\",\"error\":\"" << step.error << "\"}\n";
  }

  // A root that is itself a single leaf is a valid coverage base case.
  auto single = fixture;
  single.input.mappings = {{"full_pinyin", "one", single.input.lexemes[0].stable_key, 1}};
  auto built = ResourceBuilder().build(single.input, ResourceBuildOptions{96U});
  expect(built.ok, built.error);
  single.built = std::move(built.resources);
  auto one_leaf = open_index(single, 1U, {1U, 1U, 1U}, 1U);
  expect(one_leaf.index->info().page_count == 1U &&
             same_hits(query_code(one_leaf.index.get(), "one", {1U, 1U, 1U}, 1U).hits,
                       reference_hits(single, "one")),
         "single-leaf root failed validation or enumeration");

  // Full valid tree, minimum legal budgets: cover every distinct code, not just same.
  auto valid = open_index(fixture, 1U, {1U, 1U, 1U}, 1U);
  std::map<std::string, bool> codes;
  for (const auto& mapping : fixture.input.mappings) {
    if (mapping.scheme_id == "full_pinyin") codes[mapping.code] = true;
  }
  for (const auto& [code, unused] : codes) {
    (void)unused;
    expect(same_hits(query_code(valid.index.get(), code, {1U, 1U, 1U}, 1U).hits,
                     reference_hits(fixture, code)),
           "minimum-budget complete enumeration failed for " + code);
  }
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"Q1-reference-cross-page", test_q1_reference_and_cross_page},
      {"Q2-budget-cache", test_q2_cache_does_not_change_results},
      {"Q3-mount-validation", test_q3_mount_validation_and_damage},
      {"Q4-cancel-stale", test_q4_cancel_stale_and_new_query_survives},
      {"Q5-scale-cost", test_q5_scale_and_cost_report},
      {"Q6-instance-ownership", test_q6_instance_ownership},
      {"Q7-semantic-structure", test_q7_semantic_structure},
  };
  std::size_t passed = 0U;
  for (const auto& [name, test] : tests) {
    try {
      test();
      ++passed;
      std::cout << "{\"test\":\"" << name
                << "\",\"status\":\"PASS\"}\n";
    } catch (const std::exception& error) {
      std::cerr << "{\"test\":\"" << name
                << "\",\"status\":\"FAIL\",\"message\":\""
                << error.what() << "\"}\n";
    }
  }
  std::cout << "{\"event\":\"STEP2A_TEST_SUMMARY\",\"passed\":"
            << passed << ",\"total\":" << tests.size() << "}\n";
  return passed == tests.size() ? 0 : 1;
}
