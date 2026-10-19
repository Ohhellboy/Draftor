// SPDX-License-Identifier: Apache-2.0
#include "draftor_qr/qr.hpp"
#include "qrcodegen.h"
#include <cstring>

namespace draftor::qr {
namespace {
bool overlaps(const void* a, std::size_t an, const void* b, std::size_t bn) noexcept {
 if (!an || !bn) return false;
 const auto x = reinterpret_cast<std::uintptr_t>(a), y = reinterpret_cast<std::uintptr_t>(b);
 return x <= y ? y - x < an : x - y < bn;
}
int alpha(std::uint8_t c) noexcept {
 constexpr char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
 if (!c) return -1;
 const char* p = std::strchr(alphabet, c);
 return p ? int(p - alphabet) : -1;
}
}
bool Matrix::valid() const noexcept {
 return data && side >= 21 && side <= 177 && (side - 17) % 4 == 0 &&
        bytes >= bufferBytes((side - 17) / 4) && data[0] == side;
}
bool Matrix::module(int x, int y) const noexcept {
 return valid() && x >= 0 && y >= 0 && x < side && y < side && qrcodegen_getModule(data, x, y);
}
Status encode(const std::uint8_t* input, std::size_t length, Mode mode, Options o,
              Buffer scratch, Buffer storage, Matrix& out) noexcept {
 out = {};
 const auto n = bufferBytes(o.max_version);
 if (!n || o.min_version < 1 || o.min_version > o.max_version ||
     int(o.ecc) < 0 || int(o.ecc) > 3 || o.mask < -1 || o.mask > 7 ||
     (mode != Mode::Bytes && mode != Mode::Alphanumeric) || (!input && length))
  return Status::InvalidArgument;
 if (!scratch.data || !storage.data || scratch.size < n || storage.size < n)
  return Status::WorkspaceTooSmall;
 if (overlaps(scratch.data, n, storage.data, n) || overlaps(input, length, scratch.data, n) ||
     overlaps(input, length, storage.data, n)) return Status::InvalidArgument;
 // Absolute QR Model 2 limits, checked before reading unbounded input or
 // converting size_t to the upstream int bit/count fields.
 if (length > (mode == Mode::Bytes ? 2953U : 4296U)) return Status::Capacity;
 const auto bits = mode == Mode::Bytes ? length * 8 : (length / 2) * 11 + (length % 2) * 6;
 if ((bits + 7) / 8 > n) return Status::Capacity;
 if (mode == Mode::Bytes) {
  if (length) std::memcpy(scratch.data, input, length);
 } else {
  for (std::size_t i = 0; i < length; ++i) if (alpha(input[i]) < 0) return Status::InvalidCharacter;
  std::memset(scratch.data, 0, (bits + 7) / 8);
  // Explicit-length version of QR alphanumeric segment packing: one pair
  // contributes 11 bits (45*a+b), a trailing character contributes 6 bits.
  std::size_t at = 0;
  for (std::size_t i = 0; i < length;) {
   unsigned value = unsigned(alpha(input[i++])); int count = 6;
   if (i < length) { value = value * 45 + unsigned(alpha(input[i++])); count = 11; }
   for (int bit = count - 1; bit >= 0; --bit, ++at)
    scratch.data[at / 8] |= std::uint8_t(((value >> bit) & 1U) << (7 - at % 8));
  }
 }
 const qrcodegen_Segment segment{mode == Mode::Bytes ? qrcodegen_Mode_BYTE : qrcodegen_Mode_ALPHANUMERIC,
                               int(length), scratch.data, int(bits)};
 if (!qrcodegen_encodeSegmentsAdvanced(&segment, 1, static_cast<qrcodegen_Ecc>(o.ecc),
     o.min_version, o.max_version, static_cast<qrcodegen_Mask>(o.mask), false, scratch.data, storage.data))
  return Status::Capacity;
 out = {storage.data, bufferBytes((storage.data[0] - 17) / 4), storage.data[0]};
 return Status::Ok;
}
const char* statusText(Status s) noexcept {
 switch (s) {
  case Status::Ok: return "Ready";
  case Status::InvalidArgument: return "QR: invalid input";
  case Status::WorkspaceTooSmall: return "QR: workspace too small";
  case Status::Capacity: return "QR: content too long";
  case Status::InvalidCharacter: return "QR: invalid character";
  case Status::AllocationFailed: return "QR: not enough PSRAM";
 }
 return "QR: invalid status";
}
void erase(void* data, std::size_t bytes) noexcept {
 auto p = static_cast<volatile std::uint8_t*>(data);
 if (p) while (bytes--) *p++ = 0;
}
}
