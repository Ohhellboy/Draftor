#pragma once
#include "WireEncoder.h"
namespace draftor::telemetry {
enum class SinkResult { Accepted, Busy, Failed };
enum class SinkPollResult { NoLoss, TransportLoss };
class TelemetrySink {
public:
    virtual ~TelemetrySink() = default;
    // Must return promptly. Accepted means bytes were copied/consumed, never
    // ownership of the caller's buffer. No platform wait is allowed here.
    virtual SinkResult try_write_frame(const std::uint8_t*, std::size_t) noexcept = 0;
    virtual SinkPollResult poll() noexcept { return SinkPollResult::NoLoss; }
};
class NullTelemetrySink final : public TelemetrySink {
public:
    SinkResult try_write_frame(const std::uint8_t*, std::size_t) noexcept override {
        return SinkResult::Accepted;
    }
};
// Small single-frame observable sink for adapter tests, not an archive.
class TestTelemetrySink final : public TelemetrySink {
public:
    SinkResult response{SinkResult::Accepted};
    std::uint8_t last_frame[kMaxFrameBytes]{};
    std::size_t last_size{};
    std::uint64_t accepted_frames{};
    SinkResult try_write_frame(const std::uint8_t* bytes, std::size_t size) noexcept override;
};
}
