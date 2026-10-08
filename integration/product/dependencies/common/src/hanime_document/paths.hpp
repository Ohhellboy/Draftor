#include "../../core_memory.hpp"
#pragma once

#include "types.hpp"

#include <string>
#include <string_view>

namespace hanime::documents::paths {

inline constexpr std::string_view kRoot = "/HANIME_S3_TEST/v01";
inline constexpr std::string_view kCollection = "/HANIME_S3_TEST/v01/COLLECTION";
inline constexpr std::string_view kCurrent = "/HANIME_S3_TEST/v01/CURRENT";
inline constexpr std::string_view kCreateIntent = "/HANIME_S3_TEST/v01/CREATE_INTENT";
inline constexpr std::string_view kMigration = "/HANIME_S3_TEST/v01/MIGRATION";
inline constexpr std::string_view kDocsPrefix = "/HANIME_S3_TEST/v01/docs/";

inline draftor::memory::String documentRoot(const DocumentId& id) {
  return draftor::memory::String(kDocsPrefix) + id.hex() + "/";
}
inline draftor::memory::String metadata(const DocumentId& id) { return documentRoot(id) + "META"; }
inline draftor::memory::String lane(const DocumentId& id, const SaveLane lane) {
  switch (lane) {
    case SaveLane::Auto: return documentRoot(id) + "AUTO";
    case SaveLane::Manual: return documentRoot(id) + "MANUAL";
    case SaveLane::Protection: return documentRoot(id) + "PROTECT";
  }
  return documentRoot(id) + "INVALID";
}
inline draftor::memory::String position(const DocumentId& id) { return documentRoot(id) + "POSITION"; }

}  // namespace hanime::documents::paths
