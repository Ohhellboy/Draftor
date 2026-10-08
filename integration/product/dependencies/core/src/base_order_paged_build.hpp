#pragma once
#include "hanime/core.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace hanime::detail {
// Host-only transformation after the publisher has checked HBO1 against the
// complete HNM/HNS facts. These helpers do not issue a pin or grant eligibility.
std::vector<std::uint8_t> build_paged_base_order(
    const std::vector<std::uint8_t>& validated_hbo1,
    std::size_t* auxiliary_capacity_bytes);
bool check_paged_base_order(const ResourceReader& final_file,
                           const std::vector<std::uint8_t>& validated_hbo1,
                           std::string* error);
}
