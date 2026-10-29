# Third-Party Notices / 第三方声明

**Draftor Alpha · v0.1.0-alpha.1**

[License Inventory / 许可证清单](docs/LICENSE_INVENTORY.md) · [Resource Status / 资源许可](docs/RESOURCE_STATUS.md) · [Firmware Distribution / 固件分发](docs/FIRMWARE_DISTRIBUTION.md)

Draftor uses original code alongside third-party libraries, fonts, dictionaries and other resources. This page identifies the main third-party materials and points to their original license and attribution records.

Draftor 除了原创代码，也使用了第三方程序库、字体、词库及其他资源。本页说明主要第三方材料的来源，以及在哪里查阅相应的许可证与署名信息。

Third-party materials retain their own copyright and licensing terms. They are not automatically covered by Draftor's Apache-2.0 license.

第三方材料保留各自的版权和许可条件，不会因为包含在 Draftor 中，就自动改为 Apache-2.0。

## 1. Draftor & ST7306 / Draftor 与独立屏幕模块

New Draftor-owned source code, including bgIME Core, is released under **Apache License 2.0**.

Draftor 本次公开的自有源码，包括 bgIME Core，采用 **Apache License 2.0**。

**Copyright © 2026 Ruotao Wang.**

See [LICENSE](LICENSE) and [NOTICE](NOTICE).

完整许可文本与署名信息见 [LICENSE](LICENSE) 和 [NOTICE](NOTICE)。

The independently published [`display/st7306`](display/st7306/) module retains its original **MIT License**, together with its existing third-party notices.

此前独立公开的 [`display/st7306`](display/st7306/) 模块继续采用原有的 **MIT License**，并保留已有的第三方声明。

Its display initialization code draws on OSPTEK reference materials. The identified OSPTEK revision contains an MIT license as well as a CC BY 4.0 statement in its README. Both upstream statements are recorded in the module's notices; they must not be replaced by a blanket claim that all OSPTEK material is MIT-only.

其中的屏幕初始化代码参考了 OSPTEK 的公开材料。已确认的 OSPTEK 版本包含 MIT 许可证，同时在 README 中标注了 CC BY 4.0。模块声明保留了这两项上游信息，不能笼统地将所有 OSPTEK 材料视为仅采用 MIT。

See [ST7306 License](display/st7306/LICENSE.md) and [ST7306 Third-Party Notices](display/st7306/THIRD_PARTY_NOTICES.md).

具体见 [ST7306 许可证](display/st7306/LICENSE.md)及[第三方声明](display/st7306/THIRD_PARTY_NOTICES.md)。

## 2. Software Libraries / 第三方程序库

**TinyUSB**

The included TinyUSB source is based on fixed revision `53f8c53c2` and retains its **MIT License** and original authorship information.

所提供的 TinyUSB 源码基于固定版本 `53f8c53c2`，保留原有的 **MIT License** 与作者信息。

License: [vendor/tinyusb/LICENSE](vendor/tinyusb/LICENSE)

**Arduino-ESP32**

The project uses Arduino-ESP32 3.3.11. Identified Arduino USB HAL portions and local modifications retain their applicable **LGPL-2.1** terms.

项目使用 Arduino-ESP32 3.3.11。已识别的 Arduino USB HAL 部分及相关本地修改保留适用的 **LGPL-2.1** 条款。

The original license, source modifications and available build materials are provided with the release.

相应的原始许可、源码修改说明及现有构建材料随发行提供。

See [Arduino License](vendor/Arduino-LICENSE.md) and [Modification Notes](vendor/MODIFICATIONS.md).

详见 [Arduino 许可证](vendor/Arduino-LICENSE.md)及[修改记录](vendor/MODIFICATIONS.md)。

**Bluetooth and SD libraries / 蓝牙与 SD 库**

The BLE and SD libraries retain their respective original licensing and attribution requirements. Project modifications are documented separately.

BLE 和 SD 库保留各自原有的许可证及署名要求。项目所作的修改另有记录。

For the modified SD library, see [SD Modification Notice](integration/product/libraries/SD/NOTICE.md).

SD 库的具体修改见 [SD 修改声明](integration/product/libraries/SD/NOTICE.md)。

