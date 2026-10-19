# Firmware distribution / 固件分发

Alpha 2 packages a newly built release derivative of the tested cumulative business source. By explicit author request, serial composition/input-timing observation is disabled. The internal label is `DRAFTOR_ALPHA2_20261019_RELEASE`; the original measured label and identity remain in the source pairing manifest as a reference. The exact release App length and checksum are in the Firmware pairing manifest, not borrowed from the predecessor.

本版按作者决定关闭输入观察后重新构建，明确区分原实测基线与发行派生版。原业务实现、format4资源及四项性能机制继承；尺寸和校验值取新App，不套用旧值。

The Firmware archive includes this App, its same-build bootloader/partition table, the fixed SDK OTA initializer and host-only file checks. It contains no private flash backup or NVS dump. It has software validation, without a separate new device session. Some original SDK assertion strings may contain build-machine paths; no binary patch is used to conceal them.

固件包包含同次构建配套和离线检查，不含私人Flash/NVS；完成软件验证，尚无新的实机会话。部分原SDK断言可能保留编译机路径，不用二进制改字节掩盖。

[Installation](INSTALL.md) separates old2 MiB migration, compatible4 MiB updating and fresh installation. Source + BuildInputs retain owned source and third-party corresponding materials under their respective terms; see [Resource Status](RESOURCE_STATUS.md).
