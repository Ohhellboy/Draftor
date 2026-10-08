// Execute the product driver against a retained-panel timing model. This is a
// wire-sequence test; it cannot certify voltage levels or physical LCD recovery.
#include "ST7306_OsptekLpm8.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::array<int,32> level{},output{};
unsigned long now=0,asserted=0,released=0,sleep_out=0;
unsigned resets=0,frames=0;bool held=false,reset_done=false,transaction=false;
bool panel_awake=false,reset_was_awake=false;std::uint8_t command=0;
std::vector<std::uint8_t> commands;
}
void pinMode(std::uint8_t pin,int mode){output[pin]=mode;}
void digitalWrite(std::uint8_t pin,int value){
  check(output[pin]==OUTPUT,"write before OUTPUT");
  if(pin==4){
    if(value==LOW&&!held){held=true;asserted=now;reset_done=false;reset_was_awake=panel_awake;}
    if(value==HIGH&&held){
      check(now-asserted>=1,"RSTB low shorter than 1ms");
      held=false;released=now;reset_done=true;++resets;
      panel_awake=false;command=0;
    }
  }
  level[pin]=value;
}
void delay(unsigned long ms){now+=ms;}
void SPIClass::begin(int sck,int miso,int mosi,int cs){
  check(sck==12&&miso==-1&&mosi==11&&cs==10,"bus pins changed");
  check(level[10]==HIGH,"CS not parked at bus start");
}
void SPIClass::beginTransaction(SPISettings){check(!transaction,"nested SPI transaction");transaction=true;}
void SPIClass::endTransaction(){check(level[10]==HIGH,"CS left asserted");transaction=false;}
void SPIClass::writeBytes(const std::uint8_t* bytes,std::size_t size){
  check(transaction&&level[10]==LOW&&level[4]==HIGH,"invalid transfer pins");
  check(reset_done&&now-released>=5,"command before reset settle");
  if(level[9]==LOW){
    check(size==1,"unexpected command length");command=bytes[0];commands.push_back(command);
    if(command==0x11){
      check(now-released>=(reset_was_awake?120UL:5UL),"Sleep Out before warm reset cancel (120ms)");
      sleep_out=now;panel_awake=true;
    }
    if(command==0x29)check(panel_awake&&now-sleep_out>=120,"Display On before Sleep Out settle");
  }else if(command==0x2c){check(size==ST7306OsptekLpm8::BufferSize,"incomplete frame");++frames;}
}
int main(){try{
  SPIClass spi;std::vector<std::uint8_t> reference;
  for(const char* cause:{"power_on","software_restart_retained_panel","panic_retained_panel","physical_RST_retained_panel"}){
    // Only MCU driver/state re-created; panel state survives until RSTB pulse.
    level[10]=LOW;output.fill(0);reset_done=false;commands.clear();
    ST7306OsptekLpm8 driver(spi,12,11,10,9,4);
    const auto before=resets;check(!driver.update(),"update before begin accepted");
    check(driver.begin()&&resets==before+1,"missing unconditional hardware reset");
    driver.fill(ST7306OsptekLpm8::Color::White);check(driver.update(),"initial frame rejected");
    if(reference.empty())reference=commands;else check(commands==reference,"boot path changes init sequence");
    check(panel_awake,"panel not reinitialized");
    std::cout<<"PASS "<<cause<<" reset_low_ms=10 reset_release_to_sleep_out_ms="<<sleep_out-released<<" frames="<<frames<<'\n';
  }
  std::cout<<"Host driver timing model only; physical LCD recovery NOT_RUN\n";
  return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
