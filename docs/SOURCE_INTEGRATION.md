# Source Structure / 源码结构

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Project Introduction / 项目介绍](../README.md) · [Build / 源码构建](BUILD.md) · [Installation / 安装](INSTALL.md)

Draftor is an offline writing device built around a microcontroller. Its software combines a text editor, a Chinese input method engine, display and keyboard handling, local document storage, and USB file transfer.

Draftor（草稿机）是一台基于单片机的离线写作设备。它的软件主要由文本编辑器、中文输入法、屏幕与键盘处理、本地文档存储及 USB 文件交换等部分组成。

This guide explains how the source code is organized and where to start if you want to read, build, or modify it.

本指南介绍源码的组织方式，以及阅读、编译或修改程序时可以从哪里开始。

The complete Alpha firmware currently targets **ESP32-S3 N16R8 (Thick)**. The Raspberry Pi Pico 2 W (Thin) has separate ST7306 display demos, but no complete Alpha writing firmware in this release.

当前完整 Alpha 固件适用于 **ESP32-S3 N16R8（Thick）**。Raspberry Pi Pico 2 W（Thin）目前提供独立的 ST7306 屏幕示例，本次发行不包含完整写作固件。

---

## 1. Repository Overview / 仓库概览

The main directories are:

主要目录如下：

```text
Draftor/
├── integration/
│   ├── product/
│   ├── module/
│   ├── shared/
│   ├── telemetry/
│   ├── fixtures/
│   └── tools/
│
├── display/
│   └── st7306/
│
├── vendor/
├── release/
├── tools/
├── docs/
│
├── EXTERNAL_INPUTS.json
├── RELEASE_PAIR.json
├── README.md
└── README.zh-CN.md
```

Each directory has a different role:

各目录的主要用途如下：

| Directory / 目录 | Purpose / 用途 |
|---|---|
| `integration/product` | Main writing application, editor, input handling, document management and ESP32-S3 integration / 写作程序、编辑器、输入处理、文档管理及 S3 整合 |
| `integration/module` | USB document transfer implementations / USB 文档交换实现 |
| `integration/shared` | Shared document exchange logic / 文档交换的公共逻辑 |
| `integration/telemetry` | Input behaviour and diagnostic tools / 输入行为与诊断工具 |
| `integration/fixtures` | Test fixtures and reference cases / 测试夹具与参考用例 |
| `integration/tools` | Development and Host testing tools / 开发及电脑端测试工具 |
| `display/st7306` | Standalone ST7306 display driver and demos / 独立 ST7306 显示驱动与示例 |
| `vendor` | Fixed upstream source, local SDK adaptations and notices / 固定上游源码、SDK 适配与声明 |
| `release` | Resource and build binding headers / 资源及构建配对文件 |
| `tools` | Build, SDK preparation and resource installation tools / 构建、SDK 准备及资源安装工具 |
| `docs` | Installation, usage, build and release documentation / 安装、使用、构建及发行文档 |

---

## 2. Writing Application / 写作程序

The main writing application lives in:

写作程序的主要源码位于：

```text
integration/product/
```

Its contents include:

其中主要包括：

| Path / 路径 | Purpose / 用途 |
|---|---|
| `platform/device_main.cpp` | ESP32-S3 device initialization and application loop / 设备初始化与主循环 |
| `platform/writing_app.cpp` | Main writing application / 写作程序主体 |
| `platform/hid_keyboard.cpp` | Keyboard HID decoding / 键盘 HID 解码 |
| `platform/ble_keyboard_host.cpp` | Bluetooth keyboard connection / 蓝牙键盘连接 |
| `platform/vertical_view.hpp` | Vertical writing view / 竖排文字视图 |
| `platform/horizontal_body.hpp` | Horizontal text rendering / 横排正文绘制 |
| `platform/body_punctuation.hpp` | Body punctuation input handling / 正文标点输入处理 |
| `dependencies/common/` | Shared document, layout, editing and navigation code / 文档、布局、编辑及导航公共代码 |
| `platform/hanime_document_sd_storage.cpp` | Document storage integration with the TF card / 文档与 TF 卡存储的连接 |

The editor uses a shared UTF-8 document representation for horizontal and vertical layouts. Switching layouts changes how the text is presented, not the underlying document content.

编辑器的横排与竖排布局使用同一份 UTF-8 正文。切换布局改变的是文字的显示方式，而不是文档本身。

Document persistence, file management and related operations are also implemented under `dependencies/common/`, including the `src/hanime_document/` directory.

文档持久化、文件管理及相关操作也位于 `dependencies/common/`，其中包括 `src/hanime_document/` 目录。

