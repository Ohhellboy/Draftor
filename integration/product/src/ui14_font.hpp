#pragma once
#include <cstdint>
#include <cstddef>
namespace s3app::ui14 {
struct Page {uint16_t page,first;uint8_t mask[32];};
static_assert(sizeof(Page)==36,"UI14 index must remain packed");
extern const Page pages[];
extern const uint8_t glyphBits[];
extern const uint8_t missingBits[28];
inline constexpr size_t PageCount=91,GlyphCount=20507,PackedGlyphBytes=25;

// Writes exactly 28 bytes; no allocation, file access, initialization or cache.
bool lookup(uint32_t cp,uint8_t* out) noexcept;
}
