#pragma once
#include "ime_composition_font.hpp"
// Test-only independent oracle. Enumerate lit pixels, account for the frozen
// caret caps inside the existing 2 px pads, and retain all actual row prefixes.
// Never call requiredCompositionWidth, widthForSlots or the product controller.
inline unsigned compositionOracleRow(std::string_view raw){
 const s3app::ImeCompositionFont font(raw.size()>=6?s3app::CompositionTypography::Compact6:raw.size()==5?s3app::CompositionTypography::Compact5:s3app::CompositionTypography::Normal);
 unsigned result=6;
 for(size_t start=0;start==0||start+6<=raw.size();++start){int pen=0,left=0,right=0;
  for(size_t i=start;i<std::min(raw.size(),start+6);++i){const auto g=font.glyph(static_cast<unsigned char>(raw[i]));CHECK(!g.missing);
   for(unsigned y=0;y<g.height;++y)for(unsigned x=0;x<g.width;++x)if(g.bits[y*g.stride+x/8]&(128U>>(x%8))){left=std::min(left,pen+g.bearing+int(x));right=std::max(right,pen+g.bearing+int(x)+1);}pen+=g.advance;
  }CHECK(left>=-2);result=std::max(result,unsigned(std::max(pen,right)+4));if(raw.size()<=6)break;
 }return result;
}
inline unsigned compositionOraclePeak(std::string_view logical_row){unsigned width=6;for(size_t i=1;i<=logical_row.size();++i)width=std::max(width,compositionOracleRow(logical_row.substr(0,i)));return width;}
