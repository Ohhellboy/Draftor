#pragma once
#include <cstdint>
namespace s3host {
struct GlyphView {
 const uint8_t* bits=nullptr;
 uint16_t width=0,height=0,stride=0,advance=0;
 int16_t bearing=0;
 bool missing=false;
};
// Bitmap borrowed only until the next glyph lookup; external adapters may cache TF reads.
// Built-in fallback remains available when an optional resource fails.
class FontPort {
 public:virtual ~FontPort()=default;
 virtual GlyphView glyph(uint32_t cp)const noexcept=0;
 virtual uint16_t lineHeight()const noexcept=0;
 virtual uint16_t defaultAdvance()const noexcept=0;
 // The raw advance has exactly glyph().advance semantics, including zero.
 // The default may look up a glyph and has the same borrowed-bitmap lifetime;
 // immutable atlas adapters can override it without materializing a bitmap.
 virtual uint16_t glyphAdvance(uint32_t cp)const noexcept{return glyph(cp).advance;}
 static uint16_t advance(const void* font,uint32_t cp) noexcept {
  auto& f=*static_cast<const FontPort*>(font);auto g=f.glyph(cp);return g.advance?g.advance:f.defaultAdvance();
 }
};
class BuiltinSmallFont final:public FontPort {
 public:GlyphView glyph(uint32_t cp)const noexcept override;
 uint16_t lineHeight()const noexcept override{return 14;}
 uint16_t defaultAdvance()const noexcept override{return 7;}
};
class BuiltinLatinFont final:public FontPort {
 public:GlyphView glyph(uint32_t cp)const noexcept override;
 uint16_t lineHeight()const noexcept override{return 34;}
 uint16_t defaultAdvance()const noexcept override{return 14;}
};
}
