# Draftor TF20 Alpha · v0.1.0-alpha.1

**Released / 发布日期：9 October 2026 / 2026年10月9日**

**Status / 版本状态：Alpha Pre-release**

[Download / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Project Introduction / 项目介绍](../README.md) · [中文导览](../README.zh-CN.md) · [English Guide](TF20_ALPHA.md)

This is the first complete public Alpha software release of Draftor, an offline writing device built around a microcontroller.

这是 Draftor（草稿机）的第一个完整公开 Alpha 软件版本。它是一台基于单片机的离线写作设备，专注于文字输入、编辑和保存。

The release includes the writing application, bgIME Chinese input engine, runtime dictionaries, fonts, source code, and device-tested firmware for the **ESP32-S3 N16R8 (Thick)**.

本次发布包括适用于 **ESP32-S3 N16R8（Thick）** 的写作程序、bgIME 中文输入法、词库、字体、源码及经过实机验证的固件。

---

## 1. What's Included / 本次发布内容

### Chinese Input / 中文输入

Draftor uses **bgIME (Big Goose Input Method Engine)**, developed for embedded Chinese text input.

草稿机搭载自主开发的 **bgIME（Big Goose Input Method Engine，大鹅输入法）**。

The current Alpha supports:

目前支持：

- **English / 英文直接输入**
- **Quanpin / 全拼**
- **Xiaohe Shuangpin / 小鹤双拼**
- **Wubi 86 / 五笔86**

Chinese output supports Simplified, Traditional, and selected variant characters, with the corresponding font resources.

中文输出支持简体字、繁体字及部分异体字，并提供相应字库资源。

### Writing & Editing / 写作与编辑

The writing application supports both horizontal and vertical text layouts.

写作程序支持横排和竖排两种文字布局。

Documents can be created, edited, saved, reopened, and renamed. Chinese input is also available when renaming files.

用户可以创建、编辑、保存、重新打开和重命名文档。文件重命名时也可以使用中文输入法。

The editor and input method share the same document text, so switching layouts does not require converting or recreating the document.

横排与竖排共用同一份文档正文，切换布局不需要转换或重新建立文件。

### USB Document Transfer / USB 文档交换

Draftor can exchange TXT files with a computer over USB, without an internet connection or a separate companion application.

草稿机可以通过 USB 与电脑交换 TXT 文件，不需要网络，也不需要安装专门的配套应用。

- **Export / 导出：** `F9` → `E` presents a read-only virtual USB drive containing the current document as `DOCUMENT.TXT`.

  按 `F9` → `E`，电脑上会出现一个只读虚拟磁盘，其中的 `DOCUMENT.TXT` 是当前文档的快照。

- **Import / 导入：** `F9` → `I` opens a temporary Inbox drive. After copying TXT files and safely ejecting the drive, select a file on Draftor to import it as a new draft.

  按 `F9` → `I` 打开临时 Inbox 磁盘。复制 TXT 文件并安全弹出后，可以在草稿机上选择文件，将其导入为新稿。

Importing does not overwrite the original document or automatically switch the active document.

导入不会覆盖原稿，也不会自动切换当前正在编辑的文档。

See [Usage Guide / 操作手册](USAGE.md) for details.

---

## 2. Improvements in This Alpha / 本次 Alpha 的改进

This release brings together the previously developed input methods, editor layouts, document management, and USB transfer functions into one ESP32-S3 firmware.

本次 Alpha 将此前分别开发的输入法、编辑器布局、文档管理和 USB 交换功能整合到同一套 ESP32-S3 固件中。

The development leading to this release also includes:

本次发行包含此前完成的以下改进：

- **Input and display / 输入与显示：** Integration of Full Pinyin, Xiaohe Shuangpin, and Wubi 86 with the writing application, along with candidate display and horizontal/vertical layout improvements.

  将全拼、小鹤双拼及五笔86接入写作程序，并改进候选显示与横竖排交互。

- **Document operations / 文档操作：** Improvements to save handling, file renaming, and the separation between document selection and actual document modification.

  改进保存流程、文件重命名，以及文件选择与实际文档修改之间的操作边界。

