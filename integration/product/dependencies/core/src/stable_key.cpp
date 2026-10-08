#include "hanime/stable_key.hpp"

namespace hanime {
bool parse_slk1(std::string_view text, StableLexemeKey* output) {
  if (!output || text.size() != 69 || text.substr(0, 5) != "SLK1:") return false;
  StableLexemeKey parsed;
  auto digit = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < parsed.bytes.size(); ++i) {
    const int hi = digit(text[5 + 2 * i]), lo = digit(text[6 + 2 * i]);
    if (hi < 0 || lo < 0) return false;
    parsed.bytes[i] = static_cast<std::uint8_t>(hi * 16 + lo);
  }
  *output = parsed;
  return true;
}

std::string format_slk1(const StableLexemeKey& key) {
  constexpr char digits[] = "0123456789abcdef";
  std::string text = "SLK1:";
  text.reserve(69);
  for (const auto byte : key.bytes) {
    text += digits[byte >> 4U];
    text += digits[byte & 15U];
  }
  return text;
}
}  // namespace hanime
