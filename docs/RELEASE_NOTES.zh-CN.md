# Draftor Alpha 2.2 · 微雪

版本 tag：`v0.1.0-alpha.2.2`。预发布资格以[当前验收说明](ALPHA2_2_VALIDATION.md)为准；不宣称无保留稳定。[Alpha 2.1 更新说明](RELEASE_NOTES_ALPHA2_1.zh-CN.md)原样留存。

- 公开整理已接受的微雪 ST7305 / SDMMC 移植、独立可变数据目录、只读共享词库和字体。
- 保留 HOGP 全 Input 订阅/CCCD 回读、菜单底栏修正，使用正常标准 F9，关闭专项诊断与临时因果快捷键。Consumer 裸 F9 不合成为 USB 命令。
- 修正合法导入稿恢复时“正常缺少可选位置记录”误报；损坏、失配、I/O、正文和元数据真实降级仍提示错误。共享源码小修影响两个 S3 构建，不改文稿格式或事务保护。
- 安装入口明确板型/端口，区分首装与更新，读取实际分区/启动槽后 App-only，独立校验。Host 夹具补 sized-delete，不关闭 sanitizer。

bgIME Core/Extension/Wubi、候选提交语义、format4/pins、ST7305 时序和同步完成、SDMMC 生命周期与保护均不改。原机仍为默认构建并继续使用既有 Alpha 2.1 发行，不发布未经本轮原机验收的 Alpha 2.2 原机固件。Pico 仍只提供独立显示示例。

见[安装](INSTALL.zh-CN.md)、[构建](BUILD.md)、[目标清单](../release/alpha2.2/WAVESHARE_RLCD42.json)和[原样资源引用](../release/alpha2.2/INPUT_REFERENCES.json)。USB 的 `Draftor USB v1.2 build236` 是旧描述字符串，不是 App 版本证据。

存储结论、实机范围、成本和未测项见[验收说明](ALPHA2_2_VALIDATION.md)。Wi-Fi/QR 继承为实验功能，微雪实测有限；电池、所有键盘/主机、新空板等不作通过宣称。公开资产不含真实正文、MAC、配对、Flash 备份、ELF、SDK 或原始设备日志。第三方许可独立保留，见[资源说明](RESOURCE_STATUS.md)。
