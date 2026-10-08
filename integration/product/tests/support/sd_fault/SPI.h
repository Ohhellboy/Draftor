#pragma once
#include "Arduino.h"
constexpr int MSBFIRST=1,SPI_MODE0=0;
struct SPISettings {SPISettings(int,int,int){}};
class SPIClass {
 public:
 void beginTransaction(SPISettings){} void endTransaction(){}
 uint8_t transfer(uint8_t){++sd_fault::ms;++sd_fault::transfers;return sd_fault::response;}
 uint16_t transfer16(uint16_t){return uint16_t(transfer(0))*257;}
 uint32_t transfer32(uint32_t){return uint32_t(transfer(0))*0x01010101;}
 void write(uint8_t value){transfer(value);}
 void write16(uint16_t value){transfer16(value);}
 void writeBytes(const uint8_t*,size_t n){sd_fault::ms+=uint32_t(n);sd_fault::transfers+=uint32_t(n);}
 void transferBytes(const uint8_t*,uint8_t* dst,size_t n){for(size_t i=0;i<n;++i)dst[i]=transfer(0xff);}
 void* bus(){return nullptr;} uint32_t getClockDivider(){return 1;}
};
inline uint32_t spiClockDivToFrequency(void*,uint32_t){return 400000;}
