# Alpha 2 文档预览与发行文件

本页用于发布前审阅。文件尚未上传GitHub，Alpha 2尚未发布。

## 查看文档

打开[项目首页](README.md)，或直接选择下面的语言页面：

| 文档 | 简体中文 | English |
| --- | --- | --- |
| 项目首页 | [中文](README.md) | [English](README.en.md) |
| 发行说明 | [中文](docs/RELEASE_NOTES.zh-CN.md) | [English](docs/RELEASE_NOTES.md) |
| 安装指南 | [中文](docs/INSTALL.zh-CN.md) | [English](docs/INSTALL.md) |
| 操作手册 | [中文](docs/USAGE.zh-CN.md) | [English](docs/USAGE.md) |

本源码包包含上述Markdown文档、原照片和相关技术说明，可直接阅读。外部网站及GitHub下载链接需要联网。

[GitHub Release发布正文](docs/RELEASE_NOTES.md)采用英文全文，顶部提供中文入口。中英文说明属于同一个版本，共用同一套下载附件。

目前正文中的v0.1.0-alpha.2链接指向计划发布的位置；对应标签和发行页尚未上线。

## 下载附件

普通安装需要 Firmware＋Resources。自行编译才需要 Source＋BuildInputs。

安装请使用本版Firmware包中的指南：

- 中文：Firmware/docs/INSTALL.zh-CN.md
- 英文：Firmware/docs/INSTALL.md

| 附件 | 用途 |
| --- | --- |
| [Firmware](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2/Draftor_v0.1.0-alpha.2_Firmware.zip) | 固件、启动及分区文件、安装工具和指南 |
| [Resources](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2/Draftor_v0.1.0-alpha.2_Resources.zip) | 配套词库、索引和字体 |
| [Source](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2/Draftor_v0.1.0-alpha.2_Source.zip) | 项目源码、工具和文档 |
| [BuildInputs](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2/Draftor_v0.1.0-alpha.2_BuildInputs.zip) | 自行构建所需的固定输入和依赖材料 |

本次文档调整只涉及Source和Firmware包。Resources与BuildInputs的内容未变，继续使用原包。

准确固件与资源配套见[配套清单](RELEASE_PAIR.json)。四包大小及分发校验文件随本地交审材料保管；发布前以最终四包清单为准。

## 本次审阅重点

请确认中文说明是否清楚、英文是否表达相同含义，以及安装步骤是否容易跟随。Release中文稿、作者介绍和照片保留原定内容；其他技术附页仍使用现有版本。

中文版从项目首页进入发行说明和安装指南时，会继续使用中文。尚未拆分语言的技术附页保持原样。

## 固件状态

本次文档调整没有重新编译固件。当前发行App仍为：

```text
DRAFTOR_ALPHA2_20261019_RELEASE
2,429,104字节
```

这份准确发行App已通过本轮简短设备检查：启动、中文输入、选词、保存，以及先离开到空白稿再真正重开测试稿。作者确认重开文字一致。按本轮授权保留发行App，现场原App的回退材料单独保管。

Wi-Fi沿用此前的测试结果，本轮不重新测试。源码审查材料已经交付，审查意见尚未回传。

## 技术记录

下面的资料供核对来源和已有结果使用，不是安装步骤：

- [准确固件与资源配套](RELEASE_PAIR.json)
- [构建入口及来源](docs/BUILD.md)
- [性能数据](docs/PERFORMANCE.md)
- [既有Wi-Fi证据与缺项](docs/WIFI_STATUS.md)
- [已知问题](docs/KNOWN_ISSUES.md)
- [资源来源及许可披露](docs/RESOURCE_STATUS.md)

此前记录中，ime/bgime/README.md及其中文页的两条失效链接已修正；项目首页没有加回已删除的内容。

本页不代表已经获得发布授权。最终材料仍交作者审阅，待明确批准后再发布。
