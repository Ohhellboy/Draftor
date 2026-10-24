# bgIME 大鹅输入法

**Big Goose Input Method Engine**

**简体中文** | [English](README.en.md) · [Draftor 草稿机](../../README.md)

bgIME（大鹅中文输入法引擎，Big Goose Input Method Engine）是一套为资源有限的嵌入式设备开发的中文输入引擎。

它支持全拼、小鹤双拼和五笔86。不同输入方案使用各自的编码规则，同时共用词条资源、候选查询和文本提交机制。词库可以存放在TF卡中，按需读取，从而在内存有限的单片机上实现中文输入。

目前，bgIME 已应用于开源写作设备 [Draftor（草稿机）](../../README.md)。引擎核心可以独立编译，仓库中也提供了命令行示例，供其他项目参考和集成。

## 为什么叫“大鹅”？

「大鹅」或 Big Goose 得名于汉字「鹅」的多种写法。在 Unicode 中，「鹅」「鵝」「鵞」「䳗」「䳘」和「𨿍」（左我右隹）分别拥有独立的码位。而据[《异体字字典》](https://dict.variants.moe.edu.tw/dictView.jsp?ID=52649)记载，「鵝」还有11种异体字形。

bgIME 没有将它们全部收录的技术野心，但是「鹅」字所代表的文字多样性，成为大鹅输入法引擎的命名来源。（声明：这与引擎设计者本身遭受家鹅、天鹅攻击的经历没有任何关系。）

## Draftor Alpha 2 中的 bgIME 改进

[Draftor Alpha 2](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) 已经发布。这次更新进一步优化了 bgIME 的候选查询和文字读取效率。

### 减少数据读取和重复工作

单片机本身内存有限。为了节约内存资源，bgIME 将词库及索引存放在TF卡中，需要时再读取。

一次中文输入大致经过三个步骤：用户输入编码、引擎查找候选、读取对应文字。

Alpha 2 主要改进了三个环节：

1. **分块查询：** 全拼和小鹤双拼使用分块组织的编码索引。输入法可以按需查找相关数据块，并复用已经读取的有效数据，减少查询候选时的存储访问。

2. **取字衔接：** 共享词条记录中保存了默认输出文字的位置信息。读取并验证这些信息后，可以直接交给取字模块，减少重复定位与读取。

3. **短文字同块读取（format4）：** 对符合条件的短文字，将默认输出文字与相关共享词条记录放在同一数据块中读取，减少额外访问。其他输出仍通过原有的完整读取路径处理。

这些改进缩短了从输入编码到取得候选文字的处理过程。

在 Draftor 中，bgIME 与编辑器、屏幕驱动共同完成输入流程。Alpha 2 也采用差量显示，减少对未变化区域的重复传输。

### 实际速度

在同一台 ESP32-S3 原型机上，对 Alpha 1 和 Alpha 2 进行对照测试，结果如下：

| 输入操作 | Alpha 1 | Alpha 2 |
|---|---:|---:|
| 小鹤双拼首次输入 `wo`，完整候选出现 | 317.3 ms | **114.6 ms** |
| 全拼首次输入 `shi`，完整候选出现 | 333.7 ms | **122.8 ms** |

计时从设备收到**按键事件**开始，到软件完成完整候选画面的**显示传输**为止，包含查询、取字和显示三个环节。

其他测试结果，包括自然写作中的响应时间，见[性能报告](../../docs/PERFORMANCE.md)。

## 快速开始

bgIME 的代码位于 Draftor 仓库中。可以先运行命令行示例，了解候选查询和文本提交的过程，再根据需要将引擎接入自己的程序。

以下命令使用 CMake 3.20 或更新版本，以及支持 C++17 的编译器。现有 Host 示例使用 POSIX 文件接口，已有 macOS 环境下的运行记录。

### 运行 Core 示例

在仓库根目录执行：

```sh
cmake -S ime/bgime/examples/host_minimal \
  -B build/host_minimal \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/host_minimal \
  --target bgime_host_minimal --parallel 2

./build/host_minimal/bgime_host_minimal \
  ./build/host_minimal/fixture
```

这个示例会自动生成一套小型测试词库，然后调用 Core 查询候选、选择文字并提交结果。

示例中，全拼编码 `yuanxingji` 和小鹤双拼编码 `yrxkji` 都对应「原型机」，用于展示不同输入方案如何共享同一个词条身份。

如果希望进一步了解逐键输入、编辑和组合提交，可以查看另外两个示例：

- [全拼与小鹤双拼组合输入示例](examples/host_composition/README.md)：使用 Core 和 Extension，演示逐键输入、修改编码、选择候选及组合提交。
- [五笔86示例](examples/host_wubi86/README.md)：使用五笔解码器与 Core，演示逐键输入、候选翻页和文字提交。

三个示例使用各自的小型测试词库。它们适合学习接口和集成方法；实际应用则需要提供匹配的正式输入资源。

### 单独编译 Core

如果只需要 bgIME Core 静态库，可以在仓库根目录执行：

```sh
cmake -S integration/product/dependencies/core \
  -B build/bgime-core \
  -DHANIME_BUILD_HOST_ADAPTERS=OFF \
  -DHANIME_BUILD_TOOLS=OFF \
  -DHANIME_BUILD_FIXTURES=OFF \
  -DHANIME_BUILD_TESTS=OFF

cmake --build build/bgime-core --target hanime_core
```

生成的库目标为 `hanime_core`。在同一个 CMake 构建工程中，可以通过 `HanIME::Core` 引用它。

Core 提供词条资源管理、查询、候选、排序及提交接口。实际接入时，宿主程序负责提供存储访问、输入事件调度和接收文字的接口。

详细接口可从 [Core 公开头文件](../../integration/product/dependencies/core/include/hanime/)开始了解。

## 源码结构

bgIME 将共享输入能力与不同输入方案分开组织。

| 模块 | 源码 | 主要职责 |
|---|---|---|
| Core | [dependencies/core](../../integration/product/dependencies/core/) | 共享词条身份、资源读取与校验、候选查询、排序及文本提交 |
| Extension | [dependencies/extension](../../integration/product/dependencies/extension/) | 全拼、小鹤双拼的输入规则、切分与组合处理 |
| Wubi | [dependencies/wubi](../../integration/product/dependencies/wubi/) | 五笔86解码与查询适配 |
| Host / Platform | [Core 接口](../../integration/product/dependencies/core/include/hanime/) · [平台层](../../integration/product/platform/) | 将资源读写、输入事件和文本输出连接到实际设备 |
| Draftor | [产品集成](../../integration/product/) | 蓝牙键盘、编辑器、横竖排显示与文稿管理 |

全拼、小鹤双拼和五笔86可以共用 Core 的词条资源，但各自保留输入规则。五笔使用自己的解码方式，不经过全拼的切分过程。

在草稿机中，bgIME 负责中文输入，Draftor 则将它与键盘、编辑器、屏幕显示及本地文稿传输组合起来。

更详细的代码入口见[源码整合说明](../../docs/SOURCE_INTEGRATION.md)。

## 版本与配套资源

### 当前源码版本

| 组件 | 版本 |
|---|---|
| [Draftor](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) | `v0.1.0-alpha.2` |
| bgIME Core | `0.13.1-rc.1` |
| Core 公开 API | `0.13.0` |
| Core CMake 库版本 | `0.13.1` |
| Extension | `0.8.4-rc.1` |
| Extension CMake 工程版本 | `0.8.4` |

本页所述的 Alpha 2 查询与取字实现，以 Draftor `v0.1.0-alpha.2` 对应的源码为准。

Core 的源码目录与 CMake 目标仍使用 `HanIME` 命名。

### 词库和索引

Draftor Alpha 2 使用 **format4** 配套资源，资源目录为：

```text
HANIME/IME/B_SHORT_TEXT_20261018
```

当前资源包括共享词条文件，以及全拼、小鹤双拼和五笔86各自的方案索引。

安装和运行时，请使用与固件配套的完整资源集。旧版资料中的 `LEXICON_R3_B2_20261006` 属于较早的 B2 资源集，Alpha 2 使用上面的 format4 资源。

准确的资源文件和版本关系见[资源清单](../../release/alpha2/RESOURCE_MANIFEST.json)，完整固件的构建方式见[构建指南](../../docs/BUILD.md)。

独立编译 Core 时不需要下载整套生产词库。实际运行输入法时，需要为所选输入方案提供匹配的资源。

## 许可与开发状态

bgIME 自有源码采用 **Apache License 2.0**，见 [LICENSE](../../LICENSE) 和 [NOTICE](../../NOTICE)。字体、词库及其他第三方材料适用各自的许可证和署名要求。

资源来源及相关许可事项可查阅[第三方声明](../../THIRD_PARTY.md)、[许可证清单](../../docs/LICENSE_INVENTORY.md)和[资源与许可说明](../../docs/RESOURCE_STATUS.md)。

共享稳定词条身份相关技术已提交中国发明专利申请。

目前还有一些功能处于开发阶段：

- Core 已有用户状态和学习接口，用户词频学习功能仍在开发中。
- 输入方案目前通过现有模块接口集成，通用插件 ABI 尚未提供。
- 现有 Host 文件适配器使用 POSIX API，接入其他平台需要实现相应的平台适配。

大鹅输入法引擎的合作者包括本人，Xiaojun，以及ChatGPT，引擎持续维护中，诸君若有建议，欢迎留言。
