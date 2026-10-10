#include "fixture.hpp"
#include <fstream>
#include <stdexcept>

namespace demo_fixture {
using namespace hanime;
void write(const std::filesystem::path& directory) {
  ResourceBuildInput input;
  input.registry_namespace = registry;
  input.source_name = "bgime-host-wubi86-artificial-fixture";
  input.source_version = "0.1";
  input.relation_basis = "manually declared test codes, NOT a real Wubi86 dictionary";
  auto add = [&](StableLexemeKey key, const char* label, const char* text, int weight) {
    LexemeSourceRecord record;
    record.stable_key = key;
    record.status = RegistryStatus::Approved;
    record.kind = LexemeKind::Word;
    record.identity_definition = std::string("artificial Host identity: ") + label;
    record.provenance = std::string("host_wubi86/fixture/") + label;
    record.base_weight = weight;
    record.default_output_utf8 = text;
    input.lexemes.push_back(record);
  };
  add(one, "one", u8"一", 60);
  add(two, "two", u8"二", 50);
  add(three, "three", u8"三", 40);
  add(four, "four", u8"四", 30);
  OutputRelationInput blocked;
  blocked.form_id = "blocked";
  blocked.text_utf8 = u8"不可";
  blocked.admission = OutputAdmission::Blocked;
  input.lexemes.back().output_relations.push_back(blocked);
  add(same_a, "same-text-A", u8"同文", 20);
  add(same_b, "same-text-B", u8"同文", 10);
  input.mappings = {
      {"wubi86", "a", one, 0}, {"wubi86", "ab", two, 0},
      {"wubi86", "abc", three, 0}, {"wubi86", "abcd", four, 10},
      {"wubi86", "abcd", four, 5}, // Two paths, one stable identity.
      {"wubi86", "bbbb", same_a, 0}, {"wubi86", "bbbb", same_b, 0}};
  const auto built = ResourceBuilder().build(input, {256, 256});
  if (!built.ok) throw std::runtime_error("Fixture build failed: " + built.error);
  std::filesystem::create_directories(directory);
  auto put = [&](const char* name, const void* data, std::size_t bytes) {
    std::ofstream file(directory / name, std::ios::binary);
    file.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
    file.close();
    if (!file) throw std::runtime_error(std::string("Cannot write fixture ") + name);
  };
  const auto& shared = built.resources.shared_bundle;
  const auto& scheme = built.resources.scheme_bundles.at("wubi86");
  const auto manifest = built.resources.manifest.to_json();
  put("shared.hns", shared.data(), shared.size());
  put("wubi86.hnm", scheme.data(), scheme.size());
  put("manifest.json", manifest.data(), manifest.size());
}
} // namespace demo_fixture
