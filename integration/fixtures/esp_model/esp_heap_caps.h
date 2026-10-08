#pragma once
#include <cstddef>
constexpr unsigned MALLOC_CAP_SPIRAM=0x400,MALLOC_CAP_8BIT=4;
void* heap_caps_aligned_alloc(std::size_t,std::size_t,unsigned);
void heap_caps_free(void*);
