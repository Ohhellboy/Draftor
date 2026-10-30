#pragma once
#include <cstddef>
#include <cstdint>
struct SPISettings {
  std::uint32_t hz;int order,mode;
  SPISettings(std::uint32_t h,int o,int m):hz(h),order(o),mode(m){}
};
struct SPIClass {
  bool begin(int,int,int,int);
  void beginTransaction(SPISettings);
  void writeBytes(const std::uint8_t*,std::size_t);
  void endTransaction();
};
