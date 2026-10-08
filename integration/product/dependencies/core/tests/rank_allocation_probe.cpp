#include "rank_allocation_probe.hpp"
#include <cstdlib>
#include <limits>
#include <new>

namespace {
bool armed=false,enabled=false;
std::size_t epoch=0,current=0,peak=0,allocations=0;
}
// Ordinary Host only: requested default-new bytes (including library control
// blocks), NOT malloc rounding/RSS/stack. Never interpose ASan allocations.
#ifndef HANIME_PROBE_DISABLED
namespace {
struct alignas(std::max_align_t) Header {std::size_t bytes,epoch;};
bool fail_next=false;
void* allocate(std::size_t n) {
  if(fail_next){fail_next=false; throw std::bad_alloc();}
  if(n>std::numeric_limits<std::size_t>::max()-sizeof(Header))throw std::bad_alloc();
  auto* p=static_cast<Header*>(std::malloc(sizeof(Header)+(n ? n : 1)));
  if(!p)throw std::bad_alloc();
  p->bytes=n; p->epoch=enabled ? epoch : 0;
  if(enabled){current+=n; if(current>peak)peak=current; ++allocations;}
  return p+1;
}
void deallocate(void* ptr) noexcept {
  if(!ptr)return;
  auto* p=static_cast<Header*>(ptr)-1;
  if(p->epoch && p->epoch==epoch)current-=p->bytes;
  std::free(p);
}
}
void* operator new(std::size_t n) {return allocate(n);}
void* operator new[](std::size_t n) {return allocate(n);}
void operator delete(void* p) noexcept {deallocate(p);}
void operator delete[](void* p) noexcept {deallocate(p);}
void operator delete(void* p,std::size_t) noexcept {deallocate(p);}
void operator delete[](void* p,std::size_t) noexcept {deallocate(p);}
#endif
namespace rank_probe {
bool fail_next_allocation(){
#ifdef HANIME_PROBE_DISABLED
  return false;
#else
  fail_next=true; return true;
#endif
}
void start(){++epoch; current=peak=allocations=0; armed=true;}
Statistics stop(){armed=false; enabled=false;
#ifdef HANIME_PROBE_DISABLED
  return {0,0,0,false};
#else
  return {current,peak,allocations,true};
#endif
}
Capture::Capture():previous(enabled){enabled=armed;}
Capture::~Capture(){enabled=previous;}
}
