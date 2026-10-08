#pragma once
#include "TelemetrySink.h"
namespace draftor::telemetry {
enum class SerialWriteStatus { Progress, WouldBlock, Disconnected };
struct SerialWriteResult { SerialWriteStatus status; std::size_t bytes; };
// Platform adapter MUST be non-blocking, with no wait-for-space or retry loop.
using SerialTryWrite = SerialWriteResult (*)(void*, const std::uint8_t*, std::size_t) noexcept;
class SerialTelemetrySink final : public TelemetrySink {
public:
    SerialTelemetrySink(SerialTryWrite writer, void* context) noexcept
        : writer_(writer), context_(context) {}
    SinkResult try_write_frame(const std::uint8_t*, std::size_t) noexcept override;
    SinkPollResult poll() noexcept override;
    bool pending() const noexcept { return size_ != 0; }
private:
    SerialTryWrite writer_;
    void* context_;
    std::uint8_t frame_[kMaxFrameBytes]{};
    std::size_t size_{};
    std::size_t offset_{};
};
}
