#include "hanime/core.hpp"
#include "hanime/small_fixture.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
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

class MemoryReader final : public ResourceReader {
 public:
  explicit MemoryReader(std::vector<std::uint8_t> bytes)
      : bytes_(std::move(bytes)) {}

  std::size_t size() const override { return bytes_.size(); }

  bool read(std::size_t offset, void* destination,
            std::size_t byte_count) const override {
    if (offset > bytes_.size() || byte_count > bytes_.size() - offset) {
      return false;
    }
    if (byte_count != 0U) {
      std::memcpy(destination, bytes_.data() + offset, byte_count);
    }
    ++read_calls_;
    bytes_read_ += byte_count;
    return true;
  }

  std::size_t read_calls() const { return read_calls_; }
  std::size_t bytes_read() const { return bytes_read_; }

 private:
  std::vector<std::uint8_t> bytes_;
  mutable std::size_t read_calls_{0};
  mutable std::size_t bytes_read_{0};
};

class RecordingPresenter final : public CandidatePresenter {
 public:
  bool present(std::uint64_t snapshot_id,
               const std::vector<CandidateView>& candidates) override {
    ++call_count;
    last_snapshot_id = snapshot_id;
    last_candidates = candidates;
    return succeed;
  }

  bool succeed{true};
  std::size_t call_count{0};
  std::uint64_t last_snapshot_id{0};
  std::vector<CandidateView> last_candidates;
};

class RecordingCommitSink final : public TextCommitSink {
 public:
  bool commit(std::string_view text_utf8) override {
    ++call_count;
    committed_texts.emplace_back(text_utf8);
    return next_result;
  }

  bool next_result{true};
  std::size_t call_count{0};
  std::vector<std::string> committed_texts;
};

class MemoryPersistence final : public UserStatePersistence {
 public:
  PersistenceLoadResult load(RegistryNamespaceId registry_namespace,
                             StableLexemeKey stable_key,
                             std::uint32_t* selection_count) override {
    ++load_calls;
    if (fail_load) {
      return PersistenceLoadResult::Error;
    }
    const auto item = values.find({registry_namespace, stable_key});
    if (item == values.end()) {
      return PersistenceLoadResult::NotFound;
    }
    *selection_count = item->second;
    return PersistenceLoadResult::Found;
  }

  bool write(RegistryNamespaceId registry_namespace,
             StableLexemeKey stable_key,
             std::uint32_t selection_count) override {
    ++write_calls;
    if (fail_next_writes != 0U) {
      --fail_next_writes;
      return false;
    }
    values[{registry_namespace, stable_key}] = selection_count;
    return true;
  }

  std::uint32_t persisted(RegistryNamespaceId registry_namespace,
                          StableLexemeKey stable_key) const {
    const auto item = values.find({registry_namespace, stable_key});
    return item == values.end() ? 0U : item->second;
  }

  bool fail_load{false};
  std::size_t fail_next_writes{0};
  std::size_t load_calls{0};
  std::size_t write_calls{0};
  std::map<std::pair<RegistryNamespaceId, StableLexemeKey>, std::uint32_t>
      values;
};

BuiltResources build_fixture(bool reversed = false) {
  ResourceBuilder builder;
  BuildResult result =
      builder.build(fixture::make_small_input(reversed));
  expect(result.ok, "fixture build failed: " + result.error);
  return std::move(result.resources);
}

std::vector<std::shared_ptr<const ResourceReader>> scheme_readers(
    const BuiltResources& resources) {
  std::vector<std::shared_ptr<const ResourceReader>> readers;
  for (const auto& [scheme_id, bytes] : resources.scheme_bundles) {
    static_cast<void>(scheme_id);
    readers.push_back(std::make_shared<MemoryReader>(bytes));
  }
  return readers;
}

void mount(MountedResourceSet* mounted, const BuiltResources& resources) {
  std::string error;
  const bool ok = mounted->mount(
      std::make_shared<MemoryReader>(resources.shared_bundle),
      scheme_readers(resources), &error);
  expect(ok, "resource mount failed: " + error);
}

RuntimeLexemeRef find_ref(const MountedResourceSet& resources,
                          StableLexemeKey key) {
  for (RuntimeLexemeRef runtime_ref = 1U; runtime_ref < 1024U;
       ++runtime_ref) {
    RuntimeLexemeInfo info;
    if (!resources.lexeme_info(runtime_ref, &info)) {
      break;
    }
    if (info.stable_key == key) {
      return runtime_ref;
    }
  }
  throw TestFailure("stable key was not found in mounted resources");
}

