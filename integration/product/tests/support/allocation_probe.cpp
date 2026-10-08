#include <cstdlib>
#include <new>
bool fail_next_new=false;
void* operator new(std::size_t n){if(fail_next_new){fail_next_new=false;throw std::bad_alloc();}if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
// Keep nothrow allocations in the same malloc/free family and failure hook.
void* operator new(std::size_t n,const std::nothrow_t&) noexcept{
 try{return ::operator new(n);}catch(const std::bad_alloc&){return nullptr;}
}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept{
 try{return ::operator new[](n);}catch(const std::bad_alloc&){return nullptr;}
}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void operator delete(void* p,const std::nothrow_t&) noexcept{std::free(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept{std::free(p);}
