// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ruotao Wang
#pragma once
#include <cstddef>
#include <cstdint>

namespace draftor::qr {
enum class Status { Ok, InvalidArgument, WorkspaceTooSmall, Capacity, InvalidCharacter, AllocationFailed };
enum class Ecc { Low, Medium, Quartile, High };
enum class Mode { Bytes, Alphanumeric };
struct Options { int min_version; int max_version; Ecc ecc; int mask = -1; };
struct Buffer { std::uint8_t* data; std::size_t size; };
struct Matrix {
 const std::uint8_t* data = nullptr;
 std::size_t bytes = 0;
 int side = 0;
 bool valid() const noexcept;
 bool module(int x, int y) const noexcept;
};
constexpr std::size_t bufferBytes(int version) noexcept {
 return version < 1 || version > 40 ? 0 : 1 + ((17 + 4 * version) * (17 + 4 * version) + 7) / 8;
}
// Owner supplies two disjoint buffers of bufferBytes(max_version) bytes and
// retains matrix storage for the lifetime of the returned read-only view.
// Input must not overlap either workspace. No allocation, globals or ECI/text
// conversion. Bytes accepts NUL; alphanumeric preserves significant spaces.
// ECC is exact (no boost or downgrade), and versions never exceed the caller's
// range. On any failure out is empty, so an old matrix cannot remain published.
Status encode(const std::uint8_t* input, std::size_t length, Mode mode,
              Options options, Buffer scratch, Buffer matrix, Matrix& out) noexcept;
const char* statusText(Status) noexcept;
void erase(void* data, std::size_t bytes) noexcept;
}
