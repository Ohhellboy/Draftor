#include "entropy_id_source.hpp"

namespace hanime::documents {
namespace {
std::uint64_t mix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}
}  // namespace

DocumentId EntropyIdSource::next(const DocumentId& collection_id,
                                 const std::uint64_t persisted_counter) {
  std::uint64_t left = boot_entropy_ ^ persisted_counter;
  std::uint64_t right = ~boot_entropy_ ^ (persisted_counter * 0xD6E8FEB86659FD93ULL);
  for (std::size_t i = 0; i < device_uid_.size(); ++i) {
    left = mix(left ^ (static_cast<std::uint64_t>(device_uid_[i]) << ((i % 8U) * 8U)));
    right = mix(right ^ (static_cast<std::uint64_t>(collection_id.bytes[i]) << ((7U - i % 8U) * 8U)));
  }
  left = mix(left ^ right); right = mix(right ^ left);
  DocumentId id{};
  for (std::size_t i = 0; i < 8U; ++i) {
    id.bytes[i] = static_cast<std::uint8_t>(left >> (i * 8U));
    id.bytes[i + 8U] = static_cast<std::uint8_t>(right >> (i * 8U));
  }
  id.bytes[6] = static_cast<std::uint8_t>((id.bytes[6] & 0x0FU) | 0x40U);
  id.bytes[8] = static_cast<std::uint8_t>((id.bytes[8] & 0x3FU) | 0x80U);
  return id;
}

}  // namespace hanime::documents