- **USB exchange / USB 交换：** Read-only TXT export and isolated Inbox import, including new-draft creation without overwriting the current manuscript.

  提供只读 TXT 导出与独立 Inbox 导入，并支持在不覆盖当前文稿的情况下创建新草稿。

- **Stability / 稳定性：** Targeted fixes and memory-use reductions in the ESP32-S3 application, including input, storage, and peripheral lifecycle handling.

  针对 ESP32-S3 的输入、存储和外设生命周期进行修复，并减少部分内部内存占用。

- **TF card access / TF 卡访问：** The TF card SPI runtime frequency is now **20 MHz**, while initialization remains at **400 kHz**.

  TF 卡 SPI 运行频率调整为 **20 MHz**，初始化频率仍为 **400 kHz**。

These changes have undergone software testing and selected device tests. They should not be taken as proof that every historical issue has been resolved.

上述改进已经过软件测试及部分实机验证，但不代表所有历史问题都已彻底解决。

---

## 3. Downloads / 发行文件

All four archives belong to **v0.1.0-alpha.1**.

本次发布包含四个相互配套的压缩包：

| File / 文件 | Contents / 内容 |
|---|---|
| `Draftor_TF20_Alpha_Source.zip` | Complete integrated source tree, including bgIME Core, display drivers, tools, and documentation / 完整整合源码 |
| `Draftor_TF20_Alpha_Resources.zip` | Chinese dictionaries, indexes, bitmap fonts, and runtime resources / 词库、索引及字体资源 |
| `Draftor_TF20_Alpha_BuildInputs.zip` | Project build inputs, fixed-version upstream materials, font sources, and license notices / 构建输入、来源及许可材料 |
| `Draftor_TF20_Alpha_Firmware.zip` | Device-tested application firmware and associated first-install files / 原实测固件及首次烧录文件 |

**Use matching files from the same release. Do not mix firmware and resource versions.**

**请使用同一版本的源码、资源和固件，不要混用不同版本的文件。**

The source can also be browsed directly in the repository. Building the firmware requires the separately installed toolchain versions listed in [BUILD.md](BUILD.md).

源码也可以直接在 GitHub 仓库中查看。自行编译需要另外安装 [BUILD.md](BUILD.md) 指定的工具链。

---

## 4. Hardware & Firmware / 硬件与固件

The complete firmware in this release targets:

本次完整固件适用于：

**ESP32-S3 N16R8 (Thick)**

| Item / 项目 | Version / 内容 |
|---|---|
| Release / 发行版本 | `v0.1.0-alpha.1` |
| Software baseline / 软件基线 | `T06_TF20_R1` |
| Firmware ID / 固件标识 | `DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007` |
| Runtime resources / 运行资源 | `LEXICON_R3_B2_20261006` |
| TF SPI runtime / 运行频率 | 20 MHz |
| TF SPI initialization / 初始化频率 | 400 kHz |

The release includes the original device-tested application binary, not a newly compiled replacement.

本次提供的是原先经过实机验证的固件，不是为了公开发布而重新编译的替代版本。

### Power / 供电

The standalone battery power module is still under development and is not included in this Alpha.

独立电池供电模块尚在开发中，不包含在本次 Alpha 中。

The current device requires a suitable external power supply. Check your board's electrical requirements before connecting power.

目前设备需要外部供电。接入电源前，请确认开发板的电压、供电接口及连接方式。

### Pico 2 W (Thin)

A complete writing firmware for Raspberry Pi Pico 2 W (Thin) is not part of this release.

Raspberry Pi Pico 2 W（Thin）的完整写作固件不包含在本次发行中。

The previously released [ST7306 standalone display driver and demos](../display/st7306/) remain available for both Pico 2 W and ESP32-S3.

此前公开的 [ST7306 独立显示驱动与示例](../display/st7306/) 仍然保留，支持 Pico 2 W 与 ESP32-S3。

---

## 5. Testing & Known Limitations / 测试与已知限制

TF20 Alpha has passed selected real-device tests covering startup, Chinese candidate selection, saving and reopening documents, TXT export, and Inbox import.

