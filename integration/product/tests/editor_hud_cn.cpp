#define DRAFTOR_FIND_TEST_FIXTURE_ONLY 1
#include "find_chinese.cpp"
#undef DRAFTOR_FIND_TEST_FIXTURE_ONLY
#include "editor_hud.hpp"
#include "horizontal_raw.hpp"
#include "docmanager_display.hpp"
#include "support/r2_raster.hpp"

#define paintHorizontalBody paintFix3HorizontalBody
#include "support/hud_fix3_horizontal_body.hpp"
#undef paintHorizontalBody

namespace hud=s3app::editor_hud;
enum class Color {Black,White,Red};
constexpr Color black=Color::Black,white=Color::White;
static unsigned hudOpens=0;
struct Lcd {
 std::array<uint8_t,120000>& pixels;
 void pixel(int x,int y,Color color){CHECK(x>=0&&x<300&&y>=0&&y<400);pixels[y*300+x]=color==white?255:color==Color::Red?128:0;}
 void fillRect(int x,int y,int w,int h,Color color){for(int j=0;j<h;++j)for(int i=0;i<w;++i)pixel(x+i,y+j,color);}
};
static std::array<uint8_t,120000>* activePixels=nullptr;
static uint32_t previewMillis=0;
static uint32_t millis(){return previewMillis;}
static struct ProductLcd {
 void pixel(int x,int y,Color c){CHECK(activePixels);Lcd{*activePixels}.pixel(x,y,c);}
 void fillRect(int x,int y,int w,int h,Color c){CHECK(activePixels);Lcd{*activePixels}.fillRect(x,y,w,h,c);}
} lcd;
static keyboard::Status ble(keyboard::Phase phase){keyboard::Status s{};s.phase=phase;s.passkey=1000000;return s;}
struct HudRig:FindRig {
 s3app::MetadataFont face{font};
 void horizontalComposition(){
  auto* app=&this->app;auto* ime=&this->ime;activePixels=&pixels;previewMillis=now;
  s3host::BuiltinSmallFont smallFont;const bool vertical=!app->horizontalImeSurface();
  const bool imeVisible=ime_device::candidateSurfaceVisible(ime,app);
  const int compositionY=ime->expanded()?210:278;
  auto glyph=[&](uint32_t cp,int x,int y,Color color,unsigned scale=1){
   const auto g=scale==2?smallFont.glyph(cp):font.glyph(cp);
   for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)
    if(g.bits[row*g.stride+col/8]&(0x80>>(col%8)))lcd.pixel(x+g.bearing+col,y+row,color);
  };
  auto label=[&](const char* text,int x,int y,Color color=black,int maxX=294){
   const std::string_view value(text);
   for(size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){
    const auto cp=s3app::display::scalar(value,at);const int width=s3app::display::advance(smallFont,cp);
    if(x+width>maxX)break;glyph(cp,x,y,color,2);x+=width;
   }
  };
#include "support/hud_horizontal_product.inc"
 }
 void scene(keyboard::Status keyboard,const char* name=nullptr,bool offMapping=false){
  if(app.horizontalActive())app.layout.setHeightPx(s3app::editor_geometry::bodyHeight(app.findOpen(),ime_device::candidateSurfaceVisible(&ime,&app),ime.expanded()?210:278));
  CHECK(FindRig::display());horizontalComposition();
  const auto original=pixels;const auto revision=app.document.revision();
  const auto cursor=app.document.cursorByte();const bool dirty=app.document.dirty();
  const auto io=ime_device::metrics.io();hudOpens=0;
  s3fs::File::openObserver=[](const char*,const char*){++hudOpens;};
  const auto state=hud::state(app,&ime,resources.rules(),keyboard,{});
  if(name&&(std::string_view(name).substr(0,2)=="02"||std::string_view(name).substr(0,2)=="03"||offMapping))CHECK(hud::feedback(state).empty());
  auto dot=[&](int x,int y){Lcd{pixels}.pixel(x,y,black);};
  hud::title(app.currentName,!app.horizontalActive(),face,dot);
  if(!app.findOpen())hud::footer(state,face,[&](int x,int y,int w,int h){Lcd{pixels}.fillRect(x,y,w,h,white);},dot);
  s3fs::File::openObserver=nullptr;
  CHECK(hudOpens==0&&(ime_device::metrics.io()-io).reads==0);
  CHECK(app.document.revision()==revision&&app.document.cursorByte()==cursor&&app.document.dirty()==dirty);
  for(int y=app.horizontalActive()&&!app.findOpen()?20:34;y<380;++y)for(int x=0;x<300;++x)CHECK(pixels[y*300+x]==original[y*300+x]);
  if(app.findOpen())for(int y=380;y<400;++y)for(int x=0;x<300;++x)CHECK(pixels[y*300+x]==original[y*300+x]);
  if(offMapping){
   // Explicitly unreachable Host caption mapping; never synthesizes a BLE Phase.
   Lcd{pixels}.fillRect(240,380,60,20,white);
   const auto* caption=hud::bluetoothText(hud::Bluetooth::Off);
   hud::text(face,caption,292-int(hud::width(caption)),383,dot);
  }
  if(name){const auto dir=fs::path(std::getenv("HUD_PREVIEW_DIR"));fs::create_directories(dir);
   std::ofstream out(dir/(std::string(name)+".pgm"),std::ios::binary);out<<"P5\n300 400\n255\n";out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());CHECK(out.good());}
 }
 void seed(bool vertical=false){
  CHECK(app.commitText("清风吹过山岗，今天写下新的故事。\nHello Draftor，中文与 English 相伴。\n保存文字，继续写作。",app.editStamp()));
  CHECK(app.session.manualSave(++now));app.service(++now,false);
  app.currentName="清风与写作";if(vertical)tap(61);CHECK(display());
 }
};
static void fontAndMappings(){
 s3host::BuiltinLatinFont body;s3app::MetadataFont face(body);hud::Metrics metrics;
 const char* strings[]={"En双全五已连接配对中未连接已关闭已保存未保存保存中…等待保存","保存失败，命令取消存储读写失败存储不可用存储已变化文稿损坏存储异常内存不足，操作失败",
 "资源错误，请查看日志蓝牙故障，请查看日志松开按键，再轻按 Shift已连接，配对未保存提交结果待确认，请勿重复提交提交未生效：空格重试，Esc编辑正在准备中文输入正在加载字体上下移动，左键载入，Esc返回左右移动，下键载入，Esc返回空格或回车提交存储异常；输入 123456 后回车"};
 unsigned glyphs=0;
 for(auto s:strings){const std::string_view value=s;for(size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){
  const auto cp=s3app::display::scalar(value,at);const auto g=face.glyph(cp);CHECK(!g.missing&&g.height==14);CHECK(g.advance==metrics.glyph(cp).advance);++glyphs;
 }}
 for(auto scheme:{hanime_extension::SchemeRules::R1Xiaohe,hanime_extension::SchemeRules::FullPinyin,hanime_extension::SchemeRules::DirectExact})CHECK(std::string_view(hud::mode(false,scheme))=="En");
 CHECK(std::string_view(hud::mode(true,hanime_extension::SchemeRules::R1Xiaohe))=="双");
 CHECK(std::string_view(hud::mode(true,hanime_extension::SchemeRules::FullPinyin))=="全");
 CHECK(std::string_view(hud::mode(true,hanime_extension::SchemeRules::DirectExact))=="五");
 for(auto p:{keyboard::Phase::Starting,keyboard::Phase::Scanning,keyboard::Phase::Connecting,keyboard::Phase::AwaitRelease,keyboard::Phase::Disconnected,keyboard::Phase::Cleaning,keyboard::Phase::Error})CHECK(hud::bluetooth(p)==hud::Bluetooth::Disconnected);
 CHECK(hud::bluetooth(keyboard::Phase::Ready)==hud::Bluetooth::Connected);
 CHECK(hud::bluetooth(keyboard::Phase::Pairing)==hud::Bluetooth::Pairing);
 for(auto save:{"已保存","未保存","保存中…","等待保存","保存失败，命令取消","内存不足，操作失败"})
  CHECK(292-int(hud::width("未连接"))-12-int(hud::width(save))>8+int(hud::width("En"))+12);
 std::cout<<"PASS exact UI14/Latin glyph coverage and advances glyphs="<<glyphs<<" H14 origins H_title=8,5 body_top=20 height=320 V_title=right292,4 footer=383; BLE Off unreachable\n";
}
static void clearAndErrors(){
 s3host::BuiltinLatinFont body;s3app::MetadataFont face(body);std::array<uint8_t,120000> p{};p.fill(255);Lcd lcd{p};hud::State s;
 s.keyboard=ble(keyboard::Phase::Ready);auto pixel=[&](int x,int y){lcd.pixel(x,y,black);};auto fill=[&](int x,int y,int w,int h){lcd.fillRect(x,y,w,h,white);};
 s.notice="Save failed; command cancelled";hud::footer(s,face,fill,pixel);CHECK(std::string_view(hud::saveText(s))!="已保存");
 s.notice="Saved";s.dirty=false;CHECK(std::string_view(hud::saveText(s))=="已保存");
 s.dirty=true;CHECK(std::string_view(hud::saveText(s))=="未保存");
 s.saving=true;CHECK(std::string_view(hud::saveText(s))=="保存中…");
 s.saving=false;s.savePending=true;CHECK(std::string_view(hud::saveText(s))=="等待保存");
 s.savePending=false;s.dirty=false;hud::footer(s,face,fill,pixel);const auto after=p;
 p.fill(255);hud::footer(s,face,fill,pixel);CHECK(after==p);
 s.keyboard=ble(keyboard::Phase::Pairing);s.keyboard.passkey=123456;hud::footer(s,face,fill,pixel);
 s.keyboard=ble(keyboard::Phase::Ready);hud::footer(s,face,fill,pixel);CHECK(after==p);
 s.keyboard=ble(keyboard::Phase::Error);CHECK(!hud::feedback(s).empty());
 s.keyboard=ble(keyboard::Phase::AwaitRelease);CHECK(hud::feedback(s).find("Shift")!=std::string_view::npos);
 s.keyboard=ble(keyboard::Phase::Ready);s.keyboard.pairing_not_saved=true;CHECK(!hud::feedback(s).empty());
 s.keyboard.phase=keyboard::Phase::Disconnected;CHECK(hud::feedback(s).empty());
 std::cout<<"PASS shorter status/passkey repaint leaves no residual pixels; errors and release gate retained\n";
}

