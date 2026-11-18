#pragma once
#include "font_port.hpp"
#include "ui_text.hpp"

namespace s3app {
// Candidate B uses upright CJK and rotated Latin. Measure the drawn axis,
// independently of horizontal advances and the editor's fixed body cells.
template<class Glyph> unsigned verticalCandidateAdvance(const Glyph& glyph,uint32_t cp){
 return cp<128?glyph.width:glyph.height;
}
inline unsigned verticalCandidateHeight(std::string_view text,const s3host::FontPort& font,unsigned labelHeight){
 unsigned height=labelHeight;
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at)){
  const auto cp=display::scalar(text,at);height+=verticalCandidateAdvance(font.glyph(cp),cp);
 }
 return height;
}
}
