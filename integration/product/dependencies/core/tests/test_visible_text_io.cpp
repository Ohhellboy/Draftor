// Reuse the existing final-byte publisher fixture and public runtime APIs.
// The test's full source vectors and I/O trace are Host-only observations.
#define main existing_publisher_main_for_visible_text
#include "test_resource_publisher.cpp"
#undef main
#include "hanime/candidate_session.hpp"
#include "session_fixture.hpp"

namespace {
constexpr RuntimeLexemeRef kColdRef = 100;
constexpr RuntimeLexemeRef kLongRef = 220;
constexpr std::size_t kTextCapacity = 4096;
const WorkBudget kOrdinaryBudget{1, 1024, 1024};

struct Access {
  std::size_t offset;
  std::size_t bytes;
};

class TracedBytes final : public ResourceReader {
 public:
  explicit TracedBytes(Bytes content) : bytes(std::move(content)) {}

  std::size_t size() const override { return bytes.size(); }

  bool read(std::size_t offset, void* destination, std::size_t count) const override {
    reads.push_back({offset, count});
    if (fail || !destination || offset > bytes.size() || count > bytes.size() - offset) return false;
    if (short_limit && (offset >= short_limit || count > short_limit - offset)) return false;
    if (count) std::memcpy(destination, bytes.data() + offset, count);
    return true;
  }

  bool touched_block(std::size_t block) const {
    for (const auto& access : reads) {
      if (access.bytes && access.offset / 1024 <= block &&
          (access.offset + access.bytes - 1) / 1024 >= block) return true;
    }
    return false;
  }

  Bytes bytes;
  mutable std::vector<Access> reads;
  bool fail{false};
  std::size_t short_limit{0};
};

void check_budget(const WorkCost& cost, const WorkBudget& budget) {
  check(cost.read_calls <= budget.max_read_calls && cost.read_bytes <= budget.max_read_bytes &&
            cost.work_items <= budget.max_work_items,
        "public output or shared read exceeded WorkBudget");
}

WorkBudget changing_budget(std::size_t tick) {
  // In-progress authentication must freeze its lower-level request size while
  // work and I/O budgets alternate. These are all legal, including one byte.
  switch (tick % 4) {
    case 0: return {1, 1, 1};
    case 1: return {1, 17, 3};
    case 2: return {2, 31, 7};
    default: return {1, 1024, 64};
  }
}

struct VisibleFixture {
  Fixture source;
  ResourceBuildInput input;
  PublishVerifiedOptions options;
  PublishVerifiedResult publication;
  Bytes shared_proof;
  Bytes scheme_proof;

  explicit VisibleFixture(const fs::path& root) : source(root), input(fixture::make_small_input(false)) {
    input.lexemes.erase(std::remove_if(input.lexemes.begin(), input.lexemes.end(), [](const auto& item) {
      return item.status != RegistryStatus::Approved;
    }), input.lexemes.end());
    for (RuntimeLexemeRef ref = 8; ref <= kLongRef; ++ref) {
      LexemeSourceRecord item;
      item.status = RegistryStatus::Approved;
      item.kind = LexemeKind::Word;
      item.stable_key = test::session_key(ref);
      item.identity_definition = "visible I/O test identity " + std::to_string(ref);
      item.provenance = "artificial regression, not a production registry";
      item.base_weight = static_cast<std::int32_t>(ref);
      item.default_output_utf8 = "词条-" + std::to_string(ref);
      if (ref == kLongRef) item.default_output_utf8 = std::string(2051, 'a') + "𨿍结束";
      input.lexemes.push_back(item);
      input.mappings.push_back({"full_pinyin", "visible", item.stable_key, 0});
    }
    auto built = ResourceBuilder().build(input, {4096, 4096});
    check(built.ok, "build visible fixture: " + built.error);
    source.built = std::move(built.resources);
    options = source.options(source.built.shared_bundle, source.built.scheme_bundles.at("full_pinyin"));
    options.text_capacity = kTextCapacity;
    options.validation_budget = {64, 16384, 4096};
    publication = publish_verified_resources(options);
    check(publication.ok, "publish final visible fixture: " + publication.error);
    shared_proof = read(fs::path(options.output_directory) / "shared.hnv");
    scheme_proof = read(fs::path(options.output_directory) / "full_pinyin.hnv");
  }

  const LexemeSourceRecord& expected(RuntimeLexemeRef ref) const { return input.lexemes.at(ref - 1); }

