> Historical Alpha 2.1 original-board documentation. Current Waveshare instructions are in [INSTALL](INSTALL.md); do not mix target images.

# Alpha 2.1 firmware and packages / 固件与配套包

The App identifies itself as `DRAFTOR_ALPHA_2_1_20261026_RELEASE` and contains **3,893,280 bytes**, leaving **301,024 bytes** in a 4 MiB slot. It is a fresh S3 build. Its exact hash and runtime pairing are in [RELEASE_PAIR.json](../release/alpha2.1/RELEASE_PAIR_ESP32S3_OSPTEK_ST7306.json); the source commit and four archive hashes are recorded in the distribution PAIRING_MANIFEST.json.

本版App使用上述发行身份，大小为**3,893,280字节**，4 MiB槽余**301,024字节**。这是一次真实S3构建，准确哈希、资源配对和源码提交分别记录于配套清单。

Firmware contains the App, same-build bootloader and partition table, the pinned SDK OTA initializer and offline installation tools. It contains no device-specific flash, NVS or boot-state backup. Use the [installation guide](INSTALL_ALPHA2_1.md) to distinguish App updates from fresh installation.

Firmware附带同次构建的启动与分区文件、固定SDK的初始化文件和离线安装工具，不包含设备Flash、NVS或启动记录备份。App更新和全新安装分别按安装指南执行。

Runtime resource files are unchanged from Alpha 2. Source includes the current code and documents; BuildInputs adds the retained Find and menu glyph inputs. Existing corresponding-source and dictionary licensing questions remain disclosed in [Resource Status](RESOURCE_STATUS.md). MiSans generated-component distribution is addressed separately in [Font Distribution](FONT_DISTRIBUTION.md).

运行资源与Alpha 2相同。Source包含当前代码和文档，BuildInputs增加Find与菜单生成输入；既有来源问题和MiSans生成组件的分发依据分别见上述说明。

The accepted predecessor has a finite device record for status display, input/save and F2/Find return. This newly built release App has software evidence only; its own device check is pending. Earlier performance measurements retain their original Alpha 2 labels.

已接受的前版有状态栏、输入保存和F2／Find往返的有限实机记录。本次新构建尚未上机，不能借用前版记录称为已测；历史性能数据仍属于Alpha 2。
