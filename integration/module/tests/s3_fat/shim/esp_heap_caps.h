#pragma once
#include <cstdlib>
#include <cstddef>
inline std::size_t host_heap_live=0, host_heap_peak=0;
constexpr int MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2;
inline void* heap_caps_calloc(std::size_t n, std::size_t size, int) {
 auto* p=static_cast<std::size_t*>(std::calloc(1,sizeof(std::size_t)+n*size));
 if(!p) return nullptr; *p=n*size; host_heap_live+=*p;
 if(host_heap_live>host_heap_peak)host_heap_peak=host_heap_live;return p+1;
}
inline void heap_caps_free(void* memory) {if(memory){auto* p=static_cast<std::size_t*>(memory)-1;host_heap_live-=*p;std::free(p);}}
