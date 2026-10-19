#pragma once
// Private experiment integration seam. Not an installed Core API.
#include "hanime/base_order.hpp"
namespace hanime::detail {
struct BlockInitializationStats {
  WorkCost auth_cost;
  std::uint64_t local_work{0}, allocations{0}, releases{0}, initialization_calls{0};
  std::size_t nav_bytes{0}, workspace_bytes{0}, private_owner_bytes{0}, cursor_bytes{0};
  std::uint64_t block_loads{0}, block_reuses{0}, prefix_hits{0}, prefix_rejects{0}, generation{0};
  bool initialized{false}, cancelled{false};
};
struct BaseOrderBlockAccess {
  static bool is_block(const BaseOrderIndex&);
  static bool cursor_descriptor(const BaseOrderIndex&, const BaseOrderCursor&,
      std::uint32_t& start, std::uint32_t& count, std::uint32_t& paths);
  static BaseOrderStep advance_initialization(BaseOrderIndex&, const WorkBudget&,
      void* (*allocate_psram)(std::size_t), void (*release_psram)(void*));
  static void cancel_initialization(BaseOrderIndex&);
  static BlockInitializationStats initialization_stats(const BaseOrderIndex&);
};
}
