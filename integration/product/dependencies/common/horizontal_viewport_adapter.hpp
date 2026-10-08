#pragma once
#include "editor_presentation.hpp"
#include "visual_cursor_navigator.hpp"

namespace editor::presentation {
// Horizontal anchor interpretation stays here; shared storage only carries a
// byte anchor and affinity. This adapter owns neither document nor editor.
class HorizontalViewportAdapter final : public PresentationPort {
 public:
  HorizontalViewportAdapter(const Document& document,
                            visual_nav::VisualCursorNavigator& nav) noexcept
      : document_(document),navigator(nav) {}
  void followCaret(hanime::renderer::CaretAffinity affinity,
                   bool reset_preferred = true) noexcept override {
    if(reset_preferred){
      navigator.invalidatePreferredX();
      // Downstream is already established by invalidation. Preserve the P23
      // cached caret in that common case; no extra font/TF traversal for Find.
      if(affinity!=hanime::renderer::CaretAffinity::Downstream)
        navigator.restoreCaretAffinity(affinity);
    }
    (void)navigator.sync();
  }
  bool captureViewport(editor::presentation::ViewportState& out) noexcept override {
    const auto view=navigator.sync();
    const auto anchor=navigator.firstVisibleAnchor();
    if(!anchor.valid||anchor.byte_offset>document_.text().size())return false;
    out={anchor.byte_offset,view.affinity,1};
    return true;
  }
  void restoreViewport(const editor::presentation::ViewportState& state) noexcept override {
    if(!navigator.restoreViewportAnchor(state.byte_offset,state.affinity)){
      navigator.invalidatePreferredX();(void)navigator.sync();
    }
  }
 private:
  const Document& document_;
  visual_nav::VisualCursorNavigator& navigator;
};
} // namespace editor::presentation
