#pragma once

#include "input_router.hpp"

#include <cstdint>

namespace integration::input_host {

// HID modifier usages E0..E7 are stored independently. This preserves the
// semantic class when one of two simultaneously-held left/right keys is
// released. Both helpers are pure so queue/callback behavior can be host-tested.
constexpr std::uint8_t updatePhysicalModifierUsages(
    std::uint8_t held,
    const std::uint8_t raw_usage,
    const bool down) noexcept {
  if (raw_usage < 0xE0U || raw_usage > 0xE7U) return held;
  const auto usage_bit = static_cast<std::uint8_t>(
      1U << static_cast<std::uint8_t>(raw_usage - 0xE0U));
  return down ? static_cast<std::uint8_t>(held | usage_bit)
              : static_cast<std::uint8_t>(held &
                                          static_cast<std::uint8_t>(~usage_bit));
}

constexpr std::uint8_t semanticModifiersForPhysicalUsages(
    const std::uint8_t held) noexcept {
  std::uint8_t result = editor::input::ModifierNone;
  if ((held & ((1U << 1U) | (1U << 5U))) != 0U) {
    result |= editor::input::ModifierShift;
  }
  if ((held & ((1U << 0U) | (1U << 4U))) != 0U) {
    result |= editor::input::ModifierCtrl;
  }
  if ((held & ((1U << 2U) | (1U << 6U))) != 0U) {
    result |= editor::input::ModifierAlt;
  }
  if ((held & ((1U << 3U) | (1U << 7U))) != 0U) {
    result |= editor::input::ModifierGui;
  }
  return result;
}

// Normalize product-facing HID keyboard events into the hardware-neutral
// C2 InputRouter key vocabulary. Tab and occurrence-time modifiers are product
// input; engineering function keys, bare modifier usages, and unsupported HID
// usages remain owned by the Arduino integration shell.
bool normalizeProductHidKey(std::uint8_t raw_usage,
                            char translated_ascii,
                            std::uint8_t modifiers,
                            editor::input::NormalizedKey* out) noexcept;

inline bool normalizeProductHidKey(
    const std::uint8_t raw_usage,
    const char translated_ascii,
    editor::input::NormalizedKey* const out) noexcept {
  return normalizeProductHidKey(
      raw_usage, translated_ascii, editor::input::ModifierNone, out);
}

}  // namespace integration::input_host
