#pragma once
#include "core_memory.hpp"
#include <new>
#include <utility>

namespace s3app {
// Owner-loop only. Publish after construction succeeds; preserve the existing
// owner on repeated calls. A throwing member is unwound before raw storage is
// returned. No second owner, retry queue, or retained partial construction.
template<class T, class... Args>
bool ensureUsbRuntime(T*& owner, Args&&... args) {
 if(owner)return true;
 void* storage=draftor::memory::allocate(sizeof(T));
 if(!storage)return false;
 try {owner=new(storage) T(std::forward<Args>(args)...);return true;}
 catch(const std::bad_alloc&) {draftor::memory::release(storage);return false;}
 catch(...) {draftor::memory::release(storage);throw;}
}
}
