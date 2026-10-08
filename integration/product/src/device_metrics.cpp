#include "device_metrics.hpp"
#ifdef ARDUINO
#include <esp_timer.h>
#else
#include <chrono>
#endif
namespace ime_device {
uint64_t clockUs() noexcept {
#ifdef ARDUINO
 return static_cast<uint64_t>(esp_timer_get_time());
#else
 return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
}
