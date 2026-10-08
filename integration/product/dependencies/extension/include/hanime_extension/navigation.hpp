#pragma once
#include "hanime_extension/extension.hpp"

namespace hanime_extension {
enum class NavigationStatus { Ready, AtEdge, Stale, CapacityExceeded, InvalidRequest };
enum class NavigationCapacity { None, RawBytes, Units, Generation };
enum class NavigationKind : std::uint8_t { CompleteSyllable, CodePair, IncompleteTail, Unknown };
enum class NavigationDirection { Left, Right };
struct NavigationConfig { std::size_t raw_bytes{64}, units{64}; };
struct NavigationSpan {
  std::uint16_t begin{0}, end{0};
  NavigationKind kind{NavigationKind::Unknown};
};
class NavigationHandle {
 private:
  friend class SyllableNavigator;
  std::weak_ptr<const OwnerIdentity> owner_;
  std::uint64_t generation_{0};
};
struct NavigationResult {
  NavigationStatus status{NavigationStatus::Stale};
  NavigationHandle handle;
  NavigationCapacity capacity{NavigationCapacity::None};
  std::size_t required{0};
};
struct NavigationView {
  NavigationStatus status{NavigationStatus::Stale};
  const std::uint16_t* boundaries{nullptr};
  std::size_t boundary_count{0};
  const NavigationSpan* spans{nullptr};
  std::size_t span_count{0};
  bool ambiguous{false}, has_unknown{false}, has_incomplete{false};
};
// Complete spelling endpoints reachable from the current unselected focus.
// This is spelling admission only: it does not assert a dictionary match.
struct NavigationPrefixView {
  NavigationStatus status{NavigationStatus::Stale};
  std::size_t begin{0};
  const std::uint16_t* ends{nullptr};
  std::size_t count{0};
};
struct NavigationMove {
  NavigationStatus status{NavigationStatus::Stale};
  std::size_t offset{0};
};
struct NavigationCost { std::size_t table_entries{0}, character_comparisons{0}; };
struct NavigationMemory {
  std::size_t owner{0}, impl{0}, workspace{0}, spans{0}, boundaries{0}, constant_table{0};
  std::size_t prefix_endpoints{0};
};
// Independent optional object: no core I/O, query ticket, learning or sink.
// Analyze/reset invalidate previous handles. Views borrow fixed arrays and must
// not be used after input mutation, analyze/reset, or destruction.
class SyllableNavigator {
 public:
  explicit SyllableNavigator(NavigationConfig = {});
  ~SyllableNavigator();
  SyllableNavigator(const SyllableNavigator&)=delete;
  SyllableNavigator& operator=(const SyllableNavigator&)=delete;
  NavigationResult analyze(const Composition&);
  void reset();
  NavigationView view(const NavigationHandle&, const Composition&) const;
  NavigationMove neighbor(const NavigationHandle&, const Composition&, std::size_t,
                          NavigationDirection) const;
  // Sorted absolute half-open endpoints, borrowed until analyze/reset/mutation.
  // No Unknown or incomplete span is admitted. No I/O or selection authority.
  NavigationPrefixView prefix_endpoints(const NavigationHandle&, const Composition&) const;
  NavigationCost cost() const;
  NavigationMemory memory_usage() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> p_;
};
} // namespace hanime_extension
