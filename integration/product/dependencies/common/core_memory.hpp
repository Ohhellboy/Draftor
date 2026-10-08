#pragma once
#include <cstdlib>
#include <new>
#include <limits>
#include <memory>
#include <string>
#include <vector>
namespace draftor::memory {
// Platform backend is selected by the build. No driver headers in this API.
struct AllocationStats {
  std::size_t calls=0, failures=0, requested_bytes=0, live_allocations=0;
};
extern long allocation_fail_after;
extern AllocationStats allocation_stats;
void* allocate(std::size_t n) noexcept;
void release(void* p) noexcept;
template<class T> struct Allocator {
  using value_type=T;
  using propagate_on_container_move_assignment=std::true_type;
  using is_always_equal=std::true_type;
  Allocator() noexcept=default;
  template<class U> Allocator(const Allocator<U>&) noexcept {}
  T* allocate(std::size_t n) {
    if(n>std::numeric_limits<std::size_t>::max()/sizeof(T)) throw std::bad_alloc();
    auto p=static_cast<T*>(draftor::memory::allocate(n*sizeof(T)));
    if(!p) throw std::bad_alloc();
    return p;
  }
  void deallocate(T* p,std::size_t) noexcept { release(p); }
};
template<class T,class U> bool operator==(Allocator<T>,Allocator<U>) {return true;}
template<class T,class U> bool operator!=(Allocator<T>,Allocator<U>) {return false;}
using String=std::basic_string<char,std::char_traits<char>,Allocator<char>>;
template<class T> using Vector=std::vector<T,Allocator<T>>;
template<class T> struct ArrayDeleter { void operator()(T* p)const noexcept {release(p);} };
template<class T> using ArrayPtr=std::unique_ptr<T[],ArrayDeleter<T>>;
template<class T> T* array(std::size_t n) noexcept {
  static_assert(std::is_trivially_destructible<T>::value,"array requires trivial destructor");
  if(n>std::numeric_limits<std::size_t>::max()/sizeof(T))return nullptr;
  auto p=static_cast<T*>(allocate(n*sizeof(T)));
  if(p)for(std::size_t i=0;i<n;++i)new(p+i) T{};
  return p;
}
}