CandidateView candidate_for(const std::vector<CandidateView>& candidates,
                            StableLexemeKey key) {
  const auto candidate = std::find_if(
      candidates.begin(), candidates.end(),
      [key](const CandidateView& value) { return value.stable_key == key; });
  if (candidate == candidates.end()) {
    throw TestFailure("expected candidate was not found");
  }
  return *candidate;
}

std::uint32_t count(UserStateStore* state, StableLexemeKey key) {
  std::uint32_t value = 0;
  expect(state->selection_count(key, &value), "state load failed");
  return value;
}

void test_a1() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");

  std::string error;
  expect(session.query_complete_code("yuanxingji", &error), error);
  const auto full = session.current_candidates();
  expect(full.size() == 1U,
         "duplicate runtime refs were not display-deduplicated");
  expect(full[0].stable_key == fixture::kPrototype,
         "full pinyin resolved the wrong stable key");
  const RuntimeLexemeRef full_ref = full[0].handle.runtime_ref;

  session.cancel();
  expect(session.switch_scheme_when_idle("xiaohe"),
         "idle scheme switch failed");
  expect(session.query_complete_code("yrxkji", &error), error);
  const auto xiaohe = session.current_candidates();
  expect(xiaohe.size() == 1U &&
             xiaohe[0].stable_key == fixture::kPrototype,
         "xiaohe resolved the wrong stable key");
  expect(xiaohe[0].handle.runtime_ref == full_ref,
         "two schemes did not converge on one runtime ref");
}

void test_a2() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;

  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateHandle handle = session.current_candidates().at(0).handle;
  expect(session.confirm(handle) == ConfirmResult::CommittedPersisted,
         "successful commit was not persisted");
  expect(count(&state, fixture::kPrototype) == 1U,
         "full-pinyin commit did not increment shared state");
  expect(session.switch_scheme_when_idle("xiaohe"),
         "post-commit idle switch failed");
  expect(session.query_complete_code("yrxkji", &error), error);
  expect(session.current_candidates().at(0).selection_count == 1U,
         "xiaohe did not observe the shared count");
}

void test_a3() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;

  expect(session.query_complete_code("yuanxingji", &error), error);
  session.cancel();
  expect(count(&state, fixture::kPrototype) == 0U,
         "cancel changed learning state");

  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateHandle handle = session.current_candidates().at(0).handle;
  sink.next_result = false;
  expect(session.confirm(handle) == ConfirmResult::FailedRetryable,
         "failed commit was not reported as retryable");
  expect(session.snapshot_state() == CandidateSnapshotState::Ready,
         "failed commit did not restore ready snapshot");
  expect(count(&state, fixture::kPrototype) == 0U,
         "failed commit changed learning state");
  sink.next_result = true;
  expect(session.confirm(handle) == ConfirmResult::CommittedPersisted,
         "retry of the same candidate did not succeed");
}

