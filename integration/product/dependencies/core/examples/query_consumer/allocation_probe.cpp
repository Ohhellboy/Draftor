// Test-only default new/new[] request meter, not part of HanIME::Core.
// Aligned allocation, malloc/libc, allocator metadata and stacks are excluded.
#include "allocation_probe.hpp"
#include <cstdlib>
#include <new>
#include <algorithm>
namespace {
bool tracking=false;
std::size_t current=0,peak=0,calls=0;
#ifndef QUERY_PROBE_DISABLED
struct alignas(std::max_align_t) Header {std::size_t bytes;bool tracked;};
void* allocate(std::size_t n) {
  if(n>static_cast<std::size_t>(-1)-sizeof(Header))throw std::bad_alloc();
  auto* h=static_cast<Header*>(std::malloc(sizeof(Header)+n));
  if(!h)throw std::bad_alloc();
  h->bytes=n;h->tracked=tracking;
  if(tracking){current+=n;peak=std::max(peak,current);++calls;}
  return h+1;
}
void free_memory(void* p) noexcept {
  if(!p)return;
  auto* h=static_cast<Header*>(p)-1;if(h->tracked)current-=h->bytes;
  std::free(h);
}
#endif
}
#ifndef QUERY_PROBE_DISABLED
void* operator new(std::size_t n) {return allocate(n);}
void* operator new[](std::size_t n) {return allocate(n);}
void operator delete(void* p) noexcept {free_memory(p);}
void operator delete[](void* p) noexcept {free_memory(p);}
void operator delete(void* p,std::size_t) noexcept {free_memory(p);}
void operator delete[](void* p,std::size_t) noexcept {free_memory(p);}
#endif
void query_probe::start() {if(current)throw std::bad_alloc();tracking=true;peak=calls=0;}
query_probe::Result query_probe::stop() {
  tracking=false;
#ifdef QUERY_PROBE_DISABLED
  return {current,peak,calls,false};
#else
  return {current,peak,calls,true};
#endif
}
