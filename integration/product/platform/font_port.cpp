#include "font_port.hpp"
#include "latin_font.hpp"
#include "small_font.hpp"
namespace s3host {
GlyphView BuiltinSmallFont::glyph(uint32_t cp)const noexcept{
 unsigned lo=0,hi=sizeof(s3small::glyphs)/sizeof(s3small::glyphs[0]),probes=0;
 while(lo<hi){++probes;unsigned mid=(lo+hi)/2;if(s3small::glyphs[mid].cp<cp)lo=mid+1;else hi=mid;}
 const bool missing=lo==sizeof(s3small::glyphs)/sizeof(s3small::glyphs[0])||s3small::glyphs[lo].cp!=cp;
 const auto& g=s3small::glyphs[missing?'?'-32:lo];s3profile::glyph(probes);
 return {g.bits,g.width,14,2,g.advance,g.bearing,missing};
}

GlyphView BuiltinLatinFont::glyph(uint32_t cp)const noexcept{
 auto g=s3font::get(cp);bool missing=!g;if(!g)g=s3font::get('?');
 return {g->bits,g->width,28,4,g->advance,g->bearing,missing};
}
}