Other linked firmware components retain their own applicable terms. A component-level record is available in [LINKED_COMPONENTS.json](docs/LINKED_COMPONENTS.json).

其他参与固件链接的组件也分别保留其适用条款。组件记录见 [LINKED_COMPONENTS.json](docs/LINKED_COMPONENTS.json)。

## 3. Fonts / 字体

Draftor's font resources include material traced to the following projects:

Draftor 的字体资源包含来自以下项目的已追溯材料：

| Font / 字体 | Recorded license / 已记录许可 |
|---|---|
| LXGW WenKai 1.522 / 霞鹜文楷 | SIL Open Font License 1.1 |
| Ysabeau Office 2.006 | SIL Open Font License 1.1 |
| Jigmo2 | CC0 1.0 |

Original font sources and applicable license notices are supplied with the companion release materials.

相应原始字体材料及适用许可声明随配套发行包提供。

The rare-character resource `u28fcd.hbf1` contains an identified Jigmo2-derived contribution associated with U+28FCD.

生僻字资源 `u28fcd.hbf1` 中包含一项已确认来源于 Jigmo2 的 U+28FCD 字形贡献。

Font-specific provenance is described in [Resource Status](docs/RESOURCE_STATUS.md).

字体来源的详细说明见[资源来源与许可情况](docs/RESOURCE_STATUS.md)。

## 4. Dictionaries & Language Data / 词库与语言数据

Draftor's Chinese input resources incorporate material associated with Rime dictionaries, CC-CEDICT and other linguistic datasets.

Draftor 的中文输入资源使用了来自 Rime 词典、CC-CEDICT 及其他语言数据集的材料。

The recorded sources include materials associated with LGPL-v3, Apache-2.0 and CC BY-SA 4.0. The corresponding upstream authorship information, source references and available license texts are retained.

已记录的来源涉及 LGPL-v3、Apache-2.0 和 CC BY-SA 4.0 等许可。相应上游作者信息、来源记录及现有许可文本予以保留。

Wubi 86 resources include material from `rime-wubi`, with its original authorship and licensing declarations.

五笔86资源包含来自 `rime-wubi` 的材料，并保留原作者及许可证声明。

Other identified materials include JIEBA and THUOCL contributions under MIT, and Unicode-related data under Unicode License V3.

其他已识别材料包括采用 MIT 的 JIEBA、THUOCL 相关贡献，以及采用 Unicode License V3 的 Unicode 数据。

**A license associated with an individual upstream source does not automatically determine the licensing terms of a combined dictionary.**

**某个上游来源的许可证，不会自动决定混合词库整体应适用的许可条件。**

The source records, available conversion materials and outstanding questions are described in [Resource Status](docs/RESOURCE_STATUS.md).

具体来源、现有转换材料及尚未明确的问题见[资源来源与许可情况](docs/RESOURCE_STATUS.md)。

## 5. Accompanying Materials / 配套发行材料

The [Alpha release](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) provides source code, runtime resources, additional build inputs and tested firmware in separate archives.

[Alpha 发行页面](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)分别提供源码、运行资源、构建输入及实测固件。

The `Draftor_Alpha_BuildInputs.zip` archive contains available upstream source materials, font sources, original license notices, dictionary attribution records and related build inputs.

`Draftor_Alpha_BuildInputs.zip` 包含现有上游源码、字体来源、原始许可声明、词库署名记录及相关构建输入。

Other accompanying archives also contain relevant resource and distribution notices.

其他配套压缩包中也包含相应资源及分发声明。

For the complete inventory, see [License Inventory](docs/LICENSE_INVENTORY.md). For firmware redistribution arrangements, see [Firmware Distribution](docs/FIRMWARE_DISTRIBUTION.md).

完整清单见[许可证清单](docs/LICENSE_INVENTORY.md)，固件再分发安排见[固件分发说明](docs/FIRMWARE_DISTRIBUTION.md)。

## 6. Outstanding Questions / 尚未明确的事项

Three specific third-party licensing questions remain documented:

目前仍记录了三项具体的第三方许可问题：

- **Mixed dictionary resources / 混合词库：** The file-level licensing treatment of resources combining contributions from differently licensed datasets remains unresolved.

  不同许可证来源的数据合并后，部分资源文件应适用的具体许可条件尚未完全明确。

