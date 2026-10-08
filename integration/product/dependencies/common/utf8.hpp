#pragma once

#include <cstddef>
#include <string_view>

namespace editor {

bool isValidUtf8(std::string_view text) noexcept;
bool isUtf8Boundary(std::string_view text, std::size_t offset) noexcept;
std::size_t nextUtf8Boundary(std::string_view text, std::size_t offset) noexcept;
std::size_t previousUtf8Boundary(std::string_view text, std::size_t offset) noexcept;
std::size_t utf8CodePointCount(std::string_view text) noexcept;

}  // namespace editor
