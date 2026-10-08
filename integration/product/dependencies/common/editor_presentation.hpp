#pragma once
#include "body_editor.hpp"

namespace editor {
// A borrowed view, never another Document, selection, or history owner.
struct SharedEditorView {
  const Document& document;
  const body_edit::BodyEditor& editor;
};

namespace presentation {
enum class SpatialDirection : std::uint8_t { Up, Down, Left, Right };
struct ByteTarget {
  std::size_t byte = 0;
  hanime::renderer::CaretAffinity affinity = hanime::renderer::CaretAffinity::Downstream;
  bool valid = false;
};
// A cache miss is work to schedule, never evidence of a physical boundary.
enum class SpatialResult : std::uint8_t { Unhandled, Ready, NeedsLayout, Boundary, Invalid, Stale };
struct NavigationContext {
  std::uint64_t presentation_revision=0, focus=0;
};
// Byte anchor exchanged with persistence; geometry and its interpretation are
// owned by the active presentation. This does not introduce layout persistence.
struct ViewportState {
  std::size_t byte_offset = 0;
  hanime::renderer::CaretAffinity affinity =
      hanime::renderer::CaretAffinity::Downstream;
  std::uint32_t layout_revision = 0;
};

// Only follow/capture/restore are needed by the shared Find and storage ports.
// No text mutation, input policy, rendering registry, or allocated module.
class PresentationPort {
 public:
  virtual ~PresentationPort() = default;
  virtual void followCaret(hanime::renderer::CaretAffinity,
                          bool reset_preferred = true) noexcept = 0;
  virtual bool captureViewport(ViewportState&) noexcept = 0;
  virtual void restoreViewport(const ViewportState&) noexcept = 0;
  // Optional geometry consumer. The App still executes every logical move.
  // Unhandled preserves the original H path. Only Ready authorizes a move.
  virtual SpatialResult spatialTarget(SpatialDirection, bool, ByteTarget&,
      NavigationContext = {}) noexcept { return SpatialResult::Unhandled; }
  virtual SpatialResult serviceNavigation(NavigationContext,std::size_t,ByteTarget&,bool&) noexcept { return SpatialResult::Unhandled; }
  virtual void cancelNavigation() noexcept {}
  virtual bool navigationPending() const noexcept { return false; }
  virtual void findSurface(bool) noexcept {}
};
} // namespace presentation
} // namespace editor
