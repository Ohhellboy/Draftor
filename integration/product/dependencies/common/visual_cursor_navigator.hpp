#pragma once

#include "document.hpp"
#include "layout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace editor::body_edit { class BodyEditor; struct Outcome; }

namespace editor::visual_nav {

enum class VisualDirection : std::uint8_t {
  Up,
  Down,
};

enum class VisualHorizontalDirection : std::uint8_t {
  Left,
  Right,
};

enum class VisualMoveResult : std::uint8_t {
  Moved,
  Clamped,
  AtStartBoundary,
  AtEndBoundary,
  InvalidCursor,
};

struct VisualNavigationView {
  std::size_t cursor_byte = 0;
  std::uint16_t visual_line = 0;
  std::uint16_t cursor_x = 0;
  std::uint16_t preferred_x = 0;
  std::uint16_t first_visible_line = 0;
  std::uint16_t visible_lines = 0;
  hanime::renderer::CaretAffinity affinity =
      hanime::renderer::CaretAffinity::Downstream;
};

// Integration-layer visual navigator.
//
// Document remains geometry-free and continues to own UTF-8 text + byte cursor.
// LayoutEngine remains the single source of soft-wrap geometry. This adapter
// owns only visual navigation state: preferred pixel x and visual viewport.
class VisualCursorNavigator {
 public:
  VisualCursorNavigator(const Document& document,
                        const hanime::renderer::LayoutEngine& layout) noexcept;

  body_edit::Outcome move(VisualDirection direction,
      body_edit::BodyEditor& editor, bool extend_selection) noexcept;
  body_edit::Outcome moveHorizontal(VisualHorizontalDirection direction,
      body_edit::BodyEditor& editor, bool extend_selection) noexcept;

  // Call before rendering. It detects normal Editor changes (Left/Right,
  // insertion/deletion, Home/End, restore with changed size) and resets the
  // preferred visual x when appropriate, then keeps the visual cursor visible.
  VisualNavigationView sync() noexcept;

  // Rebind after a vertical-only LayoutEngine height change. This preserves
  // caret affinity and preferred x, minimally scrolls a newly shrunken
  // viewport, and optionally clamps blank tail space when the viewport grows.
  VisualNavigationView rebindViewportGeometry(
      bool clamp_blank_tail) noexcept;

  // Reconstruct persisted visual state from a UTF-8 byte anchor rather than
  // an obsolete line number. The byte identifies the top visible region;
  // caret_affinity disambiguates the persisted cursor at a soft-wrap boundary.
  // A stale/non-canonical anchor is remapped through the current LayoutEngine.
  // Returns false without changing navigation state when either byte offset is
  // invalid for the current document.
  bool restoreViewportAnchor(
      std::size_t first_visible_byte,
      hanime::renderer::CaretAffinity caret_affinity) noexcept;

  // Explicit invalidation hook for a future restore/reflow path that replaces
  // text while preserving both byte cursor and byte length.
  void invalidatePreferredX() noexcept {
    preferred_valid_ = false;
    affinity_ = hanime::renderer::CaretAffinity::Downstream;
    restored_affinity_pending_ = false;
  }

  // Undo/redo restores the affinity captured with the edit transaction. The
  // next sync consumes it after processing text-revision invalidation.
  void restoreCaretAffinity(
      hanime::renderer::CaretAffinity affinity) noexcept {
    restored_affinity_ = affinity;
    restored_affinity_pending_ = true;
    preferred_valid_ = false;
    cached_caret_ = {};
  }

  bool placeCaretSlot(std::uint16_t slot) noexcept;

  std::uint16_t firstVisibleLine() const noexcept { return first_visible_line_; }
  hanime::renderer::VisualLineAnchor firstVisibleAnchor() const noexcept {
    return first_visible_anchor_;
  }
  hanime::renderer::CaretAffinity caretAffinity() const noexcept {
    return affinity_;
  }
  std::size_t lastTraversalBytes() const noexcept {
    return last_traversal_bytes_;
  }
  std::size_t maximumTraversalBytes() const noexcept {
    return maximum_traversal_bytes_;
  }
  std::uint32_t fullScanFallbackCount() const noexcept {
    return full_scan_fallback_count_;
  }

 private:
  static constexpr std::size_t kAnchorHistoryCapacity = 40;

  hanime::renderer::VisualCaretPosition currentCaret() noexcept;
  hanime::renderer::VisualCaretPosition caretAtByte(std::size_t,
      hanime::renderer::CaretAffinity) noexcept;
  hanime::renderer::VisualCaretPosition caretAtAffinity(
      hanime::renderer::CaretAffinity affinity) noexcept;
  hanime::renderer::VisualLineAnchor anchorForLine(
      std::uint16_t visual_line) noexcept;
  void rememberAnchor(hanime::renderer::VisualLineAnchor anchor) noexcept;
  void prepareAnchorsForTextEdit() noexcept;
  void recordTraversal(std::size_t bytes) noexcept;
  void observeExternalChange(
      const hanime::renderer::VisualCaretPosition& caret) noexcept;
  void ensureVisible(std::uint16_t visual_line) noexcept;
  void rememberState() noexcept;

  const Document& document_;
  const hanime::renderer::LayoutEngine& layout_;
  std::uint16_t preferred_x_ = 0;
  std::uint16_t first_visible_line_ = 0;
  hanime::renderer::VisualLineAnchor first_visible_anchor_{};
  hanime::renderer::VisualCaretPosition cached_caret_{};
  std::array<hanime::renderer::VisualLineAnchor,
             kAnchorHistoryCapacity> anchor_history_{};
  std::size_t anchor_history_next_ = 0;
  std::size_t last_traversal_bytes_ = 0;
  std::size_t maximum_traversal_bytes_ = 0;
  std::uint32_t full_scan_fallback_count_ = 0;
  std::size_t last_cursor_byte_ = 0;
  std::size_t last_text_size_ = 0;
  std::uint32_t last_document_revision_ = 0;
  std::uint32_t last_text_revision_ = 0;
  hanime::renderer::CaretAffinity affinity_ =
      hanime::renderer::CaretAffinity::Downstream;
  hanime::renderer::CaretAffinity restored_affinity_ =
      hanime::renderer::CaretAffinity::Downstream;
  bool preferred_valid_ = false;
  bool initialized_ = false;
  bool restored_affinity_pending_ = false;
};

}  // namespace editor::visual_nav
