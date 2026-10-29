# Draftor Alpha 2.2 — Waveshare

Target tag: `v0.1.0-alpha.2.2`. This is a prerelease, not a general stability certification. [Qualification/status](ALPHA2_2_VALIDATION.md) controls whether firmware may be published. [Alpha 2.1 notes](RELEASE_NOTES_ALPHA2_1.md) remain historical.

- Brings the accepted Waveshare ST7305/SDMMC port into clean public source, with isolated mutable data and unchanged read-only format4/font resources.
- Retains HOGP all-Input subscription/CCCD readback and menu-footer fixes, with standard F9 restored and special diagnostics/causal shortcut disabled. Bare F9 Consumer input is not mapped into a USB command.
- Corrects normal optional-position absence after valid document import/restore; real position/body/I/O/metadata degradation still reports errors. The small common-source fix affects both S3 builds, without changing the document format or transactions.
- Adds explicit-target, explicit-port installation, separate fresh/update modes, actual partition/OTA-slot checks and independent verify. Adds Host sized-delete portability without weakening sanitizers.

bgIME Core/Extension/Wubi, candidate/commit semantics, resources/pins, ST7305 timing/packing, SDMMC lifecycle and protection guards are retained. The original board remains the default build and continues using its existing Alpha 2.1 device release; no Alpha 2.2 original-board firmware asset is advertised. Pico remains a standalone display demo.

Use [Installation](INSTALL.md), [Build](BUILD.md), the [target manifest](../release/alpha2.2/WAVESHARE_RLCD42.json), and unchanged [input references](../release/alpha2.2/INPUT_REFERENCES.json). The USB string `Draftor USB v1.2 build236` is inherited and does not identify the App version. TF namespaces never fall back or auto-adopt unknown directories.

Device scope, storage evidence, costs and untested areas are listed in [validation](ALPHA2_2_VALIDATION.md). Wi-Fi/QR remains an inherited experiment with limited Waveshare evidence. No claim covers all keyboards, power arrangements or fresh boards. No private manuscripts, MAC, pairing, Flash backup, ELF, SDK or raw device logs are included in public assets. Third-party licenses remain separate; see [Resource Status](RESOURCE_STATUS.md) and [Third Party](../THIRD_PARTY.md).

Finite final-image result / 最终镜像有限结果：88 B 未编辑导入→正常保存→一次重启→真实导出逐字节相同；82 B 原稿保护通过，3 次正常 SCSI eject，普通键/两种 F9/菜单/横竖 Find 正常，本轮无存储警告。USB 供电、同一 Logitech/Fn 配置、Linux/Pi 主机；照片未收到，存储实际 status/stage 未采集。完整边界见验收说明。
