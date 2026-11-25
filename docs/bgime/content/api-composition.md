# Composition / Submission

`Composition` 管理原始编码、切分、候选和已选成员；`Submission` 准备组合事务，并通过 `TextCommitSink` 向宿主提交完整 UTF-8 文本。该路径不需要同时驱动 `CandidateSession`。可运行示例见 [host_composition/main.cpp](@source/ime/bgime/examples/host_composition/main.cpp)。

`Composition`、`Status`、`Selection` 和 `Operation` 来自 `<hanime_extension/extension.hpp>`；`Submission`、`SubmissionResult` 和 `SubmissionHandle` 来自 `<hanime_extension/submission.hpp>`，都位于 `hanime_extension` 命名空间。预算、输出和组合提交状态使用 `hanime::` 类型。

## 对象初始化

`Composition(hanime::PagedSchemeIndex& index, hanime::PagedSharedResource& shared, SchemeRules rules, Config config = {})` 借用已打开且校验完成的方案索引和共享资源。全拼选择 `SchemeRules::FullPinyin`，小鹤双拼选择 `SchemeRules::R1Xiaohe`。索引的方案名必须与规则匹配。

另一构造形式为 `Composition(hanime::PagedSchemeIndex&, hanime::PagedSharedResource&, SchemePolicy, Config = {}, StorageBackend = {})`，用 `SchemePolicy` 明确给出规则和资源方案名。`DirectExact` 代表直接编码查询，不运行拼音切分规划；Draftor 的五笔产品集成使用这个入口。五笔需要传入 `SchemePolicy{SchemeRules::DirectExact, "wubi86"}`，单独的 `SchemeRules::DirectExact` 不带资源方案名。它与直接绑定层的区别见[五笔路径选择](@doc/schemes#五笔接入路径)。

`Config` 中的 `raw_bytes`、`members`、`page_items` 分别约束原始编码字节数、已选词数量和页内候选数量。`word_bytes`、`name_bytes`、`text_bytes` 与 `request_bytes` 约束输出及请求缓冲区。容量超限返回错误，不截断 UTF-8 文本。`learning_counts` 是借用的计数源，`apply_learning` 控制是否用它参与排序；设置方法与产品边界见[选词学习](@doc/learning)。

构造之后检查 `valid()`。资源对象和借用的计数源要晚于输入对象销毁；若使用 `StorageBackend` 自定义分配器，其分配和释放约定也必须配对。公开声明见 [extension.hpp](@source/integration/product/dependencies/extension/include/hanime_extension/extension.hpp)。

## 编码编辑与查询

| 调用 | 参数及结果 |
| --- | --- |
| `Status edit_raw(std::size_t begin, std::size_t end, std::string_view replacement)` | 用 `replacement` 替换半开字节区间 `[begin, end)`。尾部插入时两个边界相等；退格可替换相应区间为空串。成功会复制编码并改变输入修订；活动操作期间可返回 `Busy` |
| `Status query_next(hanime::OutputRequest output = {}, std::uint64_t context_revision = 0)` | 为当前未选部分规划下一段候选；`output` 默认为默认输出形式，`context_revision` 由宿主用于区分输出上下文。通常返回 `Working` |
| `Operation operation() const` | 取得刚发起操作的句柄；后续 `tick()` 与 `cancel()` 使用同一个句柄 |
| `Step tick(const Operation& operation, const hanime::WorkBudget& budget)` | 在指定预算内推进操作，返回 `Step`，其中有状态、Core 工作量、局部工作量及输出状态 |
| `Status cancel(const Operation& operation)` | 取消这个操作，让宿主可以继续编辑；旧句柄不能取消替代它的新操作 |
| `Status cancel_input(std::uint64_t input_id)` | 明确放弃指定输入会话；不同于只取消当前查询 |

`RawInput::raw` 是借用的原始编码视图，`InputStamp` 标识它所属的输入和修订。跨事件保存前复制字符串；延后使用时间戳或规划结果时，用 `input_is_current()`、`plan_is_current()` 重新检查。

输出上下文变化后，应取消当前操作，以新的 `context_revision` 重新发起查询。不要让仍引用旧上下文的规划结果授权一次新的选择。

## 候选读取与选择

| `Composition` 成员的完整类型签名 | 返回值与用途 |
| --- | --- |
| `Page page() const` | 候选页的借用视图；在 `PageReady` 后读取 |
| `Selection selection(std::size_t slot) const` | 当前页中这个位置的选择句柄 |
| `Status read_text(const Selection& selection, hanime::OutputRequest output = {})` | 接受后返回 `Working`，推进到 `Ready` 后取字 |
| `std::string_view last_text() const` | 最近完成的文字读取结果，下一次修改前复制 |
| `Status select(const Selection& selection, hanime::OutputRequest output = {})` | 接受后返回 `Working`，推进到 `Ready` 才完成选词；旧选择返回 `Stale` |
| `Snapshot snapshot() const` | 原始编码、成员及组合状态的借用视图 |
| `Status request_page(hanime::RangeDirection direction)` | 对当前范围翻页，接受后继续推进到 `PageReady` |

查询到 `Status::PageReady` 后，调用 `page()` 读取候选数组、范围和 `has_more`。`page().items` 包含 `RankItem` 身份和排序信息，不包含可直接显示的候选文字。

先取得 `selection(slot)`，再调用 `read_text(selection, output)`，通过当次 `operation()` 与 `tick()` 推进到完成，读取 `last_text()`。它是借用的字符串视图，应在下一次修改前复制。选择候选则调用 `select(selection, output)`，并继续推进操作，使这个词进入已选部分。

`snapshot()` 返回原始编码、已选文字、成员范围、焦点和组合状态。所有页面、快照和文字视图都借用输入对象的缓冲区，只在下一次修改前有效。旧页号或旧候选位置不能替代当前选择句柄。

| `CompositionState` | 对宿主的含义 |
| --- | --- |
| `Idle` | 没有已选组合；仍需依据原始编码判断是否有输入 |
| `PartialSelected` | 已有部分词，但仍有未选编码，继续查询下一段 |
| `NeedsRecheck` | 输入或资源等上下文发生变化，已选部分需要重新验证 |
| `AllSelected` | 整个原始编码已被有效成员覆盖，可以准备组合提交 |

`request_page(direction)` 翻阅当前范围的候选，`retract_last()` 撤回最后一个已选词，`reopen_segment(index)` 重新打开指定成员，`revalidate()` 重新检查已选意图。`edit_raw()` 可能移除受影响成员或清除其学习资格；`rebind()` 清除保留成员的原有资格；`revalidate()` 保留现有选择凭据，不清空也不重新签发资格。具体区间行为见[选词资格与编辑](@doc/learning#选词资格与编辑)。活动操作之间需要遵守同样的串行和句柄约定。`ranking_is_current()` 可在延后的 UI 选择前检查排序依赖是否仍有效。

公开的 `select()` 完成选择，但不会签发 Alpha 3 产品的显式非首选学习资格。记录学习还需要满足[选词学习](@doc/learning)中的资格与提交条件。

## 组合提交

主要构造签名为 `Submission(Composition& composition, hanime::PagedSharedResource& shared, hanime::TextCommitSink& sink, hanime::CompositionConfig config = {16, 128, 64, 512, hanime::CompositionLearningPolicy::Disabled}, hanime::CompositionCountSource* counts = nullptr)`。`counts` 是可选的组合学习通知源；默认配置禁用学习。`CompositionConfig` 限定最大成员数、形式名称、单词文字和总文字容量。`Submission` 必须先于其借用的输入对象、共享资源和最终接收器销毁。

| 顺序与调用 | 参数、返回值和处理 |
| --- | --- |
| `SubmissionResult prepare(InputTarget target = InputTarget::Body)` | 默认目标为 `InputTarget::Body`；另有 `Find`、`Rename`。捕获当前 `AllSelected` 组合，不做 I/O。成功返回 `core.status == Preparing` 及 `SubmissionHandle`；未完整选择返回 `InvalidRequest` |
| `SubmissionStep advance(const SubmissionHandle& handle, const hanime::WorkBudget& budget)` | 解析并验证完整输出。反复推进到 `Ready`，保留这一次返回的事务信息；错误时按 `core.status` 处理 |
| `hanime::CompositionStatus prepared(const SubmissionHandle& handle, const std::function<void(const hanime::PreparedCompositionView&)>& callback)` | 在同步回调内查看 `PreparedCompositionView`，包含成员及完整 UTF-8 文字；视图只在回调中有效，不会调用文本接收器 |
| `SubmissionResult confirm(const SubmissionHandle& handle)` | 确认当前准备好的事务，由接收器接受一次完整组合。检查 `core.status`，成功为 `Accepted` |
| `SubmissionResult retry(const SubmissionHandle& handle)` | 仅在可重试且确定没有文本效果时调用；成功开始重试后使用返回的新句柄，旧尝试的句柄不能继续控制新尝试 |
| `SubmissionResult cancel(const SubmissionHandle& handle)`、`SubmissionResult close()` | 前者取消指定当前尝试，后者关闭整个适配器；都不能消除已经接受或效果不确定的历史事实 |

<figure class="api-flow"><svg viewBox="0 0 370 370" role="img" aria-labelledby="submission-flow-title submission-flow-desc"><title id="submission-flow-title">组合文字的准备与确认</title><desc id="submission-flow-desc">已选完编码后 prepare 捕获组合，advance 分步准备文字，prepared 可预览，confirm 提交至接收器，Accepted 后输入清空。</desc><defs><marker id="submission-flow-arrow" markerWidth="7" markerHeight="7" refX="6" refY="3" orient="auto"><path d="M0 0 L6 3 L0 6"/></marker></defs><rect class="state" x="35" y="10" width="300" height="57" rx="9"/><text class="node-label" x="185" y="34">AllSelected</text><text class="node-note" x="185" y="54">所有编码都已被选中词覆盖</text><path class="rail" d="M185 67 V105" marker-end="url(#submission-flow-arrow)"/><rect x="45" y="76" width="280" height="20" fill="var(--shell)"/><text class="action" x="185" y="91">prepare(target)</text><rect class="state" x="35" y="105" width="300" height="57" rx="9"/><text class="node-label" x="185" y="129">Preparing</text><text class="node-note" x="185" y="149">捕获组合，正在准备文字</text><path class="rail" d="M185 162 V200" marker-end="url(#submission-flow-arrow)"/><rect x="45" y="171" width="280" height="20" fill="var(--shell)"/><text class="action" x="185" y="186">advance(handle, budget)</text><rect class="state" x="35" y="200" width="300" height="57" rx="9"/><text class="node-label" x="185" y="224">Ready</text><text class="node-note" x="185" y="244">可以 prepared() 预览，还未写入</text><path class="rail" d="M185 257 V295" marker-end="url(#submission-flow-arrow)"/><rect x="45" y="266" width="280" height="20" fill="var(--shell)"/><text class="action" x="185" y="281">confirm(handle)</text><rect class="state" x="35" y="295" width="300" height="57" rx="9"/><text class="node-label" x="185" y="319">Accepted</text><text class="node-note" x="185" y="339">接收器已接受；输入被清空</text></svg><figcaption>框内第一项是 CompositionState，其余是 CompositionStatus；箭头是 Submission 的调用。发生错误时按返回状态处理，不继续沿成功路径提交。</figcaption></figure>

准备与就绪阶段会暂时占用输入对象，避免组合在提交期间被改动。正常接受后输入被清空。`UnknownOutcome` 会隔离这个输入会话，即使销毁适配器也不能把“可能已经写入”的事实变成可安全重放。

`prepare()` 的目标也参与学习决定：`Find` 和 `Rename` 不进行正文学习。`set_record_enabled(bool)` 只影响未来事务，已经捕获的记录决定和后台工作不会因此被删除或关闭。

源码：[submission.hpp](@source/integration/product/dependencies/extension/include/hanime_extension/submission.hpp) · [submission.cpp](@source/integration/product/dependencies/extension/src/submission.cpp) · [Core 组合结果类型](@source/integration/product/dependencies/core/include/hanime/composition_commit.hpp)。

## 组合输入与提交示例

以下示例采用 `host_composition` 的命令行调度方式。先按示例创建并校验 `Resources`，创建 `DemoDocument`，再构造 `Composition` 和 `Submission`，分别检查 `valid()`。全拼使用 `resources.full` 与 `FullPinyin`，小鹤使用 `resources.xiaohe` 与 `R1Xiaohe`；两个对象使用默认容量、关闭学习。下面的函数借用这些对象，不延长其生命期。

```cpp
#include <hanime_extension/submission.hpp>
#include <stdexcept>
#include <string>

namespace docs_composition {
namespace ex = hanime_extension;
constexpr hanime::WorkBudget budget{1, 17, 3};
constexpr std::size_t limit = 100000;

// 命令行示例的调度循环。GUI 应保存 operation，每次调度只调用一次 tick。
ex::Status finish(ex::Composition& input, ex::Status status) {
  const auto operation = input.operation();
  for (std::size_t n = 0; status == ex::Status::Working; ++n) {
    if (n == limit) throw std::runtime_error("input operation did not finish");
    status = input.tick(operation, budget).status;
  }
  return status; // 原样保留 Stale、Busy、资源错误等结果。
}

// 字符事件；这里只示范尾部追加，删除改用相应的 edit_raw 字节区间。
ex::Status append_key(ex::Composition& input, char key) {
  const auto end = input.raw_input().raw.size();
  const auto edited = input.edit_raw(end, end, std::string_view(&key, 1));
  return edited == ex::Status::Ready ? finish(input, input.query_next()) : edited;
}
struct Frame { ex::Selection choice; std::string text; };
ex::Status read_first(ex::Composition& input, Frame& frame) {
  frame = {};
  if (input.status() != ex::Status::PageReady || input.page().size == 0)
    return ex::Status::InvalidRequest;
  const auto choice = input.selection(0);
  const auto status = finish(input, input.read_text(choice));
  if (status == ex::Status::Ready) frame = {choice, std::string(input.last_text())};
  return status;
}
// 用户选择事件；传入显示时保存的选择，不能重新按页内序号取候选。
ex::Status choose(ex::Composition& input, const Frame& shown) {
  return finish(input, input.select(shown.choice));
}

struct Prepared { ex::SubmissionResult result; std::string text; };
Prepared prepare(ex::Submission& submission) {
  Prepared prepared{submission.prepare(ex::InputTarget::Body), {}};
  for (std::size_t n = 0;
       prepared.result.core.status == hanime::CompositionStatus::Preparing; ++n) {
    if (n == limit) throw std::runtime_error("submission did not finish");
    // 调度循环；每一步使用同一次尝试的句柄。
    prepared.result = submission.advance(prepared.result.transaction, budget).result;
  }
  if (prepared.result.core.status == hanime::CompositionStatus::Ready) {
    const auto status = submission.prepared(prepared.result.transaction,
        [&](const hanime::PreparedCompositionView& view) { prepared.text = view.text; });
    if (status != hanime::CompositionStatus::Ready) prepared.result.core.status = status;
  }
  return prepared; // 此时只是准备好文字，尚未调用接收器。
}
// 确认提交事件；调用者必须先检查 prepare 的结果为 Ready。
ex::SubmissionResult confirm(ex::Submission& submission, const Prepared& prepared) {
  return submission.confirm(prepared.result.transaction);
}
} // namespace docs_composition
```

全拼输入逐字符调用 `append_key()` 追加 `yuanxingji`。前缀可能返回 `NoPrefixMatch`、`NoMappingForPrefix` 或 `Unresolved`，继续接收编码；完整词到 `PageReady` 后调用 `read_first()`，得到「原型机」和对应选择。若查询被取消或资源失效，保留实际错误并停止使用该次候选。

显示后，由选词事件调用 `choose()`。返回 `Stale` 时丢弃那份帧并重新查询，返回 `Ready` 后检查 `snapshot().state`：`PartialSelected` 需要继续查询和选词，`AllSelected` 才进入 `prepare()`。检查返回的 `result.core.status == Ready`，再向 `confirm()` 传入同一份 `Prepared`；`Accepted` 表示接收器已接受一次完整文本。输出预览已经复制，不能用它自行插入文字来代替确认事务。

两个循环仅适合命令行示例；接入界面时，将 `tick()`、`advance()` 的单步调用放入调度器，保存操作或提交句柄，并在下一轮处理返回状态。达到示例的步数上限只表示宿主停止等待，不代表引擎自动取消：继续编辑前应显式取消当前操作或提交尝试。正常流程、重复确认与过期选择可对照 `host_composition`；`RetryableNoEffect` 和 `UnknownOutcome` 仍需按下表实现宿主恢复操作。

## 错误与有效期

| 结果 | 处理方式 |
| --- | --- |
| `Working`、`CompositionCommitPending` | 继续当前操作或等待提交，不并发编辑 |
| `NoMappingForPrefix`、`NoPrefixMatch`、`Unresolved` | 让用户修改或补全编码，不制造候选文字 |
| `RangeExhausted` | 已到当前候选范围的边界 |
| `Stale` | 废弃旧选择或事务尝试，回到当前上下文；不要重用旧页内序号 |
| `ResourceError`、`ReadError`、`IdentityMismatch` | 检查资源有效性及配对，停止使用本次结果 |
| `OutputRestricted` | 不提交不允许的输出形式 |
| `CapacityExceeded` | 检查配置；组合提交的 `capacity`、`required_capacity` 和 `member_index` 可定位不足的位置 |
| `RetryableNoEffect` | 在确认没有文本效果的约定下使用重试接口 |
| `UnknownOutcome` | 停止自动重试，先核对应用是否已经接收文字 |

`Composition::confirm_composition()` 是旧的查询性探测，不执行真实文本事务；实际提交使用这里的 `Submission` 路径。历史结果句柄可以保留用于观察，但应用需要限制保留数量，不能无限积累事务历史。
