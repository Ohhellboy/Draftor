// Reuse the existing publication fixture and the original assertions unchanged.
#define main original_publisher_main
#include "test_resource_publisher.cpp"
#undef main
#include "../src/base_order_paged_build.hpp"
#include "../src/base_order_wire.hpp"
#include "hanime/base_order.hpp"

namespace {
void paged_publication(const fs::path& root) {
  const PublishVerifiedOptions old_options{"shared", {{"full_pinyin", "scheme"}}, "new", 1, 0,
      64, 128, {64, 16384, 4096}, true, true, false};
  check(old_options.base_order && old_options.packed_auth && !old_options.packed_auth_all &&
        old_options.base_order_format == 1, "legacy Host positional options changed meaning");
  Fixture fixture(root);
  auto input = fixture::make_small_input(false);
  // Enough valid aliases for multiple directory levels; these are explicitly
  // synthetic fixture records, never source evidence or language preferences.
  for (unsigned i = 0; i < 2300; ++i) {
    input.mappings.push_back({"full_pinyin", "z" + std::to_string(10000 + i), fixture::kPrototype, 1});
  }
  const auto built = ResourceBuilder().build(input, {96, 96});
  check(built.ok, built.error);
  auto options = fixture.options(built.resources.shared_bundle, built.resources.scheme_bundles.at("full_pinyin"));
  options.base_order = true;
  const auto legacy = publish_verified_resources(options);
  check(legacy.ok, legacy.error);
  const auto legacy_directory = fs::path(options.output_directory);
  const auto original = read(legacy_directory / "full_pinyin_base.hbo");
  options.base_order_format = 2;
  options.output_directory += "_paged";
  const auto paged = publish_verified_resources(options);
  check(paged.ok, paged.error);
  const auto output_directory = fs::path(options.output_directory);
  const auto bytes = read(output_directory / "full_pinyin_base.hbo");
  Memory file(bytes);
  std::string error;
  check(detail::check_paged_base_order(file, original, &error), error);
  check(paged.resources.back().directory_and_tail_equivalent &&
        paged.resources.back().derived_source_sha256 == legacy.resources.back().spec.file_sha256 &&
        paged.resources.back().final_transport_validation.steps != 0, "missing derivation/Reader qualification");

  auto bootstrap = [](std::shared_ptr<Memory> data, std::shared_ptr<Memory> proof,
                      const ResourceReleaseSpec& spec) {
    auto reader = std::make_shared<VerifiedResourceReader>(data, proof, spec, VerifiedResourceConfig{8, 8, 1024});
    if (reader->healthy() && reader->prepare_bootstrap()) {
      for (unsigned i = 0; reader->healthy() && !reader->bootstrap_ready(); ++i) {
        check(i < 100000, "binding bootstrap stalled");
        reader->advance_bootstrap({64, 16384, 4096});
      }
    }
    return reader;
  };
  auto shared_reader = bootstrap(std::make_shared<Memory>(built.resources.shared_bundle),
      std::make_shared<Memory>(read(output_directory / "shared.hnv")), paged.resources[0].spec);
  auto scheme_reader = bootstrap(std::make_shared<Memory>(built.resources.scheme_bundles.at("full_pinyin")),
      std::make_shared<Memory>(read(output_directory / "full_pinyin.hnv")), paged.resources[1].spec);
  PagedSharedResource shared;
  PagedSchemeIndex scheme;
  check(shared.open_verified(shared_reader, 1, {2, 1024, true}, &error), error);
  check(scheme.open_verified(scheme_reader, shared, {1, 1024}, &error), error);
  const auto new_proof = read(output_directory / "full_pinyin_base.hnv");
  const auto old_proof = read(legacy_directory / "full_pinyin_base.hnv");
  for (unsigned mutation = 0; mutation < 11; ++mutation) {
    auto pin = paged.resources.back().spec;
    if (mutation == 0) pin.resource_format = 1;
    if (mutation == 1) pin.resource_format = 3;
    if (mutation == 2) pin.required_shared_sha256[0] ^= 1;
    if (mutation == 3) pin.required_scheme_sha256[0] ^= 1;
    if (mutation == 4) pin.scheme_id[0] ^= 1;
    if (mutation == 5) pin.ranking_rule_version = 2;
    if (mutation == 6) ++pin.registry_namespace;
    if (mutation == 7) ++pin.shared_bundle_id;
    if (mutation == 10) pin = legacy.resources.back().spec;
    auto reader = bootstrap(std::make_shared<Memory>(mutation == 9 ? original : bytes),
                           std::make_shared<Memory>(mutation == 8 ? old_proof : new_proof), pin);
    BaseOrderIndex order(reader, scheme, shared);
    BaseOrderCursor cursor;
    check(!order.ready(), "wrong release/proof/shared/scheme/format binding accepted");
    const auto status = order.begin("z10000", cursor);
    check(status == BaseOrderStatus::Stale || status == BaseOrderStatus::Error ||
          status == BaseOrderStatus::InvalidRequest, "binding failure became absence or query eligibility");
  }
  auto reader = bootstrap(std::make_shared<Memory>(bytes), std::make_shared<Memory>(new_proof), paged.resources.back().spec);
  BaseOrderIndex order(reader, scheme, shared);
  BaseOrderCursor cursor;
  check(order.ready() && order.begin("z10000", cursor) == BaseOrderStatus::Pending, "valid HBO2 mount/query failed");
  for (unsigned step = 0;; ++step) {
    check(step < 100000, "valid HBO2 query stalled");
    const auto result = order.locate(cursor, {1, 17, 3});
    if (result.status == BaseOrderStatus::Ready) break;
    check(result.status == BaseOrderStatus::Pending, "valid HBO2 query lost membership");
  }
  std::cout << "PASS 11 binding rejects and real HBO2 query under bounded budget; no failure becomes NoMatch\n";
  options.output_directory += "_repeat";
  const auto repeated = publish_verified_resources(options);
  check(repeated.ok, repeated.error);
  for (const auto* name : {"full_pinyin_base.hbo", "full_pinyin_base.hnv", "release_pin.hpp", "release_manifest.json"}) {
    check(read(output_directory / name) == read(fs::path(options.output_directory) / name), "nondeterministic publication");
  }
  check(!publish_verified_resources(options).ok, "publisher overwrote existing release");
  std::cout << "PASS deterministic HBO2 publication, legacy equivalence, complete authenticated final Reader, immutable destination\n";

  detail::BaseHeader header;
  check(detail::base_header(bytes.data(), bytes.size(), header), "fixture header");
  check(detail::bo32(bytes.data() + 1024) >= 2, "fixture did not exercise multiple internal levels");
  const auto root_child = detail::bo32(bytes.data() + 1024 + 16 + 4);
  for (unsigned mutation = 0; mutation < 10; ++mutation) {
    auto damaged = bytes;
    if (mutation == 0) damaged[64] = 1;
    if (mutation == 1) detail::put32(damaged.data() + 1024 + 4, 0);
    if (mutation == 2) detail::put32(damaged.data() + 1024 + 16 + 4, 0);
    if (mutation == 3) damaged[1024 + 16 + 16] ^= 1;
    if (mutation == 4) detail::put32(damaged.data() + 1024 + 16 + header.width + 4, root_child);
    if (mutation == 5) damaged[header.rank_offset + 31] ^= 1;
    if (mutation == 6) damaged[header.rank_offset + 32] ^= 1;
    if (mutation == 7) damaged[header.member_offset + 4] ^= 1;
    if (mutation == 8) damaged.pop_back();
    if (mutation == 9) detail::put32(damaged.data() + 1024 + std::size_t(root_child) * 1024, 31);
    Memory candidate(damaged);
    check(!detail::check_paged_base_order(candidate, original, &error) && !error.empty(),
          "malformed tree/tail qualified for pin");
  }
  std::cout << "PASS 10 semantic rejects: header, empty page, cycle, fence, duplicate child, full key, score, member, truncation, level\n";
  for (unsigned mutation = 0; mutation < 4; ++mutation) {
    auto invalid = original;
    if (mutation == 0) detail::put32(invalid.data() + 64 + 4, 1);
    if (mutation == 1) detail::put32(invalid.data() + 64 + 8, 0);
    if (mutation == 2) invalid[64 + 16] = 'z';
    if (mutation == 3) invalid.pop_back();
    bool rejected = false;
    try { detail::build_paged_base_order(invalid, nullptr); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, "malformed HBO1 directory accepted");
  }
  options.output_directory += "_invalid";
  options.base_order_format = 3;
  check(!publish_verified_resources(options).ok && !fs::exists(options.output_directory), "unknown output format accepted");
  options.base_order_format = 2;
  options.base_order = false;
  check(!publish_verified_resources(options).ok && !fs::exists(options.output_directory), "unused layout flag silently ignored");
  std::cout << "PASS 4 HBO1 rejects and 2 invalid publication options; no accepted pin\n";
}
}
int main(int argc, char** argv) {
  try {
    check(argc == 2, "usage: hanime_paged_publication_tests FRESH_DIRECTORY");
    paged_publication(argv[1]);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