void test_a4() {
  const BuiltResources build_a = build_fixture(false);
  const BuiltResources build_b = build_fixture(true);
  expect(build_a.manifest.shared_bundle_id !=
             build_b.manifest.shared_bundle_id,
         "reordered shared bundle kept the same bundle id");

  MemoryPersistence persistence;
  MountedResourceSet resources_a;
  mount(&resources_a, build_a);
  UserStateStore state_a(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter_a;
  RecordingCommitSink sink_a;
  ImeSession session_a(resources_a, state_a, presenter_a, sink_a,
                       "full_pinyin");
  std::string error;
  expect(session_a.query_complete_code("yuanxingji", &error), error);
  const CandidateView candidate_a = session_a.current_candidates().at(0);
  expect(session_a.confirm(candidate_a.handle) ==
             ConfirmResult::CommittedPersisted,
         "build A commit failed");

  MountedResourceSet resources_b;
  mount(&resources_b, build_b);
  UserStateStore state_b(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter_b;
  RecordingCommitSink sink_b;
  ImeSession session_b(resources_b, state_b, presenter_b, sink_b, "xiaohe");
  expect(session_b.query_complete_code("yrxkji", &error), error);
  const CandidateView candidate_b = session_b.current_candidates().at(0);
  expect(candidate_a.handle.runtime_ref != candidate_b.handle.runtime_ref,
         "reordered build did not change the runtime ref");
  expect(candidate_a.stable_key == candidate_b.stable_key &&
             candidate_b.selection_count == 1U,
         "stable identity or reloaded count changed after rebuild");
  expect(resources_a.default_payload_index(candidate_a.handle.runtime_ref) !=
             resources_b.default_payload_index(candidate_b.handle.runtime_ref),
         "fixture did not change the payload layout");
}

void test_a5() {
  const BuiltResources build_a = build_fixture(false);
  const BuiltResources build_b = build_fixture(true);
  MountedResourceSet resources;
  mount(&resources, build_a);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;
  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateHandle old_handle = session.current_candidates().at(0).handle;
  mount(&resources, build_b);
  expect(session.confirm(old_handle) == ConfirmResult::RejectedStale,
         "old mount candidate was accepted");
  expect(sink.call_count == 0U, "stale candidate called the commit sink");
}

void test_a6() {
  const BuiltResources build_a = build_fixture(false);
  const BuiltResources build_b = build_fixture(true);
  MountedResourceSet resources;
  std::string error;
  const bool mounted = resources.mount(
      std::make_shared<MemoryReader>(build_b.shared_bundle),
      scheme_readers(build_a), &error);
  expect(!mounted, "mismatched shared and scheme bundles were mounted");
  expect(error.find("another shared bundle") != std::string::npos,
         "mismatch failure did not identify bundle binding");

  std::vector<std::uint8_t> corrupted_shared = build_a.shared_bundle;
  corrupted_shared.back() ^= 0x01U;
  MountedResourceSet corrupted_resources;
  error.clear();
  expect(!corrupted_resources.mount(
             std::make_shared<MemoryReader>(std::move(corrupted_shared)),
             scheme_readers(build_a), &error) &&
             error.find("checksum") != std::string::npos,
         "corrupted shared bundle was not rejected by checksum");
}

void test_a7() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;

  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateView simplified = session.current_candidates().at(0);
  expect(simplified.text_utf8 == "原型机", "default output is wrong");
  session.cancel();
  expect(session.set_output_form_when_idle("zh-Hant"),
         "could not select zh-Hant while idle");
  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateView traditional = session.current_candidates().at(0);
  expect(traditional.text_utf8 == "原型機" &&
             traditional.stable_key == simplified.stable_key &&
             traditional.handle.runtime_ref == simplified.handle.runtime_ref,
         "output form changed identity or mapping");
  session.cancel();

  expect(session.query_complete_code("danyishuchu", &error), error);
  const CandidateView fallback = session.current_candidates().at(0);
  expect(fallback.text_utf8 == "单一输出" &&
             fallback.output_status ==
                 OutputResolutionStatus::FallbackDefault,
         "missing optional form did not fall back to default");
  expect(session.confirm(fallback.handle) == ConfirmResult::CommittedPersisted,
         "default-only lexeme could not be committed");

  expect(session.set_output_form_when_idle(""),
         "could not restore default output while idle");
  expect(session.query_complete_code("u20000", &error), error);
  const CandidateView supplementary = session.current_candidates().at(0);
  expect(supplementary.kind == LexemeKind::Character &&
             supplementary.text_utf8 == "𠀀" &&
             supplementary.text_utf8.size() == 4U &&
             is_valid_utf8(supplementary.text_utf8),
         "supplementary-plane Unicode payload was altered");
  session.cancel();

  const RuntimeLexemeRef multi_ref =
      find_ref(resources, fixture::kMultipleVariantForms);
  expect(resources.output_relation_count(multi_ref) == 2U,
         "multiple legal output relations were not preserved");
  std::vector<OutputRelationView> relations;
  expect(resources.output_relations(multi_ref, &relations) &&
             relations.size() == 2U &&
             relations[0].form_id == "zh-Variant" &&
             relations[0].text_utf8 == "發" &&
             relations[0].condition == "traditional context" &&
             relations[0].selection_basis == "curated relation A" &&
             relations[1].text_utf8 == "髮" &&
             relations[1].condition == "hair sense",
         "multi-form condition or selection basis was lost in the bundle");
  expect(resources.resolve_output(multi_ref, "zh-Variant").status ==
             OutputResolutionStatus::MultipleFormsUnsupported,
         "step 1 silently chose among unresolved multiple forms");
}

void test_a8() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  const auto full = resources.query("full_pinyin", "quanpinduyou");
  const auto xiaohe = resources.query("xiaohe", "quanpinduyou");
  expect(full.size() == 1U && xiaohe.empty(),
         "scheme-independent admission was not preserved");
  RuntimeLexemeInfo info;
  expect(resources.lexeme_info(full.at(0).runtime_ref, &info) &&
             info.stable_key == fixture::kFullPinyinOnly,
         "full-pinyin-only lexeme is absent from the shared bundle");
}

