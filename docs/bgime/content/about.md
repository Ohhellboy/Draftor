# 版本、许可与参与开发

本文档适用于 **Draftor Alpha 3 中使用的 bgIME**。版本依据是发布标签 `v0.1.0-alpha.3`，对应提交 `f18939f8880e2278c231739d553fbf983c19e0f6`。教程和接口中的源码引用固定到这个提交。

## 保留模块原有版本

| 组件 | 本版源码中的版本 |
| --- | --- |
| Draftor 产品 | Alpha 3，`v0.1.0-alpha.3` |
| Core 发行标识 | `HanIME-Core-0.13.1-rc.1` |
| Core 公开 API | `0.13.0` |
| Core CMake 工程 | `0.13.1` |
| Extension 标识 | `0.8.4-rc.1` |
| Extension CMake 工程 | `0.8.4` |
| Wubi | 绑定本次源码和配套资源，没有另设发行版本号 |

这些模块版本未因 Alpha 3 的产品集成而统一改号。`kCompositionLearningApiVersion` 是源码中的组合学习接口标记，值为 `2`，也不是一个新的 bgIME 发行版本。

版本定义见 [Core CMake](@source/integration/product/dependencies/core/CMakeLists.txt)、[Extension CMake](@source/integration/product/dependencies/extension/CMakeLists.txt)、[组合学习声明](@source/integration/product/dependencies/core/include/hanime/composition_commit.hpp)和[产品版本约定](@source/VERSIONING.md)。

## 源码与资源获取

源码随 [Ohhellboy/Draftor](https://github.com/Ohhellboy/Draftor) 维护。初次运行使用[快速开始](@doc/quickstart)里的固定标签；需要实际输入词库时，按[资源页面](@doc/resources)获取匹配的文件。文档所在网站与[草稿机安装入口](/)共用域名，路径独立为 `/bgime/`。

Alpha 3 的产品验证范围见 [ALPHA3_VALIDATION.md](@source/docs/ALPHA3_VALIDATION.md)。早期的查询与取字改进、对照测试和设备移植过程保留在[性能记录](@source/docs/PERFORMANCE.md)、[版本说明](@source/docs/RELEASE_NOTES.md)及[微雪版本历史](@source/docs/WAVESHARE_A3_HISTORY.md)中；学习接口以这里所指 Alpha 3 源码为准。

## 源码、词库和字体的许可

bgIME 自有源码采用 **Apache License 2.0**，见 [LICENSE](@source/LICENSE) 和 [NOTICE](@source/NOTICE)。复制或分发时保留适用的许可与声明。第三方材料各自适用其来源条款，不能因为它们随仓库或资源包提供，就统一视为 Apache-2.0。

| 材料 | 项目已有的许可记录 | 使用时需查阅 |
| --- | --- | --- |
| 词库与读音来源 | 包含 Rime Essay/Luna 相关 LGPL-v3 材料、Apache-2.0 材料及 CC-CEDICT 的 CC BY-SA 4.0 读音材料 | [资源来源与未决事项](@source/docs/RESOURCE_STATUS.md) |
| 五笔86来源 | 保留固定 `rime-wubi` 的作者及 LGPL 等原许可材料；还需区分共用词库部分 | [第三方声明](@source/THIRD_PARTY.md)与[许可清单](@source/docs/LICENSE_INVENTORY.md) |
| 正文与部分界面字体 | 霞鹜文楷、Ysabeau 相关材料采用 OFL；另有 Jigmo2 的 CC0 生僻字贡献 | [字体分发说明](@source/docs/FONT_DISTRIBUTION.md)及原始许可 |
| MiSans 菜单字形 | 使用小米自己的字体许可；项目记录了独立生成表分发依据尚待确认的事项 | [字体分发说明](@source/docs/FONT_DISTRIBUTION.md)，不能推定为 OFL 或 Apache-2.0 |

部分历史混合词库的文件级许可适用范围、生成材料和字体独立分发问题尚待确认。准备重新分发正式资源时，请逐项核对适用许可，并保留来源、署名、许可和修改说明。完整记录见 [RESOURCE_STATUS.md](@source/docs/RESOURCE_STATUS.md)、[LICENSE_INVENTORY.md](@source/docs/LICENSE_INVENTORY.md)和[资源文件清单](@source/docs/RESOURCE_FILES.json)。

Host 示例不依赖 Draftor 的屏幕字体。你的界面可以采用自己的合法字体，不需要为了使用 Core 而引入草稿机的菜单字形资源。

## 报告问题

在 [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues)提交问题时，请说明你正在完成的操作，以及预期和实际结果。附上源码提交、所用示例或模块、操作系统与编译器版本、完整命令和错误输出。资源问题还应说明资源包来源及核对后的身份。

个人词频、用户文稿和设备备份通常不需要公开上传。优先用小型测试资源构造可重现输入；学习问题可以描述状态转换与错误码，避免把个人记录当作必要附件。

## 参与开发

先运行相关 Host 示例，再提出范围清楚的修改。文档改进适合补充首次接入者容易遗漏的调用顺序、缓冲区有效期和失败处理；新平台适配需要说明目标系统、文件接口、调度方式及验证范围。

补充完整独立学习示例前，需要先讨论可公开的选词资格入口及宿主的调用责任。示例应使用与 Alpha 3 一致的记录规则，并覆盖跨方案共享、关闭重开和保存失败处理；当前接入范围见[选词学习](@doc/learning)。
