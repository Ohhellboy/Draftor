// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
namespace s3app {
// COMMON_BASE research policy inherited from STARTUP2. This affects ordinary
// allocations across modules, including when Wi-Fi has never been enabled.
// It is not a module-private budget or a permanent product adoption decision.
// Call once at the existing post-PSRAM-check, pre-BLE/IME startup boundary.
inline void preparePlatformMemory() noexcept {
 heap_caps_malloc_extmem_enable(0);
}
}
#endif
