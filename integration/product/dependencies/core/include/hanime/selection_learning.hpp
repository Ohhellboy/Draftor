#pragma once
#include "hanime/core.hpp"
#include <memory>
namespace hanime_extension { class Composition; }
namespace hanime {
// Bounded, transient capability: issued only after a current displayed choice.
// The shared owner prevents destruction/address reuse; action never wraps.
class SelectionLearningProof {
 public:
  SelectionLearningProof() = default;
  bool eligible(RuntimeLexemeRef ref) const { return owner_ && action_ && ref_==ref; }
  bool same_action(const SelectionLearningProof& other) const {
    return owner_ && owner_==other.owner_ && action_ && action_==other.action_;
  }
 private:
  friend class hanime_extension::Composition;
#if R2_HOST_TEST
  friend struct LearningProofTestAccess; // Contract-only fixture; no product UI issuer.
#endif
  std::shared_ptr<const void> owner_;
  std::uint64_t action_{0};
  RuntimeLexemeRef ref_{0};
};
} // namespace hanime