TF20 Alpha 已通过有限的实机测试，覆盖设备启动、中文候选选择、文档保存与重新打开、TXT 导出及 Inbox 导入等场景。

Wubi 86 and the editor integration also have earlier software and device acceptance evidence.

五笔86及编辑器整合也保留了此前的软件与实机验收记录。

However, this is still an **Alpha release**, not a fully qualified stable version.

但它仍然是 **Alpha 版本**，尚未达到全面稳定性验证的程度。

Known limitations include:

目前仍需注意：

- Long-duration operation, unexpected power loss, and extreme memory conditions have not been fully validated.

  长时间运行、异常断电和极限内存工况尚未充分验证。

- Some historical failure cases and unresolved causes remain under investigation. Earlier test failures are not automatically considered fixed.

  部分历史故障及其原因仍有待调查，既有测试失败不能自动视为已修复。

- Full Wi-Fi document transfer, emergency QR export, and several planned editing improvements are not included in this Alpha.

  完整 Wi-Fi 文档传输、应急二维码导出及部分计划中的编辑功能尚未包含在本版中。

- This release does not include a complete battery-powered standalone hardware system.

  本次发布尚未提供完整的独立电池供电硬件系统。

If you intend to write important documents, use a stable external power supply, export your work regularly, and keep backups.

如果准备使用草稿机写重要文稿，请使用稳定的外部电源，定期导出文档，并保留备份。

See [Known Issues / 已知问题](KNOWN_ISSUES.md) for further details.

---

## 6. Source Code & Licenses / 源码与许可

All newly released Draftor-owned source code, including bgIME Core, is provided under the **Apache License 2.0**.

本次新公开的 Draftor 自有源码，包括 bgIME Core，采用 **Apache License 2.0**。

Copyright © 2026 **Ruotao Wang**.

The previously published [ST7306 display module](../display/st7306/) retains its **MIT License** and original third-party notices.

此前已经公开的 [ST7306 显示模块](../display/st7306/) 继续采用原有 **MIT License**，并保留第三方声明。

Third-party libraries, fonts, and dictionary data retain their applicable copyright and licensing terms. Some historical mixed resources and supplied third-party materials still have unresolved redistribution or license-scope questions; their known sources, notices, and remaining uncertainties are documented rather than treated as Apache-2.0 material.

第三方库、字体和词库数据保留各自适用的版权及许可条件。部分历史混合资源和第三方材料的再分发依据或许可范围仍待明确；已知来源、声明及尚存问题均如实记录，不统一改为 Apache-2.0。

See [LICENSE](../LICENSE), [NOTICE](../NOTICE), [Third-party Licenses](LICENSE_INVENTORY.md), and [Resource Status](RESOURCE_STATUS.md).

bgIME's shared stable-lexeme-identity technology is the subject of a Chinese invention patent application. The patent filing documents are not part of this release.

bgIME 所采用的共享稳定词条身份相关技术已提交中国发明专利申请。专利申请文件不包含在本次发布中。

---

## 7. Getting Started & Feedback / 开始使用与反馈

- [Hardware & Wiring / 硬件与接线](HARDWARE.md)
- [Installation / 固件安装](INSTALL.md)
- [Usage Guide / 操作手册](USAGE.md)
- [Building from Source / 源码构建](BUILD.md)
- [Known Issues / 已知问题](KNOWN_ISSUES.md)

If you try Draftor TF20 Alpha, feedback on Chinese input, horizontal and vertical editing, saving documents, and USB transfers would be especially useful.

如果你尝试了 Draftor TF20 Alpha，欢迎反馈中文输入、横竖排编辑、文档保存或 USB 传输中遇到的问题。

You can report bugs, ask questions, or share improvements through [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) and Pull Requests.

可以通过 [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) 报告问题，也欢迎提交 Pull Request。

This is the first complete public Alpha, not the finished machine. There is still work to do.

这是第一个完整公开的 Alpha，而不是草稿机的最终形态。后面还有不少事情要做。

But now you can give it a try.

不过现在诸君可以尝试了。

---

[Draftor Project / 项目主页](https://github.com/Ohhellboy/Draftor) · [Download Alpha / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Usage / 操作手册](USAGE.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md)
