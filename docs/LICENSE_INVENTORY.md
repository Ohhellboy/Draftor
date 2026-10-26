# License Inventory / 许可证清单

**Version / 适用版本：Draftor Alpha 2 · v0.1.0-alpha.2**

[Project](../README.en.md) / [项目主页](../README.md) · [LICENSE](../LICENSE) · [NOTICE](../NOTICE) · [Resource Status / 资源许可情况](RESOURCE_STATUS.md) · [Third-Party Notices / 第三方声明](../THIRD_PARTY.md)

Draftor contains original source code, third-party libraries, Chinese dictionaries, fonts and other resources. They do not all use the same license.

Draftor 包含原创源码、第三方程序库、中文词库、字体及其他资源。这些材料并不全部采用相同的许可证。

This inventory identifies the applicable licenses and source materials recorded for the current Alpha release. It also distinguishes established licensing information from questions that have not yet been resolved.

本清单整理当前 Alpha 已记录的许可证与来源材料，并区分已经明确的许可信息和仍待确认的问题。

## 1. Draftor-Owned Source / Draftor 自有源码

New Draftor-owned source code, including bgIME Core, the writing application and related development tools, is released under the **Apache License 2.0**.

Draftor 本次公开的自有源码，包括 bgIME Core、写作程序及相关开发工具，采用 **Apache License 2.0**。

**Copyright © 2026 Ruotao Wang.**

The applicable text is available in [LICENSE](../LICENSE), with accompanying attribution information in [NOTICE](../NOTICE).

完整许可文本见 [LICENSE](../LICENSE)，配套署名信息见 [NOTICE](../NOTICE)。

Apache-2.0 includes its own copyright, patent and redistribution provisions. In particular, Sections 3 and 4 address patent grants and redistribution requirements.

Apache-2.0 自身包含版权、专利授权及再分发条款。其中第 3 条涉及专利授权，第 4 条涉及再分发要求。

The project does not add separate restrictions to the Apache-2.0 grant for this source code.

对于这些采用 Apache-2.0 的自有源码，项目没有额外附加独立的使用限制。

**The root Apache-2.0 license does not replace licenses that apply to separately licensed modules or third-party resources.**

**根目录的 Apache-2.0 许可证不会覆盖独立许可模块及第三方资源原有的许可证。**

## 2. Component Inventory / 组件许可证清单

The following table describes the 12 material categories recorded for this release.

下表列出本次发行记录中的 12 类材料。

| Material / 材料 | License / 许可证 | Source & status / 来源与情况 |
|---|---|---|
| **Standalone ST7306 display module / 独立屏幕模块** | MIT; original third-party notices retained / MIT，保留第三方声明 | `display/st7306/`. Previously published independent module; its original licensing terms remain in effect / 此前已独立公开，原许可继续有效 |
| **Draftor application, bgIME Core, Extension and tools / 自有程序及输入法源码** | Apache-2.0 | Root `LICENSE` and `NOTICE`; author-owned source in the repository / 仓库中的自有源码及根目录许可文本 |
| **TinyUSB** | MIT | Fixed upstream revision `53f8c53c2`; `vendor/tinyusb/LICENSE`. Original authorship retained / 固定上游版本，保留原作者声明 |
| **Arduino USB HAL and SDK modifications / Arduino USB 及 SDK 修改** | LGPL-2.1 for the identified Arduino portions / 已识别 Arduino 部分适用 LGPL-2.1 | Arduino-ESP32 3.3.11; `vendor/Arduino-LICENSE.md` and `vendor/MODIFICATIONS.md`. Corresponding materials supplied / 提供原始许可、修改记录及配套材料 |
| **BLE and SD libraries / 蓝牙与 SD 库** | Their respective bundled licenses / 各自原有许可证 | Fixed libraries and project modifications; retain individual notices, including `integration/product/libraries/SD/NOTICE.md` / 保留库内声明及修改说明 |
| **LXGW WenKai and related traced font material / 霞鹜文楷及已追溯字体材料** | SIL OFL 1.1 | LXGW WenKai 1.522; related Ysabeau Office 2.006 materials also retain their original OFL terms / 相关原字体及派生材料保留 OFL 条款 |
| **Jigmo2 glyph contributions / Jigmo2 字形材料** | CC0 1.0 | Fixed 2025-09-12 source; identified contribution includes U+28FCD / 固定来源版本，包括已识别的 U+28FCD 字形贡献 |
| **JIEBA, THUOCL and Unicode data / JIEBA、THUOCL 与 Unicode 数据** | MIT for identified JIEBA/THUOCL portions; Unicode License V3 for Unicode data / 对应部分适用 MIT 与 Unicode License V3 | Original license texts supplied; conclusions apply to identified contributions / 提供原始许可，适用范围限于已识别材料 |
| **Wubi dictionary and converted data / 五笔词典及转换数据** | Original `rime-wubi` LGPL declaration; related LGPL-v3 and GPL-v3 texts retained / 保留原 LGPL 声明及相关 LGPL-v3、GPL-v3 文本 | Fixed upstream dictionary, AUTHORS, original tables and conversion materials supplied. Shared mixed-data questions remain / 提供原词典、作者信息和转换材料；共用混合词库许可仍需确认 |
| **B2 and legacy dictionaries and fonts / B2 与历史词库、字体** | Multiple source-specific licenses, including LGPL, Apache-2.0, CC BY-SA, OFL, CC0 and Unicode / 按不同来源分别适用相应许可 | Original resources and identified source bindings retained. Some mixed-resource terms remain unresolved / 保留原始资源及来源关联，部分混合资源许可仍未明确 |
| **Espressif lib-builder recipe / Espressif 构建配置** | Redistribution permission not established / 再分发授权尚未确认 | Fixed `ee57070` source, including a 3,400-byte CMake recipe. The file has been supplied, but an applicable grant has not been established / 已提供文件，但未确认对应授权 |
| **Prebuilt firmware and installation binaries / 预编译固件及安装文件** | Combined distribution; component-specific terms apply / 组合发行，分别适用各组件条款 | Original device-tested firmware, corresponding source and available build/relink materials supplied. Specific outstanding questions remain / 提供实测固件与配套材料，仍有具体许可问题待确认 |