void test_a9() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;
  expect(session.query_complete_code("tongwen", &error), error);
  const auto candidates = session.current_candidates();
  expect(candidates.size() == 2U && candidates[0].text_utf8 == "同文" &&
             candidates[1].text_utf8 == "同文" &&
             candidates[0].stable_key != candidates[1].stable_key,
         "same text was incorrectly merged into one identity");
  const RuntimeLexemeRef ref_a = find_ref(resources, fixture::kSameTextA);
  const RuntimeLexemeRef ref_b = find_ref(resources, fixture::kSameTextB);
  expect(resources.default_payload_index(ref_a) ==
             resources.default_payload_index(ref_b),
         "identical text did not share one physical string payload");
  const CandidateView identity_a =
      candidate_for(candidates, fixture::kSameTextA);
  expect(session.confirm(identity_a.handle) ==
             ConfirmResult::CommittedPersisted,
         "same-text identity A commit failed");
  expect(count(&state, fixture::kSameTextA) == 1U &&
             count(&state, fixture::kSameTextB) == 0U,
         "same-text identities shared learning state");
}

void test_a10() {
  const BuiltResources built = build_fixture();
  const BuildManifest& manifest = built.manifest;
  expect(manifest.approved_lexeme_count == 7U &&
             manifest.pending_mapping_count == 1U,
         "approved and pending registry records were not separated");
  expect(manifest.shared_bundle_bytes == built.shared_bundle.size() &&
             manifest.scheme_mapping_counts.at("full_pinyin") == 7U &&
             manifest.scheme_mapping_counts.at("xiaohe") == 5U,
         "resource report counts do not match built resources");
  expect(manifest.to_json().find("2026-09-08.v1") != std::string::npos &&
             manifest.to_json().find("pending_mapping_count") !=
                 std::string::npos &&
             manifest.to_json().find("fixture/prototype") !=
                 std::string::npos &&
             manifest.to_json().find("fixture/pending") !=
                 std::string::npos &&
             manifest.identity_trace.size() == 7U &&
             manifest.pending_mapping_records.size() == 1U,
         "build manifest does not retain source traceability");

  auto shared_reader = std::make_shared<MemoryReader>(built.shared_bundle);
  std::vector<std::shared_ptr<MemoryReader>> tracked_scheme_readers;
  std::vector<std::shared_ptr<const ResourceReader>> resource_scheme_readers;
  for (const auto& [scheme_id, bytes] : built.scheme_bundles) {
    static_cast<void>(scheme_id);
    auto reader = std::make_shared<MemoryReader>(bytes);
    tracked_scheme_readers.push_back(reader);
    resource_scheme_readers.push_back(reader);
  }
  MountedResourceSet resources;
  std::string mount_error;
  expect(resources.mount(shared_reader, resource_scheme_readers, &mount_error),
         mount_error);
  RuntimeLexemeInfo beyond_fixture;
  expect(!resources.lexeme_info(8U, &beyond_fixture),
         "pending identity entered the runtime shared bundle");
  const std::size_t shared_mount_calls = shared_reader->read_calls();
  const std::size_t shared_mount_bytes = shared_reader->bytes_read();
  std::size_t scheme_mount_calls = 0;
  std::size_t scheme_mount_bytes = 0;
  for (const auto& reader : tracked_scheme_readers) {
    scheme_mount_calls += reader->read_calls();
    scheme_mount_bytes += reader->bytes_read();
  }

  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string query_error;
  expect(session.query_complete_code("yuanxingji", &query_error), query_error);
  const std::size_t query_shared_calls =
      shared_reader->read_calls() - shared_mount_calls;
  const std::size_t query_shared_bytes =
      shared_reader->bytes_read() - shared_mount_bytes;

  ResourceBuildInput bad_utf8 = fixture::make_small_input(false);
  bad_utf8.lexemes.front().default_output_utf8 =
      std::string("\xf0\x28\x8c\x28", 4);
  expect(!ResourceBuilder().build(bad_utf8).ok,
         "builder accepted malformed UTF-8 payload");

  ResourceBuildInput guessed_identity = fixture::make_small_input(false);
  guessed_identity.mappings.push_back(
      {"full_pinyin", "pending", {}, 0});
  expect(!ResourceBuilder().build(guessed_identity).ok,
         "builder guessed a mapping for an unapproved identity");

  std::cout << "{\"event\":\"HOST_RESOURCE_REPORT\","
            << "\"shared_bytes\":" << manifest.shared_bundle_bytes << ','
            << "\"full_pinyin_bytes\":"
            << manifest.scheme_bundle_bytes.at("full_pinyin") << ','
            << "\"xiaohe_bytes\":"
            << manifest.scheme_bundle_bytes.at("xiaohe") << ','
            << "\"string_pool_count\":" << manifest.string_pool_count << ','
            << "\"approved_lexemes\":" << manifest.approved_lexeme_count
            << ",\"pending_records\":" << manifest.pending_mapping_count
            << "}\n";
  std::cout << "{\"event\":\"HOST_RUNTIME_COST_REPORT\","
            << "\"scope\":\"small_fixture_host_only\","
            << "\"shared_mount_read_calls\":" << shared_mount_calls << ','
            << "\"shared_mount_bytes\":" << shared_mount_bytes << ','
            << "\"scheme_mount_read_calls\":" << scheme_mount_calls << ','
            << "\"scheme_mount_bytes\":" << scheme_mount_bytes << ','
            << "\"query_shared_read_calls\":" << query_shared_calls << ','
            << "\"query_shared_bytes\":" << query_shared_bytes << ','
            << "\"visible_candidates\":"
            << session.current_candidates().size() << ','
            << "\"sizeof_candidate_handle\":" << sizeof(CandidateHandle)
            << ",\"sizeof_candidate_view\":" << sizeof(CandidateView)
            << ",\"sizeof_mounted_resource_set_shallow\":"
            << sizeof(MountedResourceSet)
            << ",\"peak_ram_and_stack\":\"not_measured\"}\n";
}

