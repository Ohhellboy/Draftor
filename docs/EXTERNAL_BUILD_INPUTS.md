# Firmware Distribution / 固件分发说明

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Download / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Build / 源码构建](BUILD.md) · [Licenses / 许可证](LICENSE_INVENTORY.md) · [Resource Status / 资源与许可](RESOURCE_STATUS.md)

Draftor Alpha provides device-tested firmware for the ESP32-S3 N16R8, together with the corresponding Draftor source code, build inputs and available third-party source materials.

Draftor Alpha 提供经过实机测试的 ESP32-S3 N16R8 固件，同时公开配套的 Draftor 源码、构建输入及已有的第三方来源材料。

The firmware combines code and resources covered by different licenses. This document explains what is distributed and where to find the relevant source materials and license information.

固件包含适用不同许可证的代码与资源。本页说明具体发行内容，以及相应源码和许可信息的位置。

## 1. Firmware Package / 固件发行包

The release includes `Draftor_Alpha_Firmware.zip`, containing the original device-tested application firmware and its matching first-installation files.

发行包中的 `Draftor_Alpha_Firmware.zip` 包含原实测应用固件及配套的首次烧录文件。

| Item / 项目 | Details / 说明 |
|---|---|
| Target / 目标硬件 | ESP32-S3 N16R8 |
| Application firmware / 应用固件 | `Draftor_Alpha_T06_TF20_R1.app.bin` |
| Application size / 应用大小 | 2,061,808 bytes |
| Application offset / 应用烧录偏移 | `0x10000` |
| TF SPI runtime / TF 卡运行频率 | 20 MHz |
| TF SPI initialization / 初始化频率 | 400 kHz |

The package also provides the matching bootloader, partition table and `boot_app0` files.

压缩包还提供配套的 bootloader、分区表及 `boot_app0` 文件。

The firmware does not require a complete flash image to be distributed. Device dumps, private documents and diagnostic logs are not part of the application firmware.

固件发行不需要提供整片 Flash 镜像。设备转储、私人文稿及诊断日志不属于应用固件的发行内容。

For installation and flashing, see [Installation](INSTALL.md).

安装及烧录方法见[安装指南](INSTALL.md)。

## 2. Source Code & Build Materials / 源码与构建材料

The same release provides two additional archives relevant to firmware compilation:

本次发行还提供两个与固件编译有关的压缩包：

| Archive / 压缩包 | Contents / 内容 |
|---|---|
| `Draftor_Alpha_Source.zip` | Draftor application, editor, bgIME Core, USB modules and development tools / 写作程序、编辑器、bgIME Core、USB 模块及开发工具 |
| `Draftor_Alpha_BuildInputs.zip` | Additional project inputs, fixed upstream sources, build materials and license notices / 配套项目输入、固定上游源码、构建材料及许可声明 |

The original firmware build used Arduino-ESP32 **3.3.11**, ESP-IDF **5.5.5** components and other fixed dependencies.

原固件构建使用 Arduino-ESP32 **3.3.11**、ESP-IDF **5.5.5** 相关组件及其他固定版本的依赖。

The provided build materials include source and notices for the relevant Arduino modifications, TinyUSB components, ESP-IDF and managed components, and other identified upstream dependencies.

配套构建材料包含 Arduino 修改部分、TinyUSB 组件、ESP-IDF 及其管理组件，以及其他已识别上游依赖的相关源码和声明。

The original firmware link analysis identified contributing members from **71 libraries**. Their recorded identities are listed in [Linked Components](LINKED_COMPONENTS.json).

原固件链接分析识别出 **71 个库**的参与成员，具体清单见[链接组件清单](LINKED_COMPONENTS.json)。

This number is a description of the linked components, not an indication that all 71 libraries have identical licensing requirements.

这一数字描述的是实际参与链接的组件，并不代表 71 个库适用同一许可证或具有相同的再分发义务。

The complete SDK and compiler toolchain must still be installed separately. Source and build inputs are provided, but a newly compiled binary is not guaranteed to be byte-for-byte identical to the original tested firmware.

完整 SDK 和编译器仍需单独安装。虽然已经提供源码及构建输入，自行重新编译的固件也不保证与原实测版本逐字节相同。

See [Build Guide](BUILD.md) for the exact toolchain and build procedure.

具体工具链及编译步骤见[构建指南](BUILD.md)。

## 3. Third-Party Licenses / 第三方许可证

Draftor-owned source code, including bgIME Core, is released under **Apache License 2.0**.

Draftor 自有源码，包括 bgIME Core，采用 **Apache License 2.0**。

Third-party components retain their applicable licenses, copyright notices and attribution requirements. The project's Apache-2.0 license does not replace those terms.

第三方组件保留各自适用的许可证、版权声明及署名要求。Draftor 的 Apache-2.0 许可证不会替代这些条款。

The release includes components associated with MIT, LGPL and other licenses. Original license texts, component notices and source materials are supplied where available.

