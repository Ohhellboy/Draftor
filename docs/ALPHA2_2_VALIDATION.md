# Alpha 2.2 — evidence, decisions and limits / 验收与边界

**Current status: SOFTWARE_PASS_DEVICE_PENDING.** The final Waveshare image is built; this page will record its finite device result before any flashable release. Production inputs are identical to the public sanitized source commit `b1df4a858de39fc651b850bc9341599dd8f96ab3`; only two Host fixture texts were sanitized after the build; later installer/document commits do not rebuild or substitute that image. The original board receives a full source build, not a new device-release claim.

## Actual corrections / 实际修正

1. Shared `document_session.cpp::startupRestore()` now treats missing optional POSITION as a normal validated snapshot default. It continues reporting invalid/mismatched/I/O/degraded POSITION, and now preserves real body/catalog/CURRENT/metadata degradation instead of losing it in the final status. Document format, import/save transaction and protection guards are unchanged. Both S3 targets include this small common-code change.
2. Four Host allocation fixtures gain sized-delete overloads; normal and ASan/UBSan retain allocation matching and assertions. macOS Clang explicitly exercises `-fsized-deallocation`; this is not a claim of a new Linux runtime qualification.
3. Public installation explicitly selects board, port, identity and fresh/update mode. Actual compatible partition bytes and OTA slot are read before update; no assumed app0, no automatic full erase. The exact pinned SDK unused OTA sentinel is accepted alongside an erased record, while malformed entries/CRC/pending/ambiguous states remain refused. Installer changes do not alter product binaries.

The rest is accepted R5 integration and public packaging: HOGP subscription/CCCD readback, ordinary F9, footer ownership, ST7305, staged SDMMC and isolated namespaces. Core/Extension/Wubi implementations and three canonical resource pins were byte-compared to R5 and are unchanged; no learning branch is included.

## Storage classification / 存储结论

The old R5 device warning had no captured status/stage. Its exact historical cause remains unknown. The following **actual Host reproduction** is sufficient to identify a bounded false-warning path, not to invent that historical state:

| Path | Before (exact R5) | Alpha 2.2 |
|---|---|---|
| Import/open, no body edits, durable AUTO generation 1 + valid CRC/CURRENT, POSITION absent | `OK_DEGRADED / position_fallback` | `OK / position_default` |
| Same unedited import, normal explicit save, recreate session | Valid durable body and position | `OK / complete` |
| Valid matching position | Restore cursor | `OK / complete` |
| Position generation/CRC mismatch, corrupt record, read I/O, invalid UTF-8 cursor boundary | Must not hide body; must warn | `OK_DEGRADED / position_fallback` |
| Position A valid, B corrupt | Genuine degraded record | `OK_DEGRADED / position_degraded` |
| Body corrupt or body read I/O | Fail to install unsafe body | `CORRUPT` or `IO_ERROR / body_load` |
| Body A valid, B corrupt | Genuine recovered body | `OK_DEGRADED / body_recovered` |

Import's durable AUTO owns the text; CURRENT is committed when opening it. A POSITION record is optional, bound to body generation/CRC, and created by normal save/position maintenance. Absence does not mean text failed to save. A normal missing-position restore writes no automatic “repair”. PARTIAL and all genuine degradation still map to warning; heap/input/document guards stay intact. The finite device round requires an **unedited** import, explicit save, one reboot, direct export and byte comparison before declaring the current data path qualified.

## Board completeness matrix / 板级检查矩阵

