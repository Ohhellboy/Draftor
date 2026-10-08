#pragma once
#include "font_port.hpp"
#include "hanime_extension/navigation.hpp"

namespace s3app {
// Native Ysabeau Office Regular 22px; canvas includes all ASCII descenders.
enum class CompositionTypography : std::uint8_t { Normal, Compact5, Compact6 };
struct ImeCompositionFontMetrics {
  // The 2 px pads also contain the caret's one-pixel caps: no second
  // caret margin, slot quantization or hypothetical wider-letter reserve.
  static constexpr unsigned padding = 2;
  static constexpr unsigned minimum_width = 6;
  static constexpr unsigned height = 24;
  static constexpr int horizontal_raw_y = 22;
  static constexpr int horizontal_selected_y = -3;
  static constexpr int horizontal_ink_y = 22;
  static constexpr unsigned horizontal_ink_height = 24;
  static constexpr int horizontal_underline_y = 47;
  static constexpr unsigned vertical_line_gap = 2;
};
// A row longer than six letters uses the same bounded Compact6 viewport.
// Called only with a current navigator view, outside paint. Zero rejects an
// incomplete/malformed view without changing the previously coherent width.
std::uint16_t requiredCompositionWidth(std::string_view,
    hanime_extension::NavigationView) noexcept;
class ImeCompositionFont final : public s3host::FontPort {
  CompositionTypography typography_;
 public:
  explicit ImeCompositionFont(CompositionTypography t=CompositionTypography::Normal) noexcept : typography_(t) {}
  s3host::GlyphView glyph(std::uint32_t cp) const noexcept override;
  std::uint16_t lineHeight() const noexcept override;
  std::uint16_t defaultAdvance() const noexcept override { return 14; }
};
} // namespace s3app
