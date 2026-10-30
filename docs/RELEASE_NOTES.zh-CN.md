# 草稿机 Alpha 2.2：微雪一体板支持

[English](RELEASE_NOTES.md) | **简体中文**

Alpha 2.2 为草稿机增加了微雪 **ESP32-S3-RLCD-4.2** 一体板的支持。主控、4.2 英寸黑白反射屏和 TF 卡槽集成在一块板上，减少了搭建中文写作设备时需要连接的模块。

这次更新延续 Alpha 2.1 的写作功能，包括全拼、小鹤双拼、五笔 86、横竖排、中文查找、文稿管理和 USB 传输。输入法核心及词库保持不变。

## 这次改了什么

**适配微雪的黑白显示屏。** 新版使用 ST7305 显示驱动，并为黑白画面调整候选焦点、选区和查找结果的显示方式。菜单底部的中文提示也经过整理，避免与原有状态文字重叠。

**接入板载 TF 卡。** 微雪使用自己的 SDMMC 读卡接口。文稿保存在独立的微雪目录中，词库和字体继续使用 Alpha 2.1 的配套资源。共用 TF 卡时，原草稿机的文稿目录仍然保留。

**改善多功能蓝牙键盘的输入。** 修正部分键盘在发送媒体按键后出现的后续输入中断问题。标准 F9 继续用于打开 USB 传输菜单；具体是否需要同时按 Fn，取决于键盘的功能键设置。

**修正文稿位置恢复提示。** 新导入文稿尚未保存光标位置时，现在会使用默认位置打开。真正的读取错误、记录损坏和文稿异常仍会正常报告。

## 下载哪些文件

本次提供微雪专用固件和 Alpha 2.2 源码，词库、字体及编译输入继续使用 Alpha 2.1。

| 文件 | 用途 |
|---|---|
| [Alpha 2.2 Waveshare Firmware](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.2/Draftor_v0.1.0-alpha.2.2_WAVESHARE_RLCD42_Firmware.zip) | 微雪专用 App、启动与分区文件及安装工具 |
| [Alpha 2.1 Resources](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip) | 配套 TF 词库、索引和正文字体 |
| [Alpha 2.2 Source](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.2/Draftor_v0.1.0-alpha.2.2_Source.zip) | 本版 Draftor 源码与文档 |
| [Alpha 2.1 BuildInputs](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_BuildInputs.zip) | 自行编译所需的固定依赖与生成输入 |

普通安装只需要对应的 Firmware，以及已经准备好的配套 Resources。自行编译时再下载 Source 和 BuildInputs。

## 安装与升级

**微雪首次安装：** 根据[安装指南](INSTALL.zh-CN.md)准备 TF 卡、词库与字体，再安装 Alpha 2.2 微雪固件。首次安装会清除设备内部 Flash 中原有的程序和设置，请提前备份所需内容。

**已有兼容 Draftor 分区的微雪设备：** 核对分区和启动槽后，可以只更新 App，继续使用原来的 TF 资源和文稿。无需重新格式化 TF 卡。

**原 ESP32-S3＋Osptek ST7306 用户：** 继续使用 [Alpha 2.1](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2.1)。本次提供的微雪固件采用另一套显示和读卡驱动，请按硬件型号选择。

固件安装步骤见[安装指南](INSTALL.zh-CN.md)；USB 导入导出、中文输入和文稿管理操作见[使用手册](USAGE.zh-CN.md)。

## 仍需注意

Alpha 2.2 仍是测试版本，建议定期导出重要文稿。当前微雪版本已在 USB 供电、罗技蓝牙键盘及 Linux 主机组合下进行使用测试；其他键盘、电脑和电池供电场景仍有待扩展。

本地 Wi-Fi 传输仍是实验功能，文稿较多时文件列表及回收站操作也可能需要等待。具体说明见[已知问题](KNOWN_ISSUES.md)。

字库、词库与第三方组件的来源和许可见[资源说明](RESOURCE_STATUS.md)。bgIME 的既有版本及 Alpha 2 输入性能资料继续保留在相应历史文档中，本次不另行声称性能提升。