  std::size_t record_offset(RuntimeLexemeRef ref) const {
    return shared_address(source.built.shared_bundle, 0, ref - 1);
  }

  std::size_t page_header_offset(RuntimeLexemeRef ref) const {
    const auto& bytes = source.built.shared_bundle;
    const auto per_page = (u32(bytes, 56) - 16) / 52;
    return static_cast<std::size_t>(u64(bytes, 64)) + ((ref - 1) / per_page) * u32(bytes, 56);
  }
};

bool bootstrap_reader(VerifiedResourceReader& reader) {
  if (!reader.prepare_bootstrap()) return false;
  for (unsigned step = 0; step < 100000; ++step) {
    const auto result = reader.advance_bootstrap(kOrdinaryBudget);
    check_budget(result.cost, kOrdinaryBudget);
    if (result.status == ResourceReadStatus::Ready) return true;
    if (result.status == ResourceReadStatus::Error) return false;
  }
  throw std::runtime_error("visible bootstrap did not terminate");
}

struct SharedRuntime {
  std::shared_ptr<TracedBytes> data;
  std::shared_ptr<TracedBytes> proof;
  std::shared_ptr<VerifiedResourceReader> authenticated;
  PagedSharedResource shared;

  SharedRuntime(const VisibleFixture& fixture, Bytes content, Bytes index, bool slices = true)
      : data(std::make_shared<TracedBytes>(std::move(content))),
        proof(std::make_shared<TracedBytes>(std::move(index))),
        authenticated(std::make_shared<VerifiedResourceReader>(data, proof, fixture.publication.resources[0].spec,
                      VerifiedResourceConfig{3, 1, 1024})) {
    if (!bootstrap_reader(*authenticated)) return;
    std::string error;
    check(shared.open_verified(authenticated, 9, {1, 1024, slices}, &error), "visible fast open: " + error);
  }

