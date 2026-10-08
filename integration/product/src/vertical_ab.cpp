#include "draftor/vertical_ab.hpp"

#include <algorithm>
#include "ime_composition_font.hpp"
#include "ime_presentation_policy.hpp"

#include "utf8.hpp"
namespace draftor::vertical {
namespace {
std::size_t scalar(std::string_view text, std::size_t at, std::uint32_t& cp) noexcept {
  const auto c = static_cast<unsigned char>(text[at]);
  const std::size_t n = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
  if (n > text.size() - at || !editor::isValidUtf8(text.substr(at, n))) return 0;
  cp = n == 1 ? c : c & ((1U << (7U - n)) - 1U);
  for (std::size_t i = 1; i < n; ++i)
    cp = (cp << 6U) | (static_cast<unsigned char>(text[at + i]) & 63U);
  return n;
}
}  // namespace
// B and A start at the input edge. The body grid stays fixed to the right.
AbResult reservedAbRegions(const AbGeometry g) noexcept {
  AbResult result;
  if(!g.body_cell||!g.gap||g.composition_width<s3app::ImeCompositionFontMetrics::minimum_width||
      g.candidate_columns<1||g.candidate_columns>3||
      unsigned(g.area.x)+g.area.width>65535U||unsigned(g.area.y)+g.area.height>65535U){
    result.status=AbStatus::InvalidInput;return result;
  }
  const unsigned aw=g.composition_width;
  const unsigned bw=g.candidate_columns*(g.body_cell+g.gap)-g.gap;
  const unsigned sw=g.selected_visible?g.body_cell:0;
  const unsigned reserved=aw+bw+sw+(g.selected_visible?3U:2U)*g.gap;
  if(reserved+g.body_cell>g.area.width||g.area.height<s3app::ImeCompositionFontMetrics::height){
    result.status=AbStatus::CapacityExceeded;return result;
  }
  result.candidates={g.area.x,g.area.y,static_cast<std::uint16_t>(bw),g.area.height};
  result.composition={static_cast<std::uint16_t>(g.area.x+bw+g.gap),g.area.y,
      static_cast<std::uint16_t>(aw),g.area.height};
  result.selected={static_cast<std::uint16_t>(result.composition.x+aw+g.gap),g.area.y,static_cast<std::uint16_t>(sw),g.area.height};
  result.body={static_cast<std::uint16_t>(g.area.x+reserved),g.area.y,
      static_cast<std::uint16_t>(g.area.width-reserved),g.area.height};
  result.separator={static_cast<std::uint16_t>(result.body.x-(g.gap+1U)/2U),g.area.y,1,g.area.height};
  return result;
}

// One row borrows the real navigation span and emits only covered substrings.
// Neither candidate coverage nor this viewport changes the input owner's caret.
[[gnu::noinline]] static void compositionRow(std::string_view raw,std::size_t row_begin,
    std::size_t row_end,std::size_t cursor,const AbCompositionMark& mark,
    std::uint16_t y,bool caret,AbResult& result,const AbPort& port) noexcept {
  constexpr unsigned height=s3app::ImeCompositionFontMetrics::height;
  constexpr unsigned pad=s3app::ImeCompositionFontMetrics::padding;
  const auto box=result.composition;
  const unsigned width_limit=box.width-2*pad;
  const auto target=std::clamp(cursor,row_begin,row_end);
  const auto length=row_end-row_begin;
  const auto typography=length>=6?s3app::CompositionTypography::Compact6:
      length==5?s3app::CompositionTypography::Compact5:s3app::CompositionTypography::Normal;
  const s3app::ImeCompositionFont font(typography);
  const unsigned letter_slots=length>6?6:std::max<std::size_t>(1,length);
  std::size_t begin=row_begin,end=row_end;
  unsigned width=0;
  for(auto at=begin;at<end;++at)width+=font.glyph(static_cast<unsigned char>(raw[at])).advance;
  if(width>width_limit||end-begin>letter_slots){
    begin=target;width=0;
    while(begin>row_begin&&target-begin<letter_slots){
      const auto advance=font.glyph(static_cast<unsigned char>(raw[begin-1])).advance;
      if(width+advance>width_limit)break;
      --begin;width+=advance;
    }
    end=target;
    while(end<row_end&&end-begin<letter_slots){
      const auto advance=font.glyph(static_cast<unsigned char>(raw[end])).advance;
      if(width+advance>width_limit)break;
      ++end;width+=advance;
    }
  }
  unsigned x=box.x+pad,caret_x=x;
  for(auto at=begin;at<std::min(cursor,end);++at)
    caret_x+=font.glyph(static_cast<unsigned char>(raw[at])).advance;
  for(auto at=begin;at<end;){
    const bool covered=mark.candidate&&at>=mark.begin&&at<mark.end;
    auto stop=end;
    if(mark.candidate&&at<mark.begin)stop=std::min(stop,std::size_t(mark.begin));
    else if(covered)stop=std::min(stop,std::size_t(mark.end));
    unsigned advance=0;
    for(auto i=at;i<stop;++i)advance+=font.glyph(static_cast<unsigned char>(raw[i])).advance;
    port.run(port.context,{raw.substr(at,stop-at),
        {std::uint16_t(x),y,std::uint16_t(advance),font.lineHeight()},
        TextFlow::CompositionLatin,AbRole::Composition,0,covered,nullptr,typography});
    ++result.runs;x+=advance;at=stop;
  }
  if(caret){
    port.line(port.context,{std::uint16_t(caret_x),y,1,height},false);
    port.line(port.context,{std::uint16_t(caret_x-1),y,3,1},false);
    port.line(port.context,{std::uint16_t(caret_x-1),std::uint16_t(y+height-1),3,1},false);
  }
}

static AbStatus compositionRuns(std::string_view raw,std::size_t cursor,
    const AbCompositionMark& mark,const hanime_extension::NavigationSpan* spans,
    std::size_t span_count,AbResult& result,const AbPort& port) noexcept {
  if(!port.run||!port.line||cursor>raw.size()||(span_count&&!spans))return AbStatus::InvalidInput;
  if(mark.candidate&&(mark.begin>=mark.end||mark.end>raw.size()))return AbStatus::InvalidInput;
  for(const unsigned char c:raw)if(c<32||c>126)return AbStatus::InvalidInput;
  std::size_t prior=0;
  for(std::size_t i=0;i<span_count;++i){
    if(spans[i].begin!=prior||spans[i].end<=prior||spans[i].end>raw.size())return AbStatus::InvalidInput;
    prior=spans[i].end;
  }
  if(span_count&&prior!=raw.size())return AbStatus::InvalidInput;
  constexpr unsigned height=s3app::ImeCompositionFontMetrics::height;
  constexpr unsigned gap=s3app::ImeCompositionFontMetrics::vertical_line_gap;
  constexpr unsigned pad=s3app::ImeCompositionFontMetrics::padding;
  const auto box=result.composition;
  const unsigned slots=(box.height+gap)/(height+gap);
  if(!slots||box.width<=2*pad)return AbStatus::CapacityExceeded;
  // An analyzed input may legitimately have no segmentation. Paint its raw
  // bytes with the same row/caret mechanism without inventing navigation spans.
  if(!span_count){
    if(!raw.empty())compositionRow(raw,0,raw.size(),mark.candidate?mark.begin:cursor,
        mark,box.y,!mark.candidate,result,port);
    return AbStatus::Ready;
  }
  const std::size_t anchor=mark.candidate?mark.begin:cursor;
  const bool downstream=mark.candidate||mark.affinity==AbCompositionMark::Affinity::Downstream;
  std::size_t anchor_slot=0,first=0;
  bool found_anchor=false;
  for(unsigned pass=0;pass<2;++pass){
    std::size_t slot=0;
    for(std::size_t i=0;i<span_count;++i,++slot){
      const auto row_begin=spans[i].begin;
      const auto row_end=s3app::presentationSpanEnd(spans,span_count,i);
      if(!pass){
        const bool owns=downstream?
            (anchor>=row_begin&&(anchor<row_end||(anchor==raw.size()&&anchor==row_end))):
            ((anchor>row_begin||anchor==0)&&anchor<=row_end);
        if(!found_anchor&&owns){anchor_slot=slot;found_anchor=true;}
        continue;
      }
      if(slot<first||slot-first>=slots)continue;
      compositionRow(raw,row_begin,row_end,anchor,mark,
          std::uint16_t(box.y+(slot-first)*(height+gap)),
          !mark.candidate&&slot==anchor_slot,result,port);
    }
    first=anchor_slot>=slots?anchor_slot-slots+1:0;
  }
  return AbStatus::Ready;
}

AbResult layoutPreparingAb(const std::string_view raw,const std::size_t cursor,
    const AbGeometry g,const AbPort& port) noexcept {
  auto result=reservedAbRegions(g);
  if(result.status!=AbStatus::Ready)return result;
  if(raw.size()>65535U){result.status=AbStatus::InvalidInput;return result;}
  const hanime_extension::NavigationSpan span{0,static_cast<std::uint16_t>(raw.size()),hanime_extension::NavigationKind::Unknown};
  result.status=compositionRuns(raw,cursor,AbCompositionMark{},&span,raw.empty()?0:1,result,port);
  return result;
}

static AbResult layoutAbImpl(const hanime_extension::Snapshot& snapshot,
                             const hanime_extension::NavigationView& navigation,
                             const std::size_t cursor, const AbCandidateView& candidates,
                             const EdgeSide edge, const AbGeometry g, const AbPort& port) noexcept {
  AbResult result;
  result.body = g.area;
  if (navigation.status != hanime_extension::NavigationStatus::Ready) {
    result.status = AbStatus::Stale;
    return result;
  }
  if (!port.run || !port.line || !g.small_advance || !g.small_height || !g.body_cell ||
      cursor > snapshot.raw.size() || candidates.count > 3 ||
      (navigation.span_count && !navigation.spans)) {
    result.status = AbStatus::InvalidInput;
    return result;
  }
  if (g.small_advance != 7 || g.small_height != 14 || g.body_cell != 28 || g.latin_advance != 14) {
    result.status = AbStatus::InvalidInput;
    return result;
  }
  if (candidates.status != AbStatus::Ready) {
    result.status = candidates.status;
    return result;
  }
  for (std::size_t row = 0; row < candidates.count; ++row) {
    if (candidates.rows[row].count > candidates.rows[row].items.size()) {
      result.status = AbStatus::InvalidInput;
      return result;
    }
  }
  // Consume the actual bounded window, including two following columns at
  // sequence start. The active column need not be the middle column.
  const AbCandidateRow* columns[3]{};
  unsigned column_count=0;bool found=false;
  if(candidates.expanded){
    for(std::size_t row=0;row<candidates.count;++row){
      const auto& source=candidates.rows[row];
      if(row&&source.index!=candidates.rows[row-1].index+1){result.status=AbStatus::InvalidInput;return result;}
      if(source.index==candidates.row)found=true;
      if(source.count)columns[column_count++]=&source;
    }
    if(candidates.count&&!found){result.status=AbStatus::InvalidInput;return result;}
  }
  const bool peek = !candidates.expanded && candidates.count && candidates.rows[0].count;
  result = reservedAbRegions(g);
  result.candidate_content_rows = peek ? 1U : column_count;
  if (result.status != AbStatus::Ready) return result;
  const auto ax = result.composition.x;
  unsigned y = g.area.y;
  result.status=compositionRuns(snapshot.raw,cursor,candidates.mark,navigation.spans,navigation.span_count,result,port);
  if(result.status!=AbStatus::Ready)return result;
  if (!candidates.expanded) {
    // A peek is deliberately not a selectable candidate surface.
    if (peek) {
      const auto& item = candidates.rows[0].items[0];
      auto peek_box=result.candidates;peek_box.x=static_cast<std::uint16_t>(ax-g.gap-g.body_cell);peek_box.width=g.body_cell;
      unsigned x = 0, height = g.small_height;
      for (std::size_t at = 0; at < item.text.size();) {
        std::uint32_t cp = 0;
        const auto n = scalar(item.text,at,cp);
        if (!n) { result.status = AbStatus::InvalidInput; return result; }
        const unsigned advance = cp < 128 ? g.small_advance : g.small_height;
        if (x + advance > peek_box.width) { x = 0; height += g.small_height; }
        x += advance; at += n;
      }
      if (height > result.candidates.height) { result.status = AbStatus::CapacityExceeded; return result; }
      auto box = peek_box; box.height = static_cast<std::uint16_t>(height);
      port.run(port.context,{item.text,box,TextFlow::HorizontalSmall,AbRole::Peek,0,false,nullptr});
      ++result.runs;
    }
    return result;
  }

  for (std::size_t row = 0; row < column_count; ++row) {
    const auto& source = *columns[row];
    if (source.count > source.items.size()) {
      result.status = AbStatus::InvalidInput;
      return result;
    }
    const auto x = static_cast<std::uint16_t>(ax - g.gap - g.body_cell - row * (g.body_cell + g.gap));
    y = g.area.y;
    const bool current = source.index == candidates.row;
    for (std::size_t i = 0; i < source.count; ++i) {
      const auto& item = source.items[i];
      const auto text = item.text;
      // Preview candidates reserve the same number header as the current row.
      // Only the current row paints numbers; all candidate text shares a baseline.
      unsigned height = g.small_height;
      for (std::size_t at = 0; at < text.size();) {
        std::uint32_t cp = 0;
        const auto n = scalar(text, at, cp);
        if (!n) {
          result.status = AbStatus::InvalidInput;
          return result;
        }
        height += cp < 128 ? g.latin_advance : g.body_cell;
        at += n;
      }
      const bool clipped=height>g.area.height;
      if(clipped&&source.count==1&&y==g.area.y)height=g.area.height;
      if (y + height > unsigned(g.area.y) + g.area.height) {
        result.status = AbStatus::CapacityExceeded;
        return result;
      }
      port.run(port.context,
               {text,
                {x, static_cast<std::uint16_t>(y), g.body_cell, static_cast<std::uint16_t>(height)},
                TextFlow::VerticalBody,
                current ? AbRole::CurrentCandidate : AbRole::Preview,
                current ? unsigned(i + 1) : 0,
                current && i == candidates.col,
                item.identity});
      ++result.runs;
      if(clipped)port.line(port.context,{x,std::uint16_t(y+height-1),g.body_cell,1},true);
      y += height + g.gap;
    }
  }
  if (edge != EdgeSide::None && column_count) {
    const auto x = edge == EdgeSide::StartRight ? ax - g.gap - 1 : result.candidates.x;
    port.line(port.context, {static_cast<std::uint16_t>(x), g.area.y, 1, g.area.height}, true);
  }
  return result;
}
AbResult layoutAb(const hanime_extension::Snapshot& snapshot,
                  const hanime_extension::NavigationView& navigation, const std::size_t cursor,
                  const AbCandidateView& candidates, const EdgeSide edge,
                  const AbGeometry geometry, const AbPort& port) noexcept {
  if (!port.run || !port.line) {
    AbResult result;
    result.status = AbStatus::InvalidInput;
    return result;
  }
  // Preflight without pixel effects. On failure callers can keep the old frame;
  // no partial A/B view is mistaken for an authorized presented candidate frame.
  const AbPort check{[](void*, const AbRun&) {}, [](void*, Rect, bool) {}, nullptr};
  const auto result = layoutAbImpl(snapshot, navigation, cursor, candidates, edge, geometry, check);
  if (result.status != AbStatus::Ready) return result;
  return layoutAbImpl(snapshot, navigation, cursor, candidates, edge, geometry, port);
}

PaintResult paintAbRun(const AbRun& run, const RasterPort& port) noexcept {
  PaintResult result;
  if (!port.glyph || !port.ink) {
    result.status = Status::InvalidGeometry;
    return result;
  }
  if(run.flow==TextFlow::CompositionLatin){
    const s3app::ImeCompositionFont font(run.typography);unsigned advance=0;
    for(const unsigned char cp:run.text){
      const auto g=font.glyph(cp);
      if(g.missing){result.status=Status::InvalidGeometry;return result;}
      ++result.glyphs;
      for(unsigned y=0;y<g.height;++y)for(unsigned x=0;x<g.width;++x){
        ++result.pixels_tested;
        if(!(g.bits[y*g.stride+x/8]&(0x80U>>(x%8))))continue;
        const unsigned px=run.box.x+advance+g.bearing+x,py=run.box.y+y;
        if(px>=port.clip.x&&px<unsigned(port.clip.x)+port.clip.width&&
            py>=port.clip.y&&py<unsigned(port.clip.y)+port.clip.height&&
            px<unsigned(run.box.x)+run.box.width&&py<unsigned(run.box.y)+run.box.height){
          port.ink(port.pixel_context,static_cast<std::uint16_t>(px),static_cast<std::uint16_t>(py));++result.pixels_inked;
        }
      }
      advance+=g.advance;
    }
    return result;
  }
  unsigned advance = 0, small_y = 0;
  auto paintOne = [&](std::uint32_t cp, bool small, bool rotate, unsigned offset) {
    auto b = small && port.small_glyph ? port.small_glyph(port.font_context, cp) : Bitmap{};
    const bool native_small = b.bits && !b.missing;
    if (!native_small) b = port.glyph(port.font_context, cp);
    if (!b.bits || !b.stride || b.stride < (b.width + 7U) / 8U || b.bytes / b.stride < b.height)
      return false;
    const unsigned scale = small && !native_small ? 2 : 1;
    ++result.glyphs;
    for (unsigned y = 0; y < b.height; y += scale)
      for (unsigned x = 0; x < b.width; x += scale) {
        ++result.pixels_tested;
        bool ink = false;
        for (unsigned dy = 0; dy < scale && y + dy < b.height; ++dy)
          for (unsigned dx = 0; dx < scale && x + dx < b.width; ++dx)
            ink = ink || (b.bits[(y + dy) * b.stride + (x + dx) / 8] & (0x80U >> ((x + dx) % 8)));
        if (!ink) continue;
        const int px = run.box.x + (small    ? int(offset + x / scale)
                                    : rotate ? int(b.height - 1 - y)
                                             : int(x));
        const int py = run.box.y + (small ? int(small_y + y / scale) : int(offset + (rotate ? x : y)));
        if (px >= port.clip.x && py >= port.clip.y && px < int(port.clip.x) + port.clip.width &&
            py < int(port.clip.y) + port.clip.height && px < int(run.box.x) + run.box.width &&
            py < int(run.box.y) + run.box.height) {
          port.ink(port.pixel_context, static_cast<std::uint16_t>(px),
                   static_cast<std::uint16_t>(py));
          ++result.pixels_inked;
        }
      }
    return true;
  };
  if (run.number) {
    if (!paintOne('0' + run.number, true, false, 0)) {
      result.status = Status::InvalidGeometry;
      return result;
    }
    advance = 14;
  } else if (run.role == AbRole::Preview && run.flow == TextFlow::VerticalBody) {
    advance = 14;
  }
  for (std::size_t at = 0; at < run.text.size();) {
    std::uint32_t cp = 0;
    const auto n = scalar(run.text, at, cp);
    const bool small = run.flow == TextFlow::HorizontalSmall;
    const unsigned step = small ? (cp < 128 ? 7 : 14) : (cp < 128 ? 14 : 28);
    if (small && run.role == AbRole::Peek && advance + step > run.box.width) {
      advance = 0; small_y += 14;
    }
    if (!n || (small && small_y + 14 > run.box.height) ||
        !paintOne(cp, small, !small && cp < 128, advance)) {
      result.status = Status::InvalidGeometry;
      return result;
    }
    advance += step;
    at += n;
  }
  return result;
}
}  // namespace draftor::vertical
