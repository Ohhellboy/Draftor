#pragma once
#include "font_port.hpp"
#include <cstddef>
#include <string_view>
namespace s3app::ui16 {
struct Page { uint16_t page,first; uint8_t mask[32]; };
extern const Page pages[];
extern const size_t pageCount;
extern const uint8_t glyphBits[],missingBits[],asciiAdvance[];
// Validates the entire candidate before the renderer can publish its identity.
bool canRender(std::string_view text) noexcept;
class Font final:public s3host::FontPort {
 public:
 s3host::GlyphView glyph(uint32_t)const noexcept override;
 uint16_t lineHeight()const noexcept override{return 22;}
 uint16_t defaultAdvance()const noexcept override{return 16;}
};
}
