# Firmware distribution / 固件分发

The release App is `DRAFTOR_ALPHA2_20261019_RELEASE`, **2,429,104 bytes**. It was built from the tested cumulative business implementation with input-code/timing observation disabled at the author's request. Ordinary startup, health and error diagnostics remain. This documentation update reuses that exact App and all matching boot/partition files; it does not rebuild firmware or dictionaries.

发行 App 为 `DRAFTOR_ALPHA2_20261019_RELEASE`，**2,429,104 字节**。它由实测累计业务实现关闭输入组成码/计时观察后构建；普通启动、运行状态及错误诊断保留。本次文档整理原样复用此 App 和全部配套启动/分区文件，不重编固件、不重建词库。

The firmware's source snapshot is `1e75507b556e7790f8e5057de8c2dcd7c90087ed`; the full compilation began at `6b2492f56e9953f4a24085356bd53f7c2a6add6c`, before the existing post-build checks and packaging documentation were finalized. The new Source archive adds the 2026-10-22 documentation revision. Its commit is recorded separately from the firmware source in the distribution `PAIRING_MANIFEST.json`.

固件源码快照为上述 `1e75507…`，完整编译始于 `6b2492f…`，其后完成原有构建后检查及封包文档。本次 Source 增加 2026-10-22 文档修订，发行清单分别记录新 Source 提交与固件构建来源，不把文档提交冒充新构建。

The Firmware archive contains the App, its same-build bootloader/partition table, the fixed SDK OTA initializer, the existing offline installation checker and TF staging helper. It contains no private flash image or NVS dump. The exact release App has software checks; its brief device check is still pending separate authorization. The original measured baseline remains separately identified in `RELEASE_PAIR.json` and [Performance](PERFORMANCE.md).

Firmware 包含 App、同次构建启动文件/分区、固定 SDK 的 OTA 初始化文件及既有离线安装检查器和 TF 安装工具，不含私人 Flash/NVS。准确发行 App 已有软件检查，简短实机检查仍待单独授权。原测量基线在清单和性能附页中单独标识。

BuildInputs and Resources archives are reused unchanged. Their embedded guide copies are dated 2026-10-19; use the updated [Installation guide](INSTALL.md) in Firmware or Source for current instructions. Payload pairing is unchanged. The four actual versioned filenames and archive checksums are in the distribution manifest and `SHA256SUMS.txt`.

BuildInputs 与 Resources 压缩包原样复用；其中附带说明仍为 2026-10-19 快照。当前安装步骤以新版 Firmware 或 Source 内的[安装指南](INSTALL.md)为准，资源配对不变。四包准确名称和分发校验值见配套清单与 `SHA256SUMS.txt`。

Source and BuildInputs provide owned source and corresponding third-party materials under their respective terms; [existing resource and license disclosures](RESOURCE_STATUS.md) continue to apply.
