#pragma once
#include "EventSchema.h"
#include "ProtocolConstants.h"
#include <cstddef>
#include <cstdint>

namespace draftor::telemetry {
constexpr std::uint32_t kUnknownCount = UINT32_MAX;
enum class PrivacyLevel : std::uint8_t { Off = 0, Basic = 1, Detailed = 2 };

// Caller-supplied correlation identities are opaque session-local counters, never
// hashes, dictionary addresses, Unicode scalars, or reversible content identities.
// Zero means unavailable. Emitter owns boot_id and event_seq, not business state.
struct EventEnvelope {
    std::uint64_t boot_id{};
    std::uint64_t event_seq{};
    std::uint64_t device_monotonic_us{};
    std::uint32_t action_id{};
    std::uint32_t composition_id{};
    std::uint32_t composition_revision{};
    std::uint32_t candidate_snapshot_id{};
    std::uint32_t document_revision{};
    std::uint32_t layout_revision{};
};

// Copies only bytes already available to the product. No borrowed references,
// lazy suppliers, callbacks, or ownership of query/selection/candidate objects.
struct SensitiveDetail {
    std::uint32_t printable_codepoint{};
    std::uint64_t candidate_identity{};
    std::uint8_t text_length{};
    char text[kTextCapacity]{};
    std::uint8_t path_length{};
    char path[kPathCapacity]{};
};
struct BehaviorEvent {
    EventEnvelope envelope{};
    EventType type{EventType::StateSnapshot};
    EventPayload payload{};
    SensitiveDetail detail{};
};
struct QueuedEvent {
    BehaviorEvent event{};
    PrivacyLevel privacy{PrivacyLevel::Basic};
    std::uint64_t loss_epoch{};
};
} // namespace draftor::telemetry
