#pragma once
#include <array>
#include <cstdint>
namespace r1 {
inline constexpr std::array<uint32_t,4> BitmapCounts{145061,100131,81603,242979};
inline constexpr std::array<uint32_t,5> BitmapOffsets{0,4534,7664,10215,17809};
inline constexpr uint32_t BitmapWords=17809;
inline constexpr uint32_t BitmapBytes=BitmapWords*4;
inline constexpr uint32_t JournalLimit=8192;
inline constexpr uint32_t DirtyWordLimit=256;
}
