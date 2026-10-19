#pragma once
// Isolated private prototype adapter. Not a public Core capability or policy.
#include "hanime/paged_shared_resource.hpp"
#ifndef HANIME_STRUCTURAL_HOST_OBSERVE
#define HANIME_STRUCTURAL_HOST_OBSERVE 0
#endif
namespace hanime::detail {
struct SharedStructuralStats {
  std::size_t table_bytes{0}, table_entries{0}, initialized_entries{0};
  std::uint64_t initialization_advances{0};
  WorkCost initialization_cost;
  std::uint64_t initialization_parse_bytes{0}, initialization_store_bytes{0};
  std::uint64_t runtime_parse_bytes{0}, runtime_records{0};
  std::uint64_t runtime_locator_copy_bytes{0};
  std::uint64_t layout_locator_hits{0}, resident_locator_hits{0};
#if HANIME_STRUCTURAL_HOST_OBSERVE
  std::uint64_t short_control_parse_bytes{0}, short_inline_records{0}, short_external_records{0};
  std::uint64_t short_handoffs{0}, short_payload_copy_bytes{0};
#endif
  std::uint64_t allocation_attempts{0}, allocation_failures{0};
};
struct SharedStructuralAccess {
  using Allocate = void* (*)(std::size_t) noexcept;
  using Release = void (*)(void*) noexcept;
  // Caller opts in immediately after trusted original-layout open, before
  // activation is exposed. This call starts no query and never reads payload.
  // The old activation token is retired; Ready is withheld until every copied
  // descriptor has been authenticated, parsed and checked under this budget.
  static ValidationStepResult advance_resident(PagedSharedResource&, const WorkBudget&,
                                               Allocate, Release);
  static SharedStructuralStats stats(const PagedSharedResource&);
  static const void* allocation(const PagedSharedResource&);
};
} // namespace hanime::detail
