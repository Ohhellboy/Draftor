#pragma once

#include "hanime/paged_shared_resource.hpp"

namespace hanime::detail {
constexpr std::size_t kSharedHeaderBytes = 200;
constexpr std::uint32_t kSharedMagic = 0x32485348U;
constexpr std::uint32_t kSharedLayoutMagic = 0x32444853U;
constexpr std::uint32_t kSharedPageMagic = 0x32504753U;
constexpr std::uint64_t kSharedFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kSharedFnvPrime = 1099511628211ULL;

struct SharedBuildTables {
  std::vector<SharedLexemeRecord> lexemes;
  std::vector<SharedOutputRecord> relations;
  std::vector<SharedSelectorRecord> selectors;
  std::vector<std::string> strings;
};
bool build_shared_bundle(RegistryNamespaceId registry_namespace,
                         const SharedBuildTables& tables, std::uint32_t page_bytes,
                         std::vector<std::uint8_t>* output,
                         PagedSharedInfo* info, std::string* error);

// Explicit synchronous, full-table Host compatibility decoder. This does NOT
// drive advance_validation/read in a busy loop and is never used by paged APIs.
struct DecodedSharedBundle {
  PagedSharedInfo info;
  SharedBuildTables tables;
  std::vector<SharedStringDescriptor> descriptors;
};
bool decode_shared_host(const ResourceReader& reader, DecodedSharedBundle* output,
                        std::string* error);
}  // namespace hanime::detail
