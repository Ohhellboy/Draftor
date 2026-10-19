// SPDX-License-Identifier: Apache-2.0
// Reuse the existing Wi-Fi product rig and document transaction helpers.
#define main inherited_wifi_test_main
#include "wifi_product_flow.cpp"
#undef main
#include "wifi_display.hpp"
#include "ST7306_OsptekLpm8.h"
#include "docmanager_display.hpp"
#include "input_route.hpp"
#include <fstream>
namespace q=draftor::qr;
void pinMode(std::uint8_t,int){}
void digitalWrite(std::uint8_t,int){}
void delay(unsigned long){}
void SPIClass::begin(int,int,int,int){}
void SPIClass::beginTransaction(SPISettings){}
void SPIClass::writeBytes(const std::uint8_t*,std::size_t){}
void SPIClass::endTransaction(){}
void key(Rig& r,unsigned code,bool down=true,unsigned mods=0){CHECK(r.wifi.key({1,++r.now,std::uint8_t(code),std::uint8_t(mods),down},{}));}
std::string bits(q::Matrix m){std::string s;for(int y=0;y<m.side;++y)for(int x=0;x<m.side;++x)s+=m.module(x,y)?'1':'0';return s;}
void lifecycle(){
 Rig r;CHECK(r.app.commitText("QR original 中文",r.app.editStamp()));const auto id=r.app.session.currentId();
 const auto text=std::string(r.app.document.text());const auto revision=r.app.document.revision();
 const auto base=draftor::memory::allocation_stats.live_allocations;
 for(unsigned i=0;i<12;++i){
  CHECK(r.wifi.start({})&&r.wifi.qr().matrix().valid());CHECK(r.net.live==s3app::WifiQrBufferBytes);
  CHECK(r.net.peak==625);const auto encoded=bits(r.wifi.qr().matrix());const auto allocations=r.net.allocations;
  for(unsigned j=0;j<20;++j)r.tick();CHECK(r.net.allocations==allocations&&bits(r.wifi.qr().matrix())==encoded);
  key(r,43,false);key(r,43,true,1);CHECK(r.net.allocations==allocations);
  key(r,43);CHECK(r.wifi.qr().page()==s3app::WifiQrPage::Address&&r.net.allocations==allocations+2);
  key(r,19);CHECK(r.wifi.qr().page()==s3app::WifiQrPage::Manual&&!r.wifi.qr().matrix().valid()&&r.net.live==0);
  key(r,19);CHECK(r.wifi.qr().matrix().valid());
  key(r,41);CHECK(!r.wifi.panel()&&r.wifi.active()&&!r.wifi.qr().matrix().valid()&&r.net.live==0);
  const auto closed=r.net.allocations;for(unsigned j=0;j<20;++j)r.tick();CHECK(r.net.allocations==closed);
  key(r,67);CHECK(r.wifi.qr().matrix().valid());key(r,18);
  CHECK(!r.wifi.active()&&!r.wifi.qr().matrix().valid()&&r.net.live==0);
  CHECK(!*r.wifi.ssid()&&!*r.wifi.password());
  CHECK(draftor::memory::allocation_stats.live_allocations==base);
 }
 CHECK(r.app.session.currentId()==id&&std::string_view(r.app.document.text())==text&&r.app.document.revision()==revision&&r.app.document.dirty());
 CHECK(r.wifi.start({}));const auto first=bits(r.wifi.qr().matrix());r.wifi.stop();
 r.net.password_value="fedcba9876543210";r.net.address_value="http://10.23.4.1:8080";
 CHECK(r.wifi.start({}));CHECK(bits(r.wifi.qr().matrix())!=first);key(r,43);
 const auto second=bits(r.wifi.qr().matrix());CHECK(!second.empty());key(r,41);
 CHECK(r.request("GET / HTTP/1.1\r\nHost: 10.23.4.1:8080\r\nOrigin: http://10.23.4.1:8080\r\n\r\n").find("200 OK")!=std::string::npos);
 CHECK(r.request(r.get("/")).find("403 Error")!=std::string::npos);
 CHECK(r.request("GET / HTTP/1.1\r\nHost: 10.23.4.1:8080\r\nOrigin: http://evil.example\r\n\r\n").find("403 Error")!=std::string::npos);
 r.wifi.stop();r.net.fail_start=true;CHECK(!r.wifi.start({})&&!r.wifi.qr().matrix().valid()&&r.net.live==0);
}
void allocationFailures(){
 for(long after:{0,1}){Rig r;const auto baseline=draftor::memory::allocation_stats.live_allocations;
  draftor::memory::allocation_fail_after=after;CHECK(r.wifi.start({}));draftor::memory::allocation_fail_after=-1;
  CHECK(r.wifi.active()&&r.wifi.qr().status()==q::Status::AllocationFailed&&!r.wifi.qr().matrix().valid()&&r.net.live==0);
  key(r,19);CHECK(r.wifi.qr().page()==s3app::WifiQrPage::Manual);key(r,19);CHECK(r.wifi.qr().matrix().valid());
  draftor::memory::allocation_fail_after=after;key(r,43);draftor::memory::allocation_fail_after=-1;
  CHECK(!r.wifi.qr().matrix().valid()&&r.net.live==0);r.wifi.stop();
  CHECK(draftor::memory::allocation_stats.live_allocations==baseline);
 }
}
void routingSave(){
 Rig r;CHECK(r.app.commitText("QR save boundary",r.app.editStamp()));const auto id=r.app.session.currentId();
 s3hid::InputRoute route;CHECK(r.wifi.start({}));route.framePresented(r.app.focusToken());
 const auto focus=r.app.focusToken(),epoch=std::uint64_t(route.epoch());
 s3hid::Event queued{1,++r.now,43,0,true,route.epoch()};
 CHECK(route.accept(queued,r.now));key(r,43);route.framePresented(r.app.focusToken());
 CHECK(r.app.focusToken()==focus&&route.epoch()==epoch&&route.accept(queued,r.now)&&route.discarded==0);
 key(r,19);route.framePresented(r.app.focusToken());CHECK(r.app.focusToken()==focus&&route.epoch()==epoch);
 key(r,41);route.framePresented(r.app.focusToken());CHECK(!route.accept(queued,++r.now));
 const auto allocations=r.net.allocations;CHECK(r.app.requestSave(++r.now).status==ds::Status::Busy);
 key(r,67);CHECK(!r.wifi.panel()&&r.net.allocations==allocations); // QR cannot interrupt the writer.
 unsigned turns=0;while(r.app.persistence.backgroundSaveActive()||r.app.manualSavePending()||r.app.commandBarrier()){
  CHECK(++turns<4000);r.app.service(++r.now,false);
 }
 CHECK(!r.app.document.dirty()&&std::string_view(r.app.notice)=="Saved"&&r.app.session.currentId()==id);
 const auto saved=r.app.persistence.loadLane(id,ds::SaveLane::Manual);
 CHECK(saved.status==ds::Status::Ok&&std::string_view(saved.snapshot.text)=="QR save boundary");
 key(r,67);CHECK(r.wifi.panel()&&r.wifi.qr().matrix().valid());key(r,18);
}
void content(){
 char out[117];std::memset(out,0x55,sizeof(out));std::size_t length=99;
 CHECK(s3app::wifiQrContent("Draftor-test","12345678abcdef90",out+1,115,length)==q::Status::Ok);
 CHECK(std::string(out+1)=="WIFI:T:WPA;S:Draftor-test;P:12345678abcdef90;;");
 CHECK(s3app::wifiQrContent(R"(a\;:,"b)",R"(\;:,"abc)",out+1,115,length)==q::Status::Ok);
 CHECK(std::string(out+1)==R"(WIFI:T:WPA;S:a\\\;\:\,\"b;P:\\\;\:\,\"abc;;)");
 CHECK(s3app::wifiQrContent(std::string(32,';'),std::string(16,';'),out+1,115,length)==q::Status::Ok&&length==114);
 CHECK(out[0]==0x55&&out[116]==0x55);
 CHECK(s3app::wifiQrContent(std::string(32,';'),std::string(16,';'),out+1,114,length)==q::Status::WorkspaceTooSmall&&length==0);
 CHECK(s3app::wifiQrContent(std::string(33,'x'),"12345678",out,117,length)==q::Status::InvalidArgument);
 CHECK(s3app::wifiQrContent("valid",std::string(17,'x'),out,117,length)==q::Status::InvalidArgument);
 CHECK(s3app::wifiQrContent("valid","1234567",out,117,length)==q::Status::InvalidArgument);
 CHECK(s3app::wifiQrContent(std::string("a\0b",3),"12345678",out,117,length)==q::Status::InvalidArgument);
 CHECK(s3app::wifiQrContent("a\nb","12345678",out,117,length)==q::Status::InvalidArgument);
 Rig r;s3app::WifiQr qr(r.net);
 qr.show(s3app::WifiQrPage::Address,"","","http://192.168.4.1?token=SECRET");CHECK(qr.status()==q::Status::InvalidArgument&&!qr.matrix().valid()&&r.net.live==0);
 qr.show(s3app::WifiQrPage::Address,"","",std::string(64,'x'));CHECK(qr.status()==q::Status::InvalidArgument&&r.net.live==0);
}
void preview(Rig& r,const fs::path& dir,const std::string& name){
 SPIClass spi;ST7306OsptekLpm8 lcd(spi,12,11,10,9,4);
 const auto qr=r.wifi.qr().matrix();const auto geometry=q::fit(qr,s3app::WifiQrRect);
 const bool manual=r.wifi.qr().page()==s3app::WifiQrPage::Manual;
 auto label=[&](const char* text,int x,int y){
  CHECK(y>=0&&y+14<=400);if(!manual)CHECK(y+14<=s3app::WifiQrRect.y||y>=306);
  const std::string_view value(text);
  for(std::size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){
   const auto cp=s3app::display::scalar(value,at);const int width=s3app::display::advance(r.font,cp);
   if(x+width>294)break;const auto g=r.font.glyph(cp);
   for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)
    if(g.bits[row*g.stride+col/8]&(0x80>>(col%8)))lcd.pixel(x+g.bearing+col,y+row,ST7306OsptekLpm8::Color::Black);
   x+=width;
  }
 };
 const auto before=r.net.allocations;
 for(unsigned i=0;i<3;++i)s3app::paintWifiPanel(r.wifi,[&](int x,int y,int w,int h,bool ink){lcd.fillRect(x,y,w,h,ink?ST7306OsptekLpm8::Color::Black:ST7306OsptekLpm8::Color::White);},label);
 CHECK(r.net.allocations==before);
 if(!manual){
  CHECK(geometry.scale>=4);
  for(int y=0;y<geometry.pixels;++y)for(int x=0;x<geometry.pixels;++x){
   const int mx=x/geometry.scale-4,my=y/geometry.scale-4;
   const bool black=st7306_osptek_lpm8::readLogicalPixel(lcd.framebuffer(),x+geometry.x,y+geometry.y)==ST7306OsptekLpm8::Color::Black;
   CHECK(black==qr.module(mx,my));
  }
 }
 const auto image=dir/(name+".pgm");CHECK(!fs::exists(image));std::ofstream out(image,std::ios::binary);out<<"P5\n300 400\n255\n";
 for(int y=0;y<400;++y)for(int x=0;x<300;++x){const auto color=st7306_osptek_lpm8::readLogicalPixel(lcd.framebuffer(),x,y);CHECK(color==ST7306OsptekLpm8::Color::Black||color==ST7306OsptekLpm8::Color::White);out.put(color==ST7306OsptekLpm8::Color::Black?0:char(255));}
 CHECK(out.good());
 if(!manual){std::ofstream payload(dir/(name+".payload"),std::ios::binary);
  if(r.wifi.qr().page()==s3app::WifiQrPage::Address)payload<<r.wifi.address();
  else{char text[115];std::size_t n=0;CHECK(s3app::wifiQrContent(r.wifi.ssid(),r.wifi.password(),text,sizeof(text),n)==q::Status::Ok);payload.write(text,n);}
 }
}
void screens(){
 const char* env=std::getenv("QR1_PREVIEW_DIR");CHECK(env);fs::path dir=env;fs::create_directories(dir);
 for(bool vertical:{false,true}){Rig r;if(vertical){r.app.layoutKey({1,++r.now,61,0,true});r.app.layoutKey({1,++r.now,61,0,false});}
  const auto layout=r.app.layoutMode();const auto id=r.app.session.currentId();CHECK(r.wifi.start({}));
  const std::string prefix=vertical?"V-":"H-";preview(r,dir,prefix+"connect");key(r,43);preview(r,dir,prefix+"address");key(r,19);preview(r,dir,prefix+"manual");r.wifi.stop();
  CHECK(r.app.layoutMode()==layout&&r.app.session.currentId()==id);
 }
 for(unsigned kind=0;kind<3;++kind){Rig r;
  if(kind==0){r.net.ssid_value=std::string(32,';');r.net.password_value=std::string(16,';');}
  if(kind==1){r.net.ssid_value=R"(a\;:,"b)";r.net.password_value=R"(\;:,"abc)";}
  if(kind==2){r.net.ssid_value="Draftor-ABC123";r.net.password_value="fedcba9876543210";r.net.address_value="http://10.23.4.1:8080";}
  CHECK(r.wifi.start({}));preview(r,dir,"case-"+std::to_string(kind)+"-connect");key(r,43);preview(r,dir,"case-"+std::to_string(kind)+"-address");r.wifi.stop();
 }
}
int main(int argc,char** argv){try{CHECK(argc==2);const std::string c=argv[1];
 if(c=="lifecycle")lifecycle();else if(c=="allocation")allocationFailures();else if(c=="content")content();else if(c=="screen")screens();else if(c=="routing_save")routingSave();else CHECK(false);
 std::cout<<"PASS QR "<<c<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
