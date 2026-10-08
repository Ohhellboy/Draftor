#pragma once
#include "hanime_extension/extension.hpp"
#include "hanime/composition_commit.hpp"

namespace ime_device {
// One configuration creates both the live Composition and its loading buffer.
// Admission does not call edit_raw: that API returns Busy before checking size.
inline constexpr hanime_extension::Config DeviceCompositionConfig{};
// S3 assembly only. Library defaults, raw, text and planning budgets are unchanged.
constexpr hanime_extension::Config compositionConfig(hanime_extension::SchemeRules rules) {
    auto config = DeviceCompositionConfig;
    config.members = rules == hanime_extension::SchemeRules::DirectExact ? 1 :
        rules == hanime_extension::SchemeRules::R1Xiaohe ? 32 : 64;
    return config;
}
constexpr hanime::CompositionConfig submissionConfig(const hanime_extension::Config& config) {
    return {config.members, config.name_bytes, config.word_bytes, config.text_bytes,
            hanime::CompositionLearningPolicy::Disabled};
}
constexpr bool compatibleCapacity(const hanime_extension::Config& input,
                                  const hanime::CompositionConfig& output) {
    return input.members && input.members <= 64 && input.members == output.max_members &&
        input.name_bytes == output.form_bytes && input.word_bytes == output.member_text_bytes &&
        input.text_bytes == output.total_text_bytes && input.raw_bytes == 64 &&
        output.learning == hanime::CompositionLearningPolicy::Disabled;
}
static_assert(compatibleCapacity(compositionConfig(hanime_extension::SchemeRules::R1Xiaohe),
                                submissionConfig(compositionConfig(hanime_extension::SchemeRules::R1Xiaohe))));
static_assert(compatibleCapacity(compositionConfig(hanime_extension::SchemeRules::FullPinyin),
                                submissionConfig(compositionConfig(hanime_extension::SchemeRules::FullPinyin))));
enum class RawAdmission { Edit, NoChange, Invalid, Capacity };

inline RawAdmission admitRawEdit(size_t size, size_t begin, size_t end, std::string_view text) {
    if (begin > end || end > size) return RawAdmission::Invalid;
    if (begin == end && text.empty()) return RawAdmission::NoChange;
    const auto retained = size - (end - begin);
    const auto capacity = DeviceCompositionConfig.raw_bytes;
    if (retained > capacity || text.size() > capacity - retained) return RawAdmission::Capacity;
    for (char letter : text) {
        if (letter < 'a' || letter > 'z') return RawAdmission::Invalid;
    }
    return RawAdmission::Edit;
}
}
