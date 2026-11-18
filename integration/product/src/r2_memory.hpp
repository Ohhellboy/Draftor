#pragma once
#include "core_memory.hpp"
#include <new>
#include <limits>
#include <vector>
namespace r2 {
// Product-local allocation; never replace global new or the SDK allocator.
struct ExternalObject {
 static void* operator new(size_t n){auto p=draftor::memory::allocate(n);if(!p)throw std::bad_alloc();return p;}
 static void operator delete(void* p) noexcept {draftor::memory::release(p);}
};
template<class T> struct ExternalAllocator {
 using value_type=T;
 ExternalAllocator()=default;
 template<class U> ExternalAllocator(const ExternalAllocator<U>&) noexcept {}
 T* allocate(size_t n){if(n>std::numeric_limits<size_t>::max()/sizeof(T))throw std::bad_alloc();auto p=draftor::memory::allocate(n*sizeof(T));if(!p)throw std::bad_alloc();return static_cast<T*>(p);}
 void deallocate(T* p,size_t) noexcept {draftor::memory::release(p);}
 template<class U> bool operator==(const ExternalAllocator<U>&) const noexcept {return true;}
 template<class U> bool operator!=(const ExternalAllocator<U>&) const noexcept {return false;}
};
template<class T> using Vector=std::vector<T,ExternalAllocator<T>>;
}