If you want to understand how Draftor handles typing, editing, displaying and saving text, `integration/product` is the main starting point.

如果想了解草稿机如何接收输入、编辑文字、显示正文和保存文档，可以先阅读 `integration/product`。

---

## 3. bgIME / 大鹅输入法

Draftor uses **bgIME (Big Goose Input Method Engine)** for Chinese input.

草稿机使用 **bgIME（Big Goose Input Method Engine，大鹅输入法）** 处理中文输入。

The current Alpha supports Full Pinyin (Quanpin), Xiaohe Shuangpin and Wubi 86.

当前 Alpha 支持全拼、小鹤双拼和五笔86。

The relevant source is organized into several parts:

相关源码分布在以下目录：

| Path / 路径 | Purpose / 用途 |
|---|---|
| `integration/product/dependencies/core/` | bgIME Core runtime / 输入法核心运行时 |
| `integration/product/dependencies/extension/` | Composition, navigation and segmentation / 编码组合、导航与切分 |
| `integration/product/dependencies/wubi/` | Wubi-related input and decoding code / 五笔相关输入与解码代码 |
| `integration/product/dependencies/release_resources/` | Resource bindings used by the product / 产品使用的资源配对文件 |
| `integration/product/platform/` | Connection between the input engine, keyboard and writing application / 输入法与键盘、写作程序的连接 |

### Core

The Core contains the shared runtime used by the input methods. Its source is primarily in:

Core 提供各输入方案共用的运行时能力，主要源码位于：

```text
integration/product/dependencies/core/
├── include/hanime/
├── src/
├── examples/
├── tests/
└── tools/
```

It includes candidate handling, resource lookup, ordering, composition commit and related state management.

其中包括候选处理、资源查询、排序、编码提交及相关状态管理。

