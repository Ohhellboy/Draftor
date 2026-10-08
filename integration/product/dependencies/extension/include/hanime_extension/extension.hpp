#pragma once

#include "hanime/bounded_output.hpp"
#include "hanime/exact_ranker.hpp"
#include "hanime_extension/storage.hpp"
#include <array>
#include <memory>

namespace hanime_extension {

enum class SchemeRules { FullPinyin, R1Xiaohe, DirectExact };
// Explicit peripheral policy: DirectExact has no spelling or prefix planner.
// Resource identity is still checked by the original Core and by this owner.
struct SchemePolicy {
  SchemeRules rules;
  std::string_view resource_scheme;
};
constexpr SchemePolicy spelling_policy(SchemeRules rules) {
  return {rules, rules==SchemeRules::FullPinyin?"full_pinyin":
                 rules==SchemeRules::R1Xiaohe?"xiaohe":""};
}
enum class Status {
  Ready, Working, PageReady, NoMappingForPrefix, NoPrefixMatch,
  RangeExhausted, CapacityExceeded, InvalidRequest, InvalidBudget, Busy,
  Stale, ResourceError, OutputRestricted, Unresolved, Cancelled,
  CompositionCommitPending
};
enum class CompositionState { Idle, PartialSelected, NeedsRecheck, AllSelected };
struct PlanningConfig {
  std::size_t ranges{64}, span_syllables{8};
  std::size_t core_work{200000}, read_bytes{1048576}, local_work{200000};
};
struct Config {
  std::size_t raw_bytes{64}, members{16}, page_items{6}, raw_batch{4};
  std::size_t word_bytes{64}, name_bytes{128}, text_bytes{512}, request_bytes{256};
  PlanningConfig planning;
};
struct PlanningMemory {
  std::size_t owner{0}, impl{0}, buffers{0}, constant_table{0};
  hanime::RankMemoryUsage rank;
};
struct Member {
  hanime::StableLexemeKey key;
  hanime::RuntimeLexemeRef ref{0};
  std::size_t begin{0}, end{0}, text_begin{0}, text_size{0}, form_begin{0}, form_size{0};
  hanime::OutputInvocation invocation{hanime::OutputInvocation::Profile};
  bool valid{false};
};
struct Snapshot {
  std::uint64_t input_id{0}, revision{0};
  std::string_view raw, selected_text, output_forms;
  hanime::SharedActivationHandle activation;
  std::uint64_t scheme_epoch{0};
  const Member* members{nullptr};
  std::size_t member_count{0}, focus{0};
  CompositionState state{CompositionState::Idle};
};
struct OwnerIdentity {};
// A read-only source identity. Weak ownership rejects destruction/address reuse.
class InputStamp {
 public:
  std::uint64_t input_id() const { return input_; }
  std::uint64_t revision() const { return revision_; }
  std::uint64_t scheme_epoch() const { return epoch_; }
 private:
  friend class Composition;
  std::weak_ptr<const OwnerIdentity> owner_;
  std::uint64_t input_{0}, revision_{0}, epoch_{0};
};
struct PlanningReport {
  bool valid{false}, limited{false}, incomplete_tail{false}, unknown_tail{false};
  bool direct_range{false}; // Sole encoding interval; no pre-ranked path metadata.
  std::uint32_t policy{1};
  InputStamp source;
  std::uint64_t input_id{0}, revision{0}, context_revision{0}, generation{0};
  std::size_t begin{0}, end{0}, covered_end{0}, checked_ranges{0}, queries{0}, cache_hits{0};
  std::size_t local_work{0}, syllables{0}, words{0};
  std::size_t query_begin{0}, query_end{0}; // Diagnostic range of latest actual metadata request.
  std::int32_t score{0};
  hanime::WorkCost core_cost;
};
struct RawInput {
  std::string_view raw;
  SchemeRules rules{SchemeRules::FullPinyin};
  InputStamp stamp;
};
class Operation {
 public:
  Operation() = default;
 private:
  friend class Composition;
  std::shared_ptr<const OwnerIdentity> owner_;
  std::uint64_t sequence_{0};
};
class Selection {
 public:
  Selection() = default;
 private:
  friend class Composition;
  std::shared_ptr<const OwnerIdentity> owner_;
  std::uint64_t input_{0}, revision_{0}, page_{0};
  std::size_t begin_{0}, end_{0};
  hanime::RankItem item_;
};
struct Page {
  const hanime::RankItem* items{nullptr};
  std::size_t size{0}, begin{0}, end{0};
  bool has_more{false};
  std::uint64_t generation{0};
};
struct Step {
  Status status{Status::Ready};
  hanime::WorkCost core_cost;
  std::size_t local_work{0};
  hanime::OutputStatus output{hanime::OutputStatus::Cancelled};
};
struct MemoryUsage {
  std::size_t owner_bytes{0}, impl_bytes{0}, composition_buffers{0};
  std::size_t rank_arrays{0}, page_array{0}, output_buffers{0};
  hanime::RankMemoryUsage rank;
  hanime::ExactQueryMemoryUsage raw_query; // Live prefix/membership cursor, separate from rank.raw.
};

// No persistence or learning notification interface. A disabled lookup is
// Unavailable, never fabricated NotFound/zero. Owners and cancellation match.
class DisabledCountSource final : public hanime::CountSource {
 public:
  explicit DisabledCountSource(hanime::RegistryNamespaceId);
  hanime::StateView view() const override;
  bool view_is_current(const hanime::StateView&) const override;
  hanime::CountTicket begin_count(const hanime::StateView&, const hanime::StableLexemeKey&) override;
  hanime::CountResult advance_count(const hanime::CountTicket&, const hanime::WorkBudget&) override;
  void cancel_count(const hanime::CountTicket&) override;
  std::size_t begin_calls() const noexcept { return begins_; }
 private:
  hanime::StateView view_;
  std::uint64_t sequence_{0}, active_{0};
  std::size_t begins_{0};
};

// One serial client of borrowed scheme/shared owners; destroy this first.
// All snapshots/pages borrow this object's buffers until the next mutation.
// Commands return an operation; tick/cancel require that exact owner/sequence.
// Fixed command setup work is reported separately by command_work().
class Composition {
 public:
  Composition(hanime::PagedSchemeIndex&, hanime::PagedSharedResource&, SchemeRules, Config = {});
  Composition(hanime::PagedSchemeIndex&, hanime::PagedSharedResource&, SchemeRules, Config, StorageBackend);
  Composition(hanime::PagedSchemeIndex&, hanime::PagedSharedResource&, SchemePolicy, Config = {}, StorageBackend = {});
  ~Composition();
  Composition(const Composition&) = delete;
  Composition& operator=(const Composition&) = delete;
  bool valid() const noexcept;
  RawInput raw_input() const; // No I/O or allocation; borrowed until input mutation.
  bool input_is_current(const InputStamp&) const;
  Snapshot snapshot() const;
  Page page() const;
  Selection selection(std::size_t slot) const;
  Status status() const;
  Operation operation() const;
  Step tick(const Operation&, const hanime::WorkBudget&);
  Status cancel(const Operation&);
  Status cancel_input(std::uint64_t input_id);