## 3. Original License Texts / 原始许可证文本

The repository includes license files for identifiable components and source materials.

仓库为已识别的组件与来源材料提供相应许可证文件。

Useful references include:

主要参考文件包括：

| File / 文件 | Purpose / 用途 |
|---|---|
| [LICENSE](../LICENSE) | Draftor-owned Apache-2.0 source / Draftor 自有源码许可 |
| [NOTICE](../NOTICE) | Project copyright and attribution / 项目版权与署名 |
| [ST7306 MIT License](../display/st7306/LICENSE.md) | Independent display module / 独立显示模块 |
| [ST7306 Third-Party Notices](../display/st7306/THIRD_PARTY_NOTICES.md) | Display-related third-party information / 显示模块第三方声明 |
| [Arduino LGPL-2.1](../vendor/Arduino-LICENSE.md) | Arduino-related LGPL terms / Arduino 相关 LGPL 条款 |
| [SDK Modifications](../vendor/MODIFICATIONS.md) | Fixed upstream versions and local modifications / 上游版本与本地修改 |
| [OFL 1.1](../LICENSES/OFL-1.1.txt) | Font licensing / 字体许可 |
| [CC0 1.0](../LICENSES/CC0-1.0.txt) | Identified CC0 materials / 已识别 CC0 材料 |
| [GNU LGPL v3](../LICENSES/GNU_LGPL_v3.txt) | Relevant dictionary and source terms / 相关词库及源码条款 |
| [GNU GPL v3](../LICENSES/GNU_GPL_v3.txt) | Accompanying GPL terms / 配套 GPL 条款 |
| [Unicode License](../LICENSES/Unicode_LICENSE.txt) | Unicode-related materials / Unicode 相关材料 |
| [JIEBA License](../LICENSES/jieba_LICENSE) | Identified JIEBA material / 已识别 JIEBA 材料 |
| [THUOCL License](../LICENSES/thuocl_LICENSE) | Identified THUOCL material / 已识别 THUOCL 材料 |

Additional original source records, license notices and attribution files are included in the matching BuildInputs and other release archives.

配套 BuildInputs 及其他发行压缩包中还包含额外的原始来源记录、许可声明和署名文件。

**Original third-party license texts are preserved. This inventory is a summary, not a replacement for those texts.**

**第三方许可证原文予以保留。本清单仅用于索引和说明，不能替代原始许可文本。**

## 4. Firmware Redistribution / 固件再分发

The released firmware combines Draftor-owned code with libraries and components covered by other licenses.

本次发行的固件同时包含 Draftor 自有源码，以及采用其他许可证的程序库和组件。

In particular, the identified LGPL-covered Arduino components have corresponding-source and modification/relinking requirements that must be considered when redistributing the combined firmware.

其中，已识别的 Arduino LGPL 组件涉及对应源码、修改及重新链接等再分发要求。

The release provides available source files, modification records and build materials for these components. The applicable approach and its remaining limitations are described in [Firmware Distribution](FIRMWARE_DISTRIBUTION.md).

本次发行提供这些组件现有的源码、修改记录及构建材料。具体分发安排与仍存在的限制见[固件分发说明](FIRMWARE_DISTRIBUTION.md)。

Providing a firmware binary or its source materials does not automatically establish compliance with every third-party license.

