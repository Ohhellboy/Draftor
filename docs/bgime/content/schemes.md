# 输入方案与示例

三个 Host 示例分别覆盖 Core 完整编码查询、Extension 组合输入和 Wubi 直接查询，均使用真实引擎接口及人工测试资源。按接入需求选择示例；环境与源码准备见[快速开始](@doc/quickstart)，本页命令在同一源码根目录执行。

## 示例覆盖范围

| 示例 | 输入入口 | 展示的过程 | 未演示的部分 |
| --- | --- | --- | --- |
| `host_minimal` | 完整的全拼、小鹤编码 | 资源校验、候选查询、候选翻页、单词提交、过期选择拒绝 | 逐键解析、组合输入、学习与保存 |
| `host_composition` | `Composition::edit_raw()` | 全拼与小鹤逐键编辑、切分、单词及两词组合、一次性提交 | 候选翻页、任意句子的切分质量、学习与保存 |
| `host_wubi86` | `DirectQueryBinding` | 一至四码输入、退格、精确查询、翻页、显式提交、满码唯一候选的提交请求 | 真实五笔词库覆盖、Draftor 的完整自动上屏条件、学习与保存 |

三者均未完整演示 Alpha 3 的学习接入，也没有测试文本接收器拒绝后的恢复和不确定提交结果。产品中已有的学习功能与这些演示的范围见[选词学习](@doc/learning)。

## 构建目标与依赖

`cmake --build <构建目录>` 不带 `--target` 时，构建该示例工程的默认目标，包括运行所需的测试词库（fixture）。指定 `--target` 时只构建该目标及其依赖，三个工程的依赖如下：

| 示例工程 | 指定目标的依赖结果 | 可直接运行的指定目标 |
| --- | --- | --- |
| `host_minimal` | `bgime_host_minimal` 依赖词库生成目标，得到程序和资源 | `bgime_host_minimal` |
| `host_composition` | `bgime_host_composition` 依赖词库生成目标，得到程序和资源 | `bgime_host_composition` |
| `host_wubi86` | `bgime_host_wubi86` 只生成程序；`bgime_host_wubi86_fixture` 依赖程序并调用它生成词库 | `bgime_host_wubi86_fixture` |

例如，五笔单独构建时使用 `cmake --build build/host_wubi86 --target bgime_host_wubi86_fixture --parallel 2`。只构建 `bgime_host_wubi86` 后，还要补建 fixture 目标才能运行下面的测试。三个示例的 fixture 都位于各自构建目录中，不能相互混用。

## 全拼与小鹤双拼

运行 Core 与 Extension 组合示例：

```bash
cmake -S ime/bgime/examples/host_composition \
  -B build/host_composition -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host_composition --parallel 2
./build/host_composition/bgime_host_composition \
  ./build/host_composition/fixture
ctest --test-dir build/host_composition --output-on-failure
```

全拼使用 `SchemeRules::FullPinyin`，逐字符调用 `Composition::edit_raw()` 追加 `yuanxingji`，再调用 `query_next()` 查询。音节表与切分规划将其匹配为三个音节的「原型机」。小鹤双拼使用源码中的 `SchemeRules::R1Xiaohe`，输入 `yrxkji`，按 `yr | xk | ji` 三个编码单元查询 `xiaohe` 索引；它不先把双拼转换成全拼。

程序还会连续输入两次相同编码。第一次选词后保留后半段原始编码，第二次选择后进入 `AllSelected`，即所有编码都已被选中的词覆盖。最终「原型机原型机」通过一次接收器调用提交：

```text
selected: members=2 text="原型机原型机" remaining_raw="" state=AllSelected
submission: Preparing -> Ready -> Accepted; UTF-8="原型机原型机" sink_calls=1
```

每个场景使用新的内存文档。整个示例共完成四个场景：两种方案各自的单词和两词组合。它还检查取消、退格后重新输入、活动操作期间的 `Busy` 和旧选择的 `Stale` 状态。

宿主需将逐字符输入脚本替换为事件路由，并实现候选呈现和文本接收。全拼与小鹤要绑定各自的资源索引，不能只更改枚举而继续使用另一方案的文件。示例还检查小鹤的奇数长度前缀被识别为尚未完整；更多编码、歧义句和多词组合需要结合实际词库另行测试。

源码：[main.cpp](@source/ime/bgime/examples/host_composition/main.cpp) · [CMakeLists.txt](@source/ime/bgime/examples/host_composition/CMakeLists.txt) · [组合接口](@doc/api/composition)。

## 五笔86

运行五笔模块与 Core 的直接查询示例：

```bash
cmake -S ime/bgime/examples/host_wubi86 \
  -B build/host_wubi86 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host_wubi86 --parallel 2
./build/host_wubi86/bgime_host_wubi86 \
  ./build/host_wubi86/fixture
ctest --test-dir build/host_wubi86 --output-on-failure
```

默认构建会生成可执行程序和测试词库。如果单独指定目标，应构建 `bgime_host_wubi86_fixture`，它同时准备程序和资源；只构建可执行程序不会自动准备所有资源文件。

示例使用以下人工编码验证状态转换，**不代表实际五笔86字根编码表**。

