# 词库与资源

Alpha 3 使用 format4 共享词库、独立方案索引及 format 301 通用学习位置索引。词库与验证材料为只读资源；个人计数与派生索引使用单独的可写状态文件。各文件按资源身份配对，不能仅凭后缀互换。

## 测试资源与正式资源

三个 Host 示例在构建时自动生成小型测试资源。Core 与组合示例使用已有 `small_fixture.cpp`，五笔示例使用独立的人工条目。它们使用隔离的命名空间，既不代表真实词库覆盖，也不应与用户个人记录混用。

正式资源保存更多词条及实际方案编码。Alpha 3 沿用 format4 的共享词库和三种方案索引，并增加通用学习位置索引。正式资源需配套下载并核对身份。文件可获取不代表再分发许可已完备；构建材料与许可范围见后文。

## 资源获取与摘要

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

> [Host 示例](@doc/quickstart)不依赖这两个正式资源包。它们是 Draftor 的产品安装资源，不构成已完成独立授权与安装流程的通用 SDK；再分发前需核对[许可与来源说明](@doc/about)。

## 资源结构与身份绑定

Alpha 3 共享资源包 ID 为 `17304919664082782091`，词库格式为 `4`。在 Draftor TF 卡中，主要词库目录为 `HANIME/IME/B_SHORT_TEXT_20261018/`；独立宿主可配置其他路径，但必须保留资源之间的配对身份。

| 文件 | 内容与调用关系 |
| --- | --- |
| `shared.hns` | 共享词条身份、文字与输出关系，由 `PagedSharedResource` 读取 |
| `full_pinyin.hnm`、`xiaohe.hnm`、`wubi86.hnm` | 各方案的编码索引，由 `PagedSchemeIndex` 读取并与共享资源配对 |
| 各方案的 `*_base.hbo` | 基础排序与方案行数据，供相应的优化查询路径使用 |
| 各资源的 `.hnv` | 与该文件匹配的验证页，不是可选的个人学习记录 |
| `relations.bgr`、`relations.hnv` | 通用学习位置索引及验证文件；产品路径由清单给出 |
| `matched28.hbf1` 等字体 | Draftor 的文字绘制资源；Core 输出 UTF-8，不要求独立宿主使用同一字体 |

format4 目录中还有 `CANDIDATE_READY.json` 安装标记。完整安装应按清单逐项处理，不以这张表替代文件清单。原词库运行集的 14 个文件、正文字体、安装标记及两个学习索引文件都列在 Alpha 3 的 `TF_RESOURCES.json` 中。

## 资源校验与读取

小型示例走 `open()` 加 `advance_validation()` 的完整校验路径，并明确检查命名空间与包 ID。它不演示正式发布认证。

产品使用固定的 `ResourceReleaseSpec`、`VerifiedResourceReader` 与验证页，对正式资源建立受约束的读取路径，再调用 `open_verified()`。更换资源时，需同步提供可信发行描述和配套验证材料。

参考：[verified_resource.hpp](@source/integration/product/dependencies/core/include/hanime/verified_resource.hpp)、[产品固定资源身份](@source/integration/product/src/resource_release.cpp)、[产品资源加载](@source/integration/product/src/tf_resources.cpp)、[通用学习索引生成器](@source/tools/generate_learning_index.py)。重新生成历史混合词库或字体前，还需核对[资源来源与原始材料的缺口](@source/docs/RESOURCE_STATUS.md)；现有材料未覆盖全部资源的逐字节重建。

## 个人状态与派生索引

个人学习计数由可写状态文件保存，不能写回正式词库。`a.hns`、`b.hns` 为状态文件，`a.hns.relations` 等为依赖个人记录及配对资源的派生索引。Alpha 3 的通用学习索引也不包含个人记录。为每个用户或独立环境规划状态路径，使用相同命名空间的统一计数所有者，并通过明确的打开、后台推进和关闭流程管理它。

状态损坏、命名空间不兼容或存储不可用时，不应自动当作空文件覆盖。排序回退和保存失败的处理见[选词学习](@doc/learning)。

## Draftor 设备安装

Draftor 设备安装使用[安装网站](/)、[操作指南](/guide.html)和[TF 资源步骤](/guide.html#tf)。开发者文档描述引擎与接入约定，不替代设备安装或恢复流程。
