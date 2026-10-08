#pragma once

#include "hanime/candidate_session.hpp"
#include <array>

namespace hanime::wubi {

struct Profile {
  std::string_view family, name, resource_scheme, alphabet;
  std::size_t normal_code_limit;
  bool auto_unique_full_code;
};
// These are reference-profile rules, never Wubi-family constants.
inline constexpr Profile wubi86_profile{"WUBI", "WUBI86", "wubi86",
    "abcdefghijklmnopqrstuvwxy", 4, true};

enum class EditStatus { Changed, Unchanged, InvalidCode, Full, Busy, Stale, Closed, Exhausted };
struct DirectQueryPlan {
  std::string_view code;
  std::uint64_t revision{0};
  bool full_code{false};
};

// Scheme-local decoder/query-planning seam. No lexical payload or candidate
// ownership. DirectQueryPlan is the only plan implemented in this candidate;
// richer plan production/dispatch belongs here, not in Core or a Pinyin parser.
// Returned code borrows decoder storage until the next successful edit.
class Decoder {
 public:
  virtual ~Decoder() = default;
  virtual const Profile& profile() const = 0;
  virtual EditStatus append(char routed_ascii) = 0;
  virtual EditStatus backspace() = 0;
  virtual EditStatus clear() = 0;
  virtual DirectQueryPlan plan() const = 0;
};

class Wubi86Decoder final : public Decoder {
 public:
  const Profile& profile() const override { return wubi86_profile; }
  EditStatus append(char) override;
  EditStatus backspace() override;
  EditStatus clear() override;
  DirectQueryPlan plan() const override;
 private:
  std::array<char, wubi86_profile.normal_code_limit> code_{};
  std::size_t size_{0};
  std::uint64_t revision_{1};
};

enum class CommitReason { None, Explicit, AutoUniqueFullCode };
struct Step {
  SessionStep shared;
  CommitReason request{CommitReason::None};
  SelectionHandle selection;
  std::uint64_t code_revision{0};
};

// Minimal direct-plan adapter, not an IME core. All owners outlive this object;
// serial calls only. The caller owns the ONE CandidateSession, routing, output
// configuration, state scheduling and sink. Paging/presentation/selection are
// used directly on that shared session. Do not mutate its input/binding behind
// this adapter while active. Sink/presenter callbacks must not re-enter it.
class DirectQueryBinding {
 public:
  DirectQueryBinding(Decoder&, CandidateSession&, PagedSchemeIndex&, PagedSharedResource&,
                     std::size_t page_size = 3);
  ~DirectQueryBinding();
  DirectQueryBinding(const DirectQueryBinding&) = delete;
  DirectQueryBinding& operator=(const DirectQueryBinding&) = delete;
  SessionStatus activate(); // Caller has completed shared text admission.
  EditStatus append(char);
  EditStatus backspace();
  EditStatus cancel();
  EditStatus leave(); // Explicit discard policy; never silently commits.
  Step tick(const WorkBudget&);
  // Delegation only: shared Core owns transaction, retry and learning.
  CommitResult confirm(const SelectionHandle&, CommitReason = CommitReason::Explicit);
  CommitResult retry(const RetryPermit&);
  SessionStatus query_status() const { return status_; }
  CommitReason commit_reason() const { return reason_; }
 private:
  bool current() const;
  bool commit_busy() const;
  void begin_plan();
  Decoder& decoder_;
  CandidateSession& session_;
  PagedSchemeIndex& index_;
  PagedSharedResource& shared_;
  const std::size_t page_size_;
  QueryBatchContext index_activation_;
  SharedActivationHandle shared_activation_;
  SessionInputHandle input_;
  bool active_{false}, first_page_{false}, awaiting_commit_{false};
  std::uint64_t query_revision_{0}, commit_revision_{0};
  SessionStatus status_{SessionStatus::Idle};
  CommitReason reason_{CommitReason::None};
};
} // namespace hanime::wubi