提供固件二进制及其来源材料，并不自动意味着所有第三方许可义务均已得到确认和满足。

## 5. Unresolved Questions / 尚未确认的事项

Three specific third-party issues remain documented for the current Alpha release:

当前 Alpha 仍记录了三项具体的第三方许可问题：

| Material / 材料 | Current status / 当前情况 |
|---|---|
| **Mixed dictionary data / 混合词库** | File-level license treatment of material combining LGPL and CC BY-SA sources is not fully determined / LGPL 与 CC BY-SA 来源混合后的文件级许可适用范围尚未明确 |
| **Espressif lib-builder recipe / 构建配置** | A redistribution grant for the identified 3,400-byte file has not been established / 尚未确认这份 3,400 字节文件的再分发授权 |
| **Historical MOE dictionary records / 历史教育部辞典记录** | Copyright and adaptation status of 18 reference records remains unresolved / 18 条参考记录的著作权及改作适用范围尚未明确 |

These issues are associated with specific files or source contributions. They should not be interpreted as applying one unresolved license to the entire Draftor source tree.

上述问题针对具体文件或来源贡献，不应被理解为整个 Draftor 源码树都适用某一项未确定的许可证。

The full source references, affected archives and existing evidence are documented in [Resources & Licensing Status](RESOURCE_STATUS.md).

详细来源、受影响的发行文件及现有依据，见[资源来源与许可情况](RESOURCE_STATUS.md)。

An unresolved permission question is not evidence that material is in the public domain, nor is it an automatic Apache-2.0 grant. It is also not, by itself, a determination of infringement.

授权尚未明确，不代表材料自动进入公有领域或适用 Apache-2.0，也不能仅凭这一状态认定存在侵权。

## 6. Using or Redistributing Draftor / 使用与再分发 Draftor

If you plan to modify or redistribute Draftor, identify which source files, libraries, dictionaries and fonts your version actually includes.

如果准备修改或再分发 Draftor，请先确认自己的版本实际包含哪些源码、程序库、词库和字体。

Follow the applicable license for each material, preserve required copyright and attribution notices, and review the outstanding questions before redistributing affected third-party resources.

各类材料应分别遵守相应许可证，保留必要的版权及署名信息，并在再分发受影响的第三方资源前核查尚未解决的问题。

Reusing bgIME Core source under Apache-2.0 is not the same as acquiring rights to every dictionary or font distributed alongside the complete Draftor application.

依照 Apache-2.0 复用 bgIME Core 源码，并不等于同时取得完整 Draftor 中所有词库或字体的再分发权利。

For component-level details, refer to the original license texts and the accompanying release materials.

涉及具体组件时，应以原许可证及随发行提供的材料为依据。

---

[Third-Party Notices / 第三方声明](../THIRD_PARTY.md) · [Resource Status / 资源许可](RESOURCE_STATUS.md) · [Firmware Distribution / 固件分发](FIRMWARE_DISTRIBUTION.md) · [Resource File Inventory / 资源文件清单](RESOURCE_FILES.json)

## Alpha 2 addition / Alpha 2 新增组件

The already-integrated Wi-Fi QR feature uses Nayuki's QR-Code-generator C implementation at commit `3c6d0b3cefb4e049dc337e82237c9644399716a8` under MIT. Source, original license and source identity are in `integration/qr/vendor/nayuki/`. The wrapper is Draftor-owned. No new dictionary or font licensing grant is claimed. Existing D01/D04/D08 provenance and redistribution questions remain disclosed; preparing this release does not resolve them.

既有二维码功能使用上述 MIT 实现，原源码、许可证与来源一并保留；封装为项目自有代码。本次不新增词库/字体许可主张，原D01/D04/D08未决事项继续披露，不因封包变为已解决。

## Alpha 2.1 fonts and C++ runtime / Alpha 2.1 字体与 C++ 运行库

The editor and Find use LXGW WenKai (OFL-1.1); menus use MiSans under Xiaomi’s own license. The new UI16 data and menu tables retain their separate terms. See [font sources and distribution status](FONT_DISTRIBUTION.md).

编辑器和查找采用文楷，菜单采用MiSans。新增字形数据分别适用原许可，具体来源与分发状态见上述链接。

The link adjustment extracts `string-inst.o` from the pinned GCC 14.2.0 libstdc++ archive. GCC runtime material remains covered by its original GPL terms and GCC Runtime Library Exception 3.1, rather than Apache-2.0. BuildInputs retains `COPYING.RUNTIME`, `COPYING3` and `COPYING3.LIB` under `release-materials/sdk-notices/toolchain/gcc/`. The object is extracted during the build, not checked into Source.

字符串实现取自固定GCC 14.2.0运行库；其原GPL及运行库例外条款继续适用。BuildInputs保留上述许可文本，构建时提取对象，Source不附预编译对象。
