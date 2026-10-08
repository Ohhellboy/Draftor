#pragma once
#include "font_port.hpp"
#include "ui14_font.hpp"
#include <array>
namespace s3app {
// Fixed UI geometry with pre-rasterized 14px glyphs in read-only Flash.
// The body font argument keeps the existing consumer interface; neither its
// readiness nor Chinese IME initialization is needed to display metadata.
class MetadataFont final:public s3host::FontPort {
 s3host::BuiltinSmallFont latin_;
 mutable std::array<uint8_t,28> bits_{};
 public:
 explicit MetadataFont(const s3host::FontPort&){}
 s3host::GlyphView glyph(uint32_t cp)const noexcept override {
  const auto ascii=latin_.glyph(cp);if(!ascii.missing)return ascii;
  const bool present=ui14::lookup(cp,bits_.data());
  return {bits_.data(),14,14,2,14,0,!present};
 }
 uint16_t lineHeight()const noexcept override{return 20;}
 uint16_t defaultAdvance()const noexcept override{return 14;}
};
} // namespace s3app
