#pragma once
#include <cstdint>
// Explicit hardware identity. Pico is a reserved data directory, never an S3 target.
#define ESP32S3_OSPTEK_ST7306 1
#define WAVESHARE_RLCD42 2
#ifndef DRAFTOR_BOARD
#define DRAFTOR_BOARD ESP32S3_OSPTEK_ST7306
#endif
#if DRAFTOR_BOARD != ESP32S3_OSPTEK_ST7306 && DRAFTOR_BOARD != WAVESHARE_RLCD42
#error "Unsupported DRAFTOR_BOARD (Pico port is not implemented)"
#endif
namespace draftor::board {
inline constexpr bool Waveshare=DRAFTOR_BOARD==WAVESHARE_RLCD42;
inline constexpr bool Monochrome=Waveshare;
inline constexpr const char* Name=Waveshare?"WAVESHARE_RLCD42":"ESP32S3_OSPTEK_ST7306";
inline constexpr const char* MountPoint="/sd";
inline constexpr const char* DocumentRoot=Waveshare?"/sd/WAVESHARE_R7_L1_AREV1_DOC_TEST_20261112":"/sd";
inline constexpr std::uint8_t DisplaySck=Waveshare?11:12, DisplayMosi=Waveshare?12:11;
inline constexpr std::uint8_t DisplayCs=Waveshare?40:10, DisplayDc=Waveshare?5:9, DisplayReset=Waveshare?41:4;
inline constexpr int SdClk=38,SdCmd=21,SdD0=39;
inline constexpr unsigned SdRuntimeKHz=40000,SdCommandTimeoutMs=1000; // WS_SDMMC40_TEST_ONLY
inline constexpr int KeyboardPairButton=0; // Runtime emergency only; startup hold ignored. KEY18/PWR unchanged.
}
