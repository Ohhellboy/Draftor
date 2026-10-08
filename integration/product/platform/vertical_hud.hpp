#pragma once
#include "font_port.hpp"
#include "utf8.hpp"
#include "ui_text.hpp"
#include <string_view>
#include <cstring>
namespace s3app {
// One measured line, with explicit ellipsis for a long filename/status. Full
// diagnostic wording remains in the existing log, never overwrites IME pixels.
struct VerticalHudLine {char text[96]{};unsigned width=0;bool shortened=false;};
inline unsigned hudWidth(std::string_view text,const s3host::FontPort& font)noexcept{
 unsigned width=0;for(std::size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))width+=font.glyph(display::scalar(text,at)).advance;return width;
}
inline void hudAppend(VerticalHudLine& line,std::string_view text,const s3host::FontPort& font,unsigned limit=284)noexcept{
 auto used=std::strlen(line.text);const auto dots=hudWidth("...",font);
 for(std::size_t at=0;at<text.size();){const auto end=editor::nextUtf8Boundary(text,at);const auto advance=font.glyph(display::scalar(text,at)).advance;
  if(used+end-at+4>sizeof(line.text)||line.width+advance+(end<text.size()?dots:0)>limit){
   if(line.width+dots<=limit&&used+4<=sizeof(line.text)){std::memcpy(line.text+used,"...",4);line.width+=dots;}line.shortened=true;return;
  }
  std::memcpy(line.text+used,text.data()+at,end-at);used+=end-at;line.text[used]=0;line.width+=advance;at=end;
 }
}
inline VerticalHudLine verticalTop(std::string_view mode,std::string_view name,const s3host::FontPort& font)noexcept{
 VerticalHudLine line;hudAppend(line,mode,font);hudAppend(line," | ",font);hudAppend(line,name,font);return line;
}
} // namespace s3app
