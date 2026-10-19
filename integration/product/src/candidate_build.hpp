#pragma once
#if __has_include("block_build_metadata.hpp")
#include "block_build_metadata.hpp"
#endif
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
#if defined(DRAFTOR_B_FIRMWARE_LABEL)
inline constexpr const char* CandidateFirmware=DRAFTOR_B_FIRMWARE_LABEL;
#elif defined(DRAFTOR_AUTH_HANDOFF) && DRAFTOR_AUTH_HANDOFF
#if defined(DRAFTOR_AUTH_OBSERVE) && DRAFTOR_AUTH_OBSERVE
#if HANIME_SHA256_BATCHED_FINAL
inline constexpr const char* CandidateFirmware="DRAFTOR_AUTH_BATCH_OBS_20261015_TEST_ONLY";
#else
inline constexpr const char* CandidateFirmware="DRAFTOR_AUTH_COMMON_OBS_20261015_TEST_ONLY";
#endif
#else
#if HANIME_SHA256_BATCHED_FINAL
inline constexpr const char* CandidateFirmware="DRAFTOR_AUTH_BATCH_QR_20261015_TEST_ONLY";
#else
inline constexpr const char* CandidateFirmware="DRAFTOR_AUTH_COMMON_QR_20261015_TEST_ONLY";
#endif
#endif
#elif defined(DRAFTOR_PERFORMANCE_COMMON) && DRAFTOR_PERFORMANCE_COMMON
#if DRAFTOR_TEXT_RESIDENT
inline constexpr const char* CandidateFirmware="DRAFTOR_TEXT_RESIDENT_QR_20261015_TEST_ONLY";
#else
inline constexpr const char* CandidateFirmware="DRAFTOR_COMMON_QR_20261015_TEST_ONLY";
#endif
#elif DRAFTOR_RELEASE_BUILD
inline constexpr const char* CandidateFirmware="DRAFTOR_S3_WIFI_QR1_20261015_TEST_ONLY";
#elif DRAFTOR_TELEMETRY_PROFILE == 1
inline constexpr const char* CandidateFirmware="DRAFTOR_S3_LEXICON_R3_R1_20261006_BASIC_TEST_ONLY";
#else
inline constexpr const char* CandidateFirmware="DRAFTOR_S3_LEXICON_R3_R1_20261006_" BT_BUILD_PROFILE "_TEST_ONLY";
#endif
#if defined(DRAFTOR_B_FIRMWARE_LABEL)
#if defined(DRAFTOR_LAYOUT_ARM) && defined(DRAFTOR_LAYOUT_RESOURCE_ROOT)
inline constexpr const char* CandidateArm=DRAFTOR_LAYOUT_ARM;
inline constexpr const char* CandidateOrderFormat="105";
inline constexpr const char* CandidateResourceRoot=DRAFTOR_LAYOUT_RESOURCE_ROOT;
#elif DRAFTOR_BLOCK_BACKEND
inline constexpr const char* CandidateArm="BLOCK_CANDIDATE";
inline constexpr const char* CandidateOrderFormat="105";
inline constexpr const char* CandidateResourceRoot="/sd/HANIME/IME/B_BLOCK_20261017";
#else
inline constexpr const char* CandidateArm="BASE_HBO2";
inline constexpr const char* CandidateOrderFormat="2";
inline constexpr const char* CandidateResourceRoot="/sd/HANIME/IME/B_BASE_HBO2_20261017";
#endif
inline constexpr const char* CandidatePinSha=DRAFTOR_B_PIN_SHA;
#else
inline constexpr const char* CandidateArm="P23_PHASE2C_R3_R2_HV";
inline constexpr const char* CandidateOrderFormat="2";
inline constexpr const char* CandidatePinSha="d1ad4274b2756824b9da6e529aa7cac39c3515b8aa9059960405917f52188345";
inline constexpr const char* CandidateResourceRoot="/sd/HANIME/IME/LEXICON_R3_B2_20261006";
#endif
inline constexpr const char* CandidateCoreRelease="HanIME-Core-0.13.1-rc.1";
inline constexpr const char* CandidateFontPath="/sd/HANIME/IME/R1_API080/matched28.hbf1";
class CandidateWindow;
struct ProbePoolReceipt { const void* address=nullptr;size_t bytes=0;bool attempted=false; };
struct CandidateBuildDiagnostics {
 static ProbePoolReceipt pool(const CandidateWindow*);
 static size_t poolBytes();
};
}
