#pragma once
#include "hanime/resource_publisher.hpp"

namespace hanime::detail {
// Host-only O(blocks) tree and output vector. Runtime never calls this builder.
std::vector<std::uint8_t> build_packed_auth(const ResourceReader&, PublishedResource&,
                                          std::size_t* tree_peak_bytes);
}
