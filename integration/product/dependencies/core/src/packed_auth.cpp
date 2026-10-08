#include "hanime/verified_resource.hpp"
#include <limits>

namespace hanime {
namespace {
void put_le(std::uint8_t* out, std::uint64_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i) out[i] = static_cast<std::uint8_t>(value >> (8U * i));
}
}

bool packed_merkle_layout(std::uint64_t bytes, PackedMerkleLayout* out) {
  if (!out || !bytes || bytes > std::numeric_limits<std::size_t>::max()) return false;
  PackedMerkleLayout layout;
  layout.data_blocks = (bytes - 1) / kPackedDataBytes + 1;
  layout.padded_leaves = 1;
  while (layout.padded_leaves < layout.data_blocks) {
    if (layout.depth == kPackedMaxDepth) return false;
    layout.padded_leaves *= 2;
    ++layout.depth;
  }
  layout.packet_bytes = kPackedDataBytes + 32 * layout.depth;
  if (layout.data_blocks > std::numeric_limits<std::size_t>::max() / layout.packet_bytes) return false;
  layout.file_bytes = layout.data_blocks * layout.packet_bytes;
  *out = layout;
  return true;
}

Sha256Digest packed_leaf_hash(std::uint64_t block, const void* bytes, std::size_t length) {
  std::array<std::uint8_t, 13> prefix{};
  prefix[0] = 2;
  put_le(prefix.data() + 1, block, 8);
  put_le(prefix.data() + 9, length, 4);
  Sha256 hash;
  hash.update(prefix.data(), prefix.size());
  hash.update(bytes, length);
  return hash.final();
}

Sha256Digest packed_parent_hash(std::uint32_t level, std::uint64_t node,
                                const Sha256Digest& left, const Sha256Digest& right) {
  std::array<std::uint8_t, 13> prefix{};
  prefix[0] = 3;
  put_le(prefix.data() + 1, level, 4);
  put_le(prefix.data() + 5, node, 8);
  Sha256 hash;
  hash.update(prefix.data(), prefix.size());
  hash.update(left.data(), left.size());
  hash.update(right.data(), right.size());
  return hash.final();
}
} // namespace hanime