static void horizontalGeometry(){
 HudRig r;r.seed();std::string more;for(unsigned i=0;i<20;++i)more+="\n第十行用于核对滚动和选区。";
 CHECK(r.app.commitText(more,r.app.editStamp()));
 const std::string text(r.app.document.text());
 auto compare=[&](bool composition,int y){
  const auto revision=r.app.document.revision();
  r.app.layout.setHeightPx(s3app::editor_geometry::bodyHeight(r.app.findOpen(),composition,y));
  const int top=r.app.findOpen()?34:20;
  const auto view=r.app.navigator.sync();
  CHECK(view.visual_line>=view.first_visible_line&&view.visual_line<view.first_visible_line+r.app.layout.visibleTextLineCount());
  std::array<uint8_t,120000> actual{},prior{};actual.fill(255);prior.fill(255);
  auto fill=[&](std::array<uint8_t,120000>& pixels,int x,int y,int w,int h,bool ink){
   CHECK(x>=0&&x+w<=300&&y>=top&&y+h<=top+r.app.layout.heightPx());
   Lcd{pixels}.fillRect(x,y,w,h,ink?black:white);
  };
  auto glyph=[&](std::array<uint8_t,120000>& pixels,uint32_t cp,int x,int y,bool ink){
   const auto g=r.font.glyph(cp);CHECK(y>=top&&y+g.height<=top+r.app.layout.heightPx());
   for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)
    if(g.bits[row*g.stride+col/8]&(0x80>>(col%8)))Lcd{pixels}.pixel(x+g.bearing+col,y+row,ink?black:white);
  };
  s3app::paintHorizontalBody(r.app,r.font,[&](int x,int y,int w,int h,bool ink){fill(actual,x,y,w,h,ink);},[&](uint32_t cp,int x,int y,bool ink){glyph(actual,cp,x,y,ink);});
  // Exact frozen FIX3 painter, translated only in this comparison's output port.
  const int translation=r.app.findOpen()?0:14;
  s3app::paintFix3HorizontalBody(r.app,r.font,[&](int x,int y,int w,int h,bool ink){fill(prior,x,y-translation,w,h,ink);},[&](uint32_t cp,int x,int y,bool ink){glyph(prior,cp,x,y-translation,ink);});
  CHECK(actual==prior);CHECK(r.app.document.revision()==revision&&std::string_view(r.app.document.text())==std::string_view(text));
  if(r.app.findOpen())CHECK(r.app.layout.heightPx()==170);
  else CHECK(r.app.layout.heightPx()==(composition?y-20:320));
 };
 compare(false,278);r.tap(80,2);CHECK(r.app.editor.selection().has_selection);compare(false,278);
 r.tap(82,2);compare(false,278);r.tap(75);compare(false,278);r.tap(78);compare(false,278);
 compare(true,278);compare(true,210);
 r.tap(9,1);CHECK(r.app.findOpen());compare(false,278);
 r.tap(41);CHECK(!r.app.findOpen());CHECK(r.app.layout.heightPx()==320);compare(false,278);
 r.tap(61);CHECK(!r.app.horizontalActive());r.scene(ble(keyboard::Phase::Ready));
 r.tap(61);CHECK(r.app.horizontalActive());compare(false,278);
 std::cout<<"PASS horizontal title y=5; body y=20/height320; glyph/selection/caret exact FIX3 translation -14px; scroll and H/V/Find return; Find viewport remains34/170\n";
}

