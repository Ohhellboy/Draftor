# Build / 构建

Use Python 3, CMake/CTest 3.21+ (previously validated with 4.3.2), C++17, Arduino CLI **1.4.1**, Arduino-ESP32 **3.3.11** and **esp-x32/2601**. Exact SDK components are pinned in `vendor/versions.txt`: lib-builder ee57070, ESP-IDF v5.5.5 b774170ff46, Arduino 0d7a8614b and TinyUSB 53f8c53c2. Install the fixed version from the [official Arduino-ESP32 release](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11) / its package index, not the latest version. SDK and compiler caches are not release assets.

For `v0.1.0-alpha.1`, extract the integrated Source archive (or check out that tag) and its paired BuildInputs archive. Run these commands from the extracted `Draftor/` repository root. The standalone `display/st7306` demos keep their own build instructions and are not the full TF20 product.

Extract this source and the same-version BuildInputs archive. All 76 historical external inputs are provided at `BuildInputs/inputs/` with their original relative paths and bytes. Use a fresh working copy:

```sh
python3 -B tools/install_external_inputs.py --from "$BUILD_INPUTS/inputs"
export HANIME_WORKSPACE="$PWD"
export DRAFTOR_SDK_SOURCE="$ARDUINO_DATA_DIRECTORY"
export ARDUINO_CLI="$ARDUINO_CLI_EXECUTABLE"
mkdir -p TestBuilds/.tmp
export TMPDIR="$PWD/TestBuilds/.tmp" TMP="$PWD/TestBuilds/.tmp" TEMP="$PWD/TestBuilds/.tmp"
python3 -B tools/build_candidate.py
```

`ARDUINO_DATA_DIRECTORY` contains `packages/esp32`. The unchanged SDK restoration checks pinned versions and 111 TinyUSB headers, creates an owned overlay and rebuilds the 25 MSC-only TinyUSB units. It does not modify the upstream installation or access devices. The external-input installer checks sizes and copies only manifest entries; runtime authentication and pins remain authoritative. Use the full same-version input set, including UNKNOWN materials; the installer has no provenance filter.

输出为 `TestBuilds/product-s3/dist/HanIME_S3_IME.ino.bin`。新构建用于开发，不替代同批Firmware附件里的原实测app。完整固定SDK须另行安装；项目输入已提供不代表所有第三方分发条件已完成。

Release constants: `DRAFTOR_RELEASE_BUILD=1`, telemetry profile 0, capture gate 1, output profile 2, USB on, TF runtime 20 MHz (SD initialization remains 400 kHz), save substage diagnostic off. Capture gate 1 suppresses old verbose logging; it does not enable BASIC when profile is 0. Safety ST records, failed-allocation mailbox, stop handling, save results and authentication are retained.

Public Host validation is a **224-check portable subset**, not the internal full 230-check regression. Supply the same external resource inputs first, then run both modes below. No private workspace, prior release directory, audit report, or device is required. The explicit flag prevents accidental claims of full historical coverage.

公开 Host 入口是 **224 项可移植子集**。内部完整 230 项回归仍保留原断言和历史反例，仅在保全历史依赖的整合工作区执行；不向公开包混入内部审计材料。公开子集排除下列六项，不能据此宣称它们通过：`ble_async_read_lifetime`, `heap_diagnostic_integrity`, `ble_notification_handoff`, `heap_stop_scheduler`, `descriptor_fallback_case2`, `descriptor_lifecycle`。这些检查依赖历史源码对照和/或私有 SDK 工作区夹具；已在内部普通版和 ASan/UBSan 完整回归中执行。

```sh
DRAFTOR_BUILD_JOBS=2 python3 -B integration/tools/host_driver.py public_release --public-validation --release --profile 0 --capture 0 --configure --build --ctest '.*' --test-jobs 2 --allow-fail
DRAFTOR_BUILD_JOBS=2 python3 -B integration/tools/host_driver.py public_release --public-validation --release --profile 0 --capture 0 --san --configure --build --ctest '.*' --test-jobs 2 --allow-fail
```

Both Host entries use capture=0 because inherited assertions consume diagnostic events; actual S3 release uses capture=1 and has separate direct-state checks. 这属于测试观测配置，不改变产品状态机。完整内部回归与公开子集均存在 16 项继承失败；保留 CTest 非零及原始结果，要求同名同首签名、无新增失败或 sanitizer 报告。`--allow-fail` 仅保存完整结果，不表示全绿。公开子集的结果不得替代六项历史检查或实机验收。


## Independent Core runtime

```sh
cmake -S integration/product/dependencies/core -B TestBuilds/core -DHANIME_BUILD_HOST_ADAPTERS=OFF -DHANIME_BUILD_TOOLS=OFF -DHANIME_BUILD_FIXTURES=OFF -DHANIME_BUILD_TESTS=OFF
cmake --build TestBuilds/core -j2
```

This builds the resource-free Core runtime only. The previous accepted clean S3 build used these same product/input bytes and produced a 2,061,600-byte verification app with static internal RAM 138,012 bytes. The device-tested app is 2,061,808 bytes. No bit-identical claim is made. This release preparation reuses that build evidence and checks the new archive/install path; it does not repeat product builds or test matrices.

UI14 generation materials and original outlines are in `BuildInputs/font-generation` and `font-sources`. Existing generated inputs are sufficient for S3 compilation; do not regenerate them to install this version. Complete recreation of older mixed Latin/composition/28px inputs is not claimed. See [input disposition](EXTERNAL_BUILD_INPUTS.md) and [resource status](RESOURCE_STATUS.md).