本次发行涉及 MIT、LGPL 及其他许可证。相应原始许可文本、组件声明和已取得的来源材料随发行提供。

### LGPL-Covered Components / 采用 LGPL 的组件

Some Arduino-related components and modifications are covered by **LGPL-2.1**.

部分 Arduino 相关组件及修改采用 **LGPL-2.1**。

The distribution materials are organized around the corresponding-source and modification/relinking provisions of LGPL-2.1 Section 6, including the approaches described in Sections 6(a) and 6(d).

现有分发材料按照 LGPL-2.1 第 6 条涉及的对应源码、修改及重新链接要求组织，包括第 6(a) 与 6(d) 款所描述的方式。

The supplied materials include the relevant library sources and modifications, Draftor application source, and available build and relinking inputs. The required fixed-version toolchain is documented separately.

已提供的材料包括相关库源码及修改内容、Draftor 应用源码，以及现有构建与重新链接所需的配套输入。指定版本工具链另有说明。

Users retain the applicable rights to modify LGPL-covered libraries and debug those modifications, subject to the terms of the relevant license.

用户依照相关 LGPL 条款，保留修改对应库以及为调试这些修改进行必要操作的权利。

Providing these materials describes the project's distribution approach; it should not be read as independent confirmation that every licensing obligation has been fully satisfied.

以上说明的是项目目前采用的材料交付方式，并不代表所有许可义务已经经过独立确认、全部履行完毕。

For component-specific terms, see [License Inventory](LICENSE_INVENTORY.md) and [Third-Party Notices](../THIRD_PARTY.md).

各组件的具体许可见[许可证清单](LICENSE_INVENTORY.md)及[第三方声明](../THIRD_PARTY.md)。

## 4. Known Licensing Questions / 尚未明确的许可事项

Most of the recorded source and licensing materials are available, but several specific redistribution questions remain unresolved.

目前已提供大量来源及许可材料，但仍有少数具体的再分发问题尚未确认。

**Mixed dictionary data / 混合词库数据**

Some runtime dictionaries combine material originating under different licenses, including LGPL and CC BY-SA. The applicable terms for certain derived or combined files remain to be determined.

部分运行词库整合了原本适用 LGPL、CC BY-SA 等不同许可证的数据。部分派生或混合文件具体适用的条款尚待确认。

**Espressif build recipe / Espressif 构建配置**

The BuildInputs archive contains a fixed 3,400-byte `components/arduino_tinyusb/CMakeLists.txt` recipe from Espressif's `esp32-arduino-lib-builder` materials. An applicable redistribution grant for this particular file has not been established.

BuildInputs 包含来自 Espressif `esp32-arduino-lib-builder` 的一份固定版本、3,400 字节的 `components/arduino_tinyusb/CMakeLists.txt` 构建配置。目前尚未确认覆盖该文件的具体再分发授权。

**Historical dictionary reference records / 历史辞典参考记录**

A historical Host test fixture contains 18 short language-reference records associated with Taiwan Ministry of Education dictionary materials. Their precise copyright and adaptation scope remains unresolved.

一份历史 Host 测试夹具包含 18 条涉及台湾教育部辞典材料的简短语言参考记录。这些记录的具体著作权及改作适用范围尚待确认。

These questions concern identifiable third-party materials. They do not automatically change the license applied to Draftor-owned source code.

以上问题涉及具体的第三方材料，不会自动改变 Draftor 自有源码的许可证。

**Inclusion in a release is not, by itself, evidence of a redistribution grant.**

**材料已经随发行提供，并不意味着其再分发授权已经得到确认。**

The known sources, affected files and outstanding questions are documented in [Resource Status](RESOURCE_STATUS.md).

已确认的来源、受影响文件及待解决问题详见[资源与许可说明](RESOURCE_STATUS.md)。

## 5. Version Compatibility / 版本配套

The firmware, source code, dictionaries, fonts and build materials are associated with the same release version.

固件、源码、词库、字体及构建材料属于同一发行版本。

The version label **Draftor Alpha · v0.1.0-alpha.1** is separate from the internal firmware build identity.

发行名称 **Draftor Alpha · v0.1.0-alpha.1** 与固件内部构建标识是两回事。

The original technical identifiers, resource paths and build configuration are retained to preserve compatibility and traceability.

原有技术标识、资源路径及构建配置仍然保留，以保证兼容性和版本可追溯性。

For the exact firmware and resource pairing, see [Release Pair](../RELEASE_PAIR.json) and [Release Assets](RELEASE_ASSETS.json).

具体固件与资源配对见[版本配对清单](../RELEASE_PAIR.json)和[发行附件清单](RELEASE_ASSETS.json)。

Do not mix firmware and resources from different releases without verifying compatibility.

未经确认兼容性，不要混用不同发行版本的固件与运行资源。

---

[Build / 构建](BUILD.md) · [License Inventory / 许可证清单](LICENSE_INVENTORY.md) · [Resource Status / 资源许可](RESOURCE_STATUS.md) · [GitHub Release / 发行页面](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)
