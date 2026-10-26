#include "menu_font.hpp"
#include "menu_font_data.hpp"
#include "support/menu_measure_probe.hpp"
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
namespace {
uint32_t hash(const uint8_t* p,size_t n){uint32_t h=2166136261U;for(size_t i=0;i<n;++i){h^=p[i];h*=16777619U;}return h;}
void metric(const s3host::FontPort& font,uint32_t cp,const s3host::GlyphView& glyph){
 const auto size=size_t(glyph.height)*glyph.stride;assert(size<=112);
 std::array<uint8_t,112> saved{};std::copy_n(glyph.bits,size,saved.begin());
 assert(menu_probe::rawAdvance(font,cp)==glyph.advance);
 if constexpr(menu_probe::HasGlyphAdvance<s3host::FontPort>::value){
  // MenuFont's width-only path must preserve the previously borrowed bitmap.
  (void)menu_probe::rawAdvance(font,cp=='A'?'W':'A');
  assert(std::equal(saved.begin(),saved.begin()+size,glyph.bits));
 }
}
void check(const s3host::FontPort& f,unsigned px,const s3app::menu_data::Page* pages,size_t count,size_t expected){
 size_t covered=0;unsigned maxWidth=0;
 for(size_t page=0;page<count;++page)for(unsigned low=0;low<256;++low){
  if(!(pages[page].mask[low/8]&(1U<<(low%8))))continue;
  uint32_t cp=(uint32_t(pages[page].page)<<8)|low;auto g=f.glyph(cp);
  assert(!g.missing&&g.advance&&g.height==f.lineHeight()&&g.width<=26&&g.stride<=4);
  assert(g.stride==(g.width+7)/8);assert(g.bits);
  metric(f,cp,g);
  const uint32_t actual=hash(g.bits,size_t(g.height)*g.stride);
  std::printf("%u,%u,%u,%u,%u,%u,%d,%u\n",px,cp,g.width,g.height,g.stride,g.advance,int(g.bearing),actual);
  if(g.width>maxWidth)maxWidth=g.width;++covered;
 }
 assert(covered==expected);
 for(uint32_t cp:{0x28fcdU,0x1f600U,0x200dU,0xd800U,0x110000U,std::numeric_limits<uint32_t>::max()}){
  auto g=f.glyph(cp);assert(g.missing&&g.advance==px&&g.width==px&&g.height==f.lineHeight()&&g.bits);
  metric(f,cp,g);
  bool any=false;for(unsigned i=0;i<g.stride*g.height;++i)any|=g.bits[i]!=0;assert(any);
 }
 auto a=f.glyph('A');assert(!a.missing&&a.advance);auto space=f.glyph(' ');assert(!space.missing&&space.advance&&space.width==0);
 metric(f,' ',space);
}
}
int main(){s3app::MenuFont14 a;s3app::MenuFont16 b;check(a,14,s3app::menu_data::pages14,s3app::menu_data::PageCount14,s3app::menu_data::GlyphCount14);check(b,16,s3app::menu_data::pages16,s3app::menu_data::PageCount16,s3app::menu_data::GlyphCount16);}
