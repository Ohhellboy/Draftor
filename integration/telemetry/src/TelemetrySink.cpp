#include "draftor/telemetry/TelemetrySink.h"
#include "draftor/telemetry/SerialTelemetrySink.h"
#include <cstring>
namespace draftor::telemetry {
SinkResult TestTelemetrySink::try_write_frame(const std::uint8_t* bytes, std::size_t size) noexcept {
    if (response != SinkResult::Accepted) return response;
    if (size > kMaxFrameBytes) return SinkResult::Failed;
    std::memcpy(last_frame, bytes, size); last_size = size; ++accepted_frames;
    return SinkResult::Accepted;
}
SinkResult SerialTelemetrySink::try_write_frame(const std::uint8_t* bytes, std::size_t size) noexcept {
    if (pending()) return SinkResult::Busy;
    if (writer_ == nullptr || size == 0 || size > kMaxFrameBytes) return SinkResult::Failed;
    std::memcpy(frame_, bytes, size); size_ = size; offset_ = 0;
    return SinkResult::Accepted;
}
SinkPollResult SerialTelemetrySink::poll() noexcept {
    if (!pending()) return SinkPollResult::NoLoss;
    const auto result = writer_(context_, frame_ + offset_, size_ - offset_);
    if (result.status == SerialWriteStatus::Disconnected || result.bytes > size_ - offset_ ||
        (result.status == SerialWriteStatus::WouldBlock && result.bytes != 0)) {
        size_ = 0; offset_ = 0;
        return SinkPollResult::TransportLoss;
    }
    offset_ += result.bytes;
    if (offset_ == size_) { size_ = 0; offset_ = 0; }
    return SinkPollResult::NoLoss;
}
}
