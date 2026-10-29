# Draftor Alpha 版本约定 / Alpha versioning

Draftor 使用 **Alpha X.Y** 表示产品测试版；bgIME 的版本号独立维护。

| 变更 | 产品名称 |
| --- | --- |
| 实际集成作者接受的 bgIME 新实现或新版本 | X 增加，Y 从 0 开始；可以简称 Alpha X |
| bgIME 不变，编辑器、Find、界面、字体、传输、设备适配或构建发生变化 | X 不变，Y 增加 |
| 只修正文档，不形成产品发行 | 不强制增加版本号 |

判断 bgIME 是否更新，要核对 Core、Extension、Wubi 的实际实现、API 和配套资源绑定。构建日期、目录改名或产品包装变化不能代替这项判断。即使 API 未改，实现也可能有变化；遇到未解释的引擎差异，应先查清并由作者确认。词库或索引单独更新是否属于引擎换代，也由作者对该次更新作出决定。

本次为 **Alpha 2.2**，tag 为 **`v0.1.0-alpha.2.2`**。微雪板适配、HOGP/底栏修正及正常位置缺省恢复属于产品修订；bgIME Core/Extension/Wubi、format4 与资源 pins 未改。Core `0.13.1-rc.1`、API `0.13.0`、Core CMake `0.13.1`、Extension `0.8.4-rc.1` / CMake `0.8.4` 继续保留；Wubi 以本次源码和原资源 pin 绑定，不另造版本号。

## English

Draftor uses **Alpha X.Y** for product prereleases. Increase X only when the release actually integrates an author-accepted bgIME implementation or version update; reset Y to zero and optionally display the name as Alpha X. When bgIME is unchanged, other product changes increment Y. Documentation-only edits do not require a product release.

Compare the actual Core, Extension and Wubi implementation, API and resource bindings when making that decision. An unchanged API alone does not prove an unchanged engine. Dates, directory names and packaging do not constitute engine updates. The author decides how an independent dictionary or index update affects the series.

This release is Alpha 2.2 (Waveshare). bgIME maintains its own versions, listed above. Its existing implementation and resource bindings remain in use.

## 名称与 tag / Names and tags

| 名称 / Name | Tag |
| --- | --- |
| Alpha 2 | `v0.1.0-alpha.2` |
| Alpha 2.1 | `v0.1.0-alpha.2.1` |
| Alpha 2.2 | `v0.1.0-alpha.2.2` |
| Alpha 3（实际集成 bgIME 更新时 / after a bgIME update） | `v0.1.0-alpha.3` |

这是本项目对 Alpha 序列的约定，不重新定义 SemVer 的 MAJOR.MINOR.PATCH。`0.1.0` 保持不变，旧 tag 不改成 `alpha.2.0`；各段按整数理解，2.10 不是小数 2.1。

These are project-specific meanings for the Alpha sequence, not a redefinition of SemVer MAJOR.MINOR.PATCH. Keep `0.1.0` and the existing `alpha.2` tag. Treat each dot-separated component as an integer, so 2.10 is not the decimal 2.1.
