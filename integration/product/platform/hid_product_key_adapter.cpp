#include "hid_product_key_adapter.hpp"

namespace integration::input_host {
namespace {
constexpr std::uint8_t kHidEnter = 0x28;
constexpr std::uint8_t kHidEscape = 0x29;
constexpr std::uint8_t kHidBackspace = 0x2A;
constexpr std::uint8_t kHidTab = 0x2B;
constexpr std::uint8_t kHidSpace = 0x2C;
constexpr std::uint8_t kHidCapsLock = 0x39;
constexpr std::uint8_t kHidHome = 0x4A;
constexpr std::uint8_t kHidDelete = 0x4C;
constexpr std::uint8_t kHidEnd = 0x4D;
constexpr std::uint8_t kHidRight = 0x4F;
constexpr std::uint8_t kHidLeft = 0x50;
constexpr std::uint8_t kHidDown = 0x51;
constexpr std::uint8_t kHidUp = 0x52;
}  // namespace

bool normalizeProductHidKey(const std::uint8_t raw_usage,
                            const char translated_ascii,
                            const std::uint8_t modifiers,
                            editor::input::NormalizedKey* out) noexcept {
  if (out == nullptr) return false;

  using editor::input::KeyKind;
  using editor::input::NormalizedKey;

  switch (raw_usage) {
    case kHidEnter: *out = NormalizedKey::special(KeyKind::Enter, modifiers); return true;
    case kHidEscape: *out = NormalizedKey::special(KeyKind::Escape, modifiers); return true;
    case kHidBackspace: *out = NormalizedKey::special(KeyKind::Backspace, modifiers); return true;
    case kHidTab: *out = NormalizedKey::special(KeyKind::Tab, modifiers); return true;
    case kHidSpace: *out = NormalizedKey::special(KeyKind::Space, modifiers); return true;
    case kHidCapsLock: *out = NormalizedKey::special(KeyKind::ToggleInputMode, modifiers); return true;
    case kHidHome: *out = NormalizedKey::special(KeyKind::Home, modifiers); return true;
    case kHidDelete: *out = NormalizedKey::special(KeyKind::DeleteForward, modifiers); return true;
    case kHidEnd: *out = NormalizedKey::special(KeyKind::End, modifiers); return true;
    case kHidRight: *out = NormalizedKey::special(KeyKind::Right, modifiers); return true;
    case kHidLeft: *out = NormalizedKey::special(KeyKind::Left, modifiers); return true;
    case kHidDown: *out = NormalizedKey::special(KeyKind::Down, modifiers); return true;
    case kHidUp: *out = NormalizedKey::special(KeyKind::Up, modifiers); return true;
    default: break;
  }

  char normalized_ascii = translated_ascii;
  if ((modifiers & (editor::input::ModifierCtrl |
                    editor::input::ModifierGui)) != 0U &&
      raw_usage >= 0x04U && raw_usage <= 0x1DU) {
    normalized_ascii = static_cast<char>('a' + raw_usage - 0x04U);
  }
  const auto byte = static_cast<unsigned char>(normalized_ascii);
  if (byte >= 0x20U && byte <= 0x7EU) {
    *out = NormalizedKey::character(normalized_ascii, modifiers);
    return true;
  }
  return false;
}

}  // namespace integration::input_host
