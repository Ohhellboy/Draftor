#include "packed_auth_build.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace hanime::detail {
std::vector<std::uint8_t> build_packed_auth(const ResourceReader& file,
    PublishedResource& output, std::size_t* tree_peak_bytes) {
  PackedMerkleLayout layout;
  if (!packed_merkle_layout(file.size(), &layout) ||
      layout.padded_leaves > std::numeric_limits<std::size_t>::max() / (2 * sizeof(Sha256Digest)))
    throw std::runtime_error("packed resource exceeds Host tree dimensions");
  std::vector<Sha256Digest> tree(static_cast<std::size_t>(2 * layout.padded_leaves));
  *tree_peak_bytes = std::max(*tree_peak_bytes, tree.capacity() * sizeof(Sha256Digest));
  std::array<std::uint8_t, kPackedDataBytes> block_bytes{};
  Sha256 before;
  auto& cost = output.index_generation;
  for (std::uint64_t block = 0; block < layout.padded_leaves; ++block) {
    std::size_t count = 0;
    if (block < layout.data_blocks) {
      const auto offset = static_cast<std::size_t>(block * kPackedDataBytes);
      count = std::min(kPackedDataBytes, file.size() - offset);
      if (!file.read(offset, block_bytes.data(), count))
        throw std::runtime_error("packed tree source read failed");
      before.update(block_bytes.data(), count);
      ++cost.cost.read_calls;
      cost.cost.read_bytes += count;
    }
    tree[static_cast<std::size_t>(layout.padded_leaves + block)] =
        packed_leaf_hash(block, block_bytes.data(), count);
    ++cost.steps;
    cost.cost.work_items += count + 13;
  }
  if (before.final() != output.spec.file_sha256)
    throw std::runtime_error("source changed during packed tree construction");
  std::uint32_t level = 0;
  for (auto first = layout.padded_leaves / 2; first; first /= 2, ++level) {
    for (std::uint64_t node = 0; node < first; ++node) {
      const auto index = static_cast<std::size_t>(first + node);
      tree[index] = packed_parent_hash(level, node, tree[2 * index], tree[2 * index + 1]);
      ++cost.steps;
      cost.cost.work_items += 77;
    }
  }
  output.spec.merkle_root = tree[1];
  std::vector<std::uint8_t> packets(static_cast<std::size_t>(layout.file_bytes), 0);
  Sha256 after;
  for (std::uint64_t block = 0; block < layout.data_blocks; ++block) {
    const auto offset = static_cast<std::size_t>(block * kPackedDataBytes);
    const auto count = std::min(kPackedDataBytes, file.size() - offset);
    auto* packet = packets.data() + static_cast<std::size_t>(block * layout.packet_bytes);
    if (!file.read(offset, packet, count)) throw std::runtime_error("packed payload source read failed");
    after.update(packet, count);
    ++cost.steps;
    ++cost.cost.read_calls;
    cost.cost.read_bytes += count;
    cost.cost.work_items += count;
    auto node = static_cast<std::size_t>(layout.padded_leaves + block);
    for (std::uint32_t height = 0; height < layout.depth; ++height, node /= 2)
      std::copy(tree[node ^ 1U].begin(), tree[node ^ 1U].end(), packet + kPackedDataBytes + 32 * height);
  }
  if (after.final() != output.spec.file_sha256)
    throw std::runtime_error("source changed during packed payload construction");
  output.index_bytes = packets.size();
  output.index_sha256 = sha256(packets.data(), packets.size());
  ++cost.steps;
  cost.cost.work_items += packets.size();
  return packets;
}
} // namespace hanime::detail
