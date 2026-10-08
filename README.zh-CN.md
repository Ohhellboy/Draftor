# Draftor TF20 Alpha — v0.1.0-alpha.1

[原双语项目介绍](README.md) · [Alpha English guide](docs/TF20_ALPHA.md)

Draftor 是使用 ESP32-S3、反射屏、蓝牙键盘和 TF 存储的写作设备，支持英文、中文全拼、小鹤和五笔输入，横竖排编辑、保存重开、改名、TXT 导出和 Inbox 导入。bgIME 提供共享的输入法运行时。

以现有公开 main 为母本的首发软件包包含全部自有源码（含 Core），与原样完整资源和原实测固件准备在同一个版本发布。下载入口为 [GitHub Releases](https://github.com/Ohhellboy/Draftor/releases)，请选用同一版本的四附件。资源中来源或分发依据未确认的部分也完整保留；具体范围见[资源状态](docs/RESOURCE_STATUS.md)。

| 交付物 | 内容 |
|---|---|
| 本源码树 / Draftor_TF20_Alpha_Source.zip | 产品、Core、工具、pins、原 ST7306 独立模块／硬件概览／照片、整合文档和适用许可 |
| Draftor_TF20_Alpha_Resources.zip | B2 原14文件126,940,251字节、配对manifest/pins、验证后READY、原28px TF字体 |
| Draftor_TF20_Alpha_BuildInputs.zip | 全部76项配套输入、固定字体源、UI14生成输入与来源材料 |
| Draftor_TF20_Alpha_Firmware.zip | 原实测app和同次构建的bootloader、分区表、boot_app0；不含设备dump |

自有源码采用 **Apache-2.0**，Copyright 2026 **Ruotao Wang**。[LICENSE](LICENSE)和[NOTICE](NOTICE)保留；第三方代码、数据、字体按[各自许可与适用范围](docs/LICENSE_INVENTORY.md)，完整资源ZIP没有统一改为Apache。相关技术已提交中国专利申请；申请受理不等于授权。

固定产品为 **T06_TF20_R1**，固件标识 **DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007**，TF运行20 MHz、初始化400 kHz，原实测app 2,061,808字节。有限硬件场景已有验证；Alpha的[已知限制](docs/KNOWN_ISSUES.md)继续存在。10 MHz版本仅为既有回退，本批不提供其二进制下载。

源码与BuildInputs安装后具备既有S3构建所需的项目文件，工具链需按[BUILD](docs/BUILD.md)固定版本另行安装。字体/词库的完整上游再生成与S3编译是不同范围；历史混合素材的来源缺口见资源状态。未声称逐字节复建或测试全绿。

模块布局：`integration/product` 包含编辑器和Core等依赖，`integration/module` / `integration/shared` 为USB，`integration/telemetry` / `integration/tools` / `integration/fixtures` 为观测和Host检查，`vendor` 为固定SDK修改，`release` / `tools` 为配对和构建入口。

[构建](docs/BUILD.md) · [外部输入](docs/EXTERNAL_BUILD_INPUTS.md) · [硬件](docs/HARDWARE.md) · [安装](docs/INSTALL.md) · [使用](docs/USAGE.md) · [回退](docs/ROLLBACK.md) · [发行说明](docs/RELEASE_NOTES.md)

本版完整固件面向 ESP32-S3 N16R8（Thick）。Pico 2 W（Thin）及 ESP32-S3 的[独立屏幕示例](display/st7306/)保持原样，目录内 MIT 许可和第三方声明继续有效，根 Apache-2.0 不撤销该许可。[源码整合范围](docs/SOURCE_INTEGRATION.md)解释 Source ZIP 与原991文件候选的差别。
