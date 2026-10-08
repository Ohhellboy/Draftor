#include "selection_highlight.hpp"

#include <algorithm>
#include <limits>

namespace editor::body_edit {

HighlightLayoutResult selectionHighlightRects(
    const SelectionView& selection,
    const hanime::renderer::Placement* const placements,
    const std::size_t placement_count,
    const std::uint16_t line_height,
    HighlightRect* const output,
    const std::size_t capacity) noexcept {
  HighlightLayoutResult result;
  if (!selection.has_selection || placements == nullptr ||
      placement_count == 0U || line_height == 0U) {
    return result;
  }

  HighlightRect current{};
  bool have_current = false;
  auto emit = [&]() noexcept {
    if (!have_current) return;
    if (result.rectangles_written < capacity && output != nullptr) {
      output[result.rectangles_written++] = current;
    }
    ++result.rectangles_required;
    have_current = false;
  };

  for (std::size_t i = 0; i < placement_count; ++i) {
    const auto& placement = placements[i];
    if (placement.byte_offset < selection.begin ||
        placement.byte_offset >= selection.end || placement.advance == 0U) {
      emit();
      continue;
    }

    const std::uint32_t right =
        static_cast<std::uint32_t>(placement.x) + placement.advance;
    const std::uint16_t bounded_right = static_cast<std::uint16_t>(
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

bool SelectionHighlightDamageTracker::update(
    const SelectionView& selection,
    const std::uint16_t first_visible_line) noexcept {
  const bool changed = !initialized_ ||
      selection.version != selection_version_ ||
      first_visible_line != first_visible_line_;
  selection_version_ = selection.version;
  first_visible_line_ = first_visible_line;
  initialized_ = true;
  return changed;
}

}  // namespace editor::body_edit
