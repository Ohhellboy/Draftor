#pragma once
#include <hanime/core.hpp>
#include <filesystem>

namespace demo_fixture {
// Artificial, example-only identities. No production Registry is used.
constexpr hanime::RegistryNamespaceId registry = 0x57423836484f5354ULL; // WB86HOST
constexpr hanime::StableLexemeKey one{{0x57,0x42,0x38,0x36,0x48,0x4f,0x53,0x54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}};
constexpr hanime::StableLexemeKey two{{0x57,0x42,0x38,0x36,0x48,0x4f,0x53,0x54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2}};
constexpr hanime::StableLexemeKey three{{0x57,0x42,0x38,0x36,0x48,0x4f,0x53,0x54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3}};
constexpr hanime::StableLexemeKey four{{0x57,0x42,0x38,0x36,0x48,0x4f,0x53,0x54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4}};
constexpr hanime::StableLexemeKey same_a{{0x57,0x42,0x38,0x36,0x48,0x4f,0x53,0x54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5}};
constexpr hanime::StableLexemeKey same_b{{0x57,0x42,0x38,0x36,0x48,0x4f,0x53,0x54,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6}};
void write(const std::filesystem::path& directory);
} // namespace demo_fixture
