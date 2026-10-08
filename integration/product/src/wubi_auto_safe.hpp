#pragma once
#include "hanime/verified_resource.hpp"
#include <cstddef>
#include <string_view>

namespace ime_device {
// Borrowed firmware constants, never loaded from a card-side manifest.
// Full release specs bind the bitmap to the activated shared/scheme/BaseOrder.
struct WubiAutoSafe {
    const uint8_t* bits;
    std::size_t bytes;
    const hanime::ResourceReleaseSpec* shared;
    const hanime::ResourceReleaseSpec* scheme;
    const hanime::ResourceReleaseSpec* order;
};
bool sameRelease(const hanime::ResourceReleaseSpec&, const hanime::ResourceReleaseSpec&);
bool wubiAutoSafe(std::string_view code, const WubiAutoSafe*,
                  const hanime::ResourceReleaseSpec* shared,
                  const hanime::ResourceReleaseSpec* scheme,
                  const hanime::ResourceReleaseSpec* order);
} // namespace ime_device
