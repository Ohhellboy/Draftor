#pragma once
#include "release_config.hpp"
#include "behavior_observer.hpp"
#if DRAFTOR_TELEMETRY_PROFILE == 2
#define BT_BUILD_PROFILE "DETAILED"
#elif DRAFTOR_TELEMETRY_PROFILE == 1
#define BT_BUILD_PROFILE "BASIC"
#elif DRAFTOR_TELEMETRY_CAPTURE
#define BT_BUILD_PROFILE "CAPTURE_OFF"
#else
#define BT_BUILD_PROFILE "OFF"
#endif
#include <cstddef>
#include <cstdint>
#include "hanime_extension/candidate.hpp"
#define HANIME_N1_ROWS 0
namespace ime_device {
#if DRAFTOR_RELEASE_BUILD
inline constexpr const char* CandidateFirmware="DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007";
#elif DRAFTOR_TELEMETRY_PROFILE == 1
inline constexpr const char* CandidateFirmware="DRAFTOR_S3_LEXICON_R3_R1_20261006_BASIC_TEST_ONLY";
#else
inline constexpr const char* CandidateFirmware="DRAFTOR_S3_LEXICON_R3_R1_20261006_" BT_BUILD_PROFILE "_TEST_ONLY";
#endif
inline constexpr const char* CandidateArm="P23_PHASE2C_R3_R2_HV";
inline constexpr const char* CandidateCoreRelease="HanIME-Core-0.13.1-rc.1";
inline constexpr const char* CandidateResourceRoot="/sd/HANIME/IME/LEXICON_R3_B2_20261006";
inline constexpr const char* CandidateFontPath="/sd/HANIME/IME/R1_API080/matched28.hbf1";
class CandidateWindow;
struct ProbePoolReceipt { const void* address=nullptr;size_t bytes=0;bool attempted=false; };
struct CandidateBuildDiagnostics {
 static ProbePoolReceipt pool(const CandidateWindow*);
 static size_t poolBytes();
};
}
