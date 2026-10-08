#pragma once
#include "hanime_extension/storage.hpp"
#include <array>
#include <cstdlib>
#include <cstdio>
#include <new>

// Test-only, instance-owned storage recorder. No global allocation override.
struct TrackedBackend {
  using Role=hanime_extension::StorageRole;
  struct Entry {
    void* returned{nullptr};void* base{nullptr};Role role{};
    std::size_t bytes{0},alignment{0};bool live{false};
  };
  std::array<Entry,256> entries{};
  std::size_t calls{0},successes{0},frees{0},active{0},peak_bytes{0},live_bytes{0};
  std::size_t fail_at{SIZE_MAX};bool throw_failure{false},misalign{false},bad_free{false};
  std::uint32_t id{0x4d454d31};
  hanime_extension::StorageBackend view() {return {this,&allocate,&deallocate,id};}
  ~TrackedBackend() {
    if(active||bad_free){std::fputs("MEM1 backend unpaired/mismatched release\n",stderr);std::abort();}
  }
  static void* allocate(void* context,Role role,std::size_t bytes,std::size_t alignment) {
    auto& self=*static_cast<TrackedBackend*>(context);
    const auto call=self.calls++;
    if(call==self.fail_at&&!self.misalign) {
      if(self.throw_failure)throw std::bad_alloc();
      return nullptr;
    }
    if(!bytes||!alignment||(alignment&(alignment-1))||bytes>SIZE_MAX-alignment)throw std::bad_alloc();
    void* base=std::malloc(bytes+alignment);if(!base)throw std::bad_alloc();
    const auto aligned=(reinterpret_cast<std::uintptr_t>(base)+alignment-1)&~(alignment-1);
    void* pointer=reinterpret_cast<void*>(aligned+(self.misalign&&call==self.fail_at?1:0));
    auto* entry=static_cast<Entry*>(nullptr);
    for(auto& candidate:self.entries)if(!candidate.live){entry=&candidate;break;}
    if(!entry){std::free(base);throw std::bad_alloc();}
    *entry={pointer,base,role,bytes,alignment,true};++self.successes;++self.active;self.live_bytes+=bytes;
    if(self.live_bytes>self.peak_bytes)self.peak_bytes=self.live_bytes;
    return pointer;
  }
  static void deallocate(void* context,Role role,void* pointer,std::size_t bytes,std::size_t alignment) noexcept {
    auto& self=*static_cast<TrackedBackend*>(context);
    for(auto& entry:self.entries)if(entry.live&&entry.returned==pointer) {
      if(entry.role!=role||entry.bytes!=bytes||entry.alignment!=alignment)self.bad_free=true;
      std::free(entry.base);entry.live=false;++self.frees;--self.active;self.live_bytes-=entry.bytes;return;
    }
    self.bad_free=true;
  }
  bool owns(const hanime_extension::StorageAllocation& allocation) const {
    if(allocation.backend_id!=id)return false;
    for(const auto& entry:entries)
      if(entry.live&&entry.role==allocation.role&&entry.bytes==allocation.bytes&&entry.alignment==allocation.alignment&&
         reinterpret_cast<std::uintptr_t>(entry.returned)%allocation.alignment==0)return true;
    return false;
  }
};
