# 输入方案与示例

三个 Host 示例都使用真实的引擎接口和人工测试资源。它们是按固定脚本运行的命令行程序，帮助你理解一次输入怎样完成。先按[快速开始](@doc/quickstart)取得对应源码并准备构建环境，再在同一个源码根目录运行本页命令。

## 三个示例各自覆盖什么

| 示例 | 输入入口 | 展示的过程 | 未演示的部分 |
| --- | --- | --- | --- |
| `host_minimal` | 完整的全拼、小鹤编码 | 资源校验、候选查询、候选翻页、单词提交、过期选择拒绝 | 逐键解析、组合输入、学习与保存 |
| `host_composition` | `Composition::edit_raw()` | 全拼与小鹤逐键编辑、切分、单词及两词组合、一次性提交 | 候选翻页、任意句子的切分质量、学习与保存 |
| `host_wubi86` | `DirectQueryBinding` | 一至四码输入、退格、精确查询、翻页、显式提交、满码唯一候选的提交请求 | 真实五笔词库覆盖、Draftor 的完整自动上屏条件、学习与保存 |

三者均未完整演示 Alpha 3 的学习接入，也没有测试文本接收器拒绝后的恢复和不确定提交结果。产品中已有的学习功能与这些演示的范围见[选词学习](@doc/learning)。

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

全拼使用 `SchemeRules::FullPinyin`，把 `yuanxingji` 一键一键交给输入对象。它通过音节表和切分规划找到三个音节组成的「原型机」。小鹤双拼使用源码中的 `SchemeRules::R1Xiaohe`，输入 `yrxkji`，按 `yr | xk | ji` 三个编码单元查询 `xiaohe` 索引；它不先把双拼转换成全拼。

程序还会连续输入两次相同编码。第一次选词后保留后半段原始编码，第二次选择后进入 `AllSelected`，即所有编码都已被选中的词覆盖。最终「原型机原型机」通过一次接收器调用提交：

```text
selected: members=2 text="原型机原型机" remaining_raw="" state=AllSelected
submission: Preparing -> Ready -> Accepted; UTF-8="原型机原型机" sink_calls=1
```

每个场景使用新的内存文档。整个示例共完成四个场景：两种方案各自的单词和两词组合。它还检查取消、退格后重新输入、活动操作期间的 `Busy` 和旧选择的 `Stale` 状态。

接入时替换示例中逐字构造输入的脚本、候选输出和文本接收器。全拼与小鹤要绑定各自的资源索引，不能只更改枚举而继续使用另一方案的文件。示例还检查小鹤的奇数长度前缀被识别为尚未完整；更多编码、歧义句和多词组合需要结合实际词库另行测试。

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

示例刻意使用下面的人工编码，便于观察状态变化。**它们不是实际五笔86字根编码表。**

| 人工编码 | 返回文字 | 用来观察什么 |
| --- | --- | --- |
| `a`、`ab`、`abc` | 一、二、三 | 一至三码、退格和重新输入 |
| `abcd` | 四 | 四码、两个映射路径指向同一词条身份 |
| `bbbb` | 同文、同文 | 文字相同而身份不同的两个候选及翻页 |
| `abcc` | 无匹配 | 四码查询未命中时，不回退到 `abc` 的三字码结果 |

参考配置接受 `a`–`y`，大写 ASCII 会折为小写，最长四码。第五码返回 `Full`，`z` 或数字返回 `InvalidCode`；拒绝编辑时不改变已有编码。

当 `abcd` 的当前第一页只有一个可输出候选且没有更多候选时，绑定层产生 `AutoUniqueFullCode` 请求。此时 `sink_calls=0`，文字还没有写入应用。示例中的宿主随后确认这个请求，才发生一次提交。你的应用也应先检查当前输入和编辑目标，不能把请求直接当作已提交结果。

Draftor 的五笔接入还检查正式资源中的自动提交条件、候选窗口和输入事件上下文；这些产品条件见 [wubi_product.cpp](@source/integration/product/src/wubi_product.cpp)。产品使用 `DirectExact` 组合路径，Host 示例使用 `DirectQueryBinding`，两者的调用入口不同。

源码：[main.cpp](@source/ime/bgime/examples/host_wubi86/main.cpp) · [人工资源生成](@source/ime/bgime/examples/host_wubi86/fixture.cpp) · [CMakeLists.txt](@source/ime/bgime/examples/host_wubi86/CMakeLists.txt) · [五笔接口](@doc/api/wubi)。

## 从示例过渡到真实输入

先保持小型资源不变，替换输入来源和文本接收器，确认事件顺序与生命周期正确；再接入正式词库，验证资源身份、内存容量和显示字体。需要学习时，还要提供个人状态存储、后台调度和符合产品语义的事件资格。

更换词库后，需要同时调整示例中的固定输入和结果断言，使它们与新词库匹配。若要交互式浏览正式词库，还需要补充终端或界面的输入事件处理。
