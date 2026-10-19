# Firmware distribution / 固件分发

Alpha 2's Firmware archive contains the unchanged 2,441,680-byte cumulative App, its build-generated bootloader and partition table, the fixed SDK OTA initializer, the exact CSV and host-only checks. It contains no device flash backup or NVS dump. The internal build label remains `DRAFTOR_CUMULATIVE_ROUND1_20261019_MEASURE`.

固件包原样封装实测App，附构建/SDK启动材料及离线检查，不含私人Flash或NVS。内部构建标识保留不改。

[Install/upgrade](INSTALL.md) distinguishes old 2 MiB migration, exact compatible 4 MiB updating and first installation. The Source and BuildInputs archives provide project-owned code plus corresponding sources/notices for included third-party components under their respective terms; they do not convert all resources to Apache-2.0. See [Resource Status](RESOURCE_STATUS.md) and [License Inventory](LICENSE_INVENTORY.md).
