#pragma once
#include <cstdint>
namespace draftor::vertical {
enum class PunctuationPolicy : std::uint8_t { Legacy, CommonVertical };
struct PunctuationForm {
  std::uint32_t native = 0;
  std::int16_t x = 0, y = 0;
  bool rotate = false;
  std::uint32_t fallback = 0; // Ordinary display-only alternate before rotation.
};
// Display-only transforms for the pinned matched28 font. Source scalars and
// all byte/selection/Undo boundaries remain unchanged. Native forms take priority.
PunctuationForm punctuationForm(std::uint32_t source,
    PunctuationPolicy policy = PunctuationPolicy::Legacy) noexcept;
}  // namespace draftor::vertical
