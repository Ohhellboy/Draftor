# Draftor SD local modifications

Source: installed Arduino-ESP32 3.3.11 `libraries/SD`, Apache-2.0, Espressif Systems. Original source headers retained; `LICENSE` is the standard Apache 2.0 text from the same core. Upstream repository: https://github.com/espressif/arduino-esp32/tree/3.3.11/libraries/SD

The local source includes these existing modifications to that fixed upstream library:

- `src/SD.cpp` includes `hanime_mount_budget.h` and records the first SPI-start or disk-registration failure during a device-owned mount attempt. Public `SD.begin` remains the entry point.
- `src/hanime_mount_budget.h` supplies a cooperative initialization/mount budget, current stage, first failure and FAT mount result. `src/sd_diskio.cpp` checks that budget in command, busy/token wait, initialization and mount paths; bounded waits can end early and retry delays are limited by the remaining budget. These are changes to initialization/mount timeout behavior, not just diagnostic counters. A single SPI/VFS/RTOS call cannot be preempted by this budget.
- `src/sd_diskio.cpp` also records initialization stages and VFS/FAT failures, skips an extra idle command after budget expiry during cleanup, and restores CS high before unregistering the disk. Its disk-function table uses a positional initializer compatible with the product's C++ build.
- `src/hanime_sd_trace.h` and `src/hanime_sd_trace.cpp`, together with hooks in `sd_diskio.cpp`, retain gated timing/counter instrumentation for transactions, waits, CRC failures, retries and read/write/sync operations. Retained instrumentation does not imply that the release emits detailed counters.

The mount budget is active only during the device-owned initialization/mount attempt; normal operation runs with it inactive. Outside that budget, the underlying command/CRC algorithms, existing retry limits and wait/sync protocol remain inherited from the fixed library. This does not claim that every file is byte-identical to upstream or that initialization timeouts are unchanged. The product passes **20 MHz** to `SD.begin`; the driver's initialization transaction remains **400 kHz**, with its existing runtime-frequency cap retained. This notice does not add a new driver or frequency change.

The build explicitly selects this local library with `--library`; the installed SDK is not modified. The modified files are present beside this notice and can be compared with the fixed upstream source above. No private audit file or undistributed patch is required to understand this modification summary. This documentation correction changes no product code, resources or tested firmware.
