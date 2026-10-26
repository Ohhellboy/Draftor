#pragma once
#include <cstdint>

namespace s3app::editor_geometry {
// Editor-only geometry. Keep the accepted Find viewport and IME panels fixed.
inline constexpr int horizontalTitleY=5;
inline constexpr std::uint16_t horizontalBodyTop=20;
inline constexpr std::uint16_t horizontalBodyHeight=340-horizontalBodyTop;
inline constexpr std::uint16_t findBodyTop=34,findBodyHeight=170;
inline constexpr std::uint16_t bodyTop(bool findOpen) noexcept {
 return findOpen?findBodyTop:horizontalBodyTop;
}
inline constexpr std::uint16_t bodyHeight(bool findOpen,bool imeVisible,int compositionY) noexcept {
 return findOpen?findBodyHeight:imeVisible?compositionY-horizontalBodyTop:horizontalBodyHeight;
}
} // namespace s3app::editor_geometry
