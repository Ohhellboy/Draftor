#pragma once

#include <cstddef>
#include <cstdint>

namespace hanime_extension {
// Stable allocation roles. A/B alternate as live/stage without moving ownership.
enum class StorageRole : std::uint8_t {
  CompositionMembersA,
  CompositionMembersB,
  CompositionPlan,
  SubmissionCapture
};

// An empty backend selects the standard heap. A custom backend must provide
// both callbacks and a nonzero diagnostic ID. This descriptor is copied;
// context is borrowed and must outlive every array allocated through it.
// allocate may return null or throw. deallocate must not throw and must release
// the exact pointer with the same role/bytes/alignment and backend context.
struct StorageBackend {
  using Allocate = void* (*)(void* context, StorageRole, std::size_t bytes,
                            std::size_t alignment);
  using Deallocate = void (*)(void* context, StorageRole, void* pointer,
                              std::size_t bytes, std::size_t alignment) noexcept;
  void* context{nullptr};
  Allocate allocate{nullptr};
  Deallocate deallocate{nullptr};
  std::uint32_t id{0}; // Diagnostic label, never selection/resource authority.
};

struct StorageAllocation {
  StorageRole role{StorageRole::CompositionMembersA};
  std::size_t capacity{0}, bytes{0}, alignment{0};
  std::uint32_t backend_id{0}; // 0 is the default standard heap.
};
} // namespace hanime_extension
