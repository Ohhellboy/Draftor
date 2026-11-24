# 词库与配套资源

先用测试词库跑通接口，再为实际中文输入准备正式资源。资源文件的后缀相似，不代表可以互换；共享词库、方案索引、验证文件和个人状态需要分别管理。

## 测试词库与正式词库

三个 Host 示例在构建时自动生成小型测试资源。Core 与组合示例使用已有 `small_fixture.cpp`，五笔示例有自己的人工条目。它们使用隔离的命名空间，既不代表真实词库覆盖，也不应与用户个人记录混用。

正式资源保存更多词条及实际方案编码。Alpha 3 沿用 format4 的共享词库和三种方案索引，并增加通用学习位置索引。首次接入正式资源时，先下载配套文件，再核对身份和许可；资源构建工具与原始材料的范围见下文。

## 下载对应文件

| 材料 | 获取方式 | 用途 |
| --- | --- | --- |
| Alpha 3 源码 | [发布页](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.3) 或快速开始中的固定标签 | 引擎、产品集成、示例与生成工具 |
| format4 运行词库与既有正文字体 | [Alpha 2.1 Resources.zip](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip) | Alpha 3 继续使用这套共享词库、方案索引和正文字体 |
| Alpha 3 通用学习索引 | [Alpha 3 LearningIndex.zip](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.3/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip) | format 301 位置索引与验证文件 |
| 精确文件身份与 TF 相对路径 | [TF_RESOURCES.json](@source/release/alpha3/TF_RESOURCES.json) | 按文件核对字节大小、SHA-256、角色和来源 |
| Alpha 3 下载包摘要 | [SHA256SUMS](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.3/SHA256SUMS) | 核对 Alpha 3 发布附件 |

下载压缩包后，先核对发行清单，再解压并逐文件比对 `TF_RESOURCES.json`。例如在 macOS 上核对原资源包：

```bash
shasum -a 256 Draftor_v0.1.0-alpha.2.1_Resources.zip
```

预期摘要为 `460096846036bd01c941bc07ab98e6db4e130a247563e44fa3cb01d12a9c6636`。Alpha 3 学习索引包的摘要为 `05da4128d132c0673ae28bf9a1f35d3997a017cd1995d51d38051dc15f55c3eb`；请同时核对发布页的配套清单。不要只凭压缩包文件名或目录名称确认版本。

> Host 示例不需要这两个正式资源包。若只想完成第一次运行，回到[快速开始](@doc/quickstart)即可。为自己的程序重新分发词库之前，还应阅读[许可与来源说明](@doc/about)。

## 文件之间的关系

Alpha 3 共享资源包 ID 为 `17304919664082782091`，词库格式为 `4`。在 Draftor TF 卡中，主要词库目录为 `HANIME/IME/B_SHORT_TEXT_20261018/`；独立程序可选择自己的路径，但必须保留资源之间的配对身份。

| 文件 | 内容与调用关系 |
| --- | --- |
| `shared.hns` | 共享词条身份、文字与输出关系，由 `PagedSharedResource` 读取 |
| `full_pinyin.hnm`、`xiaohe.hnm`、`wubi86.hnm` | 各方案的编码索引，由 `PagedSchemeIndex` 读取并与共享资源配对 |
| 各方案的 `*_base.hbo` | 基础排序与方案行数据，供相应的优化查询路径使用 |
| 各资源的 `.hnv` | 与该文件匹配的验证页，不是可选的个人学习记录 |
| `relations.bgr`、`relations.hnv` | 通用学习位置索引及验证文件；产品路径由清单给出 |
| `matched28.hbf1` 等字体 | Draftor 的文字绘制资源；Core 交付 UTF-8，不要求独立宿主使用同一字体 |

format4 目录中还有 `CANDIDATE_READY.json` 安装标记。完整安装应按清单逐项处理，不以这张表替代文件清单。原词库运行集的 14 个文件、正文字体、安装标记及两个学习索引文件都列在 Alpha 3 的 `TF_RESOURCES.json` 中。

## 校验与读取方式

小型示例走 `open()` 加 `advance_validation()` 的完整校验路径，并明确检查命名空间与包 ID。它不演示正式发布认证。

产品使用固定的 `ResourceReleaseSpec`、`VerifiedResourceReader` 与验证页，对正式资源建立受约束的读取路径，再调用 `open_verified()`。换用自己的资源时，先准备相应的可信描述与配套验证材料，再使用这条读取路径。

参考：[verified_resource.hpp](@source/integration/product/dependencies/core/include/hanime/verified_resource.hpp)、[产品固定资源身份](@source/integration/product/src/resource_release.cpp)、[产品资源加载](@source/integration/product/src/tf_resources.cpp)、[通用学习索引生成器](@source/tools/generate_learning_index.py)。重新生成历史混合词库或字体前，还需核对[资源来源与原始材料的缺口](@source/docs/RESOURCE_STATUS.md)；现有材料未覆盖全部资源的逐字节重建。

## 个人状态单独存放

个人选词次数由可写状态文件保存，不能写回正式词库。Alpha 3 的通用学习索引也不包含个人记录。为每个用户或独立环境规划状态路径，使用相同命名空间的统一计数所有者，并通过明确的打开、后台推进和关闭流程管理它。

状态损坏、命名空间不兼容或存储不可用时，不应自动当作空文件覆盖。排序回退和保存失败的处理见[选词学习](@doc/learning)。

## 安装到草稿机

如果目标是已有草稿机设备，而不是接入自己的程序，请使用[安装网站](/)、[操作指南](/guide.html)和[TF 资源步骤](/guide.html#tf)。开发者文档描述引擎与接入约定，不替代设备安装或恢复流程。