| 人工编码 | 返回文字 | 验证行为 |
| --- | --- | --- |
| `a`、`ab`、`abc` | 一、二、三 | 一至三码、退格和重新输入 |
| `abcd` | 四 | 四码、两个映射路径指向同一词条身份 |
| `bbbb` | 同文、同文 | 文字相同而身份不同的两个候选及翻页 |
| `abcc` | 无匹配 | 四码查询未命中时，不回退到 `abc` 的三字码结果 |

参考配置接受 `a`–`y`，大写 ASCII 会折为小写，最长四码。第五码返回 `Full`，`z` 或数字返回 `InvalidCode`；拒绝编辑时不改变已有编码。

当 `abcd` 的当前第一页只有一个可输出候选且没有更多候选时，绑定层产生 `AutoUniqueFullCode` 请求。此时 `sink_calls=0`，文字还没有写入应用。示例中的宿主随后确认这个请求，才发生一次提交。宿主应先检查当前输入和编辑目标；请求不表示文本已提交。

Draftor 的五笔接入还检查正式资源中的自动提交条件、候选窗口和输入事件上下文；这些产品条件见 [wubi_product.cpp](@source/integration/product/src/wubi_product.cpp)。产品使用 `DirectExact` 组合路径，Host 示例使用 `DirectQueryBinding`，两者的调用入口不同。

以下是 `host_wubi86` 在同一个「满码唯一候选」场景中的关键输出，中间省略了一至三码的查询行：

```text
[unique identity: request first, Host confirmation later]
code=abcd full=1 candidates=1 has_more=0 text="四" ref=4 request=AutoUniqueFullCode sink_calls=0
two mapping paths -> one identity; valid text; request alone leaves sink_calls=0
commit=AutoUniqueFullCode (Host confirmed) Accepted UTF-8="四" sink_calls=1; duplicate confirm: no extra call
```

`request` 表示待宿主处理的请求；`Accepted` 才表示接收器接受文字。最后一行还验证了重复确认不会增加接收器调用次数。

源码：[main.cpp](@source/ime/bgime/examples/host_wubi86/main.cpp) · [人工资源生成](@source/ime/bgime/examples/host_wubi86/fixture.cpp) · [CMakeLists.txt](@source/ime/bgime/examples/host_wubi86/CMakeLists.txt) · [五笔接口](@doc/api/wubi)。

## 五笔接入路径

`DirectQueryBinding` 适用于五笔逐键查询及单候选提交，参照为 `host_wubi86`。需要与全拼、小鹤共用组合编辑、选择和提交对象时，可采用 `Composition` 的 `DirectExact` 规则；该规则将整段编码作为精确查询范围，不执行拼音切分。

| 对比项 | `DirectQueryBinding` | `Composition` 的 `DirectExact` |
| --- | --- | --- |
| 模块 | Core + Wubi；Host 示例另链接 POSIX 文件适配器 | Core + Extension；宿主可复用 Wubi 解码器校验字母和长度，Draftor 就采用这种做法 |
| 输入处理 | `Wubi86Decoder` 维护一至四码，绑定层的 `append()`、`backspace()` 驱动新查询 | 宿主校验输入后调用 `edit_raw()`、`query_next()`，用 `SchemePolicy{SchemeRules::DirectExact, "wubi86"}` 绑定索引 |
| 候选与选择 | `CandidateSession::present()` 显示，取得 `hanime::SelectionHandle`，调用绑定层确认接口 | `page()`、`read_text()` 显示，向 `select()` 传入 `hanime_extension::Selection`，选中项成为组合成员 |
| 提交 | `binding.confirm()` 后由 `binding.tick()` 推进，Core 接收器接受单个候选 | 选择覆盖全部编码后，`Submission::prepare()`、`advance()`、`confirm()` 提交组合 |
| 满码自动提交 | 绑定层生成 `AutoUniqueFullCode` 请求，宿主检查上下文后确认 | 宿主安排触发条件和选择流程；`DirectExact` 本身没有五笔自动提交策略 |
| 可运行参照 | `host_wubi86` | `host_composition` 展示通用组合流程，但运行的是全拼和小鹤；五笔产品参照是 Draftor 的集成代码 |

`DirectExact` 不自动限制五笔字母表或四码长度；它的 `edit_raw()` 接受小写 `a`–`z`，支持大写按键的宿主要先规范化输入。实际产品可参考 [admitWubiEdit 与自动提交条件](@source/integration/product/src/wubi_product.cpp)，按宿主输入目标配置检查。

两条路径都需要配对的 `shared.hns` 和 `wubi86.hnm`。`DirectExact` 只提供组合路径，**不自动提供完整 Alpha 3 学习接入**。公开的 `select()` 不签发产品使用的显式非首选资格；共享状态、资格入口和保存调度仍需接入。当前尚无完整独立学习示例，详见[学习接入缺口](@doc/learning#宿主接入要求)。

从直接查询改用组合对象时，按[迁移步骤](@doc/integration#五笔组合路径迁移)调整宿主。两条路径的性能、内存占用和迁移工作量需要结合目标设备、容量配置和宿主实现测量。

## 正式资源接入

正式资源接入前，使用测试资源验证宿主的输入路由、文本接收、事件顺序和生命周期。更换资源后核对资源身份、内存容量及字体覆盖。启用学习还需个人状态存储、后台调度和符合 Alpha 3 规则的事件资格。

更换词库后，需要同时调整示例中的固定输入和结果断言，使它们与新词库匹配。若要交互式浏览正式词库，还需要补充终端或界面的输入事件处理。

具体适配要求见[宿主集成](@doc/integration)。
