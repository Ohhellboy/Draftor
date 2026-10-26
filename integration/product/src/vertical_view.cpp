#include "vertical_view.hpp"

namespace s3app {
namespace v=draftor::vertical;
namespace {
v::Bitmap bitmap(void* font,std::uint32_t cp){
  const auto g=static_cast<const s3host::FontPort*>(font)->glyph(cp);
  return {g.bits,g.width,g.height,g.stride,g.bearing,std::size_t(g.stride)*g.height,g.missing};
}
v::Bitmap small(void*,std::uint32_t cp){
  const s3host::BuiltinSmallFont font;const auto g=font.glyph(cp);
  return {g.bits,g.width,g.height,g.stride,g.bearing,std::size_t(g.stride)*g.height,g.missing};
}
bool inside(v::Rect r,std::uint16_t x,std::uint16_t y){
  return x>=r.x&&y>=r.y&&x<unsigned(r.x)+r.width&&y<unsigned(r.y)+r.height;
}
}
VerticalPaintResult paintVerticalView(const App& app,VerticalPresentation& vertical,
    ime_device::DeviceIme* ime,const s3host::FontPort& font,std::uint32_t now,
    const VerticalPaintPort& port) noexcept {
  VerticalPaintResult result;
  if(!port.rect||!port.ink||&vertical.shared().document!=&app.sharedState().document||
      &vertical.shared().editor!=&app.sharedState().editor){result.status=v::Status::InvalidGeometry;return result;}
  const auto geometry=app.imeGeometry();
  result.ime.layout=v::reservedAbRegions(geometry);
  if(result.ime.layout.status!=v::AbStatus::Ready){result.status=v::Status::InvalidGeometry;return result;}
  if(app.findOpen())result.ime.layout.body={124,22,168,356};
  result.status=vertical.setRegion(result.ime.layout.body);
  if(result.status!=v::Status::Ready)return result;
  // Counts delimit valid entries. No per-frame allocation, clearing or body copy.
  auto& scratch=vertical.paintScratch();
  auto* selected=scratch.selected.data();auto* matches=scratch.matches.data();
  const auto selection=vertical.selectionRectangles(selected,v::Viewport::event_capacity);
  const auto hit=app.findDocument.matchHighlight();
  const auto found=vertical.rangeRectangles(hit.visible?v::ByteRange{hit.begin,hit.end}:v::ByteRange{},
      matches,v::Viewport::event_capacity);
  if(selection.status!=v::Status::Ready||found.status!=v::Status::Ready){result.status=v::Status::CapacityExceeded;return result;}
  struct Context {
    const VerticalPaintPort& port;const v::Rect* selected;const v::Rect* matches;
    std::size_t selected_count,match_count;v::RasterPort raster{};
    CandidateVisualContinuity* candidates=nullptr;
    v::Status status=v::Status::Ready;std::size_t glyphs=0;bool accent=false;
  } context{port,selected,matches,selection.written,found.written,{}};
  context.candidates=&app.candidateVisual;
  v::RasterPort raster;raster.glyph=bitmap;raster.small_glyph=small;
  raster.font_context=const_cast<s3host::FontPort*>(&font);raster.pixel_context=&context;
  raster.clip=vertical.region();
  raster.ink=[](void* opaque,std::uint16_t x,std::uint16_t y){
    const auto& c=*static_cast<Context*>(opaque);bool inverse=false;
    for(std::size_t i=0;i<c.selected_count&&!inverse;++i)inverse=inside(c.selected[i],x,y);
    for(std::size_t i=0;i<c.match_count&&!inverse;++i)inverse=inside(c.matches[i],x,y);
    c.port.ink(c.port.context,x,y,c.accent?VerticalColor::Red:inverse?VerticalColor::White:VerticalColor::Black);
  };
  port.rect(port.context,v::editor_area,VerticalColor::White);
  for(std::size_t i=0;i<selection.written;++i)port.rect(port.context,selected[i],VerticalColor::Black);
  for(std::size_t i=0;i<found.written;++i)port.rect(port.context,matches[i],VerticalColor::Black);
  const auto body=vertical.paint(raster);result.status=body.status;result.glyphs=body.glyphs;
  if(result.status!=v::Status::Ready)return result;
  if(!app.findOpen())port.rect(port.context,vertical.viewport().caretRect(vertical.caret().caret),VerticalColor::Black);
  if(app.findOpen())return result; // The Find rail owns all composition pixels.
  raster.clip=v::editor_area;context.raster=raster;
  const v::AbPort ab{
    [](void* opaque,const v::AbRun& run){
      auto& c=*static_cast<Context*>(opaque);
      c.candidates->capture(run);
      if(c.port.ab_run)c.port.ab_run(c.port.context,run);
      if(c.status!=v::Status::Ready)return;
      c.accent=run.focused;
      const auto painted=v::paintAbRun(run,c.raster);c.status=painted.status;c.glyphs+=painted.glyphs;
    },
    [](void* opaque,v::Rect box,bool red){const auto& c=*static_cast<Context*>(opaque);c.port.rect(c.port.context,box,red?VerticalColor::Red:VerticalColor::Black);},
    &context};
  result.ime=ime_device::verticalIme(ime,app,now,0,ab);
  // Preview paint never changes the real Window receipt above.
  if(context.status==v::Status::Ready)paintPendingCandidateVisual(app.candidateVisual,ab);
  result.glyphs+=context.glyphs;result.status=context.status;
  if(result.ime.layout.status!=v::AbStatus::Ready)result.status=v::Status::InvalidGeometry;
  if(result.status!=v::Status::Ready)result.ime.candidate_frame=false;
  return result;
}
} // namespace s3app
