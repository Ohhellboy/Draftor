# 草稿机 Alpha 2 · v0.1.0-alpha.2

本地作者审阅稿，尚未发布。

草稿机是一台用来写东西的离线设备。Alpha 2 延续原来的全拼、小鹤双拼、五笔86与英文输入，把工作集中在按键之后的等待：原码、候选和选词后的正文都更早出现。

[完整作者介绍](README.md) · [本次更新与四类下载](docs/RELEASE_NOTES.md) · [安装与升级](docs/INSTALL.md) · [操作手册](docs/USAGE.md) · [源码构建](docs/BUILD.md)

安装用户使用本版 Firmware + Resources；自行编译使用 Source + BuildInputs。新 App 需要准确兼容的双4 MiB布局，旧 Alpha 1 的2 MiB分区不能直接更新。新词库放入独立目录，保留旧资源、文稿和回退材料。

完整固件仍针对 ESP32-S3 N16R8。Pico 独立显示示例与原有作者介绍继续保留。重要文字请独立备份；已关闭的串口输入观察、实验 Wi-Fi、长期可靠性及来源许可事项集中见[已知问题](docs/KNOWN_ISSUES.md)和[资源说明](docs/RESOURCE_STATUS.md)。

全部自有源码（包括 bgIME Core）沿用 Apache-2.0，独立 ST7306 模块沿用 MIT。专利申请原件不在公开材料中；本地源码交付不表示外部审查已通过。
