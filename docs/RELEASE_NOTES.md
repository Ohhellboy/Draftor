# Draftor Alpha 2 — Cumulative Performance Update
# 草稿机 Alpha 2：累计性能优化版

This update is about making Chinese writing feel more immediate. Typed letters appear sooner, candidate lists spend less time waiting, and selected words reach the page faster. Full Pinyin, Xiaohe Shuangpin, Wubi 86 and English remain available, with the same horizontal and vertical writing layouts.

这次更新主要让中文输入更跟手：字母反馈更及时，候选等待减少，选中的字词也更早出现在正文里。全拼、小鹤双拼、五笔86和英文输入，以及横排、竖排写作方式都保留下来。

## What changed / 这次改了什么

The input engine now looks up candidates in blocks, carries verified text locations forward, and retrieves complete short default text alongside its shared entry. The display sends changed regions where possible. Together, these changes shorten the journey from pressing a key to seeing the result; they do not add learning or abbreviated Pinyin.

输入引擎改为分块查询，把已验证的文字位置接到后续取字过程，并让完整的短默认文字与共享词条同块取得。屏幕则在适用时只传输发生变化的区域。这些改动一起缩短了按键到看见结果的等待，本版没有新增学习或简拼功能。

On the same ESP32-S3 prototype, the paired comparison recorded:

同一台 ESP32-S3 原型机的本轮对照记录如下：

| Action / 操作 | Alpha measurement build / Alpha 测量派生版 | Alpha 2 cumulative build / Alpha 2 累计版 |
|---|---:|---:|
| First Xiaohe `wo`, complete candidates / 小鹤首次 `wo` 完整候选 | 317.3 ms | 114.6 ms |
| First Full Pinyin `shi`, complete candidates / 全拼首次 `shi` 完整候选 | 333.7 ms | 122.8 ms |
| Last letter of first Xiaohe `wo`, letter feedback / 小鹤首次 `wo` 最后一键原码反馈 | 55.3 ms | 9.6 ms |
| Xiaohe phrase selection, Space to body display / 小鹤词组空格选词至正文显示 | 72.1 ms | 36.4 ms |

The old side is a minimal measurement derivative of Alpha 1. These times run from received input to completion of the corresponding screen transfer, with Wi-Fi off and natural manual input. They are examples, not a fixed latency for every word. A separate free-writing session also checked that an approximately 1,400-character draft was byte-for-byte identical after saving and genuinely reopening it. [Measurement details](PERFORMANCE.md).

旧侧是保留 Alpha 1 业务的最小测量派生版。计时从设备收到输入开始，到相应画面发送完成；两臂 Wi-Fi 关闭，使用自然手工输入。这些是对应样例，不是每个词的固定耗时。另外一次自由写作中，约1400字符的稿件在保存后与真正重新打开后逐字节一致。[测量详情](PERFORMANCE.md)。

## Downloads / 下载哪些文件

For the ESP32-S3 N16R8 (Thick), use **Firmware + Resources** from this version. To compile it yourself, use **Source + BuildInputs** and the fixed toolchain in the [Build Guide](BUILD.md). The automatic GitHub “Source code” archive is not a complete installation package.

ESP32-S3 N16R8（Thick）安装用户下载本版的 **Firmware + Resources**；自行编译则下载 **Source + BuildInputs**，并按[构建指南](BUILD.md)准备固定工具链。GitHub 自动生成的“Source code”压缩包不是完整安装包。

| File / 文件 | Contents / 内容 |
|---|---|
| `Draftor_v0.1.0-alpha.2_Firmware.zip` | Release App with input observation disabled, startup files, partitions and checks / 关闭输入观察的发行 App、启动文件、分区及检查 |
| `Draftor_v0.1.0-alpha.2_Resources.zip` | Matching format4 dictionaries, indexes and font / 准确配套的 format4 词库、索引和字体 |
| `Draftor_v0.1.0-alpha.2_Source.zip` | Complete project-owned source, bgIME Core, tools and documentation / 完整自有源码、bgIME Core、工具和文档 |
| `Draftor_v0.1.0-alpha.2_BuildInputs.zip` | Fixed generated inputs, SDK overlay component and original source/license materials / 固定生成输入、SDK 配套组件及原始来源与许可材料 |

The [Alpha 2 release URL](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) is a **pending publication link** in this local preview. These files have not been uploaded. The release App uses the tested cumulative business source with serial input observation disabled. It is a new local build; the measurements above belong to its observed baseline.

本地预览中的 [Alpha 2 下载地址](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2)是**待发布链接**，文件尚未上传。发行 App 沿用实测累计业务源码，关闭了串口输入观察；它是新的本地构建，上述实测数字属于带观察的原基线。

## Upgrading / 旧版如何升级

**Alpha 2 does not fit the old 2 MiB application slots.** If you installed Alpha 1 using its original guide, do not use a normal App-only update. That layout needs a separate migration, whose release-ready procedure is still awaiting confirmation. An already compatible dual-4 MiB layout can use App-only updating after its actual partition and active slot are checked. First installation has a separate procedure. [Installation and migration guide](INSTALL.md).

**Alpha 2 已装不进旧的 2 MiB 应用分区。** 按 Alpha 1 原指南安装的用户不能直接 App-only 更新，需要单独迁移；这条迁移路线的正式操作材料仍待确认。已经采用准确兼容双4 MiB布局的用户，在核对实际分区和活动槽后可进行 App-only 更新。首次安装另有独立步骤，见[安装与迁移指南](INSTALL.md)。

Use the new resource directory alongside the old one. Do not mix format2 and format4 files, format the TF card, or overwrite your document directory to install this update. Keep the old firmware, matching resources and a separate copy of important writing.

新资源放在独立目录，与旧资源并存。不要混用 format2 与 format4 文件，也不要为升级格式化 TF 卡或覆盖文档目录。请保留旧固件、旧配套资源，并把重要文稿另外备份。

## Still an Alpha / 仍需注意

Some activation and warm-query stages remain slower, and full-screen redraws still block synchronously. Long-running use, unexpected power loss, new boards and cross-version rollback are not comprehensively qualified. Experimental local Wi-Fi transfer and QR pages are present; the performance comparison and free-writing session kept Wi-Fi off. This release does not include a complete Pico writing firmware.

部分激活和热查询子阶段仍有代价，整屏重绘也仍会同步阻塞。长期运行、突然断电、新空板和跨版本回退尚未全面验证。固定固件中保留了实验性的本地 Wi-Fi 传输及二维码页面；本次性能对照和自由写作期间 Wi-Fi 均关闭。本版不提供 Pico 完整写作固件。

The release build disables serial composition-code and input-timing observation. Startup, connection, health and error diagnostics remain. The writing results above come from the observed cumulative baseline; this quieter release build has software checks, but has not had a separate device session. Third-party fonts and dictionaries retain their own terms and the previously disclosed provenance questions. [Known limits and privacy](KNOWN_ISSUES.md) · [Resource and license status](RESOURCE_STATUS.md).

发行构建已关闭串口组成码与输入计时观察，保留启动、连接、运行状态和错误诊断。前述写作成绩来自带观察的累计基线；关闭观察后的发行构建完成软件检查，尚未另做实机会话。字体、词库沿用各自条款及既有来源未决披露。[已知限制与隐私](KNOWN_ISSUES.md) · [资源与许可说明](RESOURCE_STATUS.md)。
