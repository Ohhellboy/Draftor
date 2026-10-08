#pragma once
#include "hanime/exact_ranker.hpp"
#include "hanime/verified_resource.hpp"

namespace hanime {
// Optional immutable BaseOnly rule-1 derived index, never a learning identity.
// The original raw query remains available and retains every mapping path.
enum class BaseOrderStatus { Pending, Ready, NotFound, Stale, Error, InvalidRequest };
struct BaseOrderStep {
  BaseOrderStatus status{BaseOrderStatus::Stale};
  WorkCost cost;
  RankItem item;
};
// Per-index cumulative diagnostics. Member cost excludes its nested item read;
// categories may be added, unlike caller-stage/physical-role observations.
struct BaseOrderLocateStats {
  WorkCost directory, items, members;
  std::uint64_t hits{0}, misses{0}, evictions{0};
  std::size_t capacity{0}, resident{0}, entry_bytes{0}, pool_bytes{0};
};
// EXPERIMENTAL P3: one caller-owned chain, reset at every planning lifecycle
// boundary. This is an authenticated search hint, never a result or a ticket.
class BaseOrderPrefixState {
 public:
  BaseOrderPrefixState() = default;
  BaseOrderPrefixState(const BaseOrderPrefixState&) = delete;
  BaseOrderPrefixState& operator=(const BaseOrderPrefixState&) = delete;
  void reset() { identity_.reset(); size_ = 0; page_ = level_ = 0; }
 private:
  friend class BaseOrderIndex;
  std::weak_ptr<const int> identity_;
  std::array<char,128> code_{};
  std::uint32_t size_{0}, page_{0}, level_{0};
};
class BaseOrderCursor {
 public:
  BaseOrderCursor();
  ~BaseOrderCursor();
  BaseOrderCursor(const BaseOrderCursor&) = delete;
  BaseOrderCursor& operator=(const BaseOrderCursor&) = delete;
  std::uint32_t count() const;
  std::uint32_t member_position() const;
  std::size_t memory_bytes() const;
 private:
  friend class BaseOrderIndex;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
class BaseOrderIndex {
 public:
  // No I/O: authenticated bootstrap must already be Ready. Both primary
  // resource owners must outlive this index and its users. One serial cursor.
  BaseOrderIndex(std::shared_ptr<const VerifiedResourceReader>,
                 PagedSchemeIndex&, PagedSharedResource&);
  // Optional positive directory fact pool: 0..8 entries. Allocation failure or
  // size >8 disables reuse, not the index. No resource I/O; no reconfiguration.
  BaseOrderIndex(std::shared_ptr<const VerifiedResourceReader>,
                 PagedSchemeIndex&, PagedSharedResource&, std::size_t locate_entries);
  ~BaseOrderIndex();
  BaseOrderIndex(const BaseOrderIndex&) = delete;
  BaseOrderIndex& operator=(const BaseOrderIndex&) = delete;
  bool ready() const;
  bool matches(const PagedSchemeIndex&) const;
  bool matches(const PagedSharedResource&) const;
  BaseOrderStatus begin(std::string_view code, BaseOrderCursor&);
  // Experimental only. Consumes the state on entry, even on a rejected hint.
  // New comparisons and bounded hint copies are charged by locate().
  BaseOrderStatus begin_prefix(std::string_view, BaseOrderCursor&, BaseOrderPrefixState&);
  // Completed current search only; clears target on failure. A successful call
  // costs one fixed-width metadata operation, reported to the caller.
  BaseOrderStep save_prefix(const BaseOrderCursor&, BaseOrderPrefixState&);
  BaseOrderStep locate(BaseOrderCursor&, const WorkBudget&);
  BaseOrderStep read_item(BaseOrderCursor&, std::uint32_t position, const WorkBudget&);
  BaseOrderStep find_member(BaseOrderCursor&, RuntimeLexemeRef, const WorkBudget&);
  bool current(const BaseOrderCursor&) const;
  void cancel(BaseOrderCursor&); // Foreign/replaced is no-op; no query mutation.
  std::size_t memory_bytes() const;
  BaseOrderLocateStats locate_stats() const; // Read-only, no work/I/O.
 private:
  friend class ExactRanker;
  // Only the ranker may resume its own released operation lineage. A public
  // cancellation does not authorize another consumer to resume that cursor.
  bool continue_range(BaseOrderCursor&);
  BaseOrderStep save_prefix_impl(const BaseOrderCursor&, BaseOrderPrefixState&, bool released);
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace hanime
