// Host ports, real product layout/font/body painters; no substitute typesetter.
#define PHASE2C_REUSE_RIG
#include "runtime_layout_2c.cpp"
#include "horizontal_body.hpp"
#include <algorithm>
#include <array>
#include <limits>

// This fixture does not run the inherited allocation-failure cases.
bool phase2c_forbid_new=false;
std::size_t phase2c_new_calls=0;

namespace alpha_render {
constexpr unsigned Width=300,Height=400;
using Frame=std::array<std::uint8_t,Width*Height>;
constexpr std::string_view Marks="，。！？；：、（）【】《》“”‘’「」『』……——";

void rect(Frame& pixels,int x,int y,int width,int height,std::uint8_t color){
 CHECK(x>=0&&y>=0&&width>=0&&height>=0&&x+width<=int(Width)&&y+height<=int(Height));
 for(int row=y;row<y+height;++row)for(int col=x;col<x+width;++col)pixels[row*Width+col]=color;
}
void horizontal(SwitchRig& r){
 CHECK(r.app.horizontalActive());
 s3app::paintHorizontalBody(r.app,r.font,
  [&](int x,int y,int width,int height,bool ink){rect(r.pixels,x,y,width,height,ink?1:0);},
  [&](std::uint32_t cp,int x,int y,bool ink){
   const auto g=r.font.glyph(cp);CHECK(!g.missing);
   for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col){
    if(!(g.bits[row*g.stride+col/8]&(0x80>>(col%8))))continue;
    const int px=x+g.bearing+int(col),py=y+int(row);
    CHECK(px>=0&&py>=0&&px<int(Width)&&py<int(Height));r.pixels[py*Width+px]=ink?1:0;
   }
  });
}
void frame(SwitchRig& r){r.draw();if(r.app.horizontalActive())horizontal(r);}
void persistentVertical(SwitchRig& r){
 CHECK(!r.app.horizontalActive());
 const auto token=r.app.presentationFrame();
 for(unsigned n=0;!prepareImePresentation(&r.ime,r.app);++n){CHECK(n<2000000);r.step();}
 const s3app::VerticalPaintPort port{
  [](void* p,v::Rect b,s3app::VerticalColor c){rect(*static_cast<Frame*>(p),b.x,b.y,b.width,b.height,std::uint8_t(c));},
  [](void* p,std::uint16_t x,std::uint16_t y,s3app::VerticalColor c){CHECK(x<Width&&y<Height);(*static_cast<Frame*>(p))[y*Width+x]=std::uint8_t(c);},&r.pixels,nullptr};
 const auto painted=s3app::paintVerticalView(r.app,r.app.verticalPresentation(),&r.ime,r.font,r.now,port);
 CHECK(painted.status==v::Status::Ready);r.vertical_frame=painted.ime;
 const auto published=publishActivePresentation(r.app,&r.ime,token,r.vertical_frame);CHECK(published.accepted);r.frame=published.candidates;
}
void ppm(const fs::path& folder,const char* name,const Frame& pixels){
 const auto path=folder/(std::string("host_product_")+name+".ppm");
 std::ofstream out(path,std::ios::binary);CHECK(out.good());
 out<<"P6\n# HOST_REAL_PRODUCT_FRAME: 28px current font, actual H/V body painter; not a device photograph\n300 400\n255\n";
 for(auto color:pixels){const std::array<unsigned char,3> rgb=color==0?std::array<unsigned char,3>{255,255,255}:color==2?std::array<unsigned char,3>{180,0,0}:std::array<unsigned char,3>{0,0,0};out.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());}
 CHECK(out.good());std::cout<<"FRAME "<<path.string()<<" origin=HOST_REAL_PRODUCT_FRAME width=300 height=400 font_px=28\n";
}
std::vector<v::Event> events(SwitchRig& r){
 CHECK(r.app.verticalPresentation().settle()==v::Status::Ready);const auto& viewport=r.app.verticalPresentation().viewport();std::vector<v::Event> result;
 for(std::size_t i=0;i<viewport.eventCount();++i){const auto e=viewport.eventAt(i);if(e.glyph())result.push_back(e);}return result;
}
std::string utf8(std::uint32_t cp){
 if(cp<0x80)return std::string(1,char(cp));
 if(cp<0x800)return {char(0xc0|(cp>>6)),char(0x80|(cp&63))};
 return {char(0xe0|(cp>>12)),char(0x80|((cp>>6)&63)),char(0x80|(cp&63))};
}
struct InkBox {int left=28,top=28,right=-1,bottom=-1;unsigned count=0;};
InkBox bounds(const std::vector<std::uint8_t>& ink,unsigned width){
 InkBox b;for(std::size_t at=0;at<ink.size();++at)if(ink[at]){const int x=int(at%width),y=int(at/width);b.left=std::min(b.left,x);b.top=std::min(b.top,y);b.right=std::max(b.right,x);b.bottom=std::max(b.bottom,y);++b.count;}return b;
}
unsigned sourceInk(s3host::GlyphView g){unsigned count=0;for(unsigned y=0;y<g.height;++y)for(unsigned x=0;x<g.width;++x)if(g.bits[y*g.stride+x/8]&(0x80>>(x%8)))++count;return count;}
// Exercise the production vertical flow/raster with a tight cell clip, so any
// misplaced ink is detected by comparing the surviving count with source ink.
std::vector<std::uint8_t> tightCells(const ChineseFont& font,std::string_view text,unsigned height){
 v::Config config;config.body={0,0,28,std::uint16_t(height)};config.column_gap_px=0;
 v::Input input;input.text=text;input.automatic_literals=true;input.rules=v::bodyPolicyRules(v::BodyPolicy::LatinWordsUprightAtHash);
 v::Viewport viewport;CHECK(viewport.start(input,config,{0,Affinity::Downstream})==v::Status::MoreWork);
 v::Status status;do{status=viewport.service(input,256).status;}while(status==v::Status::MoreWork);CHECK(status==v::Status::Ready);
 struct Context {std::vector<std::uint8_t> ink;unsigned height;};Context c{std::vector<std::uint8_t>(28*height),height};
 v::RasterPort port;port.font_context=const_cast<ChineseFont*>(&font);port.pixel_context=&c;port.clip=config.body;port.punctuation_policy=v::PunctuationPolicy::CommonVertical;
 port.glyph=[](void* p,std::uint32_t cp){const auto g=static_cast<ChineseFont*>(p)->glyph(cp);return v::Bitmap{g.bits,g.width,g.height,g.stride,g.bearing,std::size_t(g.stride)*g.height,g.missing};};
 port.ink=[](void* p,std::uint16_t x,std::uint16_t y){auto& context=*static_cast<Context*>(p);CHECK(x<28&&y<context.height);context.ink[y*28+x]=1;};
 const auto painted=v::paint(viewport,input.stamp,port);CHECK(painted.status==v::Status::Ready&&painted.glyphs>0);return c.ink;
}
void metricsAndForms(){
 SwitchRig r;r.body(Marks);r.move(0);
 for(std::size_t at=0;at<Marks.size();at=editor::nextUtf8Boundary(Marks,at)){
  const auto cp=s3app::display::scalar(Marks,at);const auto original=r.font.glyph(cp);CHECK(!original.missing&&original.advance==28&&s3host::FontPort::advance(&r.font,cp)==28);
  const auto ink=tightCells(r.font,utf8(cp),28);const auto b=bounds(ink,28);CHECK(b.count>0);
  auto chosen=original;const auto form=v::punctuationForm(cp,v::PunctuationPolicy::CommonVertical);
  if(form.native&&!r.font.glyph(form.native).missing)chosen=r.font.glyph(form.native);
  else if(form.fallback)chosen=r.font.glyph(form.fallback);
  CHECK(!chosen.missing&&b.count==sourceInk(chosen));
  std::cout<<"GLYPH U+"<<std::hex<<cp<<std::dec<<" advance=28 ink="<<b.count<<" bbox="<<b.left<<','<<b.top<<','<<b.right<<','<<b.bottom<<" clipped=0\n";
 }
 r.toggle();r.draw();const auto placed=events(r);CHECK(placed.size()==25);
 for(const auto& e:placed){CHECK(e.kind==v::EventKind::Upright&&e.advance_px==28);CHECK(e.after().y_px-e.before().y_px==28);}
 for(const auto cp:{0xff0cU,0x3002U,0x3001U}){const auto b=bounds(tightCells(r.font,utf8(cp),28),28);CHECK(b.left>=19&&b.top<=3&&b.bottom<=8);}
 for(const auto cp:{0xff1aU,0xff1bU}){const auto b=bounds(tightCells(r.font,utf8(cp),28),28);CHECK(b.bottom-b.top>b.right-b.left&&b.left>=12&&b.right<=14);}
 const std::pair<std::uint32_t,std::uint32_t> quotes[]={{0x2018,0x300c},{0x2019,0x300d},{0x201c,0x300e},{0x201d,0x300f}};
 for(auto pair:quotes)CHECK(tightCells(r.font,utf8(pair.first),28)==tightCells(r.font,utf8(pair.second),28));
 for(const auto pair:{std::pair<unsigned,unsigned>{0xff08,0xff09},{0x3010,0x3011},{0x300a,0x300b},{0x300c,0x300d},{0x300e,0x300f}}){
  const auto open=tightCells(r.font,utf8(pair.first),28),close=tightCells(r.font,utf8(pair.second),28);CHECK(open!=close);
  const auto a=bounds(open,28),b=bounds(close,28);CHECK(a.top>b.top&&a.bottom>b.bottom);
 }
 const auto dots=tightCells(r.font,"……",56);unsigned groups=0;bool prior=false;
 for(unsigned y=0;y<56;++y){bool row=false;for(unsigned x=0;x<28;++x)if(dots[y*28+x]){row=true;CHECK(x==13||x==14);}if(row&&!prior)++groups;prior=row;}CHECK(groups==6);
 const auto line=tightCells(r.font,"——",56);CHECK(bounds(line,28).count==112);
 for(unsigned y=0;y<56;++y){CHECK(line[y*28+13]&&line[y*28+14]);}
 std::cout<<"PASS full_cell_metrics all_forms_nonmissing clipping=0 quote_corner_forms=4 ellipsis_dot_groups=6 dash_continuous_px=56\n";
}
void literalsAndSwitching(const fs::path& folder){
 SwitchRig r;const std::string text="Hello, world! Don't change v1.2, test@example.com or 3.14. @# don’t";r.body(text);r.move(0);frame(r);ppm(folder,"h_ascii_at_hash_curly_apostrophe",r.pixels);
 const BodyFact original(r.app);r.toggle();frame(r);original.check(r.app);ppm(folder,"v_ascii_at_hash_curly_apostrophe",r.pixels);
 bool at=false,hash=false,curly=false;for(const auto& e:events(r)){
  if(e.codepoint=='@'||e.codepoint=='#'){CHECK(e.kind==v::EventKind::Upright&&e.advance_px==28);at|=e.codepoint=='@';hash|=e.codepoint=='#';}
  else if(e.codepoint==0x2019){CHECK(e.kind==v::EventKind::Upright&&e.advance_px==28);curly=true;}
  else if(e.codepoint!=' '){CHECK(e.kind==v::EventKind::RotateCW90&&e.advance_px==r.font.glyph(e.codepoint).advance);}
 }CHECK(at&&hash&&curly);
 for(unsigned i=0;i<4;++i){r.toggle();frame(r);original.check(r.app);}
 r.tap(57);r.settle();original.check(r.app);r.tap(57);r.settle();original.check(r.app);
 std::cout<<"PASS ascii_proportional_rotation upright_at_hash full_BodyFact_HV_and_mode_switches\nLIMIT inherited_curly_apostrophe: U+2019_in_don’t_is_not_part_of_ASCII_literal; no_language_detection_added\n";
}
void samples(const fs::path& folder){
 const std::pair<const char*,const char*> samples[]={
  {"chinese_sentence","他说：“你好，Draftor！”（真的？）《草稿》；今天……继续——"},
  {"english_sentence","Hello, world! Don't change v1.2, test@example.com or 3.14."},
  {"punctuation_set","，。！？；：、（）【】《》“”‘’「」『』……——"}};
 for(const auto& sample:samples){SwitchRig r;r.body(sample.second);r.move(0);frame(r);const BodyFact before(r.app);ppm(folder,(std::string("h_")+sample.first).c_str(),r.pixels);r.toggle();frame(r);before.check(r.app);ppm(folder,(std::string("v_")+sample.first).c_str(),r.pixels);r.toggle();frame(r);before.check(r.app);}
}
void boundaryEdit(const fs::path& folder){
 SwitchRig r;r.toggle();frame(r);
 const unsigned advance=r.font.glyph(0xff08).advance,height=r.app.verticalPresentation().region().height,cells=height/advance;
 CHECK(advance==28&&cells>=2);
 std::string prefix;for(unsigned i=0;i<cells-1;++i)prefix+="甲";
 const auto beforeOpen=prefix.size(),afterOpen=beforeOpen+std::string_view("（").size(),afterNext=afterOpen+std::string_view("癸").size();
 const std::string original=prefix+"（癸）……——尾",deleted=prefix+"癸）……——尾";
 r.body(original);r.move(beforeOpen);persistentVertical(r);ppm(folder,"v_boundary_before",r.pixels);
 const auto placed=events(r);CHECK(placed.size()>cells);
 CHECK(placed[cells-1].codepoint==0xff08&&placed[cells-1].y_px==(cells-1)*advance&&height-placed[cells-1].after().y_px<advance);
 CHECK(placed[cells].codepoint==0x7678&&placed[cells].column==placed[cells-1].column+1&&placed[cells].y_px==0);
 r.tap(81);persistentVertical(r);CHECK(r.app.document.cursorByte()==afterOpen);
 r.tap(81);persistentVertical(r);CHECK(r.app.document.cursorByte()==afterOpen);
 r.tap(81);persistentVertical(r);CHECK(r.app.document.cursorByte()==afterNext);
 r.tap(82);persistentVertical(r);CHECK(r.app.document.cursorByte()==afterOpen);
 r.move(afterOpen);persistentVertical(r);const auto oldPixels=r.pixels;
 r.tap(42);persistentVertical(r);CHECK(r.app.document.text()==deleted.c_str()&&r.app.document.cursorByte()==beforeOpen);CHECK(oldPixels!=r.pixels);
 ppm(folder,"v_boundary_deleted",r.pixels);
 SwitchRig fresh;fresh.body(r.app.document.text());fresh.toggle();fresh.move(beforeOpen);persistentVertical(fresh);CHECK(r.pixels==fresh.pixels);
 CHECK(r.app.commitText("（",r.app.editStamp()));persistentVertical(r);CHECK(r.app.document.text()==original.c_str()&&r.app.document.cursorByte()==afterOpen);
 SwitchRig restored;restored.body(original);restored.toggle();restored.move(afterOpen);persistentVertical(restored);CHECK(r.pixels==restored.pixels);ppm(folder,"v_boundary_reinserted",r.pixels);
 // Native dash ink reaches both cell edges: deleting it must clear every old
 // pixel when the existing product painter repaints into the retained frame.
 r.move(original.size()-3);persistentVertical(r);r.tap(42);persistentVertical(r);CHECK(r.app.document.text()==(prefix+"（癸）……—尾").c_str());
 SwitchRig dashFresh;dashFresh.body(r.app.document.text());dashFresh.toggle();dashFresh.move(r.app.document.cursorByte());persistentVertical(dashFresh);CHECK(r.pixels==dashFresh.pixels);
 std::cout<<"PASS column_boundary_utf8_navigation body_height="<<height<<" advance="<<advance<<" cells="<<cells<<" backspace_one_scalar retained_frame_delete_reinsert_equals_fresh dash_clear_equals_fresh\nLIMIT vertical_kinsoku_deferred: opening_parenthesis_may_remain_at_column_bottom\n";
}
}
#ifndef ALPHA_PUNCTUATION_REUSE_RENDER
int main(int argc,char** argv){try{
 CHECK(argc<=2);const fs::path folder=argc==2?fs::absolute(argv[1]):fs::current_path();fs::create_directories(folder);
 alpha_render::metricsAndForms();alpha_render::samples(folder);alpha_render::literalsAndSwitching(folder);alpha_render::boundaryEdit(folder);
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS alpha_punctuation_render origin=HOST_REAL_PRODUCT_FRAME device=NOT_RUN\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
