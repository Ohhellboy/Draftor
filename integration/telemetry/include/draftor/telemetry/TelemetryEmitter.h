#pragma once
#include "Privacy.h"
#include "TelemetrySink.h"
namespace draftor::telemetry {
// DroppedInvalid: fix caller-provided schema identities/enums/detail bounds.
// No queue admission, sequence allocation, or stream-loss/SYNC request.
enum class EmitResult { Enqueued, Disabled, Suppressed, DroppedQueueFull, DroppedInvalid };
struct TelemetryHealth {
    std::uint64_t attempted{}, enqueued{}, dropped_queue{}, dropped_invalid{};
    std::uint64_t sink_busy{}, dropped_sink_failed{}, dropped_encode{}, transport_failures{};
    std::uint32_t queue_high_water{};
    std::uint64_t frames_accepted{}, bytes_accepted{};
};
// Single-owner/cooperatively scheduled, NOT thread-safe or ISR-safe. Integration
// must serialize calls. No locks, allocation, platform clock, or sink on try_emit.
class TelemetryEmitter {
public:
    TelemetryEmitter(std::uint64_t boot_id, PrivacyLevel privacy) noexcept
        : boot_id_(boot_id), privacy_(privacy) {}
    EmitResult try_emit(const BehaviorEvent& event) noexcept;
    // Background budget: one poll callback, at most one event encoded/offered.
    void service(TelemetrySink& sink) noexcept;
    // Use after adapter-known disconnection; does not inspect product state.
    void record_transport_loss() noexcept;
    bool snapshot_required() const noexcept { return snapshot_required_; }
    std::size_t queued() const noexcept { return count_; }
    const TelemetryHealth& health() const noexcept { return health_; }
    BehaviorEvent health_event(std::uint64_t monotonic_us) const noexcept;
private:
    void note_loss() noexcept;
    const std::uint64_t boot_id_;
    const PrivacyLevel privacy_; // Immutable per stream; changing privacy needs a new boot/session.
    std::uint64_t next_seq_{1}, loss_epoch_{};
    TelemetryHealth health_{};
    QueuedEvent queue_[kQueueCapacity]{};
    WireScratch scratch_{}; // Placed with emitter, not on the service call stack.
    std::size_t head_{}, count_{};
    bool snapshot_required_{};
};
}
