# Draftor Alpha 3 · Waveshare firmware

The Waveshare ESP32-S3-RLCD-4.2 (ST7305, 180°) is this release's firmware target. The [target manifest](../release/alpha3/WAVESHARE_RLCD42.json) identifies the App and the three generic startup files. The App has its own SHA-256 and fits either supported 4 MiB App partition. The [device record](ALPHA3_VALIDATION.md) describes the actual acceptance and the separate blank-board validation limit.

Firmware and LearningIndex carry their own manifests. The Source ZIP contains the tagged Git tree. The [TF manifest](../release/alpha3/TF_RESOURCES.json) identifies the historical dictionary/font asset and the new generic reverse lookup and authentication data. For developer inputs see [EXTERNAL_BUILD_INPUTS](EXTERNAL_BUILD_INPUTS.md).

Bootloader and partition bytes come from the matching toolchain/build. The OTA start data comes from Arduino-ESP32 3.3.11. The independently read-back installed App is byte-identical to the generic build App. Public packages use only build and resource inputs; private board backups, identifiers, keys and user files stay with the operator. The original ELF, linker map and raw build logs are retained locally.

[中文安装指南](INSTALL.zh-CN.md) · [Installation](INSTALL.md) · [来源与许可](RESOURCE_STATUS.md) · [第三方声明](../THIRD_PARTY.md)
