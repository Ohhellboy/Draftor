#pragma once

#include <cstdint>
#include <string_view>

namespace hanime::find::display::bar_layout {

// Two fixed rows: query/status above, explicit case rule/shortcut below.
// Coordinates are relative to the top of the FindBar.
inline constexpr std::uint16_t kWidth = 300U;
inline constexpr std::uint16_t kHeight = 68U;
inline constexpr std::uint16_t kContentLeft = 8U;
inline constexpr std::uint16_t kContentRight = 292U;
inline constexpr std::uint16_t kRowSeparatorY = 34U;

inline constexpr std::uint16_t kQueryX = 20U;
inline constexpr std::uint16_t kQueryRight = 216U;
inline constexpr std::uint16_t kQueryGlyphY = 3U;
inline constexpr std::uint16_t kQueryCursorFirstY = 4U;
inline constexpr std::uint16_t kQueryCursorLastY = 30U;
inline constexpr std::uint16_t kStatusX = 220U;
inline constexpr std::uint16_t kStatusRight = 296U;
inline constexpr std::uint16_t kStatusY = 4U;

inline constexpr std::uint16_t kCaseLabelX = 8U;
inline constexpr std::uint16_t kCaseLabelRight = 180U;
inline constexpr std::uint16_t kCaseLabelY = 37U;
inline constexpr std::uint16_t kCaseShortcutX = 208U;
inline constexpr std::uint16_t kCaseShortcutRight = 292U;
inline constexpr std::uint16_t kCaseShortcutY = 44U;

inline constexpr std::string_view kCaseInsensitiveLabel = "忽略大小写";
inline constexpr std::string_view kCaseSensitiveLabel = "区分大小写";
inline constexpr std::string_view kCaseShortcut = "Ctrl+I";

constexpr std::string_view caseRuleLabel(const bool case_sensitive) noexcept {
  return case_sensitive ? kCaseSensitiveLabel : kCaseInsensitiveLabel;
}

}  // namespace hanime::find::display::bar_layout
