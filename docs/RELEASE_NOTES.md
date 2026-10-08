# Draftor TF20 Alpha — v0.1.0-alpha.1

First complete Alpha software package for **ESP32-S3 N16R8 / Thick**, integrating all self-owned source including **bgIME Core**, unchanged complete resources/build inputs and the original device-tested firmware into the existing Draftor repository. The four attachments form one versioned release.

The original bilingual project introduction, prototype photo, hardware overview and independent ST7306 demos for ESP32-S3/Pico 2 W are retained. This is an incremental continuation of public main; no history replacement. A complete Pico 2 W / Thin firmware is not part of this Alpha.

| Asset | Release role |
|---|---|
| Draftor_TF20_Alpha_Source.zip | Integrated tracked tree, including the original ST7306 module and Alpha product/Core source; one source/document packaging update from the old 991-file ZIP |
| Draftor_TF20_Alpha_Resources.zip | Existing complete resource archive, unchanged: 14 B2 payloads total 126,940,251 bytes plus paired fonts/metadata |
| Draftor_TF20_Alpha_BuildInputs.zip | Existing full inputs/source/notice archive, unchanged; 76 project inputs and supplemental source/font/license materials |
| Draftor_TF20_Alpha_Firmware.zip | Existing original app and first-install quartet, unchanged |

**Pairing:** T06_TF20_R1 / `DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007`; app 2,061,808 bytes, offset 0x10000, capacity 0x200000, static internal RAM 138,012 bytes; TF runtime 20 MHz / initialization 400 kHz; resources `LEXICON_R3_B2_20261006` and original matched28 font. The release tag changes the packaging/version label, not the build identity. The earlier 2,061,600-byte verification build is not supplied as the tested app. The 10 MHz predecessor remains rollback-only, with no replacement binary added.

**Licenses:** new self-owned source/Core Apache-2.0; pre-existing standalone display/st7306 MIT and its original upstream notices; other libraries, data and fonts keep component-specific terms. Original source/attribution materials accompany BuildInputs. Private patent application documents are excluded and are not prerequisites for applying Apache-2.0.

**Specific distribution evidence remains OPEN:** D01 mixed lexicon adaptation/file-level terms; D04 the exact 3,400-byte lib-builder CMake recipe, included at the owner's instruction without waiting for upstream despite no established redistribution grant; D08 the 14 concise-dictionary and 4 variant-dictionary fact-record scope. These decisions do not create third-party authorization or LICENSE_PASS. Historical M4 v0.1.1 exact generator revision remains unavailable; current product source completeness is distinct from recreating every historical dataset step.

Existing finite device qualification and prior product validation are reused. No recompilation, resource/font regeneration, device testing or T01–T06 rerun occurred for this integration. Known Alpha limitations and the 16 inherited Host failures remain; no all-green, blank-board requalification or long-run guarantee is asserted.

中文：完整源码（含Core）、原样完整资源、构建输入及原实测固件同批准备。旧README作者表达、照片、硬件概览及MIT屏幕模块保持；Source附件覆盖整合后的全部已跟踪文件，另外三包不重封装。未删词条、过滤UNKNOWN、替换字体或更改产品。D01/D04/D08未确定的第三方权利仍如实记录，下载与公开状态以 GitHub Release 为准。

Repository documentation: docs/INSTALL.md (installation), docs/BUILD.md (fixed toolchain/build), docs/KNOWN_ISSUES.md (limits), docs/RESOURCE_STATUS.md (specific rights scope), docs/RELEASE_ASSETS.json and RELEASE_PAIR.json (pairing).
