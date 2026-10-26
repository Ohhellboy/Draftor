#include "ui16_font.hpp"
#include "utf8.hpp"
#include "ui_text.hpp"
namespace s3app::ui16 {
namespace { unsigned bits(uint8_t v){v=uint8_t(v-((v>>1)&0x55));v=uint8_t((v&0x33)+((v>>2)&0x33));return (v+(v>>4))&15;} }
bool canRender(std::string_view text) noexcept {
 if(!editor::isValidUtf8(text))return false;
 const Font face;
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))
  if(face.glyph(display::scalar(text,at)).missing)return false;
 return true;
}
s3host::GlyphView Font::glyph(uint32_t cp)const noexcept {
 size_t lo=0,hi=pageCount;
 while(lo<hi){const auto mid=(lo+hi)/2;if(pages[mid].page<cp>>8)lo=mid+1;else hi=mid;}
 const auto advance=uint16_t(cp>=32&&cp<127?asciiAdvance[cp-32]:16);
 if(cp==173||cp==167885)return {missingBits,16,16,2,advance,0,true}; // Source ink exceeds a 16 px cell.
 if(lo==pageCount||pages[lo].page!=cp>>8)return {missingBits,16,16,2,advance,0,true};
 const auto& page=pages[lo];const unsigned byte=(cp&255)/8,bit=cp&7;
 if(!(page.mask[byte]&(1U<<bit)))return {missingBits,16,16,2,advance,0,true};
 size_t index=page.first;for(unsigned n=0;n<byte;++n)index+=bits(page.mask[n]);
 index+=bits(uint8_t(page.mask[byte]&((1U<<bit)-1)));
 return {glyphBits+index*32,16,16,2,advance,0,false};
}
}