void test_a11() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;
  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateHandle handle = session.current_candidates().at(0).handle;
  expect(session.confirm(handle) == ConfirmResult::CommittedPersisted,
         "first confirmation failed");
  expect(session.confirm(handle) == ConfirmResult::RejectedStale,
         "old confirmation was accepted twice");
  expect(sink.call_count == 1U && count(&state, fixture::kPrototype) == 1U,
         "duplicate confirmation repeated commit or learning");
}

void test_a12() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  persistence.fail_next_writes = 1U;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;
  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateHandle handle = session.current_candidates().at(0).handle;
  expect(session.confirm(handle) == ConfirmResult::CommittedStateDirty,
         "write failure did not leave a dirty committed state");
  expect(sink.call_count == 1U && count(&state, fixture::kPrototype) == 1U &&
             state.dirty_count() == 1U,
         "write failure repeated or lost the in-memory update");
  expect(state.retry_dirty() && state.dirty_count() == 0U,
         "dirty state retry failed");
  expect(sink.call_count == 1U,
         "persistence retry called the text commit sink");

  UserStateStore reloaded(fixture::kRegistryNamespace, persistence);
  expect(count(&reloaded, fixture::kPrototype) == 1U,
         "persisted retry did not reload the same count");
}

void test_a13() {
  const BuiltResources built = build_fixture();
  MountedResourceSet resources;
  mount(&resources, built);
  MemoryPersistence persistence;
  persistence.write(fixture::kRegistryNamespace, fixture::kPrototype, 3U);
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  const SharedBundleId bundle_id = resources.shared_bundle_id();
  expect(session.set_output_form_when_idle("zh-Hant"),
         "idle output-form setup failed");
  expect(session.switch_scheme_when_idle("xiaohe"),
         "idle scheme switch failed");
  expect(session.active_scheme() == "xiaohe" &&
             session.output_form() == "zh-Hant" &&
             resources.shared_bundle_id() == bundle_id &&
             count(&state, fixture::kPrototype) == 3U,
         "idle switch changed shared resource, state, or output config");

  std::string error;
  expect(session.query_complete_code("yrxkji", &error), error);
  expect(!session.switch_scheme_when_idle("full_pinyin"),
         "step 1 allowed a non-empty scheme switch");
  expect(session.active_scheme() == "xiaohe",
         "rejected non-empty switch changed the active scheme");
}

