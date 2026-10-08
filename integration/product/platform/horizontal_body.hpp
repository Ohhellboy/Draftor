#pragma once
#include "writing_app.hpp"
#include "operation_profile.hpp"
#include "body_caret.hpp"
#include "selection_highlight.hpp"
#include "find_highlight.hpp"

namespace s3app {
// The target body painter, shared with the existing Host framebuffer port.
// Keep the donor placements, clipping, highlights and caret geometry intact.
template<class Rect,class Glyph>
void paintHorizontalBody(App& app,const s3host::FontPort& font,Rect fill,Glyph glyph){
   auto v=app.navigator.sync();auto anchor=app.navigator.firstVisibleAnchor();auto selection=app.sharedState().editor.selection();
   hanime::renderer::Placement placements[80];
   for(unsigned row=0;row<app.layout.visibleTextLineCount()&&anchor.valid;++row){
    hanime::renderer::VisualLineLayoutResult r;
    {s3profile::Scope layoutTiming(s3profile::Layout);
     r=app.layout.layoutVisualLine(app.sharedState().document.text(),anchor,v.first_visible_line,placements,80);}

    editor::body_edit::HighlightRect rectangles[80];
    auto highlights=editor::body_edit::selectionHighlightRects(selection,placements,r.placements_written,app.layout.lineHeightPx(),rectangles,80);
    for(size_t i=0;i<highlights.rectangles_written;++i){auto rect=rectangles[i];fill(8+rect.x,34+rect.y,rect.width,rect.height,true);}
    const auto match=app.findDocument.matchHighlight();
    hanime::find::display::HighlightRect matchRects[80];
    auto matches=hanime::find::display::matchHighlightRects(match,placements,r.placements_written,app.layout.lineHeightPx(),matchRects,80);
    for(size_t i=0;i<matches.rectangles_written;++i){auto rect=matchRects[i];fill(8+rect.x,34+rect.y,rect.width,rect.height,true);}
    size_t pair_tail=r.placements_written;
    for(size_t i=0;i<r.placements_written;++i){auto p=placements[i];int x=p.x+8,y=p.y+34;bool selected=selection.has_selection&&p.byte_offset>=selection.begin&&p.byte_offset<selection.end;
     const bool tail=i==pair_tail;bool head=false;
     // Paint-only correction for two normal 28px cells on this visual row.
     // A longer run is consumed in disjoint pairs; no joining across a wrap,
     // no altered placements, and each scalar keeps its own highlight color.
     if(!tail&&(p.codepoint==0x2026||p.codepoint==0x2014)&&p.advance==28&&i+1<r.placements_written){
      const auto next=placements[i+1];
      if(next.codepoint==p.codepoint&&next.byte_offset==p.byte_offset+3&&next.advance==28&&next.y==p.y&&next.x==p.x+28){head=true;pair_tail=i+1;}
     }
     const bool ink=!(selected||(match.visible&&p.byte_offset>=match.begin&&p.byte_offset<match.end));
     // matched28's three dot clusters occupy x=2..14. Moving only the first
     // glyph to x=13..25 removes the oversized middle gap inside the 56px pair.
     if(p.codepoint!='\n'&&p.codepoint!='\r')glyph(p.codepoint,x+(head&&p.codepoint==0x2026?11:0),y,ink);
     // The pinned ordinary dash has a one-pixel stroke at y=15. Extend that
     // stroke to each cell edge; storage, caret and the vertical glyph stay put.
     if((head||tail)&&p.codepoint==0x2014)fill(x,y+15,28,1,ink);
    }
    if(r.document_end)break;anchor=r.next;
   }
   if(!app.findOpen()&&v.visual_line>=v.first_visible_line&&v.visual_line<v.first_visible_line+app.layout.visibleTextLineCount())
    fill(8+v.cursor_x,34+(v.visual_line-v.first_visible_line)*app.layout.lineHeightPx(),draftor::BODY_CARET_STROKE_PX,font.glyph('A').height,true);

}
}
