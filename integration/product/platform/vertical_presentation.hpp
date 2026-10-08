#pragma once
#include "editor_presentation.hpp"
#include "draftor/vertical_raster.hpp"
#include "font_port.hpp"

namespace s3app {
// Fixed geometry scratch inside the PSRAM App on S3. The serialized synchronous
// painter borrows it; it owns no text, IME state or candidate identities.
struct VerticalPaintScratch {
  std::array<draftor::vertical::Rect,draftor::vertical::Viewport::event_capacity> selected{};
  std::array<draftor::vertical::Rect,draftor::vertical::Viewport::event_capacity> matches{};
};
// A bounded presentation over borrowed shared state. Owns no editable state,
// no mode, no candidate data, and no Horizontal navigation coordinates.
class VerticalPresentation final : public editor::presentation::PresentationPort {
 public:
  explicit VerticalPresentation(editor::SharedEditorView shared,
      const s3host::FontPort& font) noexcept;
  editor::SharedEditorView shared() const noexcept { return shared_; }
  draftor::vertical::Stamp stamp() const noexcept;
  std::uint64_t geometryRevision() const noexcept { return geometry_revision_; }
  draftor::vertical::Step service(std::size_t work_budget) noexcept;
  draftor::vertical::Status settle() noexcept;
  draftor::vertical::Status setRegion(draftor::vertical::Rect, bool focus_at_left=false, std::size_t work_budget=0) noexcept;
  const draftor::vertical::Viewport& viewport() const noexcept { return viewport_; }
  bool placeCaretSlot(std::uint16_t) noexcept;
  draftor::vertical::Mapping caret() const noexcept;
  draftor::vertical::Rect region() const noexcept { return config_.body; }
  draftor::vertical::Rectangles rangeRectangles(draftor::vertical::ByteRange,
      draftor::vertical::Rect*,std::size_t) const noexcept;
  draftor::vertical::Rectangles selectionRectangles(draftor::vertical::Rect*,std::size_t) const noexcept;
  draftor::vertical::PaintResult paint(const draftor::vertical::RasterPort&) const noexcept;
  VerticalPaintScratch& paintScratch() noexcept { return paint_scratch_; }
  void followCaret(hanime::renderer::CaretAffinity,bool reset_preferred=true) noexcept override;
  bool captureViewport(editor::presentation::ViewportState&) noexcept override;
  void restoreViewport(const editor::presentation::ViewportState&) noexcept override;
  editor::presentation::SpatialResult spatialTarget(editor::presentation::SpatialDirection,bool,
      editor::presentation::ByteTarget&,editor::presentation::NavigationContext = {}) noexcept override;
  editor::presentation::SpatialResult serviceNavigation(editor::presentation::NavigationContext,
      std::size_t,editor::presentation::ByteTarget&,bool&) noexcept override;
  void cancelNavigation() noexcept override;
  bool navigationPending() const noexcept override { return pending_navigation_.active; }
  static constexpr std::size_t navigation_work_budget=128;
  // One fixed token, no key queue, retained text, target cache or extra scanner.
  struct PendingNavigation {
    std::uint64_t geometry=0,presentation=0,focus=0;
    std::size_t cursor=0;
    std::uint32_t revision=0,text_revision=0,selection=0;
    std::uint16_t preferred_y=0;
    draftor::vertical::Affinity affinity=draftor::vertical::Affinity::Downstream;
    editor::presentation::SpatialDirection direction=editor::presentation::SpatialDirection::Up;
    bool extend=false,preferred_valid=false,active=false,renewed=false;
  };
  void findSurface(bool) noexcept override;
 private:
  draftor::vertical::Input input() const noexcept;
  draftor::vertical::Status refresh() noexcept;
  bool navigationCurrent(editor::presentation::NavigationContext) const noexcept;
  editor::presentation::SpatialResult lookupNavigation(editor::presentation::SpatialDirection,bool,
      editor::presentation::ByteTarget&) noexcept;
  PendingNavigation pending_navigation_{};
  editor::SharedEditorView shared_;
  draftor::vertical::Viewport viewport_{};
  VerticalPaintScratch paint_scratch_{};
  draftor::vertical::Config config_{};
  draftor::vertical::Stamp requested_stamp_{};
  draftor::vertical::Position requested_focus_{};
  std::uint64_t geometry_revision_=1;
  std::uint32_t preferred_revision_=0,preferred_selection_=0;
  draftor::vertical::Affinity preferred_affinity_=draftor::vertical::Affinity::Downstream;
  std::uint16_t preferred_y_=0,full_height_=306;
  bool requested_=false,preferred_valid_=false,find_open_=false;
};
} // namespace s3app
