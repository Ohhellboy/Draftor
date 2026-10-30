# Historical A3 software-only snapshot

This records the old review stage; it is not the current device/release status. See [current validation](ALPHA2_2_VALIDATION.md).

## Waveshare A3 isolated software review

The default target remains `ESP32S3_OSPTEK_ST7306`. The explicit `WAVESHARE_RLCD42` target selects ST7305 and one-bit native SDMMC; it does not link the product Arduino SD fork. Unknown target names and Pico are rejected. This candidate has not been flashed or qualified on hardware.

The original logical document root is unchanged. Waveshare maps it to `/sd/WAVESHARE_RLCD42/HANIME_S3_TEST/v01`. Shared dictionaries and fonts still use `/sd/HANIME/IME/…`. An unknown existing Waveshare directory is refused; there is no original-directory fallback, migration or formatting. Pico is reserved only.

Run finite checks against an **extracted release Resources folder**, never a mounted TF card during software review:

```sh
python3 -B tools/check_alpha2.py
python3 -B tools/run_port_checks.py --resources ../Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode normal
python3 -B tools/run_port_checks.py --resources ../Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode sanitize
```

The selected tests include the real product App/storage/USB owners, actual painters, the actual ST7305 driver with a wire decoder, and the production staged SDMMC mount implementation with mocked host/FAT effects and real heap allocation/free. Host files and sanitizer allocations are not hardware timing or heap measurements. The ten inherited storage/editor regression groups retain their original assertions; the separate full historical `port_risks` suite has an obsolete Find height=306 assertion against the unchanged Alpha 2.2 height=320. Current Chinese Find is checked separately. See the review package REPORT, COSTS and DEVICE_HANDOFF for evidence and the future one-pass device procedure.

A2 retires the two A1 SDMMC review defects. `board_sdmmc.cpp` owns the mount stages directly using the pinned SDK APIs; it does not call the SDK convenience mount/unmount helpers or modify SDK files. Owner allocation is the final fallible step, and failures roll back FAT, VFS, diskio, host and card ownership. Cleanup failure retires the public handle and blocks further attempts until reset. There is no automatic format or runtime remount.

The Host checks run 19 SDMMC cases separately, including final owner allocation failure, successful retry after complete rollback, failure during rollback, consumed VFS context after unregister error, repeated cleanup, startup retry termination, and conservative quarantine when FatFs/host refuses detachment. The common retry loop retains the original SD/SPI port's three-attempt behavior and has its own regression test. The fault allocator is forbidden in an Arduino build. These tests are finite software evidence, not a guarantee that every internal SDK allocation/assert or physical card failure is recoverable.

A3 keeps the A2 SDMMC implementation unchanged. Namespace admission now returns Ready, Invalid or OutOfMemory and catches allocation failures including construction of the VFS root. Unknown or incomplete directories are never adopted. The Host runner adds a fail-at-each-allocation sweep for both new and valid-marker paths, including the exact WS startup gate, and existing marker I/O faults. The A3 delivery rebuilds WAVESHARE_RLCD42; it retains the exact A2 original-target binary and records why its runtime branch is unaffected.
