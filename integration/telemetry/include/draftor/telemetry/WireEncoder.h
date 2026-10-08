#pragma once
#include "BehaviorEvent.h"
namespace draftor::telemetry {
// Distinct record markers make mixed diagnostics unambiguous, including a lost
// end marker: START always abandons an incomplete binary record and starts anew.
constexpr std::uint8_t kFrameStart = 0xC1;
constexpr std::uint8_t kFrameEnd = 0xC0;
constexpr std::uint8_t kFrameEscape = 0xDB;
constexpr std::size_t kMaxPacketBytes = kWireHeaderBytes + kMaxPayloadBytes + 2;
constexpr std::size_t kMaxFrameBytes = 2 * kMaxPacketBytes + 2;
struct WireScratch {
    std::uint8_t packet[kMaxPacketBytes]{};
    std::uint8_t frame[kMaxFrameBytes]{};
};
enum class EncodeStatus { Encoded, InvalidEvent, BufferTooSmall };
struct EncodeResult { EncodeStatus status; std::size_t size; };
std::uint16_t crc16_ccitt(const std::uint8_t* data, std::size_t size) noexcept;
// Caller owns scratch lifetime/placement. On success scratch.frame[0:size] holds
// the encoded record. No packet/frame automatic arrays are used by this function.
EncodeResult encode_frame(const QueuedEvent& item, WireScratch& scratch,
                          std::size_t output_capacity = kMaxFrameBytes) noexcept;
}
