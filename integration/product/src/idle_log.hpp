#pragma once
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ime_device {

struct LogGate {
    bool keys_pending = false;
    bool ime_foreground = false;
    bool saving = false;
    bool app_barrier = false;
    // Called after this turn's keyboard/IME/save service. UART FIFO never waits.
    bool transportReady() const { return !keys_pending && !saving && !app_barrier; }
    bool idle() const { return !keys_pending && !ime_foreground && !saving && !app_barrier; }
};

// The platform must provide non-waiting available/write calls. On S3 the fixed
// UART0 transport uses uart_tx_chars with no TX ring; this queue never flushes.
class LogPort {
public:
    virtual ~LogPort() = default;
    virtual bool connected() const = 0;
    virtual size_t available() = 0;
    virtual size_t write(const char*, size_t) = 0;
};

struct LogCosts {
    uint64_t queued = 0, dropped = 0, oversized = 0, bytes = 0, writes = 0;
    uint64_t backpressure = 0, short_writes = 0, deferred = 0;
    uint64_t max_format_us = 0, max_write_us = 0;
};

// Main-loop owner; S3 allocates this entire bounded object in PSRAM once at
// boot. Producers never wait. A partly sent line stays at the head until sent;
// overflow rejects the new complete line, so it cannot splice two records.
class IdleLog {
public:
    static constexpr size_t Capacity = 16;
    static constexpr size_t LineBytes = 768;
    static constexpr size_t WriteBytesPerTurn = 128;
    bool append(std::string_view line);
    bool format(const char*, va_list);
    bool printf(const char*, ...) __attribute__((format(printf, 2, 3)));
    void service(LogPort&, const LogGate&);
    size_t size() const { return size_; }
    bool full() const { return size_ == Capacity; }
    bool partial() const { return sent_ != 0; } // Existing finite-device UART line ownership guard.
    const LogCosts& costs() const { return costs_; }
private:
    struct Line { std::array<char, LineBytes> bytes{}; size_t size = 0; };
    std::array<Line, Capacity> lines_{};
    size_t head_ = 0, size_ = 0, sent_ = 0;
    LogCosts costs_{};
};

// Preserve a real one-tick block at least every 4 ms of foreground CPU work.
// Other turns still yield (delay(0)); synchronous SD time is measured separately
// and can exceed this cooperative interval. Saving is serviced before yielding.
class LoopYield {
public:
    static constexpr uint64_t MaximumUnblockedUs = 4000;
    unsigned delayMs(uint64_t now_us, bool foreground) {
        if (!foreground || !last_block_us_ || now_us - last_block_us_ >= MaximumUnblockedUs) {
            last_block_us_ = now_us;
            ++blocking_turns;
            return 1;
        }
        ++yield_only_turns;
        return 0;
    }
    uint64_t blocking_turns = 0, yield_only_turns = 0;
private:
    uint64_t last_block_us_ = 0;
};

} // namespace ime_device