int main(){try{
 CHECK(std::getenv("HUD_PREVIEW_DIR"));fontAndMappings();clearAndErrors();horizontalGeometry();
 {HudRig r;r.seed();r.scene(ble(keyboard::Phase::Ready),"01-horizontal-en-saved");
  r.tap(9,1);CHECK(r.app.findOpen());r.scene(ble(keyboard::Phase::Ready));r.tap(41);CHECK(!r.app.findOpen());r.scene(ble(keyboard::Phase::Ready));
  r.tap(59);CHECK(r.app.filesOpen());r2preview::Raster menu;CHECK(s3app::display::documentMenu(r.app,menu.contentFont,menu.rect(),menu.aux(),menu.content()));r.tap(41);CHECK(!r.app.filesOpen());r.scene(ble(keyboard::Phase::Ready));}
 {HudRig r;r.seed();CHECK(r.app.commitText("未保存",r.app.editStamp()));r.chinese(0);r.text("ui");r.settle();CHECK(r.ime.candidateAvailable());r.scene(ble(keyboard::Phase::Disconnected),"02-horizontal-xiaohe-candidates");r.tap(81);r.settle();r.scene(ble(keyboard::Phase::Ready));r.tap(82);r.settle();r.scene(ble(keyboard::Phase::Ready));}
 {HudRig r;r.seed(true);r.app.currentName="这是用于核对原有截断规则的中文长文件名称，不改变正文几何";r.chinese(1);r.text("shi");r.settle();r.scene(ble(keyboard::Phase::Pairing),"03-vertical-pinyin-long-name");r.tap(80);r.settle();r.scene(ble(keyboard::Phase::Ready));r.tap(79);r.settle();r.scene(ble(keyboard::Phase::Ready));}
 {HudRig r;r.seed(true);r.chinese(2);r.text("jg");r.settle();auto p=ble(keyboard::Phase::Pairing);p.passkey=123456;r.scene(p,"04-vertical-wubi-pair-code");}
 {HudRig r;r.seed();CHECK(r.app.commitText("等待保存",r.app.editStamp()));CHECK(r.app.requestSave(++r.now).status==s3app::ds::Status::Busy);CHECK(r.app.persistence.backgroundSaveActive());r.scene(ble(keyboard::Phase::Ready),"05-horizontal-saving");}
 {HudRig r;r.seed(true);CHECK(r.app.commitText("保存失败保留正文",r.app.editStamp()));s3fs::faults.write_bytes=0;(void)r.app.requestSave(++r.now);for(unsigned i=0;i<200&&r.app.persistence.backgroundSaveActive();++i)r.app.service(++r.now,false);s3fs::faults={};CHECK(r.app.document.dirty());CHECK(hud::storageWarning(r.app.notice));r.scene(ble(keyboard::Phase::Ready),"06-vertical-save-failure");}
 {HudRig r;r.seed();r.scene(ble(keyboard::Phase::Error),"07-horizontal-ble-fault");}
 {HudRig r;r.seed(true);r.chinese(2);r.scene(ble(keyboard::Phase::Disconnected),"08-vertical-off-mapping-only",true);}
 std::cout<<"PASS 8 native real-font product frames; unchanged body/candidate/Find pixels outside HUD; no HUD IO or model mutation; real scheme and menu/Find transitions\n";
 return 0;
}catch(const std::exception& e){s3fs::faults={};std::cerr<<e.what()<<'\n';return 1;}}
