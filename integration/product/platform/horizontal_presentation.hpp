#pragma once
#include "horizontal_viewport_adapter.hpp"
#include "visual_cursor_navigator.hpp"
#include "font_port.hpp"
#include "reader_display.hpp"

namespace s3app {
// The only active presentation in Phase 2A. It can read, but cannot write, the
// shared body. All logical movements are submitted to BodyEditor.
class HorizontalPresentation final {
 public:
  HorizontalPresentation(const editor::Document& document,
                         const s3host::FontPort& font) noexcept
      : layout({284,306,font.lineHeight(),font.defaultAdvance(),
                font.defaultAdvance(),0,&s3host::FontPort::advance,&font}),
        navigator(document,layout),position(document,navigator) {}
  void setFindOpen(bool open) noexcept {
    const auto height=open?display::bodyHeight:306;
    if(layout.heightPx()!=height){layout.setHeightPx(height);navigator.rebindViewportGeometry(!open);}
  }
  hanime::renderer::LayoutEngine layout;
  editor::visual_nav::VisualCursorNavigator navigator;
  editor::presentation::HorizontalViewportAdapter position;
};
} // namespace s3app
