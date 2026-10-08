#pragma once
// Device-owned initialization only. Normal reads/writes run with active=false.
// Cooperative checks bound protocol retries; a single SPI/VFS/RTOS call cannot
// be preempted here. Preserve that measured overrun separately.
#include <Arduino.h>
#include <algorithm>
#include <cstdint>
namespace hanime_mount {
struct Budget {
  uint32_t start=0,limit=0;
  bool active=false,timed_out=false;
  const char* stage="not_started";
  const char* failure=nullptr;
  int mount_result=-1;
};
inline Budget budget;
inline void begin(uint32_t duration) { budget={millis(),duration,true,false,"spi_begin",nullptr,-1}; }
inline bool expired() {
  if(budget.active && uint32_t(millis()-budget.start)>=budget.limit)budget.timed_out=true;
  return budget.active && budget.timed_out;
}
inline void stage(const char* value) { if(budget.active)budget.stage=value; }
inline void fail(const char* value) { if(budget.active&&!budget.failure)budget.failure=value; }
inline void pause(uint32_t duration) {
  if(budget.active) {
    const auto elapsed=uint32_t(millis()-budget.start);
    if(budget.timed_out||elapsed>=budget.limit) { budget.timed_out=true;return; }
    duration=std::min(duration,budget.limit-elapsed);
  }
  delay(duration);
}
inline Budget finish() { const auto result=budget;budget.active=false;return result; }
}
