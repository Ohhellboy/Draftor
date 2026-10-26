#pragma once
#include "font_port.hpp"
#include <array>
namespace s3app {
// Menu-only, locally generated MiSans VF 4.009 wght=500. This does not replace
// the body/IME/Find fonts. Const Flash atlas; no file IO, cache, heap or framebuffer.
// A returned bitmap is borrowed until this instance's next glyph lookup.
class MenuFont : public s3host::FontPort {
 uint8_t px_;
 mutable std::array<uint8_t,112> bits_{};
 public:
 explicit MenuFont(uint8_t px) noexcept: px_(px) {}
 s3host::GlyphView glyph(uint32_t cp) const noexcept override;
 // Read the same atlas metric; unlike glyph(), leave borrowed bits untouched.
 uint16_t glyphAdvance(uint32_t cp) const noexcept override;
 uint16_t lineHeight() const noexcept override { return px_==14?24:28; }
 uint16_t defaultAdvance() const noexcept override { return px_; }
 uint16_t baseline() const noexcept { return px_==14?18:20; }
};
class MenuFont14 final : public MenuFont {public:MenuFont14() noexcept:MenuFont(14){}};
class MenuFont16 final : public MenuFont {public:MenuFont16() noexcept:MenuFont(16){}};
static_assert(sizeof(MenuFont16)<=128,"Menu FontPort must retain bounded scratch only");
}
