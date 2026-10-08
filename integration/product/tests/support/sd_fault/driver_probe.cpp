#include "sd_diskio.h"
#include "hanime_mount_budget.h"
#include "hanime_sd_trace.h"
#include "tf_mount_recovery.hpp"
#include <iostream>
#include <stdexcept>
extern "C" {
#include "diskio_impl.h"
#include "esp_vfs_fat.h"
}
namespace sd_fault {uint32_t ms=0,transfers=0;uint8_t response=0xff;int cs=HIGH;}
namespace hanime_sd_trace {bool enabled=false;Stats stats;}
static const ff_diskio_impl_t* disk=nullptr;
static int registrations=0,vfs=0,formats=0;
extern "C" {
int ff_diskio_get_drive(uint8_t* drive){*drive=disk?0xff:0;return 0;}
void ff_diskio_register(uint8_t,const ff_diskio_impl_t* impl){disk=impl;registrations+=impl?1:-1;}
int esp_vfs_fat_register(const char*,const char*,unsigned,FATFS** out){static FATFS fs;*out=&fs;vfs=1;return 0;}
int esp_vfs_fat_unregister_path(const char*){vfs=0;return 0;}
FRESULT f_mount(FATFS* fs,const char*,BYTE){return fs&&disk->init(0)?3:0;}
FRESULT f_mkfs(const char*,const MKFS_PARM*,void*,UINT){++formats;return 1;}
char CRC7(const char*,int){return 0;}
unsigned short CRC16(const char*,int){return 0;}
}
extern bool sdWait(uint8_t,int);
extern bool sdReadBytes(uint8_t,char*,int);
extern char sdCommand(uint8_t,char,unsigned int,unsigned int*);
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
int main(){try{
 SPIClass spi;bool all=true;
 for(unsigned fault=0;fault<5;++fault){
  sd_fault::ms=fault==4?UINT32_MAX-30:0;sd_fault::transfers=0;sd_fault::response=(fault==1||fault==3)?0:0xff;
  const uint32_t start=millis();hanime_mount::begin(75);
  const auto id=sdcard_init(15,&spi,1000000);CHECK(id==0);
  bool failed=false;
  if(fault<2)failed=!sdcard_mount(id,"/sd",7,false);
  else if(fault==2){char buffer[16];failed=!sdReadBytes(id,buffer,16);}
  else if(fault==3)failed=!sdWait(id,500);
  else failed=sdCommand(id,0,0,nullptr)!=1;
  sdcard_unmount(id);sdcard_uninit(id);
  const uint32_t elapsed=millis()-start;const auto b=hanime_mount::finish();
  const bool bounded=elapsed<=85;
  std::cout<<"SD_FAULT id="<<fault<<" elapsed_model_ms="<<elapsed<<" limit_ms=75 bounded="<<bounded<<" failed="<<failed<<" timed_out="<<b.timed_out<<" cs_high="<<(sd_fault::cs==HIGH)<<" registrations="<<registrations<<" vfs="<<vfs<<" formats="<<formats<<'\n';
  CHECK(failed&&registrations==0&&vfs==0&&formats==0&&sd_fault::cs==HIGH&&!hanime_mount::budget.active);
  all=all&&bounded;
 }
 // Once disabled, a normal wait still follows the original runtime protocol.
 sd_fault::ms=0;sd_fault::response=0;auto id=sdcard_init(15,&spi,1000000);
 const auto start=millis();CHECK(!sdWait(id,500));CHECK(millis()-start==500);
 sdcard_uninit(id);CHECK(!disk);
 if(!all){std::cerr<<"FAIL finite initialization budget not enforced\n";return 1;}
 std::cout<<"PASS real SD protocol loops under synthetic no-token/busy faults; runtime guard disabled; no hardware reliability claim\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
