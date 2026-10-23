# bgIME 大鹅输入法

**Big Goose Input Method Engine**

[English](README.md) · [Draftor 草稿机](../../README.zh-CN.md)

bgIME 中文全称为「大鹅中文输入法引擎」，英文名为 「Big Goose Input Method Engine」，是为资源有限的嵌入式设备开发的中文输入引擎。

「大鹅」或 Big Goose 得名于汉字「鹅」的多种写法。在 Unicode 中，「鹅」「鵝」「鵞」「䳗」「䳘」和「𨿍」（左我右隹）分别拥有独立的码位。而据[《异体字字典》](https://dict.variants.moe.edu.tw/dictView.jsp?ID=52649)记载，「鵝」还有11种异体字形。

bgIME 没有将它们全部收录的技术野心，但是「鹅」字所代表的文字多样性，成为大鹅输入法引擎的命名来源。（声明：这与引擎设计者本身遭受家鹅、天鹅攻击的经历没有任何关系。）

目前已公开的应用实现为 Draftor（草稿机）。现有方案支持全拼、小鹤双拼和五笔86：全拼与小鹤双拼通过 Extension 处理输入和组合，五笔86使用自己的解码与查询适配，再共用 Core 的资源、候选和提交能力。英文直接输入、横竖排显示和文稿传输由 Draftor 产品层提供。

这里介绍 bgIME 的源码结构和使用方法。代码仍保留在 Draftor 原有目录中，目前尚未单独发行。

bgIME 的 Core 已有独立构建能力。要运行完整输入法，还需要接入输入方案、匹配的资源和宿主程序。

## Alpha 2 带来了哪些变化

[Draftor v0.1.0-alpha.2](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) 已公开发布。它是草稿机的产品版本，不是 bgIME 的独立发行版，也没有把 Core 的公开 API 改成“Alpha 2”。

| 层次 | 当前版本 |
| --- | --- |
| Draftor 产品 | `v0.1.0-alpha.2` |
| bgIME Core | `0.13.1-rc.1`；公开 API `0.13.0`；CMake 库版本 `0.13.1` |
| Extension | `0.8.4-rc.1`；CMake 工程版本 `0.8.4` |

### 少读一些资料，少做一些重复工作

在内存有限、词库放在 TF 卡上的设备里，找到候选并不等于文字已经可以显示。引擎还要定位、读取候选文字，再交给宿主。Alpha 2 接入了三项相互衔接的改进：

1. **分块查询：**全拼和小鹤双拼的编码索引按块查找，按需读取并复用有效的工作块，减少寻找候选时的索引读取。
2. **取字衔接：**把已经确认的默认文字位置直接交给后续读取环节，减少重新定位和重复读取。
3. **format4 短文字同块读取：**让符合条件的完整短默认文字与相关共享词条记录一同取得，减少额外访问和验证工作。长文字继续走完整读取路径。

这三项一起减少了从存储取回候选文字的工作量，有助于缩短中文输入的等待；资源验证、候选有效性和文本提交规则仍然保留。它们在 Draftor Alpha 2 中使用对应的构建配置和配套资源，在其他程序中使用时，也要提供对应的配置和资源。

Alpha 2 的**差量显示**由 Draftor 显示层实现，和上述改进一起缩短整机等待；**Wi-Fi 文稿传输**是 Draftor 的产品功能。二者都不属于 bgIME Core 自身的功能。

### 怎样理解性能结果

在同一台 ESP32-S3 上，Draftor Alpha 1 与 Alpha 2 累计测量版本的一组对应样例中，小鹤首次输入 `wo` 的完整候选等待从 **317.3 ms** 降到 **114.6 ms**。这是从设备接收输入事件到对应画面完成软件 LCD 传输的**整机端到端等待**，包含查询、取字、显示等环节，不是 bgIME Core 的独立基准测试。

这些有限样例不能说明所有输入都会获得相同比例的加速。测量版本与关闭输入观察的发行 App 分别记录；完整结果、计时口径及仍存在的代价见[性能说明](../../docs/PERFORMANCE.md)。

## 怎样在其他程序里使用

### 源码结构

| 部分 | 源码 | 职责 |
|---|---|---|
| Core | [dependencies/core](../../integration/product/dependencies/core/) | 共享词条身份、资源访问与校验、查询、排序、候选及文本提交 |
| Extension | [dependencies/extension](../../integration/product/dependencies/extension/) | 拼音方案的输入、切分和组合处理，复用共享 Core |
| Wubi | [dependencies/wubi](../../integration/product/dependencies/wubi/) | 五笔86解码与直接查询适配 |
| Host / Platform | [Core 接口与 Host 适配器](../../integration/product/dependencies/core/include/hanime/)、[Draftor 平台层](../../integration/product/platform/) | 资源读写适配；输入事件、调度、显示及提交文本的接收端由宿主提供 |
| Draftor | [产品集成](../../integration/product/) | 现有键盘、编辑器及交互实现，包括横竖排候选呈现 |

不同输入方案共用 Core 的词条资源和运行时能力，但解码与切分规则并不相同。五笔不会经过全拼分段器。更多说明见[源码结构](../../docs/SOURCE_INTEGRATION.md)。

### Core 静态库

Core 的独立构建需要 **CMake 3.20 或更新版本**和 **C++17**。从仓库根目录开始，下面的命令只构建运行库，不需要下载完整生产词库：

```sh
cmake -S integration/product/dependencies/core -B build/bgime-core \
  -DHANIME_BUILD_HOST_ADAPTERS=OFF \
  -DHANIME_BUILD_TOOLS=OFF \
  -DHANIME_BUILD_FIXTURES=OFF \
  -DHANIME_BUILD_TESTS=OFF
cmake --build build/bgime-core --target hanime_core
```

选项和目标定义在 [Core CMake 入口](../../integration/product/dependencies/core/CMakeLists.txt)。

如果直接把 Core 作为顶层工程配置，这些开发组件默认启用。上述最小构建生成 `hanime_core` 静态库，其他 CMake 工程可通过 `HanIME::Core` 使用它；构建结果不是可以直接运行的输入法程序，也不是 Draftor 固件。

### 独立 Host 程序

Host 消费者程序通过现有 [Core 公开接口](../../integration/product/dependencies/core/include/hanime/)接入有效资源，调度引擎工作，并接收提交的文本。全拼和小鹤双拼的组合输入使用 Extension；五笔有自己的解码与查询适配。[Extension 已有 CMake 入口](../../integration/product/dependencies/extension/CMakeLists.txt)，产品中的五笔构建目标定义在 [Draftor 产品 CMake](../../integration/product/CMakeLists.txt) 中；下面的五笔示例通过局部构建目标直接引用同一份五笔源码。

[Core 目录中的历史 Host 示例](../../integration/product/dependencies/core/examples/)有固定版本和资源前提，可以参考其调用方式，但不能视为当前源码版本已经验证的 Quick Start。

仓库另有三个独立的命令行示例，可以参考它们如何接入引擎：

- [Core Host 示例](examples/host_minimal/README.md)：通过 Core 完成编码查询、候选选择和文本提交；输入的是完整测试编码，不包含逐键解析。
- [Composition Host 示例](examples/host_composition/README.md)：使用 Core + Extension，验证全拼和小鹤双拼的逐键输入、编辑、候选选择及组合提交。
- [Wubi86 Host 示例](examples/host_wubi86/README.md)：使用 Wubi86Decoder + DirectQueryBinding + Core，验证逐键输入、候选分页、显式提交及四码唯一候选的自动提交请求。

三个示例各自 README 记录的是已有的 macOS 小型隔离测试资源验证，包括其中注明的 Alpha 1 源码基线。本次文档更新没有重新运行这些示例，不把原验证改称 Alpha 2 验证。它们不需要完整生产词库，也不是可直接安装的系统输入法；示例结果不能代表完整产品的运行结果。五笔示例中的编码是人工 fixture，不代表生产五笔码表；自动提交请求需要由 Host 确认，不等于已经提交文本，也不代表 Draftor 完整的 autoSafe 自动上屏策略。

### Draftor 完整固件

完整固件还要构建编辑器和设备集成，需要相应工具链及外部构建输入。请按 [ESP32-S3 固件构建说明](../../docs/BUILD.md)操作，并使用下述 Alpha 2 配套资源。

## 资源与许可证

bgIME 自有源码当前采用 **Apache-2.0**，见 [LICENSE](../../LICENSE) 和 [NOTICE](../../NOTICE)。字体、词库及其他第三方材料继续遵守各自的许可要求。

Draftor Alpha 2 当前使用 **format4 配套资源 `B_SHORT_TEXT_20261018`**，TF 卡目录为 `HANIME/IME/B_SHORT_TEXT_20261018`。准确资源身份、生效绑定与构建关系见[已发布的构建指南](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.2/docs/BUILD.md)及[资源清单](../../release/alpha2/RESOURCE_MANIFEST.json)。

**`LEXICON_R3_B2_20261006` 是 Alpha 1 等旧版资料对应的 B2 资源集**，保留用于理解历史构建和测试；它不是当前 Alpha 2 的运行资源组合。

独立编译 Core 不需要完整生产词库；真正运行输入法时，宿主必须提供与所选方案、共享词条及构建配置匹配的资源。不能因为输入方案或文字相同，就把旧 B2 与 format4 文件混装。三个 Host 示例使用各自的小型测试资源，不使用这套完整产品资源。

资源来源和未解决的许可事项，以现有[第三方声明](../../THIRD_PARTY.md)、[许可证清单](../../docs/LICENSE_INVENTORY.md)及[资源与许可说明](../../docs/RESOURCE_STATUS.md)为准。

项目已公开说明，共享稳定词条身份相关技术已提交中国发明专利申请。申请文件不包含在本仓库中。

## 当前限制与后续方向

- 暂时保留内部 `HanIME` 命名、namespace 和 CMake 目标。
- 当前没有通用的输入方案插件 ABI。
- Core 已有用户状态和学习接口；相关学习功能尚未开发完成，当前 Draftor 暂不提供用户词频学习。
- 独立 Host 示例使用小型测试资源，尚未验证生产词库覆盖或完整产品行为。
- 现有 Host 文件适配器使用 POSIX API。其他平台和工具链需要完成接入与验证后，才能列为已支持。

大鹅输入法引擎的合作者包括本人，Xiaojun，以及ChatGPT，引擎持续维护中，诸君若有建议，欢迎留言。
