#pragma once

#include "hanime/core.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hanime::detail {

struct PagedSchemeBuildMapping {
  std::string code;
  RuntimeLexemeRef runtime_ref{0};
  std::int32_t scheme_weight{0};
  std::uint32_t mapping_ordinal{0};
};

struct PagedSchemeBuildStats {
  std::uint32_t page_bytes{0};
  std::uint32_t page_count{0};
  std::uint32_t leaf_page_count{0};
  std::uint32_t tree_depth{0};
};

struct DecodedPagedScheme {
  std::string scheme_id;
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId required_shared_bundle_id{0};
  std::vector<PagedSchemeBuildMapping> mappings;
};

bool build_paged_scheme_bundle(
    std::string_view scheme_id, RegistryNamespaceId registry_namespace,
    SharedBundleId shared_bundle_id,
    const std::vector<PagedSchemeBuildMapping>& mappings,
    std::uint32_t page_bytes, std::vector<std::uint8_t>* output,
    PagedSchemeBuildStats* stats, std::string* error);

// Compatibility decoder for the Step-1 synchronous small-fixture session.
// The Step-2A runtime path uses PagedSchemeIndex and never calls this helper.
bool decode_paged_scheme_bundle(const ResourceReader& reader,
                                DecodedPagedScheme* output,
                                std::string* error);

}  // namespace hanime::detail