- **Espressif build recipe / Espressif 构建配置：** A specific 3,400-byte `arduino_tinyusb` CMake recipe has been included in the build materials, but its redistribution permission has not been established.

  配套构建材料包含一份 3,400 字节的 `arduino_tinyusb` CMake 构建配置，但其再分发授权尚未确认。

- **Historical dictionary records / 历史辞典记录：** The copyright and adaptation status of 18 short reference records associated with Taiwan Ministry of Education dictionary materials remains unresolved.

  18 条涉及台湾教育部辞典材料的历史参考记录，其著作权及改作适用范围尚未明确。

These questions relate to particular third-party materials, not to the ownership of Draftor's original source code.

以上问题针对具体第三方材料，并不改变 Draftor 自有源码的权属。

Inclusion in the release does not itself establish redistribution permission. Equally, an unresolved question is not, by itself, a finding of infringement.

材料已经包含在发行包中，不等于其再分发授权已经得到确认；许可问题尚未明确，也不等于已经认定存在侵权。

Detailed affected files, upstream references and current evidence are recorded in [Resource Status](docs/RESOURCE_STATUS.md).

受影响文件、上游来源及现有依据详见[资源来源与许可情况](docs/RESOURCE_STATUS.md)。

---

Original third-party license texts and attribution records remain applicable. This page is a summary and does not replace them.

第三方许可证原文及署名记录继续有效。本页是说明与索引，不替代原始许可文本。

[LICENSE](LICENSE) · [NOTICE](NOTICE) · [License Inventory / 许可证清单](docs/LICENSE_INVENTORY.md) · [Resource Status / 资源许可](docs/RESOURCE_STATUS.md)

## Alpha 2 addition / Alpha 2 新增组件

The already-integrated Wi-Fi QR feature uses Nayuki's QR-Code-generator C implementation at commit `3c6d0b3cefb4e049dc337e82237c9644399716a8` under MIT. Source, original license and source identity are in `integration/qr/vendor/nayuki/`; the [MIT notice](LICENSES/Nayuki_QR_MIT.txt) also accompanies the binary packages. The wrapper is Draftor-owned. No new dictionary or font licensing grant is claimed. Existing D01/D04/D08 provenance and redistribution questions remain disclosed; preparing this release does not resolve them.

既有二维码功能使用上述 MIT 实现，原源码、许可证与来源一并保留，二进制附件也附带原 MIT 声明；封装为项目自有代码。本次不新增词库/字体许可主张，原D01/D04/D08未决事项继续披露，不因封包变为已解决。

## Alpha 2.1 fonts and C++ runtime / Alpha 2.1 字体与 C++ 运行库

The editor and Find use LXGW WenKai (OFL-1.1); menus use MiSans under Xiaomi’s own license. The new UI16 data and menu tables retain their separate terms. See [font sources and distribution status](docs/FONT_DISTRIBUTION.md).

编辑器和查找采用文楷，菜单采用MiSans。新增字形数据分别适用原许可，具体来源与分发状态见上述链接。

The link adjustment extracts `string-inst.o` from the pinned GCC 14.2.0 libstdc++ archive. GCC runtime material remains covered by its original GPL terms and GCC Runtime Library Exception 3.1, rather than Apache-2.0. BuildInputs retains `COPYING.RUNTIME`, `COPYING3` and `COPYING3.LIB` under `release-materials/sdk-notices/toolchain/gcc/`. The object is extracted during the build, not checked into Source.

字符串实现取自固定GCC 14.2.0运行库；其原GPL及运行库例外条款继续适用。BuildInputs保留上述许可文本，构建时提取对象，Source不附预编译对象。


Waveshare A1: ST7305 initialization/pixel mapping adapted from the fixed Apache-2.0 reference. See [vendor attribution and modifications](vendor/waveshare-st7305/NOTICE.md) and its LICENSE.
## Waveshare SDMMC lifecycle correction

The Waveshare-only staged SDMMC/FAT owner is adapted from Espressif ESP-IDF v5.5.5 `b774170ff46` (Apache-2.0). The exact reference, source identity, license and modification notice are in `vendor/sdmmc-lifecycle/`. Installed SDK files and the original target's SD driver are not modified.
