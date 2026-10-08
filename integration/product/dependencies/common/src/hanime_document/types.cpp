#include "../../core_memory.hpp"
#include "types.hpp"
#include "../../operation_profile.hpp"

#include <algorithm>

namespace hanime::documents {

const char* statusMessage(const Status status) noexcept {
  switch (status) {
    case Status::Ok: return "OK";
    case Status::OkDegraded: return "OK_DEGRADED";
    case Status::Partial: return "PARTIAL";
    case Status::NotFound: return "NOT_FOUND";
    case Status::MediaUnavailable: return "MEDIA_UNAVAILABLE";
    case Status::SessionChanged: return "SESSION_CHANGED";
    case Status::IoError: return "IO_ERROR";
    case Status::Corrupt: return "CORRUPT";
    case Status::UnsupportedVersion: return "UNSUPPORTED_VERSION";
    case Status::TooLarge: return "TOO_LARGE";
    case Status::InvalidUtf8: return "INVALID_UTF8";
    case Status::InvalidCursor: return "INVALID_CURSOR";
    case Status::InvalidName: return "INVALID_NAME";
    case Status::Conflict: return "CONFLICT";
    case Status::Busy: return "BUSY";
    case Status::CompositionActive: return "COMPOSITION_ACTIVE";
    case Status::NoCurrentDocument: return "NO_CURRENT_DOCUMENT";
    case Status::NoManualVersion: return "NO_MANUAL_VERSION";
    case Status::ConfirmationRequired: return "CONFIRMATION_REQUIRED";
    case Status::StaleToken: return "STALE_TOKEN";
    case Status::CapacityReached: return "CAPACITY_REACHED";
    case Status::NameExists: return "Name exists - choose another";
    case Status::FirstPage: return "Already at first page";
    case Status::LastPage: return "Already at last page";
    case Status::ResourceExhausted: return "RESOURCE_EXHAUSTED";
    case Status::InstallFailed: return "INSTALL_FAILED";
  }
  return "UNKNOWN";
}

bool DocumentId::empty() const noexcept {
  return std::all_of(bytes.begin(), bytes.end(), [](const std::uint8_t b) {
    return b == 0U;
  });
}

draftor::memory::String DocumentId::hex() const {
  static constexpr char kHex[] = "0123456789abcdef";
  draftor::memory::String out(32U, '0');
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    out[i * 2U] = kHex[bytes[i] >> 4U];
    out[i * 2U + 1U] = kHex[bytes[i] & 0x0FU];
  }
  return out;
}

draftor::memory::String DocumentId::shortHex() const { return hex().substr(0U, 8U); }

bool DocumentId::fromHex(const std::string_view value, DocumentId* out) noexcept {
  if (out == nullptr || value.size() != 32U) return false;
  auto nibble = [](const char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
  };
  DocumentId parsed{};
  for (std::size_t i = 0; i < parsed.bytes.size(); ++i) {
    const int high = nibble(value[i * 2U]);
    const int low = nibble(value[i * 2U + 1U]);
    if (high < 0 || low < 0) return false;
    parsed.bytes[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  if (parsed.empty()) return false;
  *out = parsed;
  return true;
}

std::uint32_t crc32(const std::string_view bytes) noexcept {
  s3profile::Scope timing(s3profile::Verify);if(s3profile::active)s3profile::record.crc_bytes+=bytes.size();
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const unsigned char ch : bytes) {
    crc ^= static_cast<std::uint32_t>(ch);
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

std::uint32_t crc32Parts(const std::string_view* parts,
                         const std::size_t part_count) noexcept {
  s3profile::Scope timing(s3profile::Verify);
  if (parts == nullptr && part_count != 0U) return 0U;
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t part = 0; part < part_count; ++part) {
    if(s3profile::active)s3profile::record.crc_bytes+=parts[part].size();
    for (const unsigned char ch : parts[part]) {
      crc ^= static_cast<std::uint32_t>(ch);
      for (int bit = 0; bit < 8; ++bit) {
        const std::uint32_t mask = 0U - (crc & 1U);
        crc = (crc >> 1U) ^ (0xEDB88320U & mask);
      }
    }
  }
  return ~crc;
}

bool isUtf8Boundary(const std::string_view text, const std::size_t offset) noexcept {
  return offset <= text.size() &&
         (offset == text.size() ||
          (static_cast<unsigned char>(text[offset]) & 0xC0U) != 0x80U);
}

bool isValidUtf8(const std::string_view text) noexcept {
  s3profile::Scope timing(s3profile::Verify);
  std::size_t i = 0;
  while (i < text.size()) {
    const auto b0 = static_cast<unsigned char>(text[i]);
    std::size_t count = 0;
    std::uint32_t codepoint = 0;
    if (b0 <= 0x7FU) {
      ++i;
      continue;
    } else if ((b0 & 0xE0U) == 0xC0U) {
      count = 2U; codepoint = b0 & 0x1FU;
      if (codepoint == 0U) return false;
    } else if ((b0 & 0xF0U) == 0xE0U) {
      count = 3U; codepoint = b0 & 0x0FU;
    } else if ((b0 & 0xF8U) == 0xF0U) {
      count = 4U; codepoint = b0 & 0x07U;
    } else {
      return false;
    }
    if (i + count > text.size()) return false;
    for (std::size_t j = 1; j < count; ++j) {
      const auto bx = static_cast<unsigned char>(text[i + j]);
      if ((bx & 0xC0U) != 0x80U) return false;
      codepoint = (codepoint << 6U) | (bx & 0x3FU);
    }
    if ((count == 2U && codepoint < 0x80U) ||
        (count == 3U && codepoint < 0x800U) ||
        (count == 4U && codepoint < 0x10000U) ||
        codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
      return false;
    }
    i += count;
  }
  return true;
}

}  // namespace hanime::documents
