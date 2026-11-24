# 接入自己的程序

接入的目标是让用户输入编码、看到候选，再把选中的文字写入你的应用。先以一个已经跑通的 Host 示例为起点，逐项替换输入、显示、文本接收和存储部分，能更容易分清问题发生在哪一层。

## 先选接入入口

| 你要完成的事 | 推荐入口 | 可运行的参照 |
| --- | --- | --- |
| 已经有完整编码，只需查询并提交一个候选 | Core `CandidateSession` | `host_minimal` |
| 全拼或小鹤双拼逐键输入，并组合多个词 | Extension `Composition` + `Submission` | `host_composition` |
| 五笔86逐键输入 | `Wubi86Decoder` + `DirectQueryBinding` + `CandidateSession` | `host_wubi86` |

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

这只生成库，不是一个可直接输入中文的程序。嵌入自己的 CMake 工程时，先设置这些选项，再用 `add_subdirectory()` 引入 Core；需要拼音时接着引入 Extension。参考三个示例的构建文件，不必引入整个 Draftor 产品工程。

## 打开资源，再接收输入

共享词库保存词条身份和文字，方案索引把编码映射到词条。初始化时先打开共享资源，再用它的资源包身份打开方案索引。**命名空间**用来区分不同词条身份体系，**资源包 ID**用来约束共享词库与方案索引的配对；这两个值都不能靠文件名猜测。

Host 示例使用 `HostResourceFile` 和完整的分步校验。`open()` 成功只表示初步打开成功，随后仍要执行 `advance_validation()` 到 `Ready`。构建 `CandidateSession` 后，还需运行 `begin_text_access()` 和 `advance_text_access()`，确认文字容量满足要求。参数和错误处理见[资源接口](@doc/api/core)。

自己的设备需要实现 `ResourceReader`，提供大小、按偏移读取及有效性检查。底层存储更换或卸载后，应让旧读者失效，并废弃依赖它的会话与候选。已有缓存不意味着被移走的 TF 卡仍然有效。

> 正式资源使用已固定身份的读取与校验路径。`open_verified()` 要求可信的发行描述和验证读者；它不是跳过校验的快捷开关。请参考[资源章节](@doc/resources)和产品资源加载代码。

## 把输入事件交给方案模块

宿主先决定事件属于输入法还是应用快捷键，再调用相应模块。全拼和小鹤双拼使用 `Composition::edit_raw(begin, end, replacement)` 修改编码，随后用 `query_next()` 查询当前未选部分。五笔使用绑定层的 `append()`、`backspace()` 和 `cancel()`。

编码编辑、查询和文字读取是串行操作。收到 `Busy` 时保留用户事件，在当前操作结束或明确取消后再处理；不要在另一个线程中强行改写输入。取消操作使用当次操作的句柄，不使用早先输入留下的句柄。

你的键盘布局、候选快捷键和取消策略由应用决定。接入触摸屏也可以沿用同样的查询与选择接口，并不要求模拟草稿机的按键。

## 分步查询与显示候选

引擎把可能较长的工作拆成小步。应用在事件循环中调用 `tick()` 或 `advance()`，并用 `WorkBudget` 限制这一步的读取次数、读取字节和逻辑工作量。出现 `Pending` 或 `Working` 时安排下一步；出现终止状态时按接口处理，不要无限重试。

这不是后台线程，也不是毫秒级耗时保证。一次存储调用或文字接收回调仍可能耗时。嵌入式程序应在各步之间处理输入与显示，并测量目标存储的实际延迟。示例中的循环只是让命令行程序一直运行到完成，不能直接替代设备的事件调度器。

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

因此，先完成上面的基本输入，再按[选词学习](@doc/learning)核对已有能力和产品边界。不能把示例中的禁用计数源改成一个可写对象，就宣称已完成 Alpha 3 学习接入。
