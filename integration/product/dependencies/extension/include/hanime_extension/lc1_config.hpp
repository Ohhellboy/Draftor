#pragma once
#include "hanime_extension/submission.hpp"

namespace lc1 {
// This is the fixed raw64 consumer profile, not a change to library defaults.
inline hanime_extension::Config input_config(hanime_extension::SchemeRules rules) {
  hanime_extension::Config config;
  config.members = rules == hanime_extension::SchemeRules::R1Xiaohe ? 32 : 64;
  return config;
}
inline hanime::CompositionConfig submission_config(const hanime_extension::Config& input) {
  return {input.members, input.name_bytes, input.word_bytes, input.text_bytes,
          hanime::CompositionLearningPolicy::Disabled};
}
}  // namespace lc1
