#pragma once
#include "hanime_extension/extension.hpp"

namespace hanime_extension {
// Internal serial worker. Composition is the sole owner/caller; never a
// second consumer running alongside a published candidate query.
class SegmentPlanner {
 public:
  SegmentPlanner(hanime::PagedSchemeIndex&, hanime::PagedSharedResource&,
                 hanime::CountSource&, const Config&);
  ~SegmentPlanner();
  Status begin(const RawInput&, std::size_t focus, hanime::OutputRequest,
               std::uint64_t context);
  Step advance(const hanime::WorkBudget&);
  void cancel();
  void retire(); // Retire report/task, retaining only completed facts.
  void accept_direct_fact(std::size_t begin, std::size_t end, const hanime::RankItem* first);
  void invalidate();
  PlanningReport report() const;
  PlanningMemory memory() const;
  std::size_t command_work() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> p_;
};
}
