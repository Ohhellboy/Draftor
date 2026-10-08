#pragma once
#include <cstdlib>
#include <cstddef>
#include <limits>
#include <stdexcept>
constexpr int MALLOC_CAP_SPIRAM=0x400,MALLOC_CAP_8BIT=4;
inline std::size_t available=0,largest=0,live=0,allocations=0,releases=0,queries=0,extra_consumption=0;
inline bool deny=false;
inline void require_caps(int caps){if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))std::abort();}
inline std::size_t heap_caps_get_free_size(int caps){require_caps(caps);++queries;return available;}
inline std::size_t heap_caps_get_largest_free_block(int caps){require_caps(caps);++queries;return largest;}
inline void* heap_caps_calloc(std::size_t n,std::size_t size,int caps){
 require_caps(caps);++allocations;
 if(deny||!n||size>std::numeric_limits<std::size_t>::max()/n)return nullptr;
 const auto bytes=n*size;if(bytes>available||bytes>largest||bytes>1024*1024)return nullptr;
 auto* p=static_cast<std::size_t*>(std::calloc(1,sizeof(std::size_t)+bytes));if(!p)return nullptr;
 *p=bytes;live+=bytes;available-=bytes;available=extra_consumption>available?0:available-extra_consumption;return p+1;
}
inline void heap_caps_free(void* ptr){if(!ptr)return;auto* p=static_cast<std::size_t*>(ptr)-1;live-=*p;available+=*p;++releases;std::free(p);}
