# Draftor TF20 Alpha — v0.1.0-alpha.1

[简体中文](../README.zh-CN.md)

Draftor is an ESP32-S3 writing device with a reflective display, Bluetooth keyboard and TF storage. It supports English, full Pinyin, Xiaohe and Wubi input, horizontal and vertical editing, save/reopen, rename, TXT export and Inbox import. bgIME is its shared input runtime.

This integrated first-release package contains the complete self-owned source, including Core, together with the unchanged resources and the exact device-tested firmware in one version. Download availability is listed at https://github.com/Ohhellboy/Draftor/releases. Resources with unresolved provenance or distribution evidence are preserved in full; see [resource status](RESOURCE_STATUS.md).

| Deliverable | Contents |
|---|---|
| This tree / Draftor_TF20_Alpha_Source.zip | Product, Core, tools, pins, retained ST7306 module, original hardware overview/photo, documentation and applicable terms |
| Draftor_TF20_Alpha_Resources.zip | All 14 B2 payloads (126,940,251 bytes), manifest/pins, verified READY and unchanged 28px TF font |
| Draftor_TF20_Alpha_BuildInputs.zip | All 76 companion inputs, fixed original font sources, UI14 generation inputs and provenance |
| Draftor_TF20_Alpha_Firmware.zip | Exact tested app and original paired bootloader, partition table and boot_app0; no device dump |

Self-owned source uses **Apache-2.0**, Copyright 2026 **Ruotao Wang**. [LICENSE](../LICENSE), [NOTICE](../NOTICE) and [component-specific terms](LICENSE_INVENTORY.md) apply within their stated scope. Data and fonts are not relicensed under Apache. Related technology has been filed in a Chinese patent application; acceptance is not a patent grant.

The fixed product is **T06_TF20_R1**, firmware **DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007**: TF runtime 20 MHz, initialization 400 kHz, exact tested app 2,061,808 bytes. Existing device qualification covers finite scenarios; [known Alpha limits](KNOWN_ISSUES.md) remain. The 10 MHz predecessor is rollback-only and its binary is not supplied in this release.

Source plus installed BuildInputs provides the project files used by the existing S3 build. Install the fixed toolchain separately as documented in [BUILD](BUILD.md). Recreating all mixed datasets/fonts from upstream is a separate, incompletely traced operation. No bit-identical rebuild or all-green test claim is made.

The existing public `display/st7306` module remains independently MIT licensed and is included unchanged; the product tree does not replace it. The Alpha module layout is retained: product/editor/Core under `integration/product`, USB under `integration/module` and `integration/shared`, instrumentation and Host checks under `integration/telemetry`, `integration/tools` and `integration/fixtures`, SDK modifications under `vendor`, and build/pairing entry points under `release` and `tools`.

[Build](BUILD.md) · [Inputs](EXTERNAL_BUILD_INPUTS.md) · [Hardware](HARDWARE.md) · [Install](INSTALL.md) · [Usage](USAGE.md) · [Rollback](ROLLBACK.md) · [Release notes](RELEASE_NOTES.md)

[Original bilingual project introduction](../README.md) · [Repository/source archive relationship](SOURCE_INTEGRATION.md). The complete firmware target is ESP32-S3 N16R8 (Thick); the Pico 2 W (Thin) entry remains a separate display demo.