Core can be compiled separately from the complete Draftor application. See [Building bgIME Core](BUILD.md#5-build-bgime-core-separately--独立编译-bgime-core).

Core 可以独立于完整草稿机程序编译。具体方式见[独立编译 bgIME Core](BUILD.md)。

### Extension and Input Methods / 扩展层与输入方案

The Extension contains additional composition, navigation and segmentation logic used by the product. Wubi-related code is kept in its own dependency directory.

Extension 提供产品使用的编码组合、导航与切分逻辑；五笔相关代码则放在独立的依赖目录中。

The input methods share parts of the runtime, but this does not mean all input methods use identical lookup or decoding rules.

不同输入方案共用部分运行时能力，但并不意味着它们使用完全相同的查询或解码规则。

### Dictionaries & Resources / 词库与资源

The runtime dictionaries, indexes and matching font files are supplied in the separate Resources archive.

运行词库、索引及配套字体通过独立的 Resources 压缩包发行。

Some generated source inputs and other build dependencies are supplied through BuildInputs.

部分生成源码及其他构建依赖则包含在 BuildInputs 中。

**The source code, build inputs and runtime resources are related, but they are not interchangeable.**

**源码、构建输入与运行资源相互关联，但不能互相代替。**

For resource locations and installation, see [Installation](INSTALL.md). For the complete build input list, see [External Inputs](EXTERNAL_BUILD_INPUTS.md).

具体资源位置见[安装指南](INSTALL.md)，完整构建输入清单见[外部输入说明](EXTERNAL_BUILD_INPUTS.md)。

---

## 4. USB Document Transfer / USB 文档交换

Draftor supports TXT export and import using USB Mass Storage Class (MSC).

草稿机使用 USB Mass Storage Class（MSC）实现 TXT 文档导入导出。

The relevant source is divided into:

相关源码主要分为：

```text
integration/
├── module/
│   ├── include/
│   ├── src/
│   ├── platform/
│   └── tests/
│
└── shared/
    ├── include/
    ├── src/
    └── tests/
```

`integration/shared` contains common document exchange logic, while `integration/module` provides USB transfer implementations and platform adapters.

`integration/shared` 包含文档交换的公共逻辑，`integration/module` 提供 USB 传输实现及平台适配。

The ESP32-S3 product connects these modules to the writing application through its own platform code.

ESP32-S3 产品再通过 `integration/product/platform/` 中的代码，将这些模块接入写作程序。

The current user-facing functions are:

目前面向用户的功能包括：

- **Export / 导出：** A read-only virtual USB drive containing a snapshot of the current document as `DOCUMENT.TXT`.

  提供只读虚拟 USB 磁盘，其中的 `DOCUMENT.TXT` 是当前文档的快照。

- **Inbox / 导入：** A temporary writable USB drive used to receive TXT files, which can then be imported as new drafts.

  提供临时可写 USB 磁盘，用于接收 TXT 文件，再由用户选择并导入为新草稿。

These modes do not expose the internal TF card filesystem directly to the computer.

这两种模式均不会直接向电脑开放内部 TF 卡文件系统。

For instructions, see [Usage Guide](USAGE.md).

具体操作见[操作手册](USAGE.md)。

---

## 5. Display / 显示

The repository includes two different places to look for display-related code.

仓库中有两处与显示相关的源码。

### Standalone Driver / 独立驱动

```text
display/st7306/
```

This is the separately published ST7306 display module. It contains drivers and minimal examples for ESP32-S3 and Raspberry Pi Pico 2 W.

这里是独立公开的 ST7306 显示模块，包括适用于 ESP32-S3 和 Raspberry Pi Pico 2 W 的驱动及最小示例。

It can be studied or tested without installing the complete Draftor writing application.

可以在不安装完整草稿机程序的情况下，单独研究或测试屏幕。

See [ST7306 Display Demo](../display/st7306/README.md).

详见[ST7306 独立屏幕示例](../display/st7306/README.md)。

### Product Display Code / 产品显示代码

```text
integration/product/platform/
```

The product includes display integration and text presentation code used by the complete editor, including horizontal and vertical layouts, candidates and interface text.

产品目录包含完整编辑器使用的显示适配与文本绘制代码，涉及横竖排正文、候选区及界面文字。

The standalone driver demo and the complete writing application's display code serve different purposes. Building one does not automatically build or validate the other.

独立驱动示例与完整写作程序中的显示代码用途不同。编译其中一部分，并不代表另一部分已经完成构建或验证。

---

## 6. Telemetry & Testing / 诊断与测试

The project includes tools for testing input behaviour, resource handling and device performance.

项目提供输入行为、资源处理及设备性能相关的测试工具。

| Directory / 目录 | Contents / 内容 |
|---|---|
| `integration/telemetry/` | Telemetry structures, host tools and diagnostic support / 遥测结构、Host 工具及诊断支持 |
| `integration/fixtures/` | Reference cases and test fixtures / 参考用例及测试夹具 |
| `integration/product/tests/` | Writing application and integration tests / 写作程序与整合测试 |
| `integration/module/tests/` | USB module tests / USB 模块测试 |
| `integration/shared/tests/` | Shared document exchange tests / 公共文档交换测试 |
| `integration/tools/` | Test drivers, diagnostic parsing and development tools / 测试驱动、诊断解析及开发工具 |

The public source provides a portable Host test subset. Some historical tests depend on materials outside that subset, and not all existing tests currently pass.

公开源码提供可移植的 Host 测试子集。部分历史测试需要该子集之外的材料，而且现有测试并非全部通过。

See [Build](BUILD.md) and [Known Issues](KNOWN_ISSUES.md) for the available test commands and limitations.

可运行的测试命令及其限制见[构建指南](BUILD.md)和[已知问题](KNOWN_ISSUES.md)。

---

## 7. Build Tools & External Inputs / 构建工具与外部输入

The main build and preparation tools are located in:

主要构建与准备工具位于：

```text
tools/
```

| File / 文件 | Purpose / 用途 |
|---|---|
| `install_external_inputs.py` | Install separately supplied project inputs / 安装单独发行的构建输入 |
| `restore_sdk.py` | Prepare the project's SDK copy and adaptations / 准备项目使用的 SDK 副本及适配 |
| `rebuild_tinyusb.py` | Compile the required TinyUSB source units / 编译所需 TinyUSB 源文件 |
| `build_candidate.py` | Run the ESP32-S3 product build / 执行 ESP32-S3 产品构建 |
| `prepare_tf_resources.py` | Prepare and verify runtime TF resources / 准备及验证 TF 卡运行资源 |
| `check_pixel_adapter.cpp` | Standalone pixel adapter check / 独立像素适配器检查 |

The ESP32-S3-specific build script is:

ESP32-S3 专用构建脚本位于：

```text
integration/product/scripts/build_s3.py
```

The current release uses the fixed Arduino-ESP32 3.3.11 toolchain and project inputs described in [Build](BUILD.md).

本版使用固定的 Arduino-ESP32 3.3.11 工具链及[构建指南](BUILD.md)中列出的项目输入。

The `vendor/` directory contains the upstream sources and modifications needed by this build. It is not a complete copy of every SDK or compiler tool.

`vendor/` 保存构建所需的部分上游源码与本地修改，并不包含完整的 SDK 或编译器。

The `release/` directory contains resource binding materials used by the build. These are not interchangeable with arbitrary dictionary versions.

`release/` 包含构建使用的资源配对材料，不能随意与其他版本的词库混用。

---

## 8. Source, Resources & Releases / 源码、资源与发行

The Alpha release provides four companion archives:

Alpha 发行包含四个配套压缩包：

| Archive / 压缩包 | Purpose / 用途 |
|---|---|
| `Draftor_Alpha_Source.zip` | Integrated repository source / 整合源码 |
| `Draftor_Alpha_Resources.zip` | Runtime dictionaries, indexes and fonts / 运行词库、索引和字体 |
| `Draftor_Alpha_BuildInputs.zip` | Additional build inputs and source materials / 额外构建输入及来源材料 |
| `Draftor_Alpha_Firmware.zip` | Device-tested firmware and installation files / 实测固件及安装文件 |

The Source archive contains a packaged source snapshot. The Git repository also provides version history and may receive documentation or development updates after a release.

Source 压缩包保存发行时封装的源码快照。Git 仓库还保存版本历史，并可能在发行之后继续更新文档或开发内容。

If you need files from a particular release, use its matching archives and version tag rather than mixing files from different revisions.

如果需要复现某个具体发行版本，应使用对应的压缩包和版本标签，不要混用不同修订中的文件。

The runtime resources are distributed separately from the source tree. Their absence from a source checkout does not mean the runtime dictionaries or fonts are missing from the release.

运行资源与源码树分开分发。源码检出目录中没有完整词库或字体，不代表发行包缺少这些资源。

Some older mixed dictionary and font generation processes cannot yet be fully recreated from the available original upstream materials. This does not prevent using the provided matching resources for the current Alpha.

部分历史混合词库及字体的原始生成过程尚未完全恢复，但当前 Alpha 所需的匹配资源已经提供。

See [External Inputs](EXTERNAL_BUILD_INPUTS.md) and [Resource Status](RESOURCE_STATUS.md) for further details.

更多说明见[外部输入](EXTERNAL_BUILD_INPUTS.md)及[资源与许可说明](RESOURCE_STATUS.md)。

---

## 9. Licenses / 许可证

Draftor-owned source code, including bgIME Core, is released under the **Apache License 2.0**.

Draftor 自有源码，包括 bgIME Core，采用 **Apache License 2.0**。

The standalone `display/st7306` module retains its original **MIT License** and third-party notices.

独立的 `display/st7306` 模块继续使用原有 **MIT License**，并保留第三方声明。

Other libraries, fonts and dictionary data retain their own applicable licenses. They are not automatically covered by Draftor's Apache-2.0 license.

其他第三方库、字体和词库数据保留各自适用的许可，不会自动归入 Draftor 的 Apache-2.0。

For licensing details, see [LICENSE](../LICENSE), [NOTICE](../NOTICE), [Third-party Licenses](LICENSE_INVENTORY.md), and [Resource Status](RESOURCE_STATUS.md).

具体许可信息见 [LICENSE](../LICENSE)、[NOTICE](../NOTICE)、[第三方许可证清单](LICENSE_INVENTORY.md)和[资源与许可说明](RESOURCE_STATUS.md)。

---

## 10. Where to Start / 从哪里开始

If you want to explore the source code, these are useful starting points:

如果准备阅读或修改源码，可以根据自己的兴趣选择入口：

| Interested in / 想研究 | Start here / 推荐入口 |
|---|---|
| **Chinese input / 中文输入法** | `integration/product/dependencies/core/` |
| **Pinyin composition / 拼音组合与切分** | `integration/product/dependencies/extension/` |
| **Wubi 86 / 五笔86** | `integration/product/dependencies/wubi/` |
| **Writing and editing / 写作与编辑** | `integration/product/dependencies/common/` |
| **ESP32-S3 integration / S3 产品整合** | `integration/product/platform/` |
| **USB file transfer / USB 文件交换** | `integration/module/` and `integration/shared/` |
| **Display driver / 屏幕驱动** | `display/st7306/` |
| **Testing and diagnostics / 测试与诊断** | `integration/fixtures/` and `integration/telemetry/` |

If you want to compile the complete application, start with [Build](BUILD.md). If you want to install the existing Alpha firmware, start with [Installation](INSTALL.md).

如果希望编译完整写作程序，请先阅读[构建指南](BUILD.md)；如果只是想安装已经发布的固件，请先阅读[安装指南](INSTALL.md)。

---

[Project / 项目主页](https://github.com/Ohhellboy/Draftor) · [Download Alpha / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Build / 源码构建](BUILD.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md)
