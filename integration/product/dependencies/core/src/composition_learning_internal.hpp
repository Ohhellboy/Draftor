#pragma once
#include "hanime/composition_commit.hpp"
namespace hanime {
// Private payload: there is no public way to forge or mutate a ticket.
struct CompositionLearningIdentity {
  RegistryNamespaceId registry_namespace{0};
  bool accepted{false};
  std::shared_ptr<const CountOwnerIdentity> claimed_owner;
  std::unique_ptr<CompositionLearningMemberReceipt[]> members;
  std::size_t count{0}, local_work{0};
};
}
