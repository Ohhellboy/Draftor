#pragma once

#include <cstddef>
#include <cstdint>

namespace st7306_osptek_lpm8 {

enum class InitStepKind : std::uint8_t {
  Command,
  Data,
  DelayMs,
};

struct InitStep {
  InitStepKind kind;
  const std::uint8_t* bytes;
  std::size_t size;
  std::uint16_t value;
};

// This concrete transaction table is the Arduino port of the device-passed
// ST7306_OsptekOfficialInit_LPM8HzTrial_v0.1 sequence. Data chunks deliberately
// preserve the donor's CS/DC transaction boundaries, not only its byte stream.
inline constexpr std::uint8_t kD6[] = {0x17, 0x02};
inline constexpr std::uint8_t kD1[] = {0x01};
inline constexpr std::uint8_t kC0[] = {0x12, 0x0A};
inline constexpr std::uint8_t kC1[] = {0x73, 0x3E, 0x3C, 0x3C};
inline constexpr std::uint8_t kC2[] = {0x00, 0x21, 0x23, 0x23};
inline constexpr std::uint8_t kC4[] = {0x32, 0x5C, 0x5A, 0x5A};
inline constexpr std::uint8_t kC5[] = {0x32, 0x35, 0x37, 0x37};
inline constexpr std::uint8_t kD8[] = {0xA6, 0xE9};
inline constexpr std::uint8_t kB2[] = {0x15};
inline constexpr std::uint8_t kB3[] = {
    0xE5, 0xF6, 0x17, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x71};
inline constexpr std::uint8_t kB4[] = {
    0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45};
inline constexpr std::uint8_t k62[] = {0x32, 0x03, 0x1F};
inline constexpr std::uint8_t kB7[] = {0x13};
inline constexpr std::uint8_t kB0[] = {0x64};
inline constexpr std::uint8_t kC9[] = {0x00};
inline constexpr std::uint8_t k36[] = {0x48};
inline constexpr std::uint8_t k3A[] = {0x11};
inline constexpr std::uint8_t kB9[] = {0x20};
inline constexpr std::uint8_t kB8[] = {0x29};
inline constexpr std::uint8_t k2AFirst[] = {0x05};
inline constexpr std::uint8_t k2ALast[] = {0x36};
inline constexpr std::uint8_t k2BFirst[] = {0x00};
inline constexpr std::uint8_t k2BLast[] = {0xC7};
inline constexpr std::uint8_t k35[] = {0x00};
inline constexpr std::uint8_t kD0[] = {0xFF};
inline constexpr std::uint8_t kBB[] = {0x4F};

#define ST7306_COMMAND_STEP(value) \
  {InitStepKind::Command, nullptr, 0U, static_cast<std::uint16_t>(value)}
#define ST7306_DATA_STEP(value) \
  {InitStepKind::Data, value, sizeof(value), 0U}
#define ST7306_DELAY_STEP(value) \
  {InitStepKind::DelayMs, nullptr, 0U, static_cast<std::uint16_t>(value)}

inline constexpr InitStep kInitSteps[] = {
    ST7306_COMMAND_STEP(0xD6),
    {InitStepKind::Data, kD6, 1U, 0U},
    {InitStepKind::Data, kD6 + 1U, 1U, 0U},
    ST7306_COMMAND_STEP(0xD1), ST7306_DATA_STEP(kD1),
    ST7306_COMMAND_STEP(0xC0), ST7306_DATA_STEP(kC0),
    ST7306_COMMAND_STEP(0xC1), ST7306_DATA_STEP(kC1),
    ST7306_COMMAND_STEP(0xC2), ST7306_DATA_STEP(kC2),
    ST7306_COMMAND_STEP(0xC4), ST7306_DATA_STEP(kC4),
    ST7306_COMMAND_STEP(0xC5), ST7306_DATA_STEP(kC5),
    ST7306_COMMAND_STEP(0xD8), ST7306_DATA_STEP(kD8),
    ST7306_COMMAND_STEP(0xB2), ST7306_DATA_STEP(kB2),
    ST7306_COMMAND_STEP(0xB3), ST7306_DATA_STEP(kB3),
    ST7306_COMMAND_STEP(0xB4), ST7306_DATA_STEP(kB4),
    ST7306_COMMAND_STEP(0x62), ST7306_DATA_STEP(k62),
    ST7306_COMMAND_STEP(0xB7), ST7306_DATA_STEP(kB7),
    ST7306_COMMAND_STEP(0xB0), ST7306_DATA_STEP(kB0),
    ST7306_COMMAND_STEP(0x11), ST7306_DELAY_STEP(120),
    ST7306_COMMAND_STEP(0xC9), ST7306_DATA_STEP(kC9),
    ST7306_COMMAND_STEP(0x36), ST7306_DATA_STEP(k36),
    ST7306_COMMAND_STEP(0x3A), ST7306_DATA_STEP(k3A),
    ST7306_COMMAND_STEP(0xB9), ST7306_DATA_STEP(kB9),
    ST7306_COMMAND_STEP(0xB8), ST7306_DATA_STEP(kB8),
    ST7306_COMMAND_STEP(0x2A), ST7306_DATA_STEP(k2AFirst),
    ST7306_DATA_STEP(k2ALast),
    ST7306_COMMAND_STEP(0x2B), ST7306_DATA_STEP(k2BFirst),
    ST7306_DATA_STEP(k2BLast),
    ST7306_COMMAND_STEP(0x35), ST7306_DATA_STEP(k35),
    ST7306_COMMAND_STEP(0xD0), ST7306_DATA_STEP(kD0),
    ST7306_COMMAND_STEP(0x39),
    ST7306_COMMAND_STEP(0x29),
    ST7306_COMMAND_STEP(0x20),
    ST7306_COMMAND_STEP(0xBB), ST7306_DATA_STEP(kBB),
};

inline constexpr std::size_t kInitStepCount =
    sizeof(kInitSteps) / sizeof(kInitSteps[0]);

#undef ST7306_COMMAND_STEP
#undef ST7306_DATA_STEP
#undef ST7306_DELAY_STEP

}  // namespace st7306_osptek_lpm8
