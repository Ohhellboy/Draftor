#pragma once
#include "ui_text.hpp"
#include <array>
namespace s3app {
inline constexpr unsigned HorizontalCandidateTextOffset=10;
// One complete glyph, without a number or any selectable presentation state.
template<class Ink> void paintHorizontalCandidatePeek(std::string_view text,
 const s3host::FontPort& font,int x,int y,unsigned width,Ink ink){
 if(text.empty())return;
 const auto g=font.glyph(display::scalar(text,0));
 for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col){
  const int px=x+int(HorizontalCandidateTextOffset)+g.bearing+int(col);
  if(g.bits&&px>=x+int(HorizontalCandidateTextOffset)&&px<x+int(width)&&(g.bits[row*g.stride+col/8]&(0x80U>>(col%8))))ink(px,y+int(row));
 }
}
// Shrink only presentation extents. Candidate text and selection identity remain
// complete. Short items retain their size; the longest share the remaining room.
inline void fitCandidateExtents(std::array<uint16_t,6>& sizes,size_t count,unsigned budget){
 unsigned total=0;for(size_t i=0;i<count;++i)total+=sizes[i];
 while(total>budget){size_t largest=0;for(size_t i=1;i<count;++i)if(sizes[i]>sizes[largest])largest=i;
  --sizes[largest];--total;
 }
}
// Existing 28px candidate face, 7px number, and an explicit five-pixel ellipsis.
// The callback receives pixels only; this presentation has no choice authority.
template<class Ink> void paintHorizontalCandidateText(std::string_view text,
 const s3host::FontPort& font,int x,int y,unsigned width,Ink ink){
 const int available=std::max(0,int(width)-12);
 const bool clipped=display::measure(text,font)>available;
 const auto end=display::fit(text,font,std::max(0,available-(clipped?5:0)));
 int atx=x+int(HorizontalCandidateTextOffset);
 for(size_t at=0;at<end;at=editor::nextUtf8Boundary(text,at)){
  const auto g=font.glyph(display::scalar(text,at));
  for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col){
   const int px=atx+g.bearing+int(col);
   if(g.bits&&px>=x&&px<x+int(width)-2&&(g.bits[row*g.stride+col/8]&(0x80U>>(col%8))))ink(px,y+int(row));
  }
  atx+=g.advance;
 }
 if(clipped)for(int dx:{0,2,4})ink(x+int(width)-7+dx,y+24);
}
}
