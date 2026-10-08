# Inherited SD diagnostic-only dependency copy

Source: installed Arduino-ESP32 3.3.11 `libraries/SD`, Apache-2.0, Espressif Systems. Original source headers retained; `LICENSE` is the standard Apache 2.0 text from the same core. Upstream repository: https://github.com/espressif/arduino-esp32/tree/3.3.11/libraries/SD

This inherited dependency copy contains diagnostic counters and timing scopes in `src/sd_diskio.cpp`, with storage/helpers in the two `hanime_sd_trace` files. Its SD.cpp, headers and CRC source match that fixed upstream library. This copy retains the upstream command, CRC, retry and initialization-wait behavior; it does not contain the later cooperative mount-budget changes.

The TF20 product build explicitly selects **integration/product/libraries/SD**, not this inherited copy. The selected driver has additional initialization/mount budget, failure-stage and cleanup protections described in [its modification notice](../../../../libraries/SD/NOTICE.md). Do not use the description of this diagnostic-only copy as the modification summary for the active product driver.

The modified files are included beside this notice and can be compared with the fixed upstream source above. No private evidence directory, undistributed patch or hash report is required. This clarification changes no library code or build selection.