  Status edit_raw(std::size_t begin, std::size_t end, std::string_view replacement);
  Status choose_prefix(std::size_t end);
  Status query_longest();
  // Optional, bounded range recommendation; no output materialization. Output
  // context changes require cancel + a new call, with a new context revision.
  Status query_next(hanime::OutputRequest = {}, std::uint64_t context_revision = 0);
  PlanningReport planning_report() const; // By value; recheck before deferred use.
  bool plan_is_current(const PlanningReport&) const;
  PlanningMemory planning_memory_usage() const;
  Status drop_planning_cache(); // No active operation; frees the optional object.
  Status request_page(hanime::RangeDirection);
  Status select(const Selection&, hanime::OutputRequest = {});
  Status read_text(const Selection&, hanime::OutputRequest = {});
  std::string_view last_text() const;
  Status retract_last();
  Status reopen_segment(std::size_t index);
  Status revalidate();
  Status rebind(); // Same namespace/scheme; all intents rechecked using full keys.
  Status confirm_composition(); // Legacy query-only probe; actual transactions use Submission.

  MemoryUsage memory_usage() const;
  // Three fixed allocation records by value; no mutable buffers or addresses.
  std::array<StorageAllocation,3> storage_usage() const noexcept;
  std::size_t command_work() const; // Fixed setup/copy bytes, separate from tick work.
  std::size_t count_begin_calls() const;
 private:
  friend class Submission;
  bool begin_submission();
  void end_submission(bool unknown);
  struct Impl;
  std::unique_ptr<Impl> p_;
};
const char* name(Status);
} // namespace hanime_extension
