#pragma once
#ifndef HANIME_OUTPUT_PROFILE
#define HANIME_OUTPUT_PROFILE 2
#endif
// Same-logic measured profiles. Profile 2 is the combined review candidate.
// Profile 0 preserves previous quotas and disables output/anchor reuse.
// 1: proposed balanced, 2: same reuse with original shared data/page quotas.
static_assert(HANIME_OUTPUT_PROFILE>=0&&HANIME_OUTPUT_PROFILE<=2,"Unknown output profile");
namespace ime_device {
constexpr unsigned OutputProfile=HANIME_OUTPUT_PROFILE;
constexpr unsigned SharedDataKiB=OutputProfile==1?15:16;
constexpr unsigned SharedPages=OutputProfile==1?1:2;
constexpr unsigned OutputEntries=OutputProfile?12:0;
constexpr unsigned OutputTextBytes=64,OutputFormBytes=64;
constexpr unsigned SharedAnchorSlots=OutputProfile?16:0;
}
