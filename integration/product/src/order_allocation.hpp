#pragma once
#include <cstddef>
#include <cstdint>
namespace ime_device {
struct OrderAllocationRecord {std::uintptr_t address=0;std::size_t bytes=0;uint32_t kind=0,region=0;};
struct OrderAllocationSnapshot {uint32_t generation=0,count=0,overflow=0;OrderAllocationRecord records[8]{};};
// Bounded observations of actual C++ allocations made by the constructing task.
// Region: 1 internal, 2 PSRAM, 0 unknown. Kind bits: 1 scalar, 2 array, 4 nothrow.
// No ownership, no allocation policy changes, no I/O; records never dereference old pointers.
class OrderAllocationScope {
 public:OrderAllocationScope();~OrderAllocationScope();
 OrderAllocationScope(const OrderAllocationScope&)=delete;
 OrderAllocationScope& operator=(const OrderAllocationScope&)=delete;
};
const OrderAllocationSnapshot& orderAllocationSnapshot();
}
