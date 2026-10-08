#include "draftor/telemetry/TelemetryEmitter.h"
namespace draftor::telemetry {
void TelemetryEmitter::note_loss() noexcept { ++loss_epoch_; snapshot_required_ = true; }
void TelemetryEmitter::record_transport_loss() noexcept { ++health_.transport_failures; note_loss(); }
EmitResult TelemetryEmitter::try_emit(const BehaviorEvent& event) noexcept {
    if (privacy_ == PrivacyLevel::Off) return EmitResult::Disabled;
    auto& slot = queue_[(head_ + count_) % kQueueCapacity];
    // A full ring's tail aliases its head: validate without writing that slot.
    // Otherwise validate and privacy-filter directly into the unused slot.
    const auto result = count_ == kQueueCapacity
        ? validate_for_emission(event, privacy_)
        : filter_for_emission(event, privacy_, slot.event);
    if (result == FilterResult::Suppressed) return EmitResult::Suppressed;
    ++health_.attempted;
    if (result == FilterResult::Invalid) {
        ++health_.dropped_invalid;
        return EmitResult::DroppedInvalid; // Never entered the stream: no seq/loss/SYNC.
    }
    const auto sequence = next_seq_++;
    if (count_ == kQueueCapacity) {
        ++health_.dropped_queue; note_loss(); return EmitResult::DroppedQueueFull;
    }
    slot.event.envelope.boot_id = boot_id_; slot.event.envelope.event_seq = sequence;
    slot.privacy = privacy_; slot.loss_epoch = loss_epoch_;
    ++count_; ++health_.enqueued;
    if (count_ > health_.queue_high_water) health_.queue_high_water = static_cast<std::uint32_t>(count_);
    return EmitResult::Enqueued;
}
void TelemetryEmitter::service(TelemetrySink& sink) noexcept {
    if (sink.poll() == SinkPollResult::TransportLoss) record_transport_loss();
    if (count_ == 0) return;
    const auto& slot = queue_[head_];
    const auto encoded = encode_frame(slot, scratch_);
    if (encoded.status != EncodeStatus::Encoded) {
        ++health_.dropped_encode; note_loss();
    } else {
        const auto result = sink.try_write_frame(scratch_.frame, encoded.size);
        if (result == SinkResult::Busy) {
            ++health_.sink_busy;
            return; // Keep exactly this head; no loss and no wait/retry loop.
        }
        if (result == SinkResult::Failed) {
            ++health_.dropped_sink_failed; note_loss();
        } else {
            ++health_.frames_accepted; health_.bytes_accepted += encoded.size;
            if (slot.event.type == EventType::SyncSnapshot && slot.loss_epoch == loss_epoch_)
                snapshot_required_ = false;
        }
    }
    head_ = (head_ + 1) % kQueueCapacity; --count_;
}
BehaviorEvent TelemetryEmitter::health_event(std::uint64_t monotonic_us) const noexcept {
    BehaviorEvent event{}; event.type = EventType::TelemetryHealth;
    event.envelope.device_monotonic_us = monotonic_us;
    event.payload.telemetry_health = {health_.attempted, health_.enqueued, health_.dropped_queue,
        health_.dropped_invalid, health_.sink_busy, health_.dropped_sink_failed, health_.dropped_encode,
        health_.transport_failures, health_.queue_high_water, health_.frames_accepted, health_.bytes_accepted};
    return event;
}
}
