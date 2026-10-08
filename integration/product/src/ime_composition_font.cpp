#include "ime_composition_font.hpp"
#include "composition_font_data.hpp"
#include "ime_presentation_policy.hpp"
namespace s3app {
std::uint16_t ImeCompositionFont::lineHeight() const noexcept {
 return typography_==CompositionTypography::Compact5?s3composition::compact5_height:
        typography_==CompositionTypography::Compact6?s3composition::compact6_height:s3composition::normal_height;
}
s3host::GlyphView ImeCompositionFont::glyph(std::uint32_t cp) const noexcept {
 const bool missing=cp<32||cp>126;
 const auto* table=typography_==CompositionTypography::Compact5?s3composition::compact5:
                   typography_==CompositionTypography::Compact6?s3composition::compact6:s3composition::normal;
 const auto& g=table[(missing?'?':cp)-32];
 return {g.bits,g.width,lineHeight(),4,g.advance,g.bearing,missing};
}
// Glyph cells in the frozen font contain the entire ink. The left/right
// pads contain both bearings and the caret caps (verified for all ASCII).
// Bound long unresolved rows by the widest actual six-character window, so
// moving the raw caret cannot require a second width observation.
[[gnu::noinline]] static unsigned compositionRowWidth(std::string_view row) noexcept {
 const auto type=row.size()>=6?CompositionTypography::Compact6:
     row.size()==5?CompositionTypography::Compact5:CompositionTypography::Normal;
 const ImeCompositionFont font(type);
 unsigned need=ImeCompositionFontMetrics::minimum_width;
 for(std::size_t first=0;first==0||first+6<=row.size();++first){
  unsigned advance=0,right=0;
  const auto end=std::min(row.size(),first+6);
  for(auto at=first;at<end;++at){const auto glyph=font.glyph(static_cast<unsigned char>(row[at]));
   right=std::max(right,advance+unsigned(std::max(0,int(glyph.bearing)+int(glyph.width))));
   advance+=glyph.advance;
  }
  need=std::max(need,2*ImeCompositionFontMetrics::padding+std::max(advance,right));
  if(row.size()<=6)break;
 }
 return need;
}
[[gnu::noinline]] std::uint16_t requiredCompositionWidth(std::string_view raw,
    hanime_extension::NavigationView view) noexcept {
 if(view.status!=hanime_extension::NavigationStatus::Ready||
     (view.span_count&&!view.spans)||raw.size()>64)return 0;
 std::size_t prior=0;
 for(std::size_t i=0;i<view.span_count;++i){const auto& span=view.spans[i];
  if(span.begin!=prior||span.end<=prior||span.end>raw.size())return 0;
  prior=span.end;
 }
 if(view.span_count&&prior!=raw.size())return 0;
 for(unsigned char c:raw)if(c<32||c>126)return 0;
 // A ready unsegmented input uses the existing raw-row width policy.
 if(!view.span_count)return static_cast<std::uint16_t>(compositionRowWidth(raw));
 unsigned need=ImeCompositionFontMetrics::minimum_width;
 for(std::size_t i=0;i<view.span_count;++i){const auto begin=view.spans[i].begin;
  const auto end=presentationSpanEnd(view.spans,view.span_count,i);
  need=std::max(need,compositionRowWidth(raw.substr(begin,end-begin)));
 }
 return static_cast<std::uint16_t>(need);
}
}
