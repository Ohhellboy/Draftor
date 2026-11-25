# Wubi（五笔86）

Wubi 通过 `Wubi86Decoder` 维护编码，通过 `DirectQueryBinding` 将编码计划绑定至 Core 候选会话。候选取字、翻页、显示和最终文字提交仍使用 Core。头文件见 [wubi.hpp](@source/integration/product/dependencies/wubi/include/hanime_wubi/wubi.hpp)，运行方法见[五笔示例](@doc/schemes)。

包含 `<hanime_wubi/wubi.hpp>`。五笔类型位于 `hanime::wubi`，Core 会话、状态和句柄位于 `hanime`。本页介绍直接查询路径；使用组合对象的五笔接入见[DirectQueryBinding 与 DirectExact 的比较](@doc/schemes#五笔的两条接入路径)。

## 编码规则与生命周期

`wubi86_profile` 的方案名为 `wubi86`，字母表为 `a`–`y`，普通编码上限为四码，并启用满码唯一候选的提交请求。使用其他五笔配置时，需要核对其字母表、编码长度和提交规则。

`DirectQueryPlan Decoder::plan() const` 返回编码计划，包含借用的编码 `code`、修订号 `revision` 和是否满码 `full_code`。编码视图只在下一次成功编辑前有效。若使用绑定层，应通过绑定层编辑，不要绕过它直接修改解码器或会话输入。

## 绑定初始化与激活

`DirectQueryBinding(Decoder& decoder, hanime::CandidateSession& session, hanime::PagedSchemeIndex& index, hanime::PagedSharedResource& shared, std::size_t page_size = 3)` 借用四个现有对象；默认页大小为 `3`，需要符合候选会话容量。它们都必须比绑定对象更早创建、更晚销毁。

先完成共享词库与方案索引的配对校验，再完成 `CandidateSession` 的文字准入，然后调用 `hanime::SessionStatus activate()`。成功为 `Ready`；`Busy` 要检查绑定是否已经激活或输入是否仍有活动；`InvalidRequest` 要检查非零页大小、索引就绪状态和方案名；`Stale` 要核对文字准入、资源配对和有效性。分配失败会返回 `AllocationFailure`。绑定活动期间，不要在外部重新设置会话的输入或资源绑定。回调同样不能递归进入绑定对象。

## 编码编辑与调度

| 调用 | 参数、结果和后续动作 |
| --- | --- |
| `EditStatus append(char routed_ascii)` | 传入由宿主路由的 ASCII 字符；大写折为小写。成功为 `Changed`，随后调用 `tick()` |
| `EditStatus backspace()` | 删除最后一码。空输入可返回 `Unchanged`；成功编辑产生新的修订 |
| `EditStatus cancel()` | 放弃当前编码和查询，不提交文字 |
| `EditStatus leave()` | 明确离开输入方案并放弃当前输入，不隐式提交 |
| `Step tick(const hanime::WorkBudget& budget)` | 推进绑定和共享会话，返回 `Step`：其中 `shared` 是会话状态与消耗，`request` 是提交请求，另有对应选择和编码修订 |

`EditStatus::InvalidCode` 表示字符不在配置字母表中，`Full` 表示编码长度已满；这两种拒绝不会改变当前编码或修订。`Busy` 表示提交等活动尚未完成。`Stale`、`Closed` 或 `Exhausted` 需要宿主停止使用旧操作并检查当前生命周期。

## 候选呈现、翻页与选择

绑定层按页大小取得候选及文字，并在查询完成后发布页面。`Step::shared.status == SessionStatus::Ready` 时，在共享会话的 `present()` 回调内复制候选显示内容。翻页调用共享会话的 `request_range()`，随后继续通过 `binding.tick()` 推进，保持当前编码与逻辑输入不变。

| 绑定层成员的完整类型签名 | 返回值 |
| --- | --- |
| `hanime::CommitResult confirm(const hanime::SelectionHandle& selection, CommitReason reason = CommitReason::Explicit)` | 合法选择返回 `Preparing`；随后推进到 `Accepted` 或其他提交终态 |
| `hanime::CommitResult retry(const hanime::RetryPermit& permit)` | 使用 Core 签发的许可重试，不新建一份学习或提交状态 |
| `hanime::SessionStatus query_status() const` | 当前查询状态 |
| `CommitReason commit_reason() const` | 当前记录的提交原因；不代表接收器已接受文字 |

用户确认当前候选时调用 `binding.confirm(selection, CommitReason::Explicit)`。接下来观察提交结果；需要重试时使用返回的 `RetryPermit` 调用绑定的 `retry()`。状态语义与 [Core 提交](@doc/api/core)相同。接受成功后绑定清空编码；重复确认已接受事务不会再次交付文字。

## 满码唯一候选请求

`Step::request == CommitReason::AutoUniqueFullCode` 表示待宿主确认的提交请求。当前首屏必须满足满码、只有一个候选、没有更多候选、排序仍有效且输出文字有效等条件。多个映射路径若指向同一词条身份，不会因此变成多个候选。

收到请求时，使用当次 `Step::selection` 和 `code_revision` 关联当前输入。由应用决定是否允许提交，允许后再调用 `confirm(selection, CommitReason::AutoUniqueFullCode)`。请求本身不会调用文本接收器。

Draftor 在这些条件之外，还会检查自动提交资源、显示窗口、当前输入事件和编辑目标。完整的产品处理见 [wubi_product.cpp](@source/integration/product/src/wubi_product.cpp) 和 [device_ime.cpp](@source/integration/product/src/device_ime.cpp)。

## 直接查询与提交示例

沿用 `host_wubi86` 的 `Resources`、`NoLearning` 和 `DemoDocument`：资源已经完整校验，`NoLearning` 提供固定计数所有者与命名空间，但不读取或记录个人次数。按顺序创建 `Wubi86Decoder`、`CandidateSession(index, shared, counts, sink, {4, 2, 1, 64, 64, 4})` 和 `DirectQueryBinding(decoder, session, index, shared, 1)`。完成会话文字准入、设置输出后，检查 `binding.activate() == SessionStatus::Ready`。绑定先销毁，随后才是会话、解码器及其他所有者。

```cpp
#include <hanime_wubi/wubi.hpp>
#include <stdexcept>
#include <string>

namespace docs_wubi {
using namespace hanime;
namespace wb = hanime::wubi;
constexpr WorkBudget budget{1, 17, 3};
constexpr std::size_t limit = 100000;
struct Query { wb::EditStatus edit; wb::Step step; };

// 字符事件 + 命令行调度；page_size 要与构造绑定时相同。
Query key(wb::DirectQueryBinding& binding, CandidateSession& session,
          std::size_t page_size, char character) {
  Query result{binding.append(character), {}};
  if (result.edit != wb::EditStatus::Changed) return result;
  // 与 host_wubi86 一致：第一次 tick 只建立逻辑输入。
  result.step = binding.tick(budget);
  if (result.step.shared.status != SessionStatus::Pending) return result;
  // 本示例无个人状态，在任何计数读取前明确改用基础排序。
  result.step.shared.status = session.restart_first(page_size, page_size, ScoringMode::BaseOnly);
  for (std::size_t n = 0; result.step.shared.status == SessionStatus::Pending; ++n) {
    if (n == limit) throw std::runtime_error("Wubi query did not finish");
    result.step = binding.tick(budget); // GUI 每次调度只做一步。
  }
  return result; // Ready、NoMatches 或实际错误；这里从未调用 confirm。
}
struct Frame { std::string text; SelectionHandle choice; OutputStatus output{OutputStatus::Pending}; };
SessionStatus read_first(CandidateSession& session, Frame& frame) {
  frame = {};
  bool order_stale = false;
  const auto status = session.present([&](const CandidateViewPage& page) {
    order_stale = page.order_stale;
    if (page.size) {
      frame.output = page.items[0].output;
      if (!order_stale && output_has_text(frame.output)) {
        frame.text = page.text(0);
        frame.choice = session.selection(0);
      }
    }
  });
  if (status == SessionStatus::Ready && order_stale)
    return SessionStatus::NeedsRankRestart;
  return status;
}
// 选择事件或宿主已批准的自动请求；不能仅凭 request 自动调用这个函数。
CommitResult confirm(wb::DirectQueryBinding& binding, CandidateSession& session,
                     const SelectionHandle& choice, wb::CommitReason reason) {
  auto result = binding.confirm(choice, reason);
  for (std::size_t n = 0; result.status == CommitStatus::Preparing; ++n) {
    if (n == limit) throw std::runtime_error("Wubi commit did not finish");
    binding.tick(budget);
    result = session.commit_result();
  }
  return result; // Stale、OutputRestricted、UnknownOutcome 等不自动重试。
}
} // namespace docs_wubi
```

依次以 `a`、`b`、`c`、`d` 调用 `key(binding, session, 1, character)`。先检查 `edit`：`InvalidCode` 或 `Full` 时保留原编码，`Busy` 时暂存事件。再检查 `step.shared.status`：`Ready` 后调用 `read_first()` 复制候选，`NoMatches` 时清空显示，其他终止状态保留错误并停止使用该次结果。`key()` 仅用于这个无学习 Host 的基础模式，不要把每轮调度都改成 `restart_first()`。

`read_first()` 返回 `Ready` 表示页面读取成功，仍需检查 `frame.output`。只有 `output_has_text(frame.output)` 为真时才显示文字并提供确认操作；没有有效文字时展示实际输出状态，例如 `OutputStatus::Blocked`，不把它当成没有匹配候选。返回 `NeedsRankRestart` 表示排序已过期，应重新建立当前排序并刷新显示；这一帧不保留可确认的选择。

当最后一步含有自动请求，先核对 `step.code_revision == decoder.plan().revision`，确认编辑目标仍有效、应用允许自动提交且候选输出有效，再向 `confirm()` 传入 `step.selection` 与 `AutoUniqueFullCode`。显式选择则传入显示帧保存的 `choice` 与 `Explicit`。接收结果以 `CommitResult::status` 为准；`Stale` 要刷新候选，`UnknownOutcome` 要核对接收端，不能再次插入文字。

查询与提交中的循环是 CLI 示例的等待方式。界面程序应将每个 `binding.tick()` 安排到事件循环，保留本次查询状态；不要同时再调用 `session.tick()` 驱动同一个会话。达到宿主步数上限不会自动取消引擎工作。可对照[真实运行输出](@doc/schemes#五笔86)，观察请求出现前后 `sink_calls` 的变化。

## 学习接入范围

绑定只把提交和学习通知委托给 Core，不拥有第二份个人计数。`host_wubi86` 明确使用基础排序和禁用的学习通知源，没有个人状态文件。

Alpha 3 产品的五笔共享学习走 Draftor 的组合与选词资格路径。接入这套学习行为时，除计数源外还需要处理选词资格和共享状态的生命周期，详见[选词学习](@doc/learning)。
