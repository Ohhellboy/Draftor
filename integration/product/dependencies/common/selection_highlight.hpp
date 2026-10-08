#pragma once

#include "body_editor.hpp"
#include "layout.hpp"

#include <cstddef>
#include <cstdint>

namespace editor::body_edit {

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

// Converts already-authoritative LayoutEngine placements into merged visible
// selection rectangles. It performs no display I/O and owns no text copy.
HighlightLayoutResult selectionHighlightRects(
    const SelectionView& selection,
    const hanime::renderer::Placement* placements,
    std::size_t placement_count,
    std::uint16_t line_height,
    HighlightRect* output,
    std::size_t capacity) noexcept;

// Tracks whether the visible body must be repainted. Repainting the body before
// drawing the current rectangles is the explicit rule that clears stale
// highlight pixels after shrink, cancel, or viewport movement.
class SelectionHighlightDamageTracker {
 public:
  bool update(const SelectionView& selection,
              std::uint16_t first_visible_line) noexcept;
  void invalidate() noexcept { initialized_ = false; }

 private:
  std::uint32_t selection_version_ = 0;
  std::uint16_t first_visible_line_ = 0;
  bool initialized_ = false;
};

}  // namespace editor::body_edit
