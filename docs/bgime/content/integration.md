# 接入自己的程序

接入的目标是让用户输入编码、看到候选，再把选中的文字写入你的应用。先以一个已经跑通的 Host 示例为起点，逐项替换输入、显示、文本接收和存储部分，能更容易分清问题发生在哪一层。

## 先选接入入口

| 你要完成的事 | 推荐入口 | 可运行的参照 |
| --- | --- | --- |
| 已经有完整编码，只需查询并提交一个候选 | Core `CandidateSession` | `host_minimal` |
| 全拼或小鹤双拼逐键输入，并组合多个词 | Extension `Composition` + `Submission` | `host_composition` |
| 五笔86逐键输入并直接提交候选 | `Wubi86Decoder` + `DirectQueryBinding` + `CandidateSession` | `host_wubi86` |

五笔还可以使用 `Composition` 的 `DirectExact` 路径。先阅读[两条路径的选型说明](@doc/schemes#五笔的两条接入路径)，再决定是否沿用组合接口。

先复用示例的 CMake 文件，使程序链接实际使用的模块。Core 的目标名为 `HanIME::Core`，Extension 为 `HanIME::Extension`；五笔示例从现有 `wubi.cpp` 建立本地库目标，目前没有独立的 Wubi 安装包。只需要 Core 库时可以这样构建：

```bash
cmake -S integration/product/dependencies/core \
  -B build/bgime-core \
  -DHANIME_BUILD_HOST_ADAPTERS=OFF \
  -DHANIME_BUILD_TOOLS=OFF \
  -DHANIME_BUILD_FIXTURES=OFF \
  -DHANIME_BUILD_TESTS=OFF
cmake --build build/bgime-core --target hanime_core
```

这只生成库，不是一个可直接输入中文的程序。嵌入自己的 CMake 工程时，先设置这些选项，再用 `add_subdirectory()` 引入 Core；需要组合路径时接着引入 Extension。参考三个示例的构建文件，不必引入整个 Draftor 产品工程。

## 打开资源，再接收输入

共享词库保存词条身份和文字，方案索引把编码映射到词条。初始化时先打开共享资源，再用它的资源包身份打开方案索引。**命名空间**用来区分不同词条身份体系，**资源包 ID**用来约束共享词库与方案索引的配对；这两个值都不能靠文件名猜测。

Host 示例使用 `HostResourceFile` 和完整的分步校验。`open()` 成功只表示初步打开成功，随后仍要执行 `advance_validation()` 到 `Ready`。构建 `CandidateSession` 后，还需运行 `begin_text_access()` 和 `advance_text_access()`，确认文字容量满足要求。参数和错误处理见[资源接口](@doc/api/core)。

自己的设备需要实现 `ResourceReader`，提供大小、按偏移读取及有效性检查。底层存储更换或卸载后，应让旧读者失效，并废弃依赖它的会话与候选；缓存读取也需要遵守这一有效性检查。

> 正式资源使用已固定身份的读取与校验路径。调用 `open_verified()` 前，需要准备可信的发行描述和验证读者。请参考[资源章节](@doc/resources)和产品资源加载代码。

## 把输入事件交给方案模块

宿主先决定事件属于输入法还是应用快捷键，再调用相应模块。全拼和小鹤双拼使用 `Composition::edit_raw(begin, end, replacement)` 修改编码，随后用 `query_next()` 查询当前未选部分。五笔直接查询使用绑定层的 `append()`、`backspace()` 和 `cancel()`；组合路径则在宿主校验字母和长度后，也使用 `edit_raw()` 与 `query_next()`。

编码编辑、查询和文字读取是串行操作。收到 `Busy` 时保留用户事件，在当前操作结束或明确取消后再处理；不要在另一个线程中强行改写输入。取消操作使用当次操作的句柄，不使用早先输入留下的句柄。

你的键盘布局、候选快捷键和取消策略由应用决定。接入触摸屏也可以沿用同样的查询与选择接口，并不要求模拟草稿机的按键。

## 分步查询与显示候选

引擎把可能较长的工作拆成小步。应用在事件循环中调用 `tick()` 或 `advance()`，并用 `WorkBudget` 限制这一步的读取次数、读取字节和逻辑工作量。出现 `Pending` 或 `Working` 时安排下一步；出现终止状态时按接口处理，不要无限重试。

这些步骤由宿主主动调用，`WorkBudget` 限制的是工作量。一次存储调用或文字接收回调仍可能耗时。嵌入式程序应在各步之间处理输入与显示，并测量目标存储的实际延迟。示例中的循环会一直运行到完成；接入设备时，需要将这些调用安排到自己的事件调度器中。

Core 候选会话先生成暂存页，`publish()` 后成为当前可用页。文字可以按需通过 `request_text()` 请求；在 `present()` 的同步回调内复制文字和当前选择句柄，然后再交给界面。Extension 则使用 `page()`、`selection()`、`read_text()` 和 `last_text()`。

**不要在回调结束后持有借用的字符地址。** 页面序号只是当前页面中的位置；用户按下选择键时，应检查显示内容仍对应当前输入和排序。若接口返回 `Stale`，刷新候选并让用户重新选择，不能凭相同文字自动替换成另一词条身份。

## 把文字交给应用

实现 `TextCommitSink::commit(std::string_view)`，在回调返回前接收完整的 UTF-8 文本。下面是一个容量固定的内存接收器，适合先观察调用关系：

```cpp
#include <hanime/core.hpp>
#include <array>
#include <cstring>

class TextBuffer final : public hanime::TextCommitSink {
 public:
  std::array<char, 1024> bytes{};
  std::size_t used = 0;

  bool commit(std::string_view text) override {
    if (text.size() > bytes.size() - used) return false;
    if (!text.empty()) std::memcpy(bytes.data() + used, text.data(), text.size());
    used += text.size();
    return true;
  }
};
```

它不追加字符串终止符，读取时用 `used` 确定字节长度。换成编辑器时，请让一次提交要么完整接收，要么完全不改变文档：`true` 表示已接收；`false` 必须表示没有产生任何效果。不要写入半段后返回 `false`。

Core 的 `confirm()` 开始一次候选提交，随后 `tick()` 推进到终态。Extension 则在所有编码都已选择后执行 `Submission::prepare()` → `advance()` → `confirm()`，一次交付整个组合的文字。`Accepted` 表示应用接收成功，不表示文稿已写入磁盘，也不表示个人学习已保存。

收到 `RetryableNoEffect` 时，只能使用接口提供的重试许可或当次事务重试。收到 `UnknownOutcome` 时不要重新插入文字：应用可能已经改动，需要先核对接收端状态。成功后的学习错误也不能通过再次提交正文来恢复。

## 五笔从直接查询转到组合路径

如果已有 `DirectQueryBinding` 接入，先保留原来的资源和接收端，再逐项替换控制流程。两条路径的适用需求见[方案选择](@doc/schemes#五笔的两条接入路径)。

1. **更换输入对象。** 用 Core + Extension 构建 `Composition(index, shared, SchemePolicy{SchemeRules::DirectExact, "wubi86"})` 和 `Submission`。停止由直接绑定层驱动同一份输入；新的操作由 `Composition::tick()` 与 `Submission::advance()` 推进。
2. **重新连接编辑事件。** 将追加、删除转换为 `edit_raw()` 的字节区间，再调用 `query_next()`。Wubi 解码器可以复用于输入校验，但组合对象自己持有原始编码；必须让校验结果与实际编辑一致。编辑会废弃 DirectExact 的原有已选成员，不能把旧选择直接搬到新编码上。
3. **更新候选显示和选择。** 将 Core 页与 `SelectionHandle` 改为组合页、`read_text()` 和 `Selection`。界面可继续使用复制后的 UTF-8 字符串，但需要重新绑定当前输入、页面和选择句柄。旧类型的句柄不能转换后复用。
4. **更新提交与取消。** 选择完成后检查 `AllSelected`，再准备和确认 `Submission` 事务。重新连接准备期间的 `Busy`、取消、重试及 `UnknownOutcome` 处理。原先绑定层发出的自动提交请求不再存在，宿主需单独实现自己的触发和上下文检查。
5. **单独安排学习。** 使用公开 `select()` 能完成普通输入与提交；Alpha 3 的非首选学习还缺少通用公开资格入口，不能仅设置 `apply_learning` 就完成。存储、状态调度和保存也要按[学习章节](@doc/learning)接入。

配对且仍有效的只读词库文件、平台 `ResourceReader` 和 `TextCommitSink` 实现通常可以复用；重新创建对象时仍要遵守挂载代次和所有者生命周期。旧的会话缓冲区、候选帧和提交句柄不随迁移继承。若已有个人状态，先核对词条命名空间与计数所有者的一致性，不按显示文字转换记录。

## 宿主需要提供的能力

| 能力 | 最小输入示例 | 接入学习或正式设备时 |
| --- | --- | --- |
| 只读资源存储 | POSIX 文件读者，完整校验小型资源 | 按偏移读取、有效性检查、匹配的正式资源与验证数据 |
| 调度 | 有上限的循环推进 | 串行事件循环、取消和过期处理、前台与后台工作机会 |
| 显示 | 终端输出复制后的文字 | UTF-8 绘制、候选与当前页面绑定、足够的文字缓冲区 |
| 文本接收 | 内存字符串或固定缓冲区 | 一次完整接收的语义、编辑目标检查、错误与不确定结果处理 |
| 个人状态 | 示例禁用学习 | 可写的 `StateFilePort`、同一记录所有者、后台写入与明确关闭保存 |

资源读者、共享资源、计数源和文本接收器的生命周期必须长于使用它们的会话。组合提交适配器先于 `Composition` 销毁；不要从显示或提交回调递归调用引擎。

## 接入学习前还要确认什么

`BoundedUserState` 和 `StateV3` 已提供共享计数与持久化能力。Alpha 3 的产品学习还依赖显式选择非首选候选的事件资格，并不是每次提交都增加次数。当前通用公开接口没有暴露与 Draftor 候选窗口等价的资格签发入口。

先完成上面的基本输入，再按[选词学习](@doc/learning)准备学习接入。除可写的计数源外，还需要解决选词资格入口、共享状态的存储与调度，以及保存失败后的处理。

完成这些连接后，可从[接口导览](@doc/api)查找具体声明与完整调用示例。
