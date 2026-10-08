#pragma once

#include "storage_port.hpp"

#include <array>
#include <cstdint>

namespace hanime::documents {

// Platform-neutral 128-bit ID generator. Device integration supplies the Pico
// unique-board bytes plus a boot entropy sample (ROSC/TRNG source chosen by the
// host). No network or wall clock is required. Catalog persists the counter and
// checks the complete document namespace before accepting an ID.
class EntropyIdSource final : public IdSource {
 public:
  EntropyIdSource(std::array<std::uint8_t, 16> device_uid,
                  std::uint64_t boot_entropy) noexcept
      : device_uid_(device_uid), boot_entropy_(boot_entropy) {}

  DocumentId next(const DocumentId& collection_id,
                  std::uint64_t persisted_counter) override;

 private:
  std::array<std::uint8_t, 16> device_uid_{};
  std::uint64_t boot_entropy_ = 0;
};

}  // namespace hanime::documents
