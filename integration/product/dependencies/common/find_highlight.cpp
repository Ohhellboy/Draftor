#include "find_highlight.hpp"

#include <algorithm>
#include <limits>

namespace hanime::find::display {

HighlightLayoutResult matchHighlightRects(
    const integration::MatchHighlight match,
    const hanime::renderer::Placement* const placements,
    const std::size_t placement_count,
    const std::uint16_t line_height,
    HighlightRect* const output,
    const std::size_t capacity) noexcept {
  HighlightLayoutResult result;
  if (!match.visible || match.begin >= match.end || placements == nullptr ||
      placement_count == 0U || line_height == 0U) return result;

  HighlightRect current{};
  bool have_current = false;
  auto emit = [&]() noexcept {
    if (!have_current) return;
    if (output != nullptr && result.rectangles_written < capacity) {
      output[result.rectangles_written++] = current;
    }
    ++result.rectangles_required;
    have_current = false;
  };

  for (std::size_t i = 0; i < placement_count; ++i) {
    const auto& placement = placements[i];
    if (placement.byte_offset < match.begin ||
        placement.byte_offset >= match.end || placement.advance == 0U) {
      emit();
      continue;
    }
    const std::uint32_t right =
        static_cast<std::uint32_t>(placement.x) + placement.advance;
    const auto bounded_right = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(right,
            std::numeric_limits<std::uint16_t>::max()));
    if (have_current && current.y == placement.y &&
        static_cast<std::uint32_t>(current.x) + current.width == placement.x) {
      current.width = static_cast<std::uint16_t>(bounded_right - current.x);
      continue;
    }
    emit();
    current = {placement.x, placement.y, placement.advance, line_height};
    have_current = true;
  }
  emit();
  result.output_truncated =
      result.rectangles_written < result.rectangles_required;
  return result;
}

bool MatchHighlightDamageTracker::update(
    const integration::MatchHighlight match,
    const std::uint16_t first_visible_line,
    const std::uint32_t layout_generation) noexcept {
  const bool changed = !initialized_ || match.version != match_version_ ||
      first_visible_line != first_visible_line_ ||
      layout_generation != layout_generation_;
  match_version_ = match.version;
  first_visible_line_ = first_visible_line;
  layout_generation_ = layout_generation;
  initialized_ = true;
  return changed;
}

}  // namespace hanime::find::display
