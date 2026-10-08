#pragma once
#include <cstdint>
constexpr int OUTPUT=1,HIGH=1,LOW=0,MSBFIRST=1,SPI_MODE0=0;
void pinMode(std::uint8_t,int);
void digitalWrite(std::uint8_t,int);
void delay(unsigned long);
