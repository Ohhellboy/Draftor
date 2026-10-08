#pragma once

#include "hanime_document_adapter.hpp"
#include "layout.hpp"

#include <cstddef>
#include <cstdint>

namespace hanime::find::display {

struct HighlightRect {
  std::uint16_t x = 0;
  std::uint16_t y = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
};

struct HighlightLayoutResult {
  std::size_t rectangles_written = 0;
  std::size_t rectangles_required = 0;
  bool output_truncated = false;
};

HighlightLayoutResult matchHighlightRects(
    integration::MatchHighlight match,
    const hanime::renderer::Placement* placements,
    std::size_t placement_count,
    std::uint16_t line_height,
    HighlightRect* output,
    std::size_t capacity) noexcept;

class MatchHighlightDamageTracker {
 public:
  bool update(integration::MatchHighlight match,
              std::uint16_t first_visible_line,
              std::uint32_t layout_generation) noexcept;
  void invalidate() noexcept { initialized_ = false; }

 private:
  std::uint32_t match_version_ = 0;
  std::uint32_t layout_generation_ = 0;
  std::uint16_t first_visible_line_ = 0;
  bool initialized_ = false;
};

}  // namespace hanime::find::display
