#pragma once
#include <cstddef>
#include <cstdint>
struct SPISettings { SPISettings(std::uint32_t,int,int){} };
struct SPIClass {
  void begin(int,int,int,int);
  void beginTransaction(SPISettings);
  void writeBytes(const std::uint8_t*,std::size_t);
  void endTransaction();
};
