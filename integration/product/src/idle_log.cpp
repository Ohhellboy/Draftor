#include "idle_log.hpp"
#include "device_metrics.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ime_device {

bool IdleLog::append(std::string_view line) {
    if (line.empty() || line.back() != '\n' || line.size() >= LineBytes) {
        ++costs_.oversized;
        return false;
    }
    if (full()) {
        ++costs_.dropped;
        return false;
    }
    auto& slot = lines_[(head_ + size_) % Capacity];
    std::memcpy(slot.bytes.data(), line.data(), line.size());
    slot.size = line.size();
    ++size_;
    ++costs_.queued;
    return true;
}

bool IdleLog::format(const char* format, va_list arguments) {
    if (full()) {
        ++costs_.dropped;
        return false;
    }
    const auto start = clockUs();
    auto& slot = lines_[(head_ + size_) % Capacity];
    const auto length = std::vsnprintf(slot.bytes.data(), slot.bytes.size(), format, arguments);
    costs_.max_format_us = std::max(costs_.max_format_us, clockUs() - start);
    if (length <= 0 || size_t(length) >= LineBytes || slot.bytes[size_t(length) - 1] != '\n') {
        ++costs_.oversized;
        return false;
    }
    slot.size = size_t(length);
    ++size_;
    ++costs_.queued;
    return true;
}

bool IdleLog::printf(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const bool accepted = this->format(format, arguments);
    va_end(arguments);
    return accepted;
}

void IdleLog::service(LogPort& port, const LogGate& gate) {
    if (!gate.transportReady()) {
        ++costs_.deferred;
        return;
    }
    if (!size_) return;
    if (!port.connected()) {
        ++costs_.backpressure;
        return;
    }
    auto& line = lines_[head_];
    const auto count = std::min({line.size - sent_, WriteBytesPerTurn, port.available()});
    if (!count) {
        ++costs_.backpressure;
        return;
    }
    const auto start = clockUs();
    const auto written = port.write(line.bytes.data() + sent_, count);
    costs_.max_write_us = std::max(costs_.max_write_us, clockUs() - start);
    ++costs_.writes;
    if (written < count) ++costs_.short_writes;
    if (!written) ++costs_.backpressure;
    // A platform violating the byte-count contract cannot advance beyond this
    // line or accidentally publish a following record as sent.
    const auto accepted = std::min(written, count);
    costs_.bytes += accepted;
    sent_ += accepted;
    if (sent_ == line.size) {
        head_ = (head_ + 1) % Capacity;
        --size_;
        sent_ = 0;
    }
}

} // namespace ime_device
