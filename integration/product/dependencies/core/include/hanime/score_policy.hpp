#pragma once
#include "hanime/exact_ranker.hpp"

namespace hanime {
// Runtime identity only. HBO remains rule-1 raw S; this policy preserves its
// complete zero-count order. This revision also names the event qualification.
constexpr std::uint64_t ARev1PolicyRevision = 0x4c314152455631ULL;
inline bool base_order_compatible(RankRule rule) {
  return rule.revision == 1 || rule.revision == ARev1PolicyRevision;
}
inline std::int64_t arev1_transform(std::int64_t raw) {
  // Domain: sum of two separately widened int32 weights. No signed shifts.
  const auto magnitude = std::uint64_t(raw < 0 ? -raw : raw);
  const auto x = magnitude + 1024;
  const unsigned e = 63U - unsigned(__builtin_clzll(x));
  const auto power = std::uint64_t(1) << e;
  const auto h = std::int64_t(4096U * e + ((4096ULL * (x-power)) >> e));
  const auto t = h - 40960;
  return raw < 0 ? -t : t;
}
inline std::int64_t rank_score_from_raw(std::int64_t raw, std::uint32_t count, RankRule rule) {
  if (rule.revision != ARev1PolicyRevision)
    return raw + std::int64_t(count) * std::int64_t(rule.lambda);
  const auto steps = count <= 1 ? 0U : (count >= 49 ? 48U : count-1);
  const auto main = arev1_transform(raw) + std::int64_t(4096U * steps);
  return main * 8589934592LL + (raw + 4294967296LL);
}
} // namespace hanime
