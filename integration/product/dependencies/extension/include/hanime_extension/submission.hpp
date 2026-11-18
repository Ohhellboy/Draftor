#pragma once
#include "hanime_extension/extension.hpp"
#include "hanime/composition_commit.hpp"

namespace hanime_extension {
// Captured by the Host at prepare; Core remains independent of input widgets.
enum class InputTarget { Body, Find, Rename };
// Operations authorize one immutable (owner, transaction sequence, core attempt).
// Copying/observing an old handle never upgrades it to a later attempt.
class SubmissionHandle {
 public:
  // Observe this attempt while current; after a retry, return Stale with its
  // original attempt and no new permit. Save SubmissionResult::core for history.
  hanime::CompositionResult result() const;
  std::uint64_t attempt() const { return attempt_; }
  InputTarget target() const { return target_; }
 private:
  friend class Submission;
  std::shared_ptr<const OwnerIdentity> owner_;
  std::uint64_t sequence_{0};
  std::uint64_t attempt_{0};
  hanime::CompositionHandle core_;
  InputTarget target_{InputTarget::Body};
};
struct SubmissionResult {
  hanime::CompositionResult core;
  SubmissionHandle transaction;
};
struct SubmissionStep { SubmissionResult result; hanime::WorkCost cost; };
struct SubmissionMemory {
  std::size_t owner_bytes{0}, impl_bytes{0}, capture_array_bytes{0};
  hanime::CompositionMemoryUsage core;
};

// Construct after Composition; destroy before it, shared and the final sink.
// One active transaction; callers must bound retained historical handles.
// Preparing/Ready reserve the input session. Cancel releases it for editing.
// Unknown quarantines this input session, including after adapter destruction.
class Submission {
 public:
  Submission(Composition&, hanime::PagedSharedResource&, hanime::TextCommitSink&,
             hanime::CompositionConfig = {16,128,64,512,hanime::CompositionLearningPolicy::Disabled},
             hanime::CompositionCountSource* = nullptr);
  Submission(Composition&, hanime::PagedSharedResource&, hanime::TextCommitSink&,
             hanime::CompositionConfig, StorageBackend, hanime::CompositionCountSource* = nullptr);
  ~Submission();
  Submission(const Submission&) = delete;
  Submission& operator=(const Submission&) = delete;
  bool valid() const;
  // Future transactions only. Captured/prepared and already-owned state work
  // retain their Record decision; this never deletes or closes a state file.
  void set_record_enabled(bool);
  SubmissionResult prepare(InputTarget = InputTarget::Body); // Bounded capture, no I/O. Same current origin does not create a duplicate.
  SubmissionStep advance(const SubmissionHandle&, const hanime::WorkBudget&);
  hanime::CompositionStatus prepared(const SubmissionHandle&,
    const std::function<void(const hanime::PreparedCompositionView&)>&);
  SubmissionResult confirm(const SubmissionHandle&);
  // A successful retry returns a new attempt handle. Use that returned handle;
  // old attempts cannot advance, confirm, retry, cancel, or release the input.
  SubmissionResult retry(const SubmissionHandle&);
  SubmissionResult cancel(const SubmissionHandle&);
  SubmissionResult close(); // Owner-wide shutdown, not a queued attempt event.
  SubmissionResult result() const;
  const hanime::CompositionCosts& costs() const;
  SubmissionMemory memory_usage() const;
  StorageAllocation storage_usage() const noexcept;
 private:
  struct Impl;
  std::unique_ptr<Impl> p_;
};
} // namespace hanime_extension
