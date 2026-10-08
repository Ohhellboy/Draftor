// Actual body renderers and current fonts; the R1 donor below is retained only
// as a Host comparison oracle. It is never linked into device product code.
#define ALPHA_PUNCTUATION_REUSE_RENDER
#include "alpha_punctuation_render.cpp"

namespace alpha_r2_render {
using alpha_render::Frame;
// Exact R1 horizontal_body.hpp painter, before the R2 pair-only paint change.
template<class Rect,class Glyph>
void paintHorizontalBodyR1(s3app::App& app,const s3host::FontPort& font,Rect fill,Glyph glyph){
 auto v=app.navigator.sync();auto anchor=app.navigator.firstVisibleAnchor();auto selection=app.sharedState().editor.selection();
 hanime::renderer::Placement placements[80];
 for(unsigned row=0;row<app.layout.visibleTextLineCount()&&anchor.valid;++row){
  hanime::renderer::VisualLineLayoutResult r;
  {s3profile::Scope layoutTiming(s3profile::Layout);r=app.layout.layoutVisualLine(app.sharedState().document.text(),anchor,v.first_visible_line,placements,80);}
  editor::body_edit::HighlightRect rectangles[80];
  auto highlights=editor::body_edit::selectionHighlightRects(selection,placements,r.placements_written,app.layout.lineHeightPx(),rectangles,80);
  for(size_t i=0;i<highlights.rectangles_written;++i){auto rect=rectangles[i];fill(8+rect.x,34+rect.y,rect.width,rect.height,true);}
  const auto match=app.findDocument.matchHighlight();hanime::find::display::HighlightRect matchRects[80];
  auto matches=hanime::find::display::matchHighlightRects(match,placements,r.placements_written,app.layout.lineHeightPx(),matchRects,80);
  for(size_t i=0;i<matches.rectangles_written;++i){auto rect=matchRects[i];fill(8+rect.x,34+rect.y,rect.width,rect.height,true);}
  for(size_t i=0;i<r.placements_written;++i){auto p=placements[i];int x=p.x+8,y=p.y+34;bool selected=selection.has_selection&&p.byte_offset>=selection.begin&&p.byte_offset<selection.end;
   if(p.codepoint!='\n'&&p.codepoint!='\r')glyph(p.codepoint,x,y,(selected||(match.visible&&p.byte_offset>=match.begin&&p.byte_offset<match.end))?false:true);
  }
  if(r.document_end)break;anchor=r.next;
 }
 if(!app.findOpen()&&v.visual_line>=v.first_visible_line&&v.visual_line<v.first_visible_line+app.layout.visibleTextLineCount())fill(8+v.cursor_x,34+(v.visual_line-v.first_visible_line)*app.layout.lineHeightPx(),draftor::BODY_CARET_STROKE_PX,font.glyph('A').height,true);
}
Frame paint(SwitchRig& r,bool r1=false){
 CHECK(r.app.horizontalActive());r.draw();
 if(!r1)alpha_render::horizontal(r);
 else paintHorizontalBodyR1(r.app,r.font,
  [&](int x,int y,int w,int h,bool ink){alpha_render::rect(r.pixels,x,y,w,h,ink?1:0);},
  [&](std::uint32_t cp,int x,int y,bool ink){const auto g=r.font.glyph(cp);CHECK(!g.missing);for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)if(g.bits[row*g.stride+col/8]&(0x80>>(col%8))){const int px=x+g.bearing+int(col),py=y+int(row);CHECK(px>=0&&px<300&&py>=0&&py<400);r.pixels[py*300+px]=ink?1:0;}});
 return r.pixels;
}
std::uint8_t pixel(const Frame& frame,unsigned x,unsigned y){return frame[(34+y)*300+8+x];}
unsigned ink(const Frame& frame,unsigned first,unsigned last,unsigned y){unsigned count=0;for(unsigned x=first;x<last;++x)count+=pixel(frame,x,y)==1;return count;}
unsigned maxInternalGap(const Frame& frame,unsigned first,unsigned last,unsigned y){
 bool started=false;unsigned gap=0,maximum=0;for(unsigned x=first;x<last;++x){if(pixel(frame,x,y)==1){if(started)maximum=std::max(maximum,gap);started=true;gap=0;}else if(started)++gap;}return maximum;
}
unsigned groups(const Frame& frame,unsigned first,unsigned last,unsigned y){bool prior=false;unsigned result=0;for(unsigned x=first;x<last;++x){const bool current=pixel(frame,x,y)==1;if(current&&!prior)++result;prior=current;}return result;}
void pairMetrics(const fs::path& folder){
 for(const auto cp:{0x2026U,0x2014U}){
  const auto scalar=alpha_render::utf8(cp);SwitchRig r;r.body(scalar+scalar+"甲");r.move(6);
  const BodyFact body(r.app);const auto before=paint(r,true),after=paint(r);body.check(r.app);CHECK(before!=after);
  alpha_render::ppm(folder,cp==0x2026?"r1_h_ellipsis_pair":"r1_h_dash_pair",before);alpha_render::ppm(folder,cp==0x2026?"r2_h_ellipsis_pair":"r2_h_dash_pair",after);
  CHECK(r.font.glyph(cp).advance==28);
  for(unsigned i=0;i<3;++i){r.move(i*3);CHECK(r.app.navigator.sync().cursor_x==i*28&&r.app.document.cursorByte()==i*3);}
  if(cp==0x2026){CHECK(ink(before,0,56,19)==14&&ink(after,0,56,19)==14&&ink(after,0,56,20)==14);CHECK(groups(after,0,56,19)==6&&maxInternalGap(before,0,56,19)==15&&maxInternalGap(after,0,56,19)==4);}
  else {CHECK(ink(before,0,56,15)==40&&maxInternalGap(before,0,56,15)==8&&ink(after,0,56,15)==56);}
  // Four consecutive scalars are two disjoint pairs. The fifth scalar is
  // deliberately left alone; its source pixels must remain the R1 pixels.
  SwitchRig run;run.body(scalar+scalar+scalar+scalar+scalar+"甲");run.move(15);const auto runR2=paint(run),runR1=paint(run,true);
  for(unsigned y=0;y<28;++y)for(unsigned x=0;x<56;++x){CHECK(pixel(runR2,x,y)==pixel(after,x,y));CHECK(pixel(runR2,x+56,y)==pixel(after,x,y));}
  for(unsigned y=0;y<28;++y)for(unsigned x=112;x<140;++x)CHECK(pixel(runR2,x,y)==pixel(runR1,x,y));
  SwitchRig single;single.body(scalar+"甲");single.move(3);CHECK(paint(single)==paint(single,true));
 }
 std::cout<<"PASS R2_H_pairs geometry=28_each_56_pair ellipsis_ink=28 six_groups=1 gap_R1_15_R2_4 dash_ink_R1_40_R2_56 caret_bytes_0_3_6_px_0_28_56 four_run_disjoint_pairs=1 odd_tail_unchanged=1\n";
}
void wrappedAndSeparate(){
 SwitchRig probe;std::string probeText;for(unsigned i=0;i<32;++i)probeText+="甲";
 std::size_t firstWrap=0;for(std::size_t byte=3;byte<probeText.size();byte+=3){const auto c=probe.app.layout.visualCaretAt(probeText,byte);if(c.line){firstWrap=byte;break;}}CHECK(firstWrap>=6);
 std::string prefix;for(std::size_t i=0;i<firstWrap/3-1;++i)prefix+="甲";
 for(const auto cp:{0x2026U,0x2014U}){
  const auto scalar=alpha_render::utf8(cp);SwitchRig r;r.body(prefix+scalar+scalar+"甲");r.move(0);
  const auto before=r.app.layout.visualCaretAt(r.app.document.text(),prefix.size()),next=r.app.layout.visualCaretAt(r.app.document.text(),prefix.size()+3);CHECK(next.line==before.line+1);
  CHECK(paint(r)==paint(r,true));
  for(const auto separator:{"甲"," ","\n"}){SwitchRig separate;separate.body(scalar+separator+scalar+"甲");separate.move(0);CHECK(paint(separate)==paint(separate,true));}
 }
 std::cout<<"PASS R2_H_wrap_and_nonadjacent_unchanged no_cross_row_join no_geometry_change\n";
}
void highlightsAndDeletion(){
 for(const auto cp:{0x2026U,0x2014U}){
  const auto scalar=alpha_render::utf8(cp);SwitchRig r;r.body(scalar+scalar+"甲");r.move(6);const auto normal=paint(r);
  for(unsigned selected=0;selected<2;++selected){r.move(selected*3);r.move(selected*3+3,true);const auto highlighted=paint(r);const unsigned y=cp==0x2026?19:15;
   // The product draws its two-pixel caret after highlights and glyphs.
   // Account for that real overlay while checking every pixel in both cells.
   const unsigned caret=r.app.navigator.sync().cursor_x;
   for(unsigned cell=0;cell<2;++cell)for(unsigned x=0;x<28;++x){const auto at=cell*28+x;const auto n=pixel(normal,at,y);const auto expected=at>=caret&&at<caret+draftor::BODY_CARET_STROKE_PX?1:(cell==selected?1-n:n);CHECK(pixel(highlighted,at,y)==expected);}
  }
  r.move(3);r.tap(42);const auto deleted=paint(r);CHECK(r.app.document.text()==(scalar+"甲").c_str()&&r.app.document.cursorByte()==0);
  SwitchRig fresh;fresh.body(scalar+"甲");fresh.move(0);CHECK(deleted==paint(fresh));
  CHECK(r.app.commitText(scalar,r.app.editStamp()));const auto restored=paint(r);SwitchRig restoredFresh;restoredFresh.body(scalar+scalar+"甲");restoredFresh.move(3);CHECK(restored==paint(restoredFresh));
  SwitchRig find;find.body(scalar+scalar+"甲");find.move(0);find.tap(9,1);CHECK(find.app.findOpen());CHECK(find.app.find.setQuery(scalar)==hanime::find::QueryEditResult::Applied);find.tap(40);
  for(unsigned i=0;find.app.find.view().status==hanime::find::Status::Searching;++i){CHECK(i<10000);find.app.service(++find.now,false);}
  const auto match=find.app.findDocument.matchHighlight();CHECK(match.visible&&match.begin==0&&match.end==3);const auto found=paint(find);const unsigned y=cp==0x2026?19:15;
  for(unsigned cell=0;cell<2;++cell)for(unsigned x=0;x<28;++x){const auto n=pixel(normal,cell*28+x,y);CHECK(pixel(found,cell*28+x,y)==(cell==0?1-n:n));}
 }
 std::cout<<"PASS R2_H_per_scalar_selection_find_colors delete_half_pair_equals_fresh reinsert_equals_fresh\n";
}
void specimens(const fs::path& folder){
 SwitchRig r;r.body("，。！？；：、\n（）【】《》\n“”‘’「」『』\n……\n——");r.move(0);const BodyFact before(r.app);
 alpha_render::ppm(folder,"r1_h_grouped_punctuation",paint(r,true));alpha_render::ppm(folder,"r2_h_grouped_punctuation",paint(r));before.check(r.app);
 r.toggle();alpha_render::frame(r);before.check(r.app);alpha_render::ppm(folder,"r2_v_grouped_punctuation",r.pixels);
 // These are the unchanged R1 sample texts and identical real product calls.
 // Supplying the R1 reference directory below proves vertical pixel equality.
 alpha_render::samples(folder);
}
std::vector<unsigned char> ppmPixels(const fs::path& path){
 std::ifstream in(path,std::ios::binary);CHECK(in.good());
 auto token=[&](){std::string value;for(;;){in>>std::ws;if(in.peek()!='#')break;std::getline(in,value);}in>>value;return value;};
 CHECK(token()=="P6"&&token()=="300"&&token()=="400"&&token()=="255");CHECK(in.get()=='\n');
 std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};CHECK(bytes.size()==300*400*3);return bytes;
}
void verticalReference(const fs::path& output,const fs::path& reference){
 for(const auto* name:{"host_product_v_chinese_sentence.ppm","host_product_v_english_sentence.ppm","host_product_v_punctuation_set.ppm"}){CHECK(ppmPixels(output/name)==ppmPixels(reference/name));std::cout<<"PASS R2_V_R1_PIXEL_IDENTICAL reference="<<(reference/name).string()<<'\n';}
}
}
int main(int argc,char** argv){try{
 CHECK(argc<=3);const fs::path output=argc>=2?fs::absolute(argv[1]):fs::current_path()/"r2_render_frames";fs::create_directories(output);
 const fs::path reference=argc==3?fs::absolute(argv[2]):fs::path(__FILE__).parent_path().parent_path().parent_path()/"evidence/parent-r1/render";
 alpha_r2_render::pairMetrics(output);alpha_r2_render::wrappedAndSeparate();alpha_r2_render::highlightsAndDeletion();alpha_r2_render::specimens(output);
 // Reuse the existing finite character, vertical tight-cell and six-dot/
 // continuous vertical dash checks; this is the same production raster.
 alpha_render::metricsAndForms();
 alpha_r2_render::verticalReference(output,reference);
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS alpha_punctuation_r2_render device=NOT_RUN\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
