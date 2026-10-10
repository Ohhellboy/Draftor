# Draftor TF20 Alpha · v0.1.0-alpha.1

[Draftor 项目介绍 / Project Introduction](README.md) · [English Alpha Guide](docs/TF20_ALPHA.md)

Draftor（草稿机）是一台面向专注写作场景的嵌入式硬件设备。系统基于微控制器架构构建，采用反射式液晶屏（Reflective LCD）、蓝牙键盘输入与 TF 卡本地存储，在完全脱网且无算法介入的环境下提供低延迟的纯文本书写与归档支持。

本次发布的 **TF20 Alpha** 运行于 **ESP32-S3 N16R8（Thick）** 硬件平台，搭载自主实现的嵌入式输入引擎 **bgIME（Big Goose Input Method Engine，大鹅输入法）**。核心系统支持英文、全拼、小鹤双拼与五笔86输入，提供水平（横排）与垂直（竖排）两种排版编辑模式，并具备文档读写、重命名及基于 USB MSC 协议的 TXT 双向文件传输功能。

## 发行包下载 (Release Assets)

**[Draftor TF20 Alpha · GitHub Release](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)**

本次发布公开了全部自有核心源码（含 bgIME Core），并提供配套词库、点阵字库、固定版本构建依赖及经过实机验证的固件二进制文件。构建与烧录请确保各资源包版本一致：

| 文件包 | 内容描述 |
|---|---|
| `Draftor_TF20_Alpha_Source.zip` | 核心源码、bgIME Engine、显示控制器驱动、构建工具及工程文档 |
| `Draftor_TF20_Alpha_Resources.zip` | 嵌入式中文词库索引、字库点阵数据及相关运行时资源 |
| `Draftor_TF20_Alpha_BuildInputs.zip` | 确定性编译输入、锁定版本的外部材料及对应许可证声明 |
| `Draftor_TF20_Alpha_Firmware.zip` | 经实机验证的 ESP32-S3 目标固件与初次烧录引导配置 |

*注：源码亦可直接在仓库主分支检出。为确保构建可复现性，建议以 Release 归档文件为基准。*

## 部署与参考文档

- [硬件设计与电气接线规范](docs/HARDWARE.md)
- [固件烧录与环境初始化指南](docs/INSTALL.md)
- [交互协议与快捷键操作手册](docs/USAGE.md)
- [工具链配置与源码构建流程](docs/BUILD.md)
- [已知问题与当前边界限制](docs/KNOWN_ISSUES.md)

本构建针对 **T06_TF20_R1** 硬件修订版进行配置，TF 卡 SPI 总线设定频率为 **20 MHz**。编译依赖特定版本的交叉编译工具链，具体约束详见构建文档。

当前版本处于 Alpha 阶段。尽管核心输入、编辑与文件系统操作已通过实机通路测试，但在特定边界工况下可能仍存在未完全暴露的问题，排查请对照已知问题清单。

> **关于 Pico 2 W 的说明：**
> 针对 RP2350 / Pico 2 W（Thin）平台的完整编辑系统固件暂未合入本次发布。如需在相关平台评估显示性能，可参考已开源的 [ST7306 独立显示驱动及测试范例](display/st7306/)（兼容 Pico 2 W 与 ESP32-S3）。

## 知识产权与许可协议

Draftor 及 bgIME 本次新公开的自有源码遵循 **Apache License 2.0**，版权所有 © 2026 **Ruotao Wang**。

- **ST7306 显示模块：** 延续此前公开时的 **MIT License**。
- **第三方资产：** 所包含的第三方库、外部字体及基础词库数据保留原作者版权声明及原始许可协议，不统一切换至 Apache-2.0。各组件许可边界详见 [资源与许可说明清单](docs/RESOURCE_STATUS.md)。
- **专利声明：** bgIME 输入法引擎的相关核心技术已向中国国家知识产权局提交发明专利申请。相关专利文本与交底材料不包含在公开仓库中。

## 源码目录架构

工程代码按职责进行模块化组织：

- `integration/product`：编辑器状态机、排版引擎与 bgIME 输入法核心
- `integration/module`、`integration/shared`：USB 大容量存储（MSC）与外部数据交换协议
- `integration/telemetry`：输入行为性能剖析与击键延迟观测工具
- `vendor`：第三方硬件抽象层、底层驱动适配与打补丁代码
- `release`、`tools`：构建配置清单、资源打包工具与校验脚本

关于系统架构细节与详细构建说明，请查阅 [构建指南](docs/BUILD.md)、[架构集成设计](docs/SOURCE_INTEGRATION.md) 与 [发行日志](docs/RELEASE_NOTES.md)。

---

Draftor 正处于持续迭代周期。若对嵌入式中文输入系统、低功耗离线写作终端实现或专用 MCU 文本排版架构有进一步探讨需求，欢迎提交 Issue 或 PR 参与技术交流。
