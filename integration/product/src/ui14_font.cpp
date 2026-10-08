#include "ui14_font.hpp"
#include <cstring>
namespace s3app::ui14 {
namespace {
unsigned countBits(uint8_t value) noexcept {
 value=uint8_t(value-((value>>1)&0x55));
 value=uint8_t((value&0x33)+((value>>2)&0x33));
 return (value+(value>>4))&0x0f;
}
}
bool lookup(uint32_t cp,uint8_t* out) noexcept {
 if(!out)return false;
 const auto missing=[&](){std::memcpy(out,missingBits,28);return false;};
 if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return missing();
 size_t lo=0,hi=PageCount;const auto page=cp>>8;
 while(lo<hi){const auto mid=(lo+hi)/2;if(pages[mid].page<page)lo=mid+1;else hi=mid;}
 if(lo==PageCount||pages[lo].page!=page)return missing();
 const auto& entry=pages[lo];const unsigned byte=(cp&255)/8,bit=cp&7;
 if(!(entry.mask[byte]&(1U<<bit)))return missing();
 size_t index=entry.first;
 for(unsigned n=0;n<byte;++n)index+=countBits(entry.mask[n]);
 index+=countBits(uint8_t(entry.mask[byte]&((1U<<bit)-1)));
 if(index>=GlyphCount)return missing();
 const auto* packed=glyphBits+index*PackedGlyphBytes;
 std::memset(out,0,28);
 for(unsigned y=0;y<14;++y)for(unsigned x=0;x<14;++x){
  const unsigned position=y*14+x;
  if(packed[position/8]&(0x80>>(position%8)))out[y*2+x/8]|=uint8_t(0x80>>(x%8));
 }
 return true;
}
}
