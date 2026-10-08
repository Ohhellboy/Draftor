#include "allocation_probe.hpp"
#include <cstdlib>
#include <new>
#include <cstdint>
#ifndef EXTENSION_PROBE_DISABLED
namespace {
struct alignas(std::max_align_t) Header { std::size_t n; void* base; };
allocation_probe::Stats counters{};
std::size_t failure_after=SIZE_MAX;
void* allocate(std::size_t n,std::size_t alignment) {
  if (failure_after!=SIZE_MAX) { if (!failure_after) throw std::bad_alloc(); --failure_after; }
  if (n>SIZE_MAX-alignment-sizeof(Header)) throw std::bad_alloc();
  void* base=std::malloc(n+alignment+sizeof(Header)); if (!base) throw std::bad_alloc();
  auto v=(reinterpret_cast<std::uintptr_t>(base)+sizeof(Header)+alignment-1)&~(alignment-1);
  auto* h=reinterpret_cast<Header*>(v)-1; h->n=n; h->base=base;
  counters.live+=n; if(counters.live>counters.peak)counters.peak=counters.live;
  counters.requested+=n; ++counters.allocations; return reinterpret_cast<void*>(v);
}
void release(void* p) noexcept { if(p) {auto* h=static_cast<Header*>(p)-1;counters.live-=h->n;std::free(h->base);} }
}
void* operator new(std::size_t n) {return allocate(n,alignof(std::max_align_t));}
void* operator new[](std::size_t n) {return allocate(n,alignof(std::max_align_t));}
void operator delete(void* p) noexcept {release(p);}
void operator delete[](void* p) noexcept {release(p);}
void operator delete(void* p,std::size_t) noexcept {release(p);}
void operator delete[](void* p,std::size_t) noexcept {release(p);}
void* operator new(std::size_t n,std::align_val_t a) {return allocate(n,static_cast<std::size_t>(a));}
void* operator new[](std::size_t n,std::align_val_t a) {return allocate(n,static_cast<std::size_t>(a));}
void operator delete(void* p,std::align_val_t) noexcept {release(p);}
void operator delete[](void* p,std::align_val_t) noexcept {release(p);}
#endif
namespace allocation_probe {
bool fail_after(std::size_t n) {
#ifndef EXTENSION_PROBE_DISABLED
  failure_after=n;return true;
#else
  (void)n;return false;
#endif
}
void clear_failure() {
#ifndef EXTENSION_PROBE_DISABLED
  failure_after=SIZE_MAX;
#endif
}
void reset_peak() {
#ifndef EXTENSION_PROBE_DISABLED
  counters.peak=counters.live; counters.requested=0; counters.allocations=0;
#endif
}
Stats stats() {
#ifndef EXTENSION_PROBE_DISABLED
  return counters;
#else
  return {};
#endif
}
}
