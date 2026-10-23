> Alpha 2: current cumulative product build and active format4 bindings are documented in [the Build Guide](../../docs/BUILD.md). The standalone Core source remains here in `integration/product/dependencies/core`; the older fixture notes below describe their original test scope.
> Alpha 2累计构建与生效format4绑定见构建指南；下列旧回放资料保留原适用范围。

# bgIME 大鹅输入法

**Big Goose Input Method Engine**

[English](README.md) · [Draftor 草稿机](../../README.zh-CN.md)

bgIME 中文全称为「大鹅中文输入法引擎」，英文名为 「Big Goose Input Method Engine」，是为资源有限的嵌入式设备开发的中文输入引擎。

「大鹅」或 Big Goose 得名于汉字「鹅」的多种写法。在 Unicode 中，「鹅」「鵝」「鵞」「䳗」「䳘」和「𨿍」（左我右隹）分别拥有独立的码位。而据[《异体字字典》](https://dict.variants.moe.edu.tw/dictView.jsp?ID=52649)记载，「鵝」还有11种异体字形。

bgIME 没有将它们全部收录的技术野心，但是「鹅」字所代表的文字多样性，成为大鹅输入法引擎的命名来源。（声明：这与引擎设计者本身遭受家鹅、天鹅攻击的经历没有任何关系。）

目前已公开的应用实现为 Draftor（草稿机）。Draftor 现有中文输入支持全拼、小鹤双拼和五笔86；英文输入由产品的输入路由处理，并不是一套独立的 bgIME 英文词库引擎。

这里介绍 bgIME 的源码结构和使用方法。代码仍保留在 Draftor 原有目录中，目前尚未单独发行。

bgIME 的 Core 已有独立构建能力。要运行完整输入法，还需要接入输入方案、匹配的资源和宿主程序。

## 源码结构

| 部分 | 源码 | 职责 |
|---|---|---|
| Core | [dependencies/core](../../integration/product/dependencies/core/) | 共享词条身份、资源访问与校验、查询、排序、候选及文本提交 |
| Extension | [dependencies/extension](../../integration/product/dependencies/extension/) | 拼音方案的输入、切分和组合处理，复用共享 Core |
| Wubi | [dependencies/wubi](../../integration/product/dependencies/wubi/) | 五笔86解码与直接查询适配 |
| Host / Platform | [Core 接口与 Host 适配器](../../integration/product/dependencies/core/include/hanime/)、[Draftor 平台层](../../integration/product/platform/) | 资源读写适配；输入事件、调度、显示及提交文本的接收端由宿主提供 |
| Draftor | [产品集成](../../integration/product/) | 现有键盘、编辑器及交互实现，包括横竖排候选呈现 |

不同输入方案共用 Core 的词条资源和运行时能力，但解码与切分规则并不相同。五笔不会经过全拼分段器。更多说明见[源码结构](../../docs/SOURCE_INTEGRATION.md)。

## 构建与使用

当前源码中的 Core 发行标识为 **0.13.1-rc.1**，公开 API 为 **0.13.0**，CMake 库版本为 **0.13.1**。Extension 为 **0.8.4-rc.1**，CMake 工程版本为 **0.8.4**。这些是各组件的版本，产品发行版本则是 **Draftor Alpha v0.1.0-alpha.1**。

### Core 静态库

Core 的独立构建需要 **CMake 3.20 或更新版本**和 **C++17**。具体命令见[独立编译 bgIME Core](../../docs/BUILD.md)。

该文档中的最小运行库构建将以下选项设为 `OFF`：

- `HANIME_BUILD_HOST_ADAPTERS`
- `HANIME_BUILD_TOOLS`
- `HANIME_BUILD_FIXTURES`
- `HANIME_BUILD_TESTS`

如果直接把 Core 作为顶层工程配置，这些开发组件默认启用。上述最小构建生成 `hanime_core` 静态库，其他 CMake 工程可通过 `HanIME::Core` 使用它；构建结果不是可以直接运行的输入法程序，也不是 Draftor 固件。

### 独立 Host 程序

Host 消费者程序通过现有 [Core 公开接口](../../integration/product/dependencies/core/include/hanime/)接入有效资源，调度引擎工作，并接收提交的文本。全拼和小鹤双拼的组合输入使用 Extension；五笔有自己的解码与查询适配。[Extension 已有 CMake 入口](../../integration/product/dependencies/extension/CMakeLists.txt)，产品中的五笔构建目标定义在 [Draftor 产品 CMake](../../integration/product/CMakeLists.txt) 中；下面的五笔示例通过局部构建目标直接引用同一份五笔源码。

[现有 Host 示例](../../integration/product/dependencies/core/examples/)有固定版本和资源前提，可以参考其调用方式，但不能视为当前源码版本已经验证的 Quick Start。

当前版本已有三个独立的命令行示例：

- [Core Host 示例](examples/host_minimal/README.md)：通过 Core 完成编码查询、候选选择和文本提交；输入的是完整测试编码，不包含逐键解析。
- [Composition Host 示例](examples/host_composition/README.md)：使用 Core + Extension，验证全拼和小鹤双拼的逐键输入、编辑、候选选择及组合提交。
- [Wubi86 Host 示例](examples/host_wubi86/README.md)：使用 Wubi86Decoder + DirectQueryBinding + Core，验证逐键输入、候选分页、显式提交及四码唯一候选的自动提交请求。

三个示例已在 macOS 上使用小型隔离测试资源验证，无需完整 B2。它们不是可直接安装的系统输入法。五笔示例中的编码是人工 fixture，不代表生产五笔码表；自动提交请求需要由 Host 确认，不等于已经提交文本，也不代表 Draftor 完整的 autoSafe 自动上屏策略。

### Draftor 完整固件

完整固件还要构建编辑器和设备集成，需要相应工具链及外部构建输入。请按 [ESP32-S3 固件构建说明](../../docs/BUILD.md)操作，并使用配套的 Alpha 资源。

## 资源与许可证

bgIME 自有源码当前采用 **Apache-2.0**，见 [LICENSE](../../LICENSE) 和 [NOTICE](../../NOTICE)。字体、词库及其他第三方材料继续遵守各自的许可要求。

B2 资源集 `LEXICON_R3_B2_20261006` 是 Draftor Alpha 的配套资源，不是编译 Core 的必要条件。实际运行输入法时，宿主仍需提供所选方案使用的有效资源。

资源来源和未解决的许可事项，以现有[第三方声明](../../THIRD_PARTY.md)、[许可证清单](../../docs/LICENSE_INVENTORY.md)及[资源与许可说明](../../docs/RESOURCE_STATUS.md)为准。

项目已公开说明，共享稳定词条身份相关技术已提交中国发明专利申请。申请文件不包含在本仓库中。

## 当前限制与后续方向

- 暂时保留内部 `HanIME` 命名、namespace 和 CMake 目标。
- 当前没有通用的输入方案插件 ABI。
- Core 已有用户状态和学习接口；相关学习功能尚未开发完成，当前 Draftor 暂不提供用户词频学习。
- 独立 Host 示例使用小型测试资源，尚未验证生产词库覆盖或完整产品行为。
- 现有 Host 文件适配器使用 POSIX API。其他平台和工具链需要完成接入与验证后，才能列为已支持。

大鹅输入法引擎的合作者包括本人，Xiaojun，以及ChatGPT，引擎持续维护中，诸君若有建议，欢迎留言。
