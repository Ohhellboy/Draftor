#define DRAFTOR_FIND_TEST_FIXTURE_ONLY 1
#include "../../integration/product/tests/find_chinese.cpp"
#undef DRAFTOR_FIND_TEST_FIXTURE_ONLY
#include "board_config.hpp"
#include "st7305_waveshare_adapter.hpp"
#include "editor_hud.hpp"
#include "horizontal_raw.hpp"
#include "docmanager_display.hpp"
#include "metadata_font.hpp"
using Color=st7305_waveshare::Color;
constexpr auto white=Color::White,black=Color::Black;
static unsigned redInk=0;
static struct MonoLcd {
 std::array<std::uint8_t,15000> fb{};
 void pixel(int x,int y,Color c){if(c==Color::Red)++redInk;st7305_waveshare::pixel(fb.data(),x,y,c);}
 void fillRect(int x,int y,int w,int h,Color c){for(int j=0;j<h;++j)for(int i=0;i<w;++i)pixel(x+i,y+j,c);}
} lcd;
static unsigned previewTime=0;
static unsigned millis(){return previewTime;}
struct MonoRig:FindRig {
 void render(){
  CHECK(ime_device::prepareImePresentation(&ime,app));
  st7305_waveshare::fill(lcd.fb.data(),white);redInk=0;previewTime=now;
  const bool vertical=!app.horizontalImeSurface();
  const bool imeVisible=ime_device::candidateSurfaceVisible(&ime,&app);
  const int compositionY=ime.expanded()?210:278;
  if(!vertical)app.layout.setHeightPx(s3app::editor_geometry::bodyHeight(app.findOpen(),imeVisible,compositionY));
  auto glyph=[&](uint32_t cp,int x,int y,Color c,unsigned scale=1){
   s3host::BuiltinSmallFont small;const auto g=scale==2?small.glyph(cp):font.glyph(cp);
   for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)
    if(g.bits[row*g.stride+col/8]&(0x80>>(col%8)))lcd.pixel(x+g.bearing+col,y+row,c);
  };
  auto label=[&](const char* text,int x,int y,Color c=black,int maxX=294){
   s3host::BuiltinSmallFont small;const std::string_view value(text);
   for(size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){
    const auto cp=s3app::display::scalar(value,at);const int advance=s3app::display::advance(small,cp);
    if(x+advance>maxX)break;glyph(cp,x,y,c,2);x+=advance;
   }
  };
  s3app::MetadataFont metadata(font);
  s3app::editor_hud::title(app.currentName,vertical,metadata,[](int x,int y){lcd.pixel(x,y,black);});
  s3app::VerticalPaintResult v;
  if(vertical){
   const s3app::VerticalPaintPort port{
    [](void*,draftor::vertical::Rect r,s3app::VerticalColor c){lcd.fillRect(r.x,r.y,r.width,r.height,c==s3app::VerticalColor::White?white:c==s3app::VerticalColor::Red?Color::Red:black);},
    [](void*,uint16_t x,uint16_t y,s3app::VerticalColor c){lcd.pixel(x,y,c==s3app::VerticalColor::White?white:c==s3app::VerticalColor::Red?Color::Red:black);},nullptr,nullptr,true};
   v=s3app::paintVerticalView(app,app.verticalPresentation(),app.findOpen()?nullptr:&ime,font,now,port);CHECK(v.status==draftor::vertical::Status::Ready);
  }else s3app::paintHorizontalBody(app,font,[](int x,int y,int w,int h,bool ink){lcd.fillRect(x,y,w,h,ink?black:white);},[&](uint32_t cp,int x,int y,bool ink){glyph(cp,x,y,ink?black:white);});
  s3app::find_ui::Receipt find;
  if(app.findOpen())find=s3app::find_ui::paint(app,&ime,{[](void*,int x,int y,bool ink){lcd.pixel(x,y,ink?black:white);},nullptr});
  {
   auto* app=&this->app;auto* ime=&this->ime;
#include "ws_horizontal_product.inc"
  }
  keyboard::Status ble{};ble.phase=keyboard::Phase::Ready;
  if(!app.findOpen())s3app::editor_hud::footer(s3app::editor_hud::state(app,&ime,resources.rules(),ble,{}),metadata,[](int x,int y,int w,int h){lcd.fillRect(x,y,w,h,white);},[](int x,int y){lcd.pixel(x,y,black);});
  const auto result=ime_device::publishActivePresentation(app,&ime,app.presentationFrame(),v.ime,find);CHECK(result.accepted);frame=result.candidates;
 }
 void saveMono(const fs::path& out,const char* name,bool red=false){
  const auto before=std::string(app.document.text());const auto query=std::string(app.find.view().query);
  render();if(red)CHECK(redInk>0);CHECK(std::string_view(app.document.text())==before&&std::string_view(app.find.view().query)==query);
  fs::create_directories(out);std::ofstream image(out/(std::string(name)+".pgm"),std::ios::binary);image<<"P5\n300 400\n255\n";
  for(int y=0;y<400;++y)for(int x=0;x<300;++x)image.put(st7305_waveshare::isWhite(lcd.fb.data(),x,y)?char(255):char(0));
  CHECK(image.good());std::ofstream raw(out/(std::string(name)+".st7305"),std::ios::binary);raw.write(reinterpret_cast<const char*>(lcd.fb.data()),lcd.fb.size());
  std::cout<<"PREVIEW "<<name<<" 300x400 actual painters native_bytes=15000 red_pixels_mapped="<<redInk<<" device=NOT_TESTED\n";
 }
 void seed(){CHECK(app.commitText("清风吹过山岗，今天写下新的故事。\nHello Draftor，中文与 English 相伴。\n保存文字，继续写作。",app.editStamp()));CHECK(app.session.manualSave(++now));app.service(++now,false);app.currentName="清风与写作";}
};
int main(int argc,char** argv){try{
 CHECK(argc==2);const fs::path out=argv[1];
 {MonoRig r;r.seed();r.app.moveToByte(6,false);r.app.moveToByte(18,true);CHECK(r.app.editor.selection().begin==6&&r.app.editor.selection().end==18);r.saveMono(out,"01-horizontal-selection");}
 {MonoRig r;r.seed();r.tap(61);r.saveMono(out,"02-vertical-editor");}
 {MonoRig r;r.seed();r.chinese(0);r.text("ui");r.settle();r.saveMono(out,"03-horizontal-candidates-red-focus",true);}
 {MonoRig r;r.seed();r.tap(61);r.chinese(0);r.text("ui");r.settle();r.saveMono(out,"04-vertical-candidates-red-focus",true);}
 for(bool vertical:{false,true}){
  MonoRig r;r.enter(vertical);CHECK(r.app.commitQuery("清风",r.app.queryStamp()));r.serviceSearch();r.saveMono(out,vertical?"06-vertical-find":"05-horizontal-find");
 }
 std::cout<<"PASS real product painters and exact horizontal product block, exact font, mono focus and Find inverse contrast, no mutation of text/query semantics\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
