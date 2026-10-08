#include "core_memory.hpp"
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
#endif
namespace {
void* rawAllocate(std::size_t n) noexcept {
#ifdef ARDUINO_ARCH_ESP32
return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
return std::malloc(n);
#endif
}
void rawRelease(void* p) noexcept {
#ifdef ARDUINO_ARCH_ESP32
heap_caps_free(p);
#else
std::free(p);
#endif
}
}
namespace draftor::memory {
long allocation_fail_after=-1;
AllocationStats allocation_stats{};
void* allocate(std::size_t n) noexcept {
  if(n==0)return nullptr;
  ++allocation_stats.calls;
  allocation_stats.requested_bytes+=n;
  if(allocation_fail_after==0){++allocation_stats.failures;return nullptr;}
  if(allocation_fail_after>0)--allocation_fail_after;
  void* p=rawAllocate(n);
  if(p)++allocation_stats.live_allocations;else ++allocation_stats.failures;
  return p;
}
void release(void* p) noexcept {
  if(!p)return;
  --allocation_stats.live_allocations;
  rawRelease(p);
}
}
