#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <cstddef>
inline std::size_t checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (false)
inline bool bytesEqual(std::string_view a, std::string_view b) {
  return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}
inline const std::string bom = "\xef\xbb\xbf";
inline const std::string semantic =
    u8"2026 二〇二六 简体 繁體 𨿍\nv12 @2026-09-24@ 123.45";

