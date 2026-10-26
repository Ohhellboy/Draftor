#pragma once
// Host-only counters. Include product interfaces through the current arm's
// include path, so the same source can be linked with the old or new ABI.
#include "menu_font.hpp"
#include <type_traits>
#include <utility>
namespace menu_probe {
template<class Font,class=void>struct HasGlyphAdvance:std::false_type{};
template<class Font>struct HasGlyphAdvance<Font,std::void_t<decltype(std::declval<const Font&>().glyphAdvance(uint32_t{}))>>:std::true_type{};
template<class Font>
uint16_t rawAdvance(const Font& font,uint32_t cp)noexcept{
 if constexpr(HasGlyphAdvance<Font>::value)return font.glyphAdvance(cp);
 else return font.glyph(cp).advance;
}
template<class Base,bool HasMetric>struct CountedMenuFontImpl;
template<class Base>struct CountedMenuFontImpl<Base,false>:Base {
 s3app::MenuFont16 face;mutable unsigned lookups=0,metrics=0;
 s3host::GlyphView glyph(uint32_t cp)const noexcept override{++lookups;return face.glyph(cp);}
 uint16_t lineHeight()const noexcept override{return face.lineHeight();}
 uint16_t defaultAdvance()const noexcept override{return face.defaultAdvance();}
};
// Only instantiate the override when this arm actually has the virtual slot.
template<class Base>struct CountedMenuFontImpl<Base,true> final:CountedMenuFontImpl<Base,false> {
 uint16_t glyphAdvance(uint32_t cp)const noexcept override{++this->metrics;return rawAdvance(this->face,cp);}
};
using CountedMenuFont=CountedMenuFontImpl<s3host::FontPort,HasGlyphAdvance<s3host::FontPort>::value>;
}

