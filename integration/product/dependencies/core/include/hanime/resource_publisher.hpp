#pragma once

// Host-only final-byte validation and release publication. Not linked into the
// portable runtime target; no device may derive a trust pin from an untrusted
// manifest on its resource medium.
#include "hanime/verified_resource.hpp"
#include <string>
#include <vector>

namespace hanime {

struct PublishSchemeInput {
  std::string scheme_id;
  std::string path;
};
struct PublishVerifiedOptions {
  std::string shared_path;
  std::vector<PublishSchemeInput> schemes; // Independently optional; at least one.
  std::string output_directory; // Must not exist. Never overwrites an accepted pin.
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId expected_shared_bundle_id{0}; // Zero allows discovery by validation.
  std::uint32_t text_capacity{64}, name_capacity{128};
  WorkBudget validation_budget{64, 16384, 4096};
  bool base_order{false}; // Optional BaseOnly rule-1 derived files; HNM/HNS unchanged.
  bool packed_auth{false}; // Optional format-2 transport; logical files unchanged.
  bool packed_auth_all{false}; // Explicit diagnostic alternative; normally shared only.
  // Appended to preserve existing positional Host option initialization.
  // Runtime public query/selection API is unchanged. 1 preserves HBO1;
  // 2 emits the equivalently checked paged HBO2 directory.
  std::uint32_t base_order_format{1};
};
struct PublicationPhaseCost {
  std::uint64_t steps{0};
  WorkCost cost;
};
struct PublishedResource {
  std::string logical_name;
  std::string source_file;
  std::string index_file;
  ResourceReleaseSpec spec;
  std::uint64_t index_bytes{0};
  Sha256Digest index_sha256{};
  PublicationPhaseCost hash_before, fixed_open, semantic_validation, text_admission;
  PublicationPhaseCost index_generation, hash_after;
  PublicationPhaseCost derived_build; // Host-only pass; not a runtime advance.
  PublicationPhaseCost final_transport_validation;
  std::uint64_t derived_source_bytes{0};
  Sha256Digest derived_source_sha256{};
  bool directory_and_tail_equivalent{false};
};
struct PublishVerifiedResult {
  bool ok{false};
  std::string error;
  std::vector<PublishedResource> resources; // Qualifications exist only if ok.
  // Largest temporary index vector allocation; NOT the publisher total peak or
  // device RAM. Host construction uses O(index size), runtime does not.
  std::size_t host_index_peak_bytes{0};
  std::size_t host_packed_tree_peak_bytes{0}; // Coexists with output vector while building.
  // Counted vector capacities for the paged transformation only. This excludes
  // allocator overhead and the existing source semantic reference maps.
  std::size_t host_paged_live_vector_bytes{0};
};

// Validates the actual final serialized files using the SAME public complete
// validators and TextAccessCheck as the diagnostic path. The caller guarantees
// input files remain immutable throughout publication and later distribution.
// Pre-validation, index-generation and final SHA passes must agree. A failure
// produces no release_pin.hpp / accepted manifest and returns ok=false.
// No whole-file trusted-source decision is inferred from a filename or bundle.
PublishVerifiedResult publish_verified_resources(const PublishVerifiedOptions&);

} // namespace hanime