void test_r1_late_old_confirmation_preserves_new_snapshot() {
  const BuiltResources build_a = build_fixture(false);
  const BuiltResources build_b = build_fixture(true);
  MountedResourceSet resources;
  mount(&resources, build_a);
  MemoryPersistence persistence;
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;

  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateHandle old_handle =
      session.current_candidates().at(0).handle;
  mount(&resources, build_b);
  expect(session.query_complete_code("yuanxingji", &error), error);
  const CandidateView new_candidate = session.current_candidates().at(0);
  expect(old_handle.mount_epoch != new_candidate.handle.mount_epoch,
         "fixture did not create an old and a new resource generation");

  expect(session.confirm(old_handle) == ConfirmResult::RejectedStale,
         "late old confirmation was not rejected");
  const auto candidates_after_old_event = session.current_candidates();
  expect(session.snapshot_state() == CandidateSnapshotState::Ready &&
             !session.idle() && candidates_after_old_event.size() == 1U &&
             candidates_after_old_event[0].handle == new_candidate.handle,
         "late old confirmation cleared or changed the current snapshot");
  expect(sink.call_count == 0U,
         "late old confirmation reached the commit sink");

  expect(session.confirm(new_candidate.handle) ==
             ConfirmResult::CommittedPersisted,
         "current candidate could not commit after the late old event");
  expect(sink.call_count == 1U &&
             count(&state, fixture::kPrototype) == 1U,
         "current candidate did not commit and learn exactly once");
}

void test_r2_single_conditional_output_is_undetermined() {
  constexpr RegistryNamespaceId kConditionalNamespace =
      fixture::kRegistryNamespace + 1U;
  constexpr StableLexemeKey kConditionalLexeme{{0x52,0x32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}};

  ResourceBuildInput input;
  input.registry_namespace = kConditionalNamespace;
  input.source_name = "conditional-output-regression";
  input.source_version = "2026-09-08.review-fix";
  input.relation_basis = "single relation with unevaluated context";
  LexemeSourceRecord lexeme;
  lexeme.stable_key = kConditionalLexeme;
  lexeme.status = RegistryStatus::Approved;
  lexeme.kind = LexemeKind::Word;
  lexeme.identity_definition = "conditional fixture lexeme";
  lexeme.provenance = "fixture/conditional-single";
  lexeme.base_weight = 10;
  lexeme.default_output_utf8 = "默认";
  lexeme.output_relations.push_back(
      {"zh-Context", "条件输出", "requires-context",
       "regression fixture condition", true, ConditionKind::WordOrSense});
  input.lexemes.push_back(std::move(lexeme));
  input.mappings.push_back(
      {"full_pinyin", "tiaojian", kConditionalLexeme, 10});

  ResourceBuilder builder;
  BuildResult result = builder.build(input);
  expect(result.ok, "conditional fixture build failed: " + result.error);
  MountedResourceSet resources;
  mount(&resources, result.resources);
  const RuntimeLexemeRef runtime_ref =
      find_ref(resources, kConditionalLexeme);
  const OutputResolution unresolved =
      resources.resolve_output(runtime_ref, "zh-Context");
  expect(unresolved.status ==
             OutputResolutionStatus::ConditionUndetermined &&
             unresolved.text_utf8.empty(),
         "single conditional relation was treated as an exact output");
  expect(resources.resolve_output(runtime_ref, "default").status ==
             OutputResolutionStatus::Exact,
         "conditional relation changed the unconditional default output");

  MemoryPersistence persistence;
  UserStateStore state(kConditionalNamespace, persistence);
  RecordingPresenter presenter;
  RecordingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  expect(session.set_output_form_when_idle("zh-Context"),
         "conditional output form could not be selected while idle");
  std::string error;
  expect(!session.query_complete_code("tiaojian", &error) &&
             error.find("unevaluated condition") != std::string::npos &&
             session.current_candidates().empty() && sink.call_count == 0U,
         "session exposed a candidate whose condition was not evaluated");
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"A1", test_a1},   {"A2", test_a2},   {"A3", test_a3},
      {"A4", test_a4},   {"A5", test_a5},   {"A6", test_a6},
      {"A7", test_a7},   {"A8", test_a8},   {"A9", test_a9},
      {"A10", test_a10}, {"A11", test_a11}, {"A12", test_a12},
      {"A13", test_a13},
      {"R1-late-old-confirmation",
       test_r1_late_old_confirmation_preserves_new_snapshot},
      {"R2-single-conditional-output",
       test_r2_single_conditional_output_is_undetermined},
  };

  std::size_t passed = 0;
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
      return 1;
    }
  }
  std::cout << "{\"event\":\"HOST_TEST_SUMMARY\",\"passed\":" << passed
            << ",\"total\":" << tests.size() << "}\n";
  return passed == tests.size() ? 0 : 1;
}
