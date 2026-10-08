#pragma once

#include "hanime/core.hpp"

namespace hanime::fixture {

constexpr RegistryNamespaceId kRegistryNamespace =
    0x48414e494d453242ULL;  // Explicit isolated Step2B test registry, not production.
// Manually registered full values; the first eight bytes are deliberately equal.
constexpr StableLexemeKey kPrototype{{0x48,0x41,0x4e,0x49,0x4d,0x45,0x32,0x42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}};
constexpr StableLexemeKey kDefaultOnly{{0x48,0x41,0x4e,0x49,0x4d,0x45,0x32,0x42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2}};
constexpr StableLexemeKey kFullPinyinOnly{{0x48,0x41,0x4e,0x49,0x4d,0x45,0x32,0x42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3}};
constexpr StableLexemeKey kSameTextA{{0x48,0x41,0x4e,0x49,0x4d,0x45,0x32,0x42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4}};
constexpr StableLexemeKey kSameTextB{{0x48,0x41,0x4e,0x49,0x4d,0x45,0x32,0x42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5}};
constexpr StableLexemeKey kSupplementaryCharacter{{0x48,0x41,0x4e,0x49,0x4d,0x45,0x32,0x42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6}};
constexpr StableLexemeKey kMultipleVariantForms{{0x48,0x41,0x4e,0x49,0x4d,0x45,0x32,0x42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7}};

ResourceBuildInput make_small_input(bool reverse_approved_lexeme_order);

}  // namespace hanime::fixture
