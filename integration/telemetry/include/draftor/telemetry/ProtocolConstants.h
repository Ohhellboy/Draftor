// Generated wire/capacity constants; edit schema/events.json.
#pragma once
#include <cstddef>
#include <cstdint>
namespace draftor::telemetry {
constexpr std::uint8_t kWireVersion = 1;
constexpr std::uint8_t kSchemaRevision = 2;
constexpr std::size_t kWireHeaderBytes = 56;
constexpr std::size_t kMaxPayloadBytes = 272;
constexpr std::size_t kTextCapacity = 96;
constexpr std::size_t kPathCapacity = 64;
constexpr std::size_t kQueueCapacity = 32;
} // namespace draftor::telemetry
