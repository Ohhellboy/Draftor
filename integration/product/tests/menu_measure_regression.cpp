// Finite Host comparison using real menu, font and document implementations.
#define main inherited_menu_measure_usb_main
#include "usb_product_flow.cpp"
#undef main
#include "docmanager_display.hpp"
#include "support/r2_raster.hpp"
#include "support/menu_measure_probe.hpp"
#include "operation_profile.hpp"
#include <fstream>
#include <vector>
#include <array>

namespace {
struct Item{std::string name;};
std::string hex(std::string_view s){
 static constexpr char digits[]="0123456789abcdef";std::string out;
 for(unsigned char c:s){out+=digits[c>>4];out+=digits[c&15];}return out;
}
struct Output {
 fs::path root;std::ofstream semantics,counts;unsigned cases=0,frames=0;
 explicit Output(fs::path path):root(std::move(path)){
  if(root.empty())return;fs::create_directories(root);
  semantics.open(root/"semantics.tsv");counts.open(root/"COUNTS.tsv");
  counts<<"case\tglyph\tmetric\traster_glyph\traster_metric\n";
  CHECK(semantics.good()&&counts.good());
 }
 void record(const std::string& key,const menu_probe::CountedMenuFont& font,unsigned raster=0,unsigned rasterMetric=0){
  ++cases;if(root.empty())return;
  counts<<key<<'\t'<<font.lookups<<'\t'<<font.metrics<<'\t'<<raster<<'\t'<<rasterMetric<<'\n';
 }
 void frame(const std::string& key,const r2preview::Raster& raster){
  ++frames;if(root.empty())return;
  std::ofstream image(root/(key+".pgm"),std::ios::binary);image<<"P5\n300 400\n255\n";
  image.write(reinterpret_cast<const char*>(raster.pixels.data()),raster.pixels.size());
  std::ofstream labels(root/(key+".txt"));
  for(const auto& text:raster.labels)labels<<text<<'\n';
  CHECK(image.good()&&labels.good());
 }
};
std::string repeated(const char* part,unsigned count){std::string s;while(count--)s+=part;return s;}
void matrix(Output& out){
 const std::string suffix=repeated("城",25)+"WW（恢复）（1）";CHECK(suffix.size()==96);
 std::vector<Item> items={
  {"文章"},{"Draft 121"},{"中英 Draft 121 2026"},
  {"城市公共空间与居民日常生活研究甲组访谈记录修订版"},
  {"城市公共空间与居民日常生活研究乙组访谈记录修订版"},
  {std::string(90,'W')+"000001"},{std::string(90,'W')+"000002"},
  {std::string(90,'W')+"000001"}, // Duplicate real names, never invent an ID.
  {suffix},{repeated("汉",32)},{"a((x))b"},{"（x)y）"},
  {repeated("）",32)},{"文😀𨿍"},{"a\xcc\x81汉"},{""}
 };
 const std::vector<Item> invalidPeers={{std::string(90,'W')+"000001"},
  {std::string(97,'W')},{std::string("\xe4\xb8",2)},{""},
  {std::string(90,'W')+"000002"}};
 unsigned page=0;
 for(const auto* set:std::array<const std::vector<Item>*,2>{&items,&invalidPeers}){
  CHECK(set->size()<=16);
  for(int width:{-1,0,1,192,264,280})for(size_t i=0;i<set->size();++i){
   const auto& name=(*set)[i].name;const std::string key="p"+std::to_string(page)+"_i"+std::to_string(i)+"_w"+std::to_string(width);
   menu_probe::CountedMenuFont previewFont;
   const auto preview=s3app::display::menuNamePreview(*set,i,previewFont,width);
   CHECK(preview.size<100&&editor::isValidUtf8(preview.text()));
   out.record("preview_"+key,previewFont);
   menu_probe::CountedMenuFont wrapFont;
   const auto lines=s3app::display::wrapMenuName(name,wrapFont,width);
   out.record("wrap_"+key,wrapFont);
   if constexpr(menu_probe::HasGlyphAdvance<s3host::FontPort>::value){
    CHECK(!previewFont.lookups&&!wrapFont.lookups);
   }
   if(!out.root.empty()){
    out.semantics<<key<<'\t'<<hex(name)<<'\t'<<hex(preview.text())<<'\t'<<preview.truncated<<'\t'<<preview.disambiguated
     <<'\t'<<lines.valid<<'\t'<<lines.relaxed<<'\t'<<unsigned(lines.count);
    for(auto offset:lines.offsets)out.semantics<<','<<unsigned(offset);
    out.semantics<<'\n';
   }
  }
  ++page;
 }
}
struct LegacyFont final:s3host::FontPort {
 mutable unsigned calls=0;mutable uint8_t bits=0;
 s3host::GlyphView glyph(uint32_t cp)const noexcept override{
  ++calls;bits=uint8_t(cp);return {&bits,1,1,1,uint16_t(cp=='0'?0:cp=='W'?9:4),0,false};
 }
 uint16_t lineHeight()const noexcept override{return 1;}
 uint16_t defaultAdvance()const noexcept override{return 7;}
};
void genericFallback(){
 LegacyFont font;const s3host::FontPort& port=font;
 const auto before=font.calls;CHECK(menu_probe::rawAdvance(port,'0')==0&&font.calls==before+1);
 CHECK(s3app::display::advance(port,'0')==1);
 CHECK(s3app::display::measure("00",port)==2);
 const auto wrapped=s3app::display::wrapMenuName("00",port,7);
 CHECK(wrapped.valid&&wrapped.count==2&&wrapped.offsets[1]==1&&wrapped.offsets[2]==2);
 CHECK(s3host::FontPort::advance(&port,'0')==7);
 std::cout<<"PASS generic FontPort default metric dispatch, raw zero advance, unchanged max1/wrap default/static fallback\n";
}
void nativeMenus(Output& out){
 Rig r;const std::vector<std::string> names={
  "文章","Draft 121","中英 Draft 121 2026",
  "城市公共空间与居民日常生活研究甲组访谈记录修订版",
  "城市公共空间与居民日常生活研究乙组访谈记录修订版",
  std::string(90,'W')+"000001",std::string(90,'W')+"000002",
  "关于城市公共空间与日常写作的田野调查记录（恢复）（1）",
  repeated("城",25)+"WW（恢复）（1）",repeated("汉",32),
  "a((x))b","（x)y）",repeated("）",32),"阅读笔记.txt","临时记录"
 };
 for(const auto& name:names)CHECK(r.app.session.create(name,++r.now));
 r.app.key({1,++r.now,59,0,true});CHECK(r.app.filesOpen());
 CHECK(r.app.manager.state().page.items.size()==16);
 unsigned index=0;
 for(size_t selection:{size_t(0),size_t(5),size_t(10),size_t(15)}){
  r.app.manager.moveSelection(int(selection)-int(r.app.manager.state().selected));CHECK(r.app.manager.state().selected==selection);
  const auto& state=r.app.manager.state();const auto id=state.page.items[state.selected].id;
  const auto selected=state.selected,offset=state.page.offset,count=state.page.items.size();
  const auto current=r.app.session.currentId();const auto revision=r.app.document.revision();
  menu_probe::CountedMenuFont font;r2preview::Raster canvas;unsigned rasterGlyph=0,rasterMetric=0,opened=0;
  static unsigned* observer=nullptr;observer=&opened;
  s3fs::File::openObserver=[](const char*,const char*){++*observer;};
  if(s3profile::active)(void)s3profile::finish();s3profile::begin("menu_measure_frame");
  CHECK(s3app::display::documentMenu(r.app,font,canvas.rect(),canvas.aux(),
   [&](std::string_view text,int x,int y,bool inverse){
    const auto g=font.lookups,m=font.metrics;canvas.text(text,x,y,inverse,font);
    rasterGlyph+=font.lookups-g;rasterMetric+=font.metrics-m;
   }));
  s3app::display::menuFeedback(r.app,canvas.rect(),canvas.aux());
  const auto storage=s3profile::finish();s3fs::File::openObserver=nullptr;observer=nullptr;
  CHECK(!opened&&!storage.reads&&!storage.writes&&!storage.opens&&!storage.calls[s3profile::Opendir].count);
  CHECK(state.selected==selected&&state.page.offset==offset&&state.page.items.size()==count&&state.page.items[selected].id==id);
  CHECK(r.app.session.currentId()==current&&r.app.document.revision()==revision);
  CHECK(rasterGlyph&&std::count(canvas.pixels.begin(),canvas.pixels.end(),0)>0);
  if constexpr(menu_probe::HasGlyphAdvance<s3host::FontPort>::value)CHECK(font.lookups==rasterGlyph);
  const auto key="menu_frame_"+std::to_string(index++);
  out.record(key,font,rasterGlyph,rasterMetric);out.frame(key,canvas);
  std::cout<<"MENU_FRAME selected="<<selected<<" glyph="<<font.lookups<<" metric="<<font.metrics
   <<" raster_glyph="<<rasterGlyph<<" raster_metric="<<rasterMetric<<" storage_io=0 body_unchanged=1\n";
 }
}
}
int main(int argc,char** argv){try{
 Output out(argc>1?fs::path(argv[1]):fs::path{});genericFallback();matrix(out);nativeMenus(out);
 CHECK(out.cases==256&&out.frames==4);
 std::cout<<"PASS finite menu semantics/counts/native frames cases="<<out.cases<<" frames="<<out.frames
  <<" native_metric_interface="<<menu_probe::HasGlyphAdvance<s3host::FontPort>::value
  <<" HOST_ONLY_DEVICE_TIME_NOT_MEASURED\n";return 0;
}catch(const std::exception& e){s3fs::File::openObserver=nullptr;std::cerr<<e.what()<<'\n';return 1;}}

