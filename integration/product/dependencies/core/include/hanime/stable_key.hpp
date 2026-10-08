#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace hanime {

// Value representation only. Allocation/adoption belongs to LexemeRegistry.
// There is deliberately no integer/hash/text-to-identity constructor.
struct StableLexemeKey {
  std::array<std::uint8_t, 32> bytes{};
  bool empty() const { return bytes == std::array<std::uint8_t, 32>{}; }
  bool operator==(const StableLexemeKey& rhs) const { return bytes == rhs.bytes; }
  bool operator!=(const StableLexemeKey& rhs) const { return !(*this == rhs); }
  bool operator<(const StableLexemeKey& rhs) const { return bytes < rhs.bytes; }
};

constexpr std::uint32_t kStableKeyFormat = 1;  // SLK1_256
constexpr const char* kStableKeyFormatName = "SLK1_256";
bool parse_slk1(std::string_view text, StableLexemeKey* output);
std::string format_slk1(const StableLexemeKey& key);

}  // namespace hanime

template <> struct std::hash<hanime::StableLexemeKey> {
  std::size_t operator()(const hanime::StableLexemeKey& key) const noexcept {
    std::size_t value = 2166136261U;
    for (const auto byte : key.bytes) value = (value ^ byte) * 16777619U;
    return value;  // Hash-table accelerator; equality still checks all 32 bytes.
  }
};
