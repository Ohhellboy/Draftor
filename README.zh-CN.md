# Draftor Alpha · v0.1.0-alpha.1

[Draftor 项目介绍 / Project Introduction](README.md) · [English Alpha Guide](docs/ALPHA.md)

Draftor（草稿机）是一台基于单片机的离线写作设备。它使用反射式液晶屏、蓝牙键盘和 TF 卡存储文字，不需要联网，也不依赖 AI 或云端服务。

它是一台用来写东西的机器，而非全能掌上电脑（PDA, Personal Digital Assistant）。它只保留必要的编辑和文件管理功能，把注意力放在写作自身。

**Alpha** 适用于 **ESP32-S3 N16R8（Thick）**，搭载自主开发的 **bgIME（Big Goose Input Method Engine，大鹅输入法）**。

目前支持英文、全拼、小鹤双拼和五笔86输入，提供横排、竖排两种写作布局。文档可以保存、重新打开和重命名，也可以通过 USB 导出 TXT 文件，或者从电脑导入新的草稿。

这是 Draftor 的第一个完整 Alpha 软件版本。**未包含电源模块，该模块尚在开发中，设备目前需要外部供电**。

---

## 1. 下载 / Download

**[Draftor Alpha · v0.1.0-alpha.1](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)**

本次发布提供全部自有源码，包括 bgIME Core，同时附带完整的运行词库、字体资源、构建材料和经过实机测试的固件。

下载页面有四个压缩包：

| 文件 | 内容 |
|---|---|
| `Draftor_TF20_Alpha_Source.zip` | 完整源码、bgIME Core、显示驱动、开发工具和文档 |
| `Draftor_TF20_Alpha_Resources.zip` | 中文输入法词库、索引、字库及配套运行资源 |
| `Draftor_TF20_Alpha_BuildInputs.zip` | 配套构建输入、固定版本的外部源码、字体来源和许可证材料 |
| `Draftor_TF20_Alpha_Firmware.zip` | 已经过实机验证的 ESP32-S3 固件，以及首次烧录所需的配套文件 |

源码也可以直接在本仓库浏览。

**请使用同一版本的源码、资源和固件，不要混用不同版本的文件。** 如果准备自己编译，还需要安装构建文档中指定版本的工具链。

## 2. 安装与使用 / Installation & Usage

如果你准备在自己的硬件上运行 Draftor，可以从以下文档开始：

| 文档 | 内容 |
|---|---|
| [硬件与接线](docs/HARDWARE.md) | 所需硬件、GPIO 引脚及供电注意事项 |
| [固件安装](docs/INSTALL.md) | TF 卡资源准备、固件烧录与首次安装 |
| [操作手册](docs/USAGE.md) | 输入法、快捷键、文件管理和 USB 文档交换 |
| [源码构建](docs/BUILD.md) | 编译环境、工具链和构建步骤 |
| [已知问题](docs/KNOWN_ISSUES.md) | 当前版本的限制与尚未解决的问题 |

### USB 文档交换

Draftor 可以通过 USB 与电脑交换 TXT 文件，不需要额外的应用程序。

- **导出：** 按 `F9` → `E`，电脑上会出现一个只读 USB 磁盘，其中的 `DOCUMENT.TXT` 是当前文档的快照。
- **导入：** 按 `F9` → `I`，将 TXT 文件复制到临时 Inbox 磁盘，安全弹出后，在草稿机上选择文件并导入为新稿。

导入不会直接覆盖正在编辑的原稿。更详细的操作方式见[操作手册](docs/USAGE.md)。

## 3. 硬件与版本 / Hardware & Version

本次完整 Alpha 固件针对 **ESP32-S3 N16R8（Thick）** 开发。

当前软件构建基线为 `T06_TF20_R1`。TF 卡 SPI 运行频率为 **20 MHz**，初始化频率为 400 kHz。

本次提供的固件是经过实机验证的版本。

**关于 Pico 2 W（Thin）：**

Pico 2 W 的完整写作固件暂时不包含在这次 Alpha 中。

已经公开的 [ST7306 独立屏幕驱动与示例](display/st7306/) 仍然可以使用，支持 ESP32-S3 和 Raspberry Pi Pico 2 W。你可以先用它测试屏幕、接线和显示效果。

### 这是一个 Alpha 版本

目前，中文输入、横竖排编辑、文档保存与重新打开，以及 USB 导入导出等主要功能，已经通过有限的实机测试。

但它还不是稳定版。长时间运行、异常断电、内存极限等场景尚未得到充分验证，也仍然有一些已知问题。

如果你准备拿它写重要的东西，建议**外接稳定电源**并定期导出文档并保留备份。

具体情况见[已知问题与限制](docs/KNOWN_ISSUES.md)。

---

## 4. 源码与许可 / Source & Licenses

Draftor 和 bgIME 本次新公开的自有源码采用 **Apache License 2.0**。

Copyright © 2026 **Ruotao Wang**.

此前已经公开的 [ST7306 显示模块](display/st7306/) 继续使用原有的 **MIT License**。

第三方库、字体和词库数据按照已确认来源的材料保留相应版权声明、许可证及署名；部分历史资源的来源或许可适用范围仍有待明确。

详细信息见：

[LICENSE](LICENSE) · [NOTICE](NOTICE) · [第三方许可证清单](docs/LICENSE_INVENTORY.md) · [资源与许可说明](docs/RESOURCE_STATUS.md)

### 关于专利 / Patent

bgIME 所采用的共享稳定词条身份相关技术，已经提交中国发明专利申请。

专利申请文件不包含在本仓库中。

## 5. 源码结构 / Source Structure

如果你对输入法或嵌入式开发感兴趣，可以从这些目录开始：

| 目录 | 内容 |
|---|---|
| `integration/product` | 写作设备、编辑器、横竖排布局及 bgIME 核心 |
| `integration/module`、`integration/shared` | USB 文档导入导出 |
| `integration/telemetry` | 输入行为记录与性能分析工具 |
| `integration/tools`、`integration/fixtures` | 工具、测试及验证用例 |
| `display/st7306` | 独立的 ST7306 显示驱动与示例 |
| `vendor` | 第三方 SDK 适配及修改 |
| `release`、`tools` | 发行配置、构建工具及资源处理 |

源码和配套材料已经提供了当前 S3 构建所需的项目输入，但仍需另外安装指定工具链。部分历史混合词库、字体的完整上游再生成过程尚未全部恢复，不能据此认为所有历史版本都可以逐字节复建。

如果需要了解具体构建方式、文件组织或版本对应关系，可以继续阅读：

[构建指南](docs/BUILD.md) · [源码整合说明](docs/SOURCE_INTEGRATION.md) · [发行说明](docs/RELEASE_NOTES.md)

---

## 6. 反馈与交流 / Feedback & Contributions

Draftor 还在开发中。

如果你对中文输入法、离线写作设备、反射屏或者 MCU 上的文字排版感兴趣，欢迎查看源码、提出问题，或者分享自己的开发经验。

如果你实际运行了这个 Alpha，发现某些地方不好用，也请告诉我。尤其是输入法、文档编辑以及 USB 传输中的问题。

可以通过 [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) 报告问题，或者提交 Pull Request。

这个项目还远没有完成。不过现在诸君可以尝试了。

---

[Draftor 项目主页](https://github.com/Ohhellboy/Draftor) · [下载 Alpha](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [操作手册](docs/USAGE.md) · [已知问题](docs/KNOWN_ISSUES.md)
