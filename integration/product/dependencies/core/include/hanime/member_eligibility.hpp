#pragma once
#include "hanime/base_order.hpp"
#include "hanime/bounded_output.hpp"
namespace hanime {
struct MemberRequest {
  SharedActivationHandle shared;
  std::string_view code;
  RuntimeLexemeRef ref{0};
  StableLexemeKey key;
  std::uint64_t input_revision{0}, selection_revision{0};
  std::uint32_t begin{0},end{0},source_bytes{0};
  OutputRequest output;
};
enum class MemberStatus { Pending, Eligible, NotMapped, IdentityMismatch, OutputRestricted,
                          Stale, Cancelled, ResourceError, InvalidRequest };
struct MemberProof;
class MemberTicket {
 public:
  MemberTicket()=default;
 private:
  friend class MemberEligibility;
  std::shared_ptr<const MemberProof> proof_;
};
struct MemberStep {
  MemberStatus status{MemberStatus::Stale};
  OutputStatus output_status{OutputStatus::Pending};
  WorkCost cost;
};
// One bounded public operation; NO counts, learning, sink or query revision.
// Owners AND caller buffers outlive this object. Calls are serial with other
// shared reads. Request strings are copied (<=128 B each); text never certifies
// a final commit and is not exposed as a substitute for CompositionCommit.
class MemberEligibility {
 public:
  MemberEligibility(BaseOrderIndex&,PagedSharedResource&,char* names,std::size_t name_capacity,
                    char* text,std::size_t text_capacity);
  ~MemberEligibility();
  MemberStatus begin(const MemberRequest&,MemberTicket*);
  MemberStep advance(const MemberTicket&,const WorkBudget&);
  // Rechecks the entire captured member, not only a caller's valid flag. Bump
  // selection_revision for edits and input_revision for a new input. Successful
  // proofs survive another member lookup, NOT activation replacement. No I/O.
  bool eligible(const MemberTicket&,const MemberRequest& current) const;
  void cancel(const MemberTicket&);
  std::size_t memory_bytes() const;
  static std::size_t ticket_payload_bytes(); // Excludes shared_ptr control block.
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace hanime
