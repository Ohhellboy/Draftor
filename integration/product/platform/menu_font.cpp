#include "menu_font.hpp"
// Generated only under TestBuilds/menu-font and copied into the local TEST_ONLY
// sketch. Glyph data is not an Apache-2.0 resource or part of the source patch.
#include "menu_font_data.hpp"
#include <cstring>
namespace s3app {
namespace {
unsigned popcount(uint8_t v) noexcept {
 v=uint8_t(v-((v>>1)&0x55));v=uint8_t((v&0x33)+((v>>2)&0x33));
 return (v+(v>>4))&0xf;
}
const uint8_t* entry(uint32_t cp,const menu_data::Page* pages,size_t count,const uint8_t* records) noexcept {
 if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return nullptr;
 size_t lo=0,hi=count;const auto page=cp>>8;
 while(lo<hi){const size_t mid=(lo+hi)/2;if(pages[mid].page<page)lo=mid+1;else hi=mid;}
 if(lo==count||pages[lo].page!=page)return nullptr;
 const auto& p=pages[lo];const unsigned byte=(cp&255)/8,bit=cp&7;
 if(!(p.mask[byte]&(1U<<bit)))return nullptr;
 size_t n=p.first;for(unsigned i=0;i<byte;++i)n+=popcount(p.mask[i]);
 n+=popcount(uint8_t(p.mask[byte]&((1U<<bit)-1)));
 return records+n*7;
}
}
uint16_t MenuFont::glyphAdvance(uint32_t cp) const noexcept {
 const bool small=defaultAdvance()==14;const auto* e=small?
  entry(cp,menu_data::pages14,menu_data::PageCount14,menu_data::records14):
  entry(cp,menu_data::pages16,menu_data::PageCount16,menu_data::records16);
 if(!e)return defaultAdvance();
 const uint32_t packed=uint32_t(e[3])|(uint32_t(e[4])<<8)|(uint32_t(e[5])<<16)|(uint32_t(e[6])<<24);
 return uint16_t((packed>>15)&31);
}
s3host::GlyphView MenuFont::glyph(uint32_t cp) const noexcept {
 const bool small=defaultAdvance()==14;const auto* e=small?
  entry(cp,menu_data::pages14,menu_data::PageCount14,menu_data::records14):
  entry(cp,menu_data::pages16,menu_data::PageCount16,menu_data::records16);
 const uint16_t pitch=lineHeight();std::memset(bits_.data(),0,bits_.size());
 if(!e){
  // A visible hollow square is one unavailable scalar, not a replacement in
  // document bytes. Missing never denies file operations or invents a glyph.
  const unsigned width=defaultAdvance(),stride=(width+7)/8;
  const unsigned top=baseline()-width,bottom=baseline()-1;
  for(unsigned y=top;y<=bottom;++y)for(unsigned x=1;x+1<width;++x)
   if(y==top||y==bottom||x==1||x+2==width)bits_[y*stride+x/8]|=uint8_t(0x80>>(x%8));
  return {bits_.data(),uint16_t(width),pitch,uint16_t(stride),uint16_t(width),0,true};
 }
 const uint32_t offset=uint32_t(e[0])|(uint32_t(e[1])<<8)|(uint32_t(e[2])<<16);
 const uint32_t packed=uint32_t(e[3])|(uint32_t(e[4])<<8)|(uint32_t(e[5])<<16)|(uint32_t(e[6])<<24);
 const unsigned width=packed&31,height=(packed>>5)&31,top=(packed>>10)&31,advance=(packed>>15)&31;
 const int16_t bearing=int16_t(int((packed>>20)&63)-32);const unsigned stride=(width+7)/8;
 const auto* source=(small?menu_data::bits14:menu_data::bits16)+offset;
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
  const unsigned position=y*width+x;
  if(source[position/8]&(0x80>>(position%8)))bits_[(top+y)*stride+x/8]|=uint8_t(0x80>>(x%8));
 }
 return {bits_.data(),uint16_t(width),pitch,uint16_t(stride),uint16_t(advance),bearing,false};
}
}
