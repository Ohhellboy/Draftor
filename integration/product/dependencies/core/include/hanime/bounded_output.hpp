#pragma once
#include "hanime/paged_shared_resource.hpp"

namespace hanime {
enum class TextAccessStatus { Pending, Ready, Incompatible, Stale, ReadError, InvalidBudget, InvalidRequest };
struct TextAccessStep {
  TextAccessStatus status{TextAccessStatus::Pending};
  WorkCost cost;
  SharedRecordKind location_kind{SharedRecordKind::Lexeme};
  std::uint32_t location_index{0};
  std::size_t required_bytes{0};
};
class TextAccessCertificate {
 public:
  bool current(const PagedSharedResource&, std::size_t l, std::size_t f) const;
  const SharedActivationHandle& activation() const { return activation_; }
 private:
  friend class TextAccessCheck;
  SharedActivationHandle activation_;
  std::size_t l_{0}, f_{0};
  bool ready_{false};
};
// One cooperative pass over relevant records/descriptors; no table or text copy.
class TextAccessCheck {
 public:
  ~TextAccessCheck() { cancel(); }
  TextAccessStatus begin(PagedSharedResource&, std::size_t l, std::size_t f);
  TextAccessStep advance(const WorkBudget&);
  void cancel();
  TextAccessCertificate certificate() const { return certificate_; }
 private:
  PagedSharedResource* resource_{nullptr};
  TextAccessCertificate certificate_;
  SharedReadCursor cursor_;
  TextAccessStep result_;
  std::uint32_t index_{0}, descriptor_{0};
  unsigned section_{0};
  bool reading_{false}, descriptor_pending_{false};
};

struct OutputRequest {
  // Empty means Default. Named requests borrow stable configuration bytes.
  std::string_view form;
  OutputInvocation invocation{OutputInvocation::Profile};
};
enum class OutputStatus {
  Pending, Default, Exact, FallbackDefault, ConditionUndetermined, Blocked,
  InvocationDenied, MultipleRelations, ReadError, Stale, TooLarge, InvalidRequest, Cancelled
};
bool output_has_text(OutputStatus);
struct OutputStep {
  OutputStatus status{OutputStatus::Pending};
  WorkCost cost;
  std::string_view text; // Only complete text; caller owns destination.
  std::uint32_t payload{kNoSharedString};
};
struct OutputCosts {
  WorkCost metadata, names, payload; // Names/payload include descriptor access.
  std::size_t compared_bytes{0}, name_payload_bytes{0}, text_payload_bytes{0};
};
struct OutputCapacityRequirement {
  std::size_t bytes{0};
  bool name{false};
};

// One resolver, caller-owned F-byte name scratch and L-byte destination. Optional
// reuse belongs to the shared activation, not this query. It never grants member
// or selection eligibility. The original shared owner must outlive this reader.
// Immutable semantics/UTF-8 are fully validated or trusted-release authenticated.
class OutputReader {
 public:
  ~OutputReader() { cancel(); }
  OutputStatus begin(PagedSharedResource&, const SharedActivationHandle&, RuntimeLexemeRef,
                     const StableLexemeKey&, OutputRequest, char* names, std::size_t f,
                     char* destination, std::size_t l);
  OutputStep advance(const WorkBudget&);
  void cancel();
  const OutputCosts& costs() const { return costs_; } // Cumulative, across begin calls.
  // TooLarge diagnostic, reset by successful begin. No I/O; old OutputStep
  // aggregate shape and status semantics stay source-compatible.
  OutputCapacityRequirement capacity_requirement() const { return capacity_; }
 private:
  enum class Phase { LexemeStart, LexemeRead, RelationStart, RelationRead, SelectorStart,
                     SelectorRead, NameStart, NameRead, Compare, Decide, TextStart, TextRead, Done,
                     ReuseLookup, ReuseCompare, ReuseCopy, ReuseReserve, ReuseStore };
  PagedSharedResource* resource_{nullptr};
  SharedActivationHandle activation_;
  SharedReadCursor record_cursor_;
  SharedTextCursor text_cursor_;
  SharedLexemeRecord lexeme_;
  SharedOutputRecord relation_, chosen_;
  StableLexemeKey expected_key_;
  OutputRequest request_;
  RuntimeLexemeRef ref_{0};
  char* names_{nullptr}; char* destination_{nullptr};
  std::size_t f_{0}, l_{0}, name_length_{0}, compare_pos_{0}, length_{0};
  std::uint32_t relation_index_{0}, name_id_{0}, payload_{kNoSharedString};
  unsigned matches_{0};
  bool equal_{false}, reading_record_{false}, reading_text_{false};
  Phase phase_{Phase::Done};
  OutputStatus status_{OutputStatus::Cancelled}, resolved_{OutputStatus::Default};
  OutputCosts costs_;
  OutputCapacityRequirement capacity_;
  detail::OutputReuseCache* reuse_{nullptr}; // Borrowed only while activation_ is current.
  std::size_t reuse_slot_{0}, reuse_position_{0};
  std::uint64_t reuse_revision_{0};
};
} // namespace hanime
