#pragma once

#include "hanime_extension/storage.hpp"
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace hanime_extension::detail {
inline void validate_storage_backend(const StorageBackend& backend) {
  const bool standard = !backend.context && !backend.allocate && !backend.deallocate && !backend.id;
  const bool custom = backend.allocate && backend.deallocate && backend.id;
  if (!standard && !custom) throw std::invalid_argument("Incomplete member storage backend");
}

// Only the four fixed member arrays use this owner. No growth, propagation,
// copy, move or allocator swap is possible. Composition swaps Data pointers.
template<class T> class FixedMemberArray {
 public:
  FixedMemberArray(std::size_t count, StorageRole role, StorageBackend backend)
      : backend_(backend), count_(count), role_(role) {
    static_assert(std::is_nothrow_destructible<T>::value, "Member destruction must not throw");
    validate_storage_backend(backend_);
    if (!count_ || count_ > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::length_error("Invalid fixed member array length");
    if (backend_.allocate) {
      data_ = static_cast<T*>(backend_.allocate(backend_.context, role_, bytes(), alignof(T)));
      if (!data_) throw std::bad_alloc();
      if (reinterpret_cast<std::uintptr_t>(data_) % alignof(T)) {
        release_storage();
        throw std::bad_alloc();
      }
    } else {
      data_ = std::allocator<T>{}.allocate(count_);
    }
    std::size_t initialized = 0;
    try {
      for (; initialized < count_; ++initialized) ::new (static_cast<void*>(data_ + initialized)) T();
    } catch (...) {
      while (initialized) data_[--initialized].~T();
      release_storage();
      throw;
    }
  }
  ~FixedMemberArray() {
    for (std::size_t i = count_; i; --i) data_[i - 1].~T();
    release_storage();
  }
  FixedMemberArray(const FixedMemberArray&) = delete;
  FixedMemberArray& operator=(const FixedMemberArray&) = delete;
  FixedMemberArray(FixedMemberArray&&) = delete;
  FixedMemberArray& operator=(FixedMemberArray&&) = delete;
  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }
  T* begin() noexcept { return data_; }
  const T* begin() const noexcept { return data_; }
  T* end() noexcept { return data_ + count_; }
  const T* end() const noexcept { return data_ + count_; }
  T& operator[](std::size_t index) noexcept { return data_[index]; }
  const T& operator[](std::size_t index) const noexcept { return data_[index]; }
  std::size_t size() const noexcept { return count_; }
  std::size_t capacity() const noexcept { return count_; }
  StorageAllocation allocation() const noexcept { return {role_, count_, bytes(), alignof(T), backend_.id}; }
 private:
  std::size_t bytes() const noexcept { return count_ * sizeof(T); }
  void release_storage() noexcept {
    if (backend_.deallocate)
      backend_.deallocate(backend_.context, role_, data_, bytes(), alignof(T));
    else
      std::allocator<T>{}.deallocate(data_, count_);
    data_ = nullptr;
  }
  StorageBackend backend_;
  T* data_{nullptr};
  std::size_t count_;
  StorageRole role_;
};
} // namespace hanime_extension::detail