| Area | Existing evidence / 本次检查 | Decision / 决定 | Reason / 理由 | Release impact / 发布影响 |
|---|---|---|---|---|
| Target/pins/parking | `board_config.hpp`, `board_display.hpp`, actual both-target compilation; old TF park/SPI GPIOs inside original-board branch | 检查后不改 | WS uses reset41, SPI11/12/40/5 and SDMMC38/21/39; old reset4/audio/RTC pins are not reached | No new HAL or pin change |
| ST7305 update | Accepted wire-decoder/packing/window completion evidence; driver byte-identical to R5; current display integration Host test | 继承通过 | 300×400, 4×2/15,000 B, synchronous 10 MHz/dirty window semantics unchanged | No TE wait/DMA/thread/frequency claim |
| Monochrome UI | Actual horizontal candidate underline/invalid marker, vertical monochrome port, inverted selection, menu footer ownership; inherited renderer previews | 继承通过 | Information is not conveyed by red alone; original color behavior retained | Finite horizontal/vertical Find visual confirmation pending; no UI redesign |
| SDMMC ownership | A2 rollback/final-owner-allocation/unmount-quarantine evidence; unchanged `board_sdmmc`; actual linked WS native path | 继承通过 | 1-bit20 MHz, single owner, bounded mount, retired handles/mount epoch; no runtime convenience remount | No speed/cache change; physical failure recovery not universally guaranteed |
| Namespace/resources | A3 allocation-failure/valid-marker/new-space tests; code and resource pins unchanged | 继承通过 | Unknown/incomplete roots refused, no original-data fallback; shared resources read-only | No TF migration/reformat/resource rewrite |
| Startup restore | Real App/import/session regression with valid body/CURRENT/CRC and directed fault cases above | 已修并验证 | Normal optional absence distinguished from actual degradation | Shared fix in both builds; final device chain pending |
| HOGP/F9 | R4/R5 accepted finite observations; diagnostics-off HOGP test and linked product closure gate | 继承通过 | All Input reports subscribed/read back, non-keyboard routing isolated; standard F9 and protections retained | F-01 closed only for accepted board/keyboard/Fn configuration; no new root-cause claim |
| USB export/Inbox | R5 actual Linux/Pi transfers/eject; current transaction/first-init/guard/cleanup Host checks | 继承通过 | Snapshot is read-only; Inbox is RAM512 KiB; TF not exposed | Final binary unedited roundtrip and normal eject pending |
| Menus/trash | Accepted R5 list/help/rename/trash footer and response observations; current actual painter tests | 继承通过 | Menu owns footer, no retained IME overlay | No quantified trash performance or large-library claim |
| Power/keys/peripherals | Existing board mapping and reachable startup/parking; USB-powered scope | 检查后不改 | BOOT downloads, KEY18 unmapped, PWR not save; unused peripherals are not requirements | Battery/RTC/audio/sensors/low power 延后 |
| SDK/build/resource costs | Both final builds pass actual deps/pins/link/stack; exact fixed SDK/overlay | 已修并验证 | No SDK, partition or engine upgrade; common-source change built twice | Original physical device untouched; no new original-board asset |
| Public install/privacy/license | Explicit parameterized installer; controlled failures and real-package local plan; clean tracked-source assets | 已修并验证 | Old personal installer is not shipped; vendor notices retained separately | Physical new-board fresh install 未测且限范围 |
| Wi-Fi/QR | Inherited experiment and QR renderer; no new WS actual exchange | 未测且限范围 | No concrete defect found; it remains available without full-device claim | No Wi-Fi/QR qualification, no battery/performance gate |

## Capability coverage / 功能证据范围

| Feature | Software/source | Waveshare device evidence |
|---|---|---|
| Xiaohe / Full Pinyin / Wubi86 / English | Same accepted engine, bindings and commit semantics | A3 finite four-scheme observation inherited; R5 English/Xiaohe observations; not exhaustive per-scheme certification on this new image |
| Horizontal/vertical writing and Find | Same Alpha 2.1 editor/Find; WS monochrome renderer branches retained | A3 limited layout/Find observation inherited; this final-image finite visual check pending |
| F9, menus, save, trash | Current bounded regressions; no causal shortcut or diagnostic dependency | R5 accepted keyboard/Fn/menus/footer observations; final-image smoke pending |
| Real USB and reboot | Current import/save/error/restore tests | R5 normal eject/return passed; original preserved; old 88 B fixture became89 B by intentional edit, **not** an unedited roundtrip PASS; new round pending |
| Local Wi-Fi / QR | Inherited experimental code, unchanged | No new Waveshare exchange; original-board evidence is not relabelled |
| Battery, other keyboards/cards/hosts, endurance, fresh empty board | No new implementation or universal claim | Not qualified in this round |

## Executed software checks and resource costs

`tools/validate_alpha2_2.py`: normal and ASan/UBSan for startup restore, real USB document transaction/product flow/Inbox cleanup, first initialization and guard failure, product F9 route, actual menu painter, display integration and diagnostics-off HOGP. The installer has 15 controlled cases after adding exact SDK unused-OTA acceptance and malformed/CRC counterexamples. Configuration/build steps are not additional functional tests. No complete historical CTest rerun is claimed.

Both complete builds use Arduino CLI1.4.1, Arduino-ESP32 3.3.11, esp-x322601/GCC14.2.0, ESP-IDF v5.5.5 and the fixed TinyUSB overlay. Current actual build warning is the inherited unused `behavior_service_start` variable with observation disabled; it is not hidden and did not require changing firmware.

| Cost | WAVESHARE_RLCD42 | ESP32S3_OSPTEK_ST7306 |
|---|---:|---:|
| App file bytes | 3,911,888 | 3,894,368 |
| Free bytes in 4 MiB App slot | 282,416 | 299,936 |
| Static DRAM bytes | 113,180 | 128,252 |
| Maximum checked critical-path stack budget | 4,832 | 4,832 |
| Runtime heap/PSRAM/stack peaks | Not measured | Not measured |

Waveshare versus accepted R5: App +48 B, static DRAM +1 B. OPI PSRAM capacity is8 MiB; that is not a measured free/peak value. The compiler stack gate covers bounded critical paths with SDK reserves, not whole-program maximum. [Machine-readable costs](../release/alpha2.2/BUILD_COSTS.json) and [four-file/ELF identity](../release/alpha2.2/WAVESHARE_RLCD42.json) bind the real outputs. ELF stays private.

## Device result / 实机结果

Pending the single authorized final-image round. No new binary has device PASS yet. The public source/docs PR can proceed; flashable prerelease remains held until identity, update verification, unedited save/reboot/export comparison and product smoke are recorded. Main is not automatically merged. Private evidence includes real identity/manuscripts and is intentionally excluded from this repository.
