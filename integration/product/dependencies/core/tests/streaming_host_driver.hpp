#pragma once

#include "hanime/paged_shared_resource.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace hanime::test {
// Minimal cooperative Host driver: one tick performs at most one budgeted
// advance. The host may service input between ticks. Never calls the synchronous
// MountedResourceSet, never collects all hits/texts, never advances user pages.
class StreamingHostDriver {
 public:
  using Consumer = std::function<void(const RawQueryHit&, const SharedReadResult&)>;
  StreamingHostDriver(PagedSchemeIndex& scheme, PagedSharedResource& shared,
                      std::string code, MountEpoch epoch, WorkBudget budget,
                      std::size_t batch_capacity, Consumer consumer)
      : scheme_(scheme), shared_(shared), shared_activation_(shared.activation_handle()),
        budget_(budget), capacity_(batch_capacity),
        consumer_(std::move(consumer)) {
    if (!shared.activation_is_current(shared_activation_) || !scheme.ready() || !capacity_ ||
        epoch != shared_activation_.mount_epoch() ||
        scheme.info().registry_namespace != shared_activation_.registry_namespace() ||
        scheme.info().required_shared_bundle_id != shared_activation_.shared_bundle_id())
      throw std::runtime_error("driver resource binding mismatch");
    std::string error;
    if (!scheme_.begin_exact_query(std::move(code), epoch, &query_, &error)) throw std::runtime_error(error);
  }
  bool tick() {
    check_shared_activation();
    if (done_) return true;
    if (!reading_) {
      auto step = scheme_.advance_exact_query(&query_, budget_, capacity_);
      check_cost(step.cost);
      if (step.status == ExactQueryStepStatus::Complete || step.status == ExactQueryStepStatus::NoResults) {
        done_ = true; return true;
      }
      if (step.status == ExactQueryStepStatus::InProgress) return false;
      if (step.status != ExactQueryStepStatus::HitsAvailable) throw std::runtime_error("driver query failed/stale");
      batch_ = std::move(step.batch); position_ = 0;
      maximum_retained_hits = std::max(maximum_retained_hits, batch_.hits.size());
      start_current(); return false;
    }
    if (!scheme_.batch_is_current(batch_)) throw std::runtime_error("late batch rejected by driver");
    auto step = shared_.advance_read(&read_, budget_); check_cost(step.cost);
    if (step.status == SharedReadStatus::InProgress) return false;
    check_shared_activation();
    if (!scheme_.batch_is_current(batch_) || !shared_.result_is_current(step) ||
        step.context.mount_epoch != shared_activation_.mount_epoch() ||
        batch_.context.mount_epoch != shared_activation_.mount_epoch())
      throw std::runtime_error("late/shared-mismatched result rejected by driver");
    consumer_(batch_.hits[position_], step); ++consumed;
    // A host callback may reopen resources or start new work. The delivered
    // result was valid; retain its count, but never begin the next old read.
    check_shared_activation();
    if (++position_ < batch_.hits.size()) start_current();
    else { batch_.hits.clear(); reading_ = false; }
    return false;
  }
  std::size_t consumed{0}, maximum_retained_hits{0}; WorkCost cost;
 private:
  void check_shared_activation() {
    if (failed_ || !shared_.activation_is_current(shared_activation_) ||
        !scheme_.ready() ||
        scheme_.info().registry_namespace != shared_activation_.registry_namespace() ||
        scheme_.info().required_shared_bundle_id != shared_activation_.shared_bundle_id()) {
      failed_ = true;
      // No cancel/deactivate/read: new owner work belongs to the host.
      throw std::runtime_error("driver shared activation is stale");
    }
  }
  void check_cost(const WorkCost& c) {
    if (c.read_calls > budget_.max_read_calls || c.read_bytes > budget_.max_read_bytes ||
        c.work_items > budget_.max_work_items) throw std::runtime_error("driver budget exceeded");
    cost.read_calls += c.read_calls; cost.read_bytes += c.read_bytes; cost.work_items += c.work_items;
  }
  void start_current() {
    check_shared_activation();
    if (!scheme_.batch_is_current(batch_) || shared_.begin_read(SharedRecordKind::Lexeme,
        batch_.hits.at(position_).runtime_ref, &read_) != SharedReadStatus::InProgress)
      throw std::runtime_error("driver cannot begin metadata read");
    reading_ = true;
  }
  PagedSchemeIndex& scheme_; PagedSharedResource& shared_;
  const SharedActivationHandle shared_activation_;
  WorkBudget budget_; std::size_t capacity_; Consumer consumer_;
  ExactQueryCursor query_; SharedReadCursor read_; RawQueryBatch batch_;
  std::size_t position_{0}; bool reading_{false}, done_{false}, failed_{false};
};
}  // namespace hanime::test
