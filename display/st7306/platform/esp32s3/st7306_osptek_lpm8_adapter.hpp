#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace st7306_osptek_lpm8 {

inline constexpr int kPanelWidth = 300;
inline constexpr int kPanelHeight = 400;
inline constexpr std::size_t kFramebufferSize = 30000U;

// Keep HANIME's logical color identity stable. The device-passed donor maps
// logical Red/White to the opposite two-bit wire codes at the adapter boundary.
enum class Color : std::uint8_t {
  Off = 0,
  Red = 1,
  White = 2,
  Black = 3,
};

constexpr std::uint8_t logicalColorToWire(Color color) {
  const auto logical = static_cast<std::uint8_t>(color);
  return static_cast<std::uint8_t>(((logical & 0x01U) << 1U) |
                                   ((logical & 0x02U) >> 1U));
}

constexpr Color wireColorToLogical(std::uint8_t wire) {
  wire &= 0x03U;
  return static_cast<Color>(((wire & 0x01U) << 1U) |
                            ((wire & 0x02U) >> 1U));
}

constexpr int physicalXFromLogical(int logical_x) {
  return kPanelWidth - 1 - logical_x;
}

constexpr int physicalYFromLogical(int logical_y) {
  return kPanelHeight - 1 - logical_y;
}

struct PackedPixel {
  std::size_t byte_index;
  std::uint8_t bit1;
  std::uint8_t bit0;
};

constexpr PackedPixel packedPixelForLogical(int logical_x, int logical_y) {
  const int physical_x = physicalXFromLogical(logical_x);
  const int physical_y = physicalYFromLogical(logical_y);
  const auto bit1 = static_cast<std::uint8_t>(
      7 - ((physical_x & 1) * 4 + (physical_y & 1)));
  return {
      static_cast<std::size_t>(physical_y / 2) * (kPanelWidth / 2) +
          static_cast<std::size_t>(physical_x / 2),
      bit1,
      static_cast<std::uint8_t>(bit1 - 2U),
  };
}

constexpr std::uint8_t repeatedColorByte(Color color) {
  const auto wire = logicalColorToWire(color);
  return static_cast<std::uint8_t>(((wire & 0x02U) ? 0xCCU : 0x00U) |
                                   ((wire & 0x01U) ? 0x33U : 0x00U));
}

inline void fillFramebuffer(std::uint8_t* framebuffer, Color color) {
  std::memset(framebuffer, repeatedColorByte(color), kFramebufferSize);
}

inline void writeLogicalPixel(std::uint8_t* framebuffer, int x, int y,
                              Color color) {
  if (x < 0 || x >= kPanelWidth || y < 0 || y >= kPanelHeight) return;
  const auto packed = packedPixelForLogical(x, y);
  const auto wire = logicalColorToWire(color);
  const auto mask = static_cast<std::uint8_t>(
      (1U << packed.bit1) | (1U << packed.bit0));
  const auto bits = static_cast<std::uint8_t>(
      ((wire & 0x02U) ? (1U << packed.bit1) : 0U) |
      ((wire & 0x01U) ? (1U << packed.bit0) : 0U));
  framebuffer[packed.byte_index] = static_cast<std::uint8_t>(
      (framebuffer[packed.byte_index] & static_cast<std::uint8_t>(~mask)) |
      bits);
}

inline Color readLogicalPixel(const std::uint8_t* framebuffer, int x, int y) {
  if (x < 0 || x >= kPanelWidth || y < 0 || y >= kPanelHeight) {
    return Color::Off;
  }
  const auto packed = packedPixelForLogical(x, y);
  const auto byte = framebuffer[packed.byte_index];
  const auto wire = static_cast<std::uint8_t>(
      (((byte >> packed.bit1) & 1U) << 1U) |
      ((byte >> packed.bit0) & 1U));
  return wireColorToLogical(wire);
}

static_assert(logicalColorToWire(Color::Off) == 0U);
static_assert(logicalColorToWire(Color::Red) == 2U);
static_assert(logicalColorToWire(Color::White) == 1U);
static_assert(logicalColorToWire(Color::Black) == 3U);
static_assert(kPanelWidth * kPanelHeight / 4 ==
              static_cast<int>(kFramebufferSize));

}  // namespace st7306_osptek_lpm8
