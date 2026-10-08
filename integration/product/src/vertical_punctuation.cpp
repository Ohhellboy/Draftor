#include "draftor/vertical_punctuation.hpp"
namespace draftor::vertical {
PunctuationForm punctuationForm(const std::uint32_t cp, const PunctuationPolicy policy) noexcept {
  switch (cp) {
    case 0xff0c:
      return {0xfe10, 18, -15, false};
    case 0x3002:
      return {0xfe12, 17, -14, false};
    case 0x3001:
      return policy == PunctuationPolicy::CommonVertical ? PunctuationForm{0xfe11, 18, -15, false} : PunctuationForm{};
    case 0xff01:
      return {0xfe15, 9, 0, false};
    case 0xff1f:
      return {0xfe16, 8, 0, false};
    case 0xff1a:
      return {0xfe13, 8, 0, false};
    case 0xff1b:
      return {0xfe14, 8, 0, false};
    case 0x300c:
      return {0xfe41, 0, 0, true};
    case 0x300d:
      return {0xfe42, 0, 0, true};
    case 0x300e:
      return {0xfe43, 0, 0, true};
    case 0x300f:
      return {0xfe44, 0, 0, true};
    case 0xff08:
      return {0xfe35, 0, 0, true};
    case 0xff09:
      return {0xfe36, 0, 0, true};
    case 0x3010:
      return {0xfe3b, 0, 0, true};
    case 0x3011:
      return {0xfe3c, 0, 0, true};
    case 0x300a:
      return {0xfe3d, 0, 0, true};
    case 0x300b:
      return {0xfe3e, 0, 0, true};
    case 0x2026:
      return {0xfe19, 6, 5, true};
    case 0x2014:
      return policy == PunctuationPolicy::CommonVertical ? PunctuationForm{0xfe31, 0, 0, true} : PunctuationForm{};
    case 0x2018:
      return policy == PunctuationPolicy::CommonVertical ? PunctuationForm{0xfe41, 0, 0, true, 0x300c} : PunctuationForm{};
    case 0x2019:
      return policy == PunctuationPolicy::CommonVertical ? PunctuationForm{0xfe42, 0, 0, true, 0x300d} : PunctuationForm{};
    case 0x201c:
      return policy == PunctuationPolicy::CommonVertical ? PunctuationForm{0xfe43, 0, 0, true, 0x300e} : PunctuationForm{};
    case 0x201d:
      return policy == PunctuationPolicy::CommonVertical ? PunctuationForm{0xfe44, 0, 0, true, 0x300f} : PunctuationForm{};
    default:
      return {};
  }
}
}  // namespace draftor::vertical
