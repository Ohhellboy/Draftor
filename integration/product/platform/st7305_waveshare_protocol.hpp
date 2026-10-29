#pragma once
// Waveshare d8bf7813d3b53251577ff03c03596ad6ad3acae0; Apache-2.0.
// Portrait, 0x38 high power mode, vendor B2=02; no low-refresh transition or TE wait.
#include <cstddef>
#include <cstdint>
namespace st7305_waveshare {
enum class InitStepKind {Command,Data,DelayMs};
struct InitStep {InitStepKind kind;unsigned value;const std::uint8_t* bytes;std::size_t size;};
inline constexpr std::uint8_t d6[]={ 0x17, 0x02 };
inline constexpr std::uint8_t d1[]={ 0x01 };
inline constexpr std::uint8_t c0[]={ 0x11, 0x04 };
inline constexpr std::uint8_t c1[]={ 0x69, 0x69, 0x69, 0x69 };
inline constexpr std::uint8_t c2[]={ 0x19, 0x19, 0x19, 0x19 };
inline constexpr std::uint8_t c4[]={ 0x4B, 0x4B, 0x4B, 0x4B };
inline constexpr std::uint8_t d8[]={ 0x80, 0xE9 };
inline constexpr std::uint8_t b2[]={ 0x02 };
inline constexpr std::uint8_t b3[]={ 0xE5, 0xF6, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45 };
inline constexpr std::uint8_t b4[]={ 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45 };
inline constexpr std::uint8_t g_timing[]={ 0x32, 0x03, 0x1F };
inline constexpr std::uint8_t b7[]={ 0x13 };
inline constexpr std::uint8_t b0[]={ 0x64 };
inline constexpr std::uint8_t c9[]={ 0x00 };
inline constexpr std::uint8_t m36[]={ 0x48 };
inline constexpr std::uint8_t m3a[]={ 0x11 };
inline constexpr std::uint8_t b9[]={ 0x20 };
inline constexpr std::uint8_t b8[]={ 0x29 };
inline constexpr std::uint8_t win_a[]={ 0x12, 0x2A };
inline constexpr std::uint8_t win_b[]={ 0x00, 0xC7 };
inline constexpr std::uint8_t m35[]={ 0x00 };
inline constexpr std::uint8_t d0[]={ 0xFF };
inline constexpr InitStep kInitSteps[]={
{InitStepKind::Command,0xD6,nullptr,0},
{InitStepKind::Data,0,d6,sizeof(d6)},
{InitStepKind::Command,0xD1,nullptr,0},
{InitStepKind::Data,0,d1,sizeof(d1)},
{InitStepKind::Command,0xC0,nullptr,0},
{InitStepKind::Data,0,c0,sizeof(c0)},
{InitStepKind::Command,0xC1,nullptr,0},
{InitStepKind::Data,0,c1,sizeof(c1)},
{InitStepKind::Command,0xC2,nullptr,0},
{InitStepKind::Data,0,c2,sizeof(c2)},
{InitStepKind::Command,0xC4,nullptr,0},
{InitStepKind::Data,0,c4,sizeof(c4)},
{InitStepKind::Command,0xC5,nullptr,0},
{InitStepKind::Data,0,c2,sizeof(c2)},
{InitStepKind::Command,0xD8,nullptr,0},
{InitStepKind::Data,0,d8,sizeof(d8)},
{InitStepKind::Command,0xB2,nullptr,0},
{InitStepKind::Data,0,b2,sizeof(b2)},
{InitStepKind::Command,0xB3,nullptr,0},
{InitStepKind::Data,0,b3,sizeof(b3)},
{InitStepKind::Command,0xB4,nullptr,0},
{InitStepKind::Data,0,b4,sizeof(b4)},
{InitStepKind::Command,0x62,nullptr,0},
{InitStepKind::Data,0,g_timing,sizeof(g_timing)},
{InitStepKind::Command,0xB7,nullptr,0},
{InitStepKind::Data,0,b7,sizeof(b7)},
{InitStepKind::Command,0xB0,nullptr,0},
{InitStepKind::Data,0,b0,sizeof(b0)},
{InitStepKind::Command,0x11,nullptr,0},
{InitStepKind::DelayMs,120,nullptr,0},
{InitStepKind::Command,0xC9,nullptr,0},
{InitStepKind::Data,0,c9,sizeof(c9)},
{InitStepKind::Command,0x36,nullptr,0},
{InitStepKind::Data,0,m36,sizeof(m36)},
{InitStepKind::Command,0x3A,nullptr,0},
{InitStepKind::Data,0,m3a,sizeof(m3a)},
{InitStepKind::Command,0xB9,nullptr,0},
{InitStepKind::Data,0,b9,sizeof(b9)},
{InitStepKind::Command,0xB8,nullptr,0},
{InitStepKind::Data,0,b8,sizeof(b8)},
{InitStepKind::Command,0x21,nullptr,0},
{InitStepKind::Command,0x2A,nullptr,0},
{InitStepKind::Data,0,win_a,sizeof(win_a)},
{InitStepKind::Command,0x2B,nullptr,0},
{InitStepKind::Data,0,win_b,sizeof(win_b)},
{InitStepKind::Command,0x35,nullptr,0},
{InitStepKind::Data,0,m35,sizeof(m35)},
{InitStepKind::Command,0xD0,nullptr,0},
{InitStepKind::Data,0,d0,sizeof(d0)},
{InitStepKind::Command,0x38,nullptr,0},
{InitStepKind::Command,0x29,nullptr,0}
};
inline constexpr auto kInitStepCount=sizeof(kInitSteps)/sizeof(kInitSteps[0]);
}