  explicit SharedRuntime(const VisibleFixture& fixture, bool slices = true)
      : SharedRuntime(fixture, fixture.source.built.shared_bundle, fixture.shared_proof, slices) {}
};

OutputStep complete_output(OutputReader& reader, bool varying = false) {
  for (unsigned tick = 0; tick < 1000000; ++tick) {
    const auto budget = varying ? changing_budget(tick) : kOrdinaryBudget;
    const auto result = reader.advance(budget);
    check_budget(result.cost, budget);
    if (result.status != OutputStatus::Pending) return result;
    check(result.text.empty(), "partial UTF-8 exposed before output Ready");
  }
  throw std::runtime_error("visible output did not terminate");
}

OutputStep get_output(SharedRuntime& runtime, RuntimeLexemeRef ref, const StableLexemeKey& key,
                      OutputRequest request, std::array<char, kTextCapacity>& destination,
                      bool varying = false) {
  std::array<char, 128> names{};
  OutputReader reader;
  const auto started = reader.begin(runtime.shared, runtime.shared.activation_handle(), ref, key, request,
                                    names.data(), names.size(), destination.data(), destination.size());
  if (started != OutputStatus::Pending) return {started, {}, {}, kNoSharedString};
  return complete_output(reader, varying);
}

void cold_record_cost(const VisibleFixture& fixture) {
  SharedRuntime runtime(fixture);
  check(runtime.shared.ready(), "cold runtime not ready");
  const auto header_block = fixture.page_header_offset(kColdRef) / 1024;
  const auto record_block = fixture.record_offset(kColdRef) / 1024;
  check(header_block != record_block && header_block != 0, "cold fixture does not isolate redundant header block");
  runtime.data->reads.clear();
  runtime.proof->reads.clear();
  const auto memory = runtime.shared.memory_usage();
  const auto authentication_memory = runtime.authenticated->memory_usage();
  std::cout << "fixed_memory shared_owner=" << memory.owner_fixed_bytes
            << " shared_cache=" << memory.cache_payload_allocated_bytes
            << " shared_metadata=" << memory.cache_metadata_allocated_bytes
            << " authenticated_owner=" << authentication_memory.owner_fixed_bytes
            << " authenticated_data=" << authentication_memory.data_payload_bytes
            << " authenticated_proof=" << authentication_memory.index_payload_bytes
            << " output_reader=" << sizeof(OutputReader) << '\n';
  std::array<char, kTextCapacity> destination{};
  const auto result = get_output(runtime, kColdRef, fixture.expected(kColdRef).stable_key, {}, destination);
  check(result.status == OutputStatus::Default && result.text == fixture.expected(kColdRef).default_output_utf8,
        "cold default output or identity differs");
  std::cout << "cold_default data_calls=" << runtime.data->reads.size()
            << " proof_calls=" << runtime.proof->reads.size()
            << " redundant_header_touched=" << runtime.data->touched_block(header_block) << '\n';
  check(!runtime.data->touched_block(header_block),
        "cold fixed record unnecessarily loaded its unrelated page-header block");
  check(runtime.data->touched_block(record_block), "cold record was not actually authenticated");
  check(!runtime.authenticated->snapshot().whole_file_checked, "cold output claimed full-file check");
  // The default descriptor is already checked by LexemeRead; its logical reread
  // must not be confused with proof that a second disk fetch was necessary.
  const auto before = runtime.data->reads.size() + runtime.proof->reads.size();
  auto wrong_key = fixture.expected(kColdRef).stable_key;
  wrong_key.bytes[31] ^= 1;
  const auto wrong = get_output(runtime, kColdRef, wrong_key, {}, destination);
  check(wrong.status == OutputStatus::Stale && wrong.text.empty(), "cached metadata bypassed full-key check");
  std::cout << "cached_wrong_key additional_calls="
            << runtime.data->reads.size() + runtime.proof->reads.size() - before << '\n';
}

void outputs_and_crossings(const VisibleFixture& fixture) {
  RuntimeLexemeRef crossing = 0;
  for (RuntimeLexemeRef ref = 1; ref <= kLongRef; ++ref) {
    if (fixture.record_offset(ref) % 1024 + 52 > 1024) { crossing = ref; break; }
  }
  check(crossing != 0, "fixture lacks cross-authentication-block record");
  SharedRuntime runtime(fixture);
  std::array<char, kTextCapacity> destination{};
  auto result = get_output(runtime, crossing, fixture.expected(crossing).stable_key, {}, destination, true);
  check(result.status == OutputStatus::Default && result.text == fixture.expected(crossing).default_output_utf8,
        "cross-block fixed record differs under varying tiny budgets");
  result = get_output(runtime, kLongRef, fixture.expected(kLongRef).stable_key, {}, destination, true);
  check(result.status == OutputStatus::Default && result.text == fixture.expected(kLongRef).default_output_utf8 &&
            is_valid_utf8(result.text) && result.text.size() > 2048,
        "multi-block UTF-8 payload differs or terminates early");
  result = get_output(runtime, 1, fixture::kPrototype, {"zh-Hant", OutputInvocation::Profile}, destination, true);
  check(result.status == OutputStatus::Exact && result.text == "原型機", "generic named output changed");
  result = get_output(runtime, 1, fixture::kPrototype, {"not-present", OutputInvocation::Profile}, destination, true);
  check(result.status == OutputStatus::FallbackDefault && result.text == "原型机", "true missing-form fallback changed");
  result = get_output(runtime, 7, fixture::kMultipleVariantForms,
                      {"zh-Variant", OutputInvocation::Profile}, destination, true);
  check(result.status == OutputStatus::MultipleRelations && result.text.empty(), "multiple relation restriction bypassed");
  result = get_output(runtime, 4, fixture::kSameTextA, {}, destination);
  check(result.status == OutputStatus::Default && result.text == "同文", "first same-text identity lost");
  result = get_output(runtime, 5, fixture::kSameTextB, {}, destination);
  check(result.status == OutputStatus::Default && result.text == "同文", "second same-text identity lost");
  result = get_output(runtime, 5, fixture::kSameTextA, {}, destination);
  check(result.status == OutputStatus::Stale, "same text merged different full identities");
}

void failures(const VisibleFixture& fixture) {
  for (unsigned variant = 0; variant < 6; ++variant) {
    auto data = fixture.source.built.shared_bundle;
    auto proof = fixture.shared_proof;
    if (variant == 0) data.at(fixture.record_offset(kColdRef)) ^= 1;
    if (variant == 1) data.back() ^= 1; // Last payload belongs to the long final lexeme.
    if (variant == 2) proof.at(0) ^= 1;
    if (variant == 3) data.pop_back();
    SharedRuntime runtime(fixture, std::move(data), std::move(proof));
    if (variant == 4) runtime.data->fail = true;
    if (variant == 5) runtime.data->short_limit = fixture.record_offset(kColdRef);
    if (!runtime.shared.ready()) {
      check(variant == 2 || variant == 3, "unrelated failure during bootstrap");
      check(!runtime.authenticated->healthy(), "failed bootstrap remained healthy");
      continue;
    }
    std::array<char, kTextCapacity> destination{};
    const auto ref = variant == 1 ? kLongRef : kColdRef;
    const auto result = get_output(runtime, ref, fixture.expected(ref).stable_key, {}, destination);
    check(result.status == OutputStatus::ReadError || result.status == OutputStatus::Stale,
          "damaged data/proof or short/failed read yielded usable text");
    check(result.text.empty() && !runtime.authenticated->healthy() && !runtime.shared.ready(),
          "failed authentication remained usable");
  }
}

void unread_header_and_validator(VisibleFixture& fixture) {
  auto damaged = fixture.source.built.shared_bundle;
  const auto header = fixture.page_header_offset(kColdRef);
  damaged.at(header) ^= 1;
  SharedRuntime runtime(fixture, damaged, fixture.shared_proof);
  check(runtime.shared.ready(), "unread page-header damage affected authenticated global bootstrap");
  std::array<char, kTextCapacity> destination{};
  const auto result = get_output(runtime, kColdRef, fixture.expected(kColdRef).stable_key, {}, destination);
  check(result.status == OutputStatus::Default && result.text == fixture.expected(kColdRef).default_output_utf8,
        "unused redundant page header still gates unrelated authenticated fixed record");
  check(!runtime.data->touched_block(header / 1024) && !runtime.authenticated->snapshot().whole_file_checked,
        "unread damaged header was claimed checked");
  ResourceReadContext context{std::make_shared<int>(0), 1};
  std::uint8_t header_byte = 0;
  bool rejected = false;
  for (unsigned tick = 0; tick < 100000; ++tick) {
    const auto step = runtime.authenticated->read_budgeted(header, &header_byte, 1, kOrdinaryBudget, context);
    check_budget(step.cost, kOrdinaryBudget);
    check(step.status != ResourceReadStatus::Ready, "damaged header block became readable without authentication");
    if (step.status == ResourceReadStatus::Error) { rejected = true; break; }
  }
  check(rejected && !runtime.shared.ready(), "later access to damaged header did not fail closed");
  // Diagnostics and publisher still reject the page's semantic structure even
  // after the byte checksum and self-reported bundle are recomputed.
  rehash(damaged, true);
  checksum_self_consistent(damaged);
  check(shared_validator_rejects(damaged), "complete validation skipped bad semantic page header");
  auto options = fixture.source.options(damaged, fixture.source.built.scheme_bundles.at("full_pinyin"));
  options.text_capacity = kTextCapacity;
  fixture.source.rejected(options, "final-byte bad page header");
  SharedRuntime legacy(fixture, [&] {
    auto bytes = fixture.source.built.shared_bundle;
    bytes.at(header) ^= 1;
    return bytes;
  }(), fixture.shared_proof, false);
  const auto old_mode = get_output(legacy, kColdRef, fixture.expected(kColdRef).stable_key, {}, destination);
  check(old_mode.status == OutputStatus::ReadError || old_mode.status == OutputStatus::Stale,
        "slice=false silently changed physical page validation behavior");
}

SharedReadResult finish_record(PagedSharedResource& shared, SharedReadCursor& cursor) {
  for (unsigned tick = 0; tick < 100000; ++tick) {
    const auto result = shared.advance_read(&cursor, kOrdinaryBudget);
    check_budget(result.cost, kOrdinaryBudget);
    if (result.status != SharedReadStatus::InProgress) return result;
  }
  throw std::runtime_error("record did not terminate");
}

void activation_and_cleanup(const VisibleFixture& fixture) {
  SharedRuntime runtime(fixture);
  const auto old_activation = runtime.shared.activation_handle();
  std::array<char, 128> names{};
  std::array<char, kTextCapacity> destination{};
  auto output = std::make_unique<OutputReader>();
  check(output->begin(runtime.shared, old_activation, kColdRef, fixture.expected(kColdRef).stable_key, {},
                      names.data(), names.size(), destination.data(), destination.size()) == OutputStatus::Pending,
        "old output begin");
  output->advance({1, 1, 1}); // Owns a record operation, not just an idle handle.
  auto new_reader = std::make_shared<VerifiedResourceReader>(runtime.data, runtime.proof,
      fixture.publication.resources[0].spec, VerifiedResourceConfig{3, 1, 1024});
  check(bootstrap_reader(*new_reader), "same-package reader bootstrap");
  std::string error;
  check(runtime.shared.open_verified(new_reader, 9, {1, 1024, true}, &error), "same-epoch reactivation");
  check(!runtime.shared.activation_is_current(old_activation) &&
            output->advance(kOrdinaryBudget).status == OutputStatus::Stale,
        "same package/epoch revived old output work");
  SharedReadCursor current;
  check(runtime.shared.begin_read(SharedRecordKind::Lexeme, kColdRef + 1, &current) == SharedReadStatus::InProgress,
        "successor read begin");
  const auto calls = runtime.data->reads.size() + runtime.proof->reads.size();
  output->cancel();
  output.reset();
  check(runtime.data->reads.size() + runtime.proof->reads.size() == calls, "old cleanup performed resource I/O");
  const auto value = finish_record(runtime.shared, current);
  check(value.status == SharedReadStatus::Ready &&
            value.lexeme.identity.stable_key == fixture.expected(kColdRef + 1).stable_key,
        "old output cancel/destructor harmed successor read");

  auto scheme_data = std::make_shared<TracedBytes>(fixture.source.built.scheme_bundles.at("full_pinyin"));
  auto scheme_proof = std::make_shared<TracedBytes>(fixture.scheme_proof);
  auto scheme_reader = std::make_shared<VerifiedResourceReader>(scheme_data, scheme_proof,
      fixture.publication.resources[1].spec, VerifiedResourceConfig{8, 2, 1024});
  check(bootstrap_reader(*scheme_reader), "session scheme bootstrap");
  PagedSchemeIndex scheme;
  check(scheme.open_verified(scheme_reader, runtime.shared, {1, 1024}, &error), "session scheme open");
  test::SessionCounts counts(fixture::kRegistryNamespace);
  test::SessionSink sink;
  CandidateSession session(scheme, runtime.shared, counts, sink, {2, 2, 1, kTextCapacity, 128, 128});
  check(session.begin_text_access() == TextAccessStatus::Pending &&
            session.advance_text_access(kOrdinaryBudget).status == TextAccessStatus::Ready,
        "session certificate");
  check(session.begin_input("yuanxingji", 9, 1, 1, ScoringMode::BaseOnly) == SessionStatus::Pending,
        "session begin");
  bool staged = false;
  for (unsigned tick = 0; tick < 100000; ++tick) {
    const auto step = session.tick(kOrdinaryBudget);
    check_budget(step.cost, kOrdinaryBudget);
    if (step.status == SessionStatus::StagingReady) { staged = true; break; }
    check(step.status == SessionStatus::Pending, "session failed before staging");
  }
  check(staged && session.publish(session.staging()) == SessionStatus::Ready, "session publish");
  const auto selection = session.selection(0);
  check(runtime.shared.open_verified(new_reader, 9, {1, 1024, true}, &error), "snapshot resource reopen");
  check(session.confirm(selection).status == CommitStatus::Stale && sink.calls == 0,
        "stale cached text authorized a new selection");
  SharedReadCursor external;
  runtime.shared.begin_read(SharedRecordKind::Lexeme, 2, &external);
  session.cancel_input(session.input_handle());
  session.tick(kOrdinaryBudget);
  check(finish_record(runtime.shared, external).status == SharedReadStatus::Ready,
        "stale session cleanup cancelled an external successor");
  check(counts.notifications == 0, "cancelled or stale selection produced learning");
}
}  // namespace

int main(int argc, char** argv) {
  try {
    check(argc == 2, "visible text test needs an output directory");
    fs::create_directories(argv[1]);
    const auto root = fs::path(argv[1]) / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    VisibleFixture fixture(root);
    unsigned passed = 0;
    auto run = [&](const char* name, const auto& operation) {
      try {
        operation();
        ++passed;
        std::cout << "PASS " << name << '\n';
      } catch (const std::exception& error) {
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
      }
    };
    run("VT1 cold fixed-record I/O and complete-key gate", [&] { cold_record_cost(fixture); });
    run("VT2 generic outputs, same text identities, cross-block and variable budgets", [&] { outputs_and_crossings(fixture); });
    run("VT3 damaged data/proof, truncation, short reads and read failure", [&] { failures(fixture); });
    run("VT4 unread redundant header and complete-validator rejection", [&] { unread_header_and_validator(fixture); });
    run("VT5 same-epoch activation, owned cleanup and stale selection", [&] { activation_and_cleanup(fixture); });
    std::cout << "visible_text_groups=" << passed << "/5\n";
    return passed == 5 ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "FAIL visible fixture setup: " << error.what() << '\n';
    return 1;
  }
}
