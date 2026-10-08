#include "horizontal_raw.hpp"

namespace s3app {
void paintHorizontalRaw(const ime_device::DeviceIme& ime,int composition_y,
    const HorizontalRawPort& port) noexcept {
  if(!port.ink||!port.accent)return;
  const ImeCompositionFont font;
  const auto raw=ime.raw();const auto viewport=ime.editViewport(font);
  const auto target=ime.editTarget();
  for(std::size_t i=viewport.begin;i<viewport.end;++i){
    const int x=8+int(viewport.at(i));const auto glyph=font.glyph(static_cast<unsigned char>(raw[i]));
    for(unsigned row=0;row<glyph.height;++row)for(unsigned col=0;col<glyph.width;++col)
      if(glyph.bits[row*glyph.stride+col/8]&(0x80U>>(col%8)))
        port.ink(port.context,x+glyph.bearing+int(col),composition_y+ImeCompositionFontMetrics::horizontal_raw_y+int(row));
    if(target.valid()&&i>=target.begin&&i<target.end)
      port.accent(port.context,x,composition_y+ImeCompositionFontMetrics::horizontal_underline_y,
          viewport.at(i+1)-viewport.at(i),1);
  }
  if(ime.rawEditing())port.accent(port.context,8+int(viewport.cursor_x)-1,
      composition_y+ImeCompositionFontMetrics::horizontal_ink_y,1,
      ImeCompositionFontMetrics::horizontal_ink_height);
}
}
