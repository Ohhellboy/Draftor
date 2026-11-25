# 资源、候选与提交

本页说明从打开资源到提交一个候选的 Core 路径。完整的可运行调用见 [host_minimal/main.cpp](@source/ime/bgime/examples/host_minimal/main.cpp)。需要逐键拼音或多个词组合时，在这些资源之上使用 [Composition 与 Submission](@doc/api/composition)。

Core 的下列类型均位于 `hanime` 命名空间。资源声明需要 `<hanime/core.hpp>` 和 `<hanime/paged_shared_resource.hpp>`；候选与提交声明需要 `<hanime/candidate_session.hpp>`。后者也引入本页使用的资源、预算和输出类型。

## 资源读取与打开

`ResourceReader` 的 `size()` 返回文件字节数，`read(offset, destination, byte_count)` 按偏移写入调用者提供的缓冲区，完整读取成功才返回 `true`。`healthy()` 是不进行 I/O 的有效性检查。更复杂的读者可以实现 `read_budgeted()`：它返回 `Pending`、`Ready` 或 `Error` 及实际 `WorkCost`；在 `Ready` 前，目标缓冲区要继续有效且不能展示给用户。

电脑示例使用 [HostResourceFile](@source/integration/product/dependencies/core/include/hanime/host_resource_file.hpp)。构造时传入路径、单次读取上限及统计对象，并检查 `valid()`；统计对象也需要活得比文件读者更久。

| 调用 | 参数含义 | 返回值与下一步 |
| --- | --- | --- |
| `PagedSharedResource::open(reader, expected_namespace, expected_bundle_id, mount_epoch, config, error)` | 持有的资源读者、预期词条命名空间、预期包 ID、挂载代次、缓存配置及错误字符串指针。包 ID 为 `0` 时允许发现当前不可变资源包的 ID | `false` 时读取 `error` 并停止；`true` 后取得 `validation_cursor()`，用 `advance_validation(cursor, budget)` 校验到 `Ready` |
| `PagedSchemeIndex::open(reader, expected_registry_namespace, expected_shared_bundle_id, maximum_runtime_ref, config, error)` | 方案读者；命名空间和包 ID 必须与共享资源一致；最大运行期引用来自共享资源的词条数量 | 成功后继续 `advance_validation(budget)`，不能在 `InProgress` 时查询 |
| `ready()`、`info()` | 观察资源是否可用及其元数据 | 校验成功后核对 `info().scheme_id` 与所需输入方案一致 |

打开接口的完整类型签名如下；参数名与上表对应，均返回 `bool`：

- `bool PagedSharedResource::open(std::shared_ptr<const ResourceReader> reader, RegistryNamespaceId expected_namespace, SharedBundleId expected_bundle_id, MountEpoch mount_epoch, const PagedSharedConfig& config, std::string* error)`
- `bool PagedSchemeIndex::open(std::shared_ptr<const ResourceReader> scheme_bundle, RegistryNamespaceId expected_registry_namespace, SharedBundleId expected_shared_bundle_id, RuntimeLexemeRef maximum_runtime_ref, const PagedSchemeIndexConfig& config, std::string* error)`

**挂载代次** `MountEpoch` 标识当前资源挂载周期。更换存储或资源时由宿主管理新代次及读者有效性，不能把旧候选带进新挂载。示例固定使用 `1`，因为它的一次进程运行只使用不可变测试文件。

校验只在 `InProgress` 时继续推进。其他非 `Ready` 状态需要处理错误；预算不足时调整预算，资源损坏或不匹配时更换正确资源，不要把失败当成空词库。共享校验光标与创建它的当前资源激活绑定。

源码：[ResourceReader 与 PagedSchemeIndex](@source/integration/product/dependencies/core/include/hanime/core.hpp) · [PagedSharedResource](@source/integration/product/dependencies/core/include/hanime/paged_shared_resource.hpp)。

## 创建候选会话与文字准入

`CandidateSession(PagedSchemeIndex& index, PagedSharedResource& shared, CommitCountSource& counts, TextCommitSink& sink, const SessionConfig& config = {})` 借用方案索引、共享资源、计数及提交通知源、文本接收器。上述对象都必须比会话活得更久。创建后检查 `valid()`，再完成文字准入：`begin_text_access()` → 多次 `advance_text_access(budget)` → `TextAccessStatus::Ready`。

`bool valid() const` 检查构造结果；`TextAccessStatus begin_text_access()` 发起准入，`TextAccessStep advance_text_access(const WorkBudget& budget)` 返回本步状态、成本和容量信息，`bool text_access_ready() const` 可检查是否完成。

文字准入会检查资源中的输出是否能放入当前配置。返回 `Incompatible`、`ReadError` 或 `Stale` 时不要继续查询；`required_bytes` 和位置字段可帮助定位容量或资源问题。需要新容量时重新配置相应对象，而不是截断 UTF-8 文本。

| `SessionConfig` 字段 | 用途 |
| --- | --- |
| `ranking_slots` | 排序工作区项数 |
| `snapshot_slots` | 候选页最大项数，不得大于 `ranking_slots` |
| `raw_batch` | 一次处理的原始查询批次容量 |
| `output_bytes` | 每个候选及提交输出的文字字节容量 |
| `form_bytes` | 输出形式名称的字节容量 |
| `code_bytes` | 输入编码的字节容量 |

`set_output(OutputRequest)` 和 `bind_scheme(index)` 只在空闲会话中调用。空的 `OutputRequest::form` 表示默认输出；指定形式时仍需处理受限或回退输出，不能假设每个词都有该形式。

## 查询、发布和取字

以下成员都属于 `CandidateSession`，返回值不是布尔值，需要检查具体枚举状态。

| 完整类型签名 | 返回内容 |
| --- | --- |
| `SessionStatus begin_input(std::string_view code, MountEpoch epoch, std::size_t q, std::size_t text_count, ScoringMode mode = ScoringMode::Exact)` | 成功启动返回 `Pending`；参数、资源或活动事务不允许时返回相应错误 |
| `SessionStep tick(const WorkBudget& budget)` | 本步 `status` 和 `cost` |
| `SnapshotHandle staging() const`、`SnapshotHandle published() const` | 当前暂存页或已发布页的句柄 |
| `SessionStatus publish(const SnapshotHandle& snapshot)` | 发布成功返回 `Ready`，过期页返回 `Stale` |
| `SessionStatus request_text(const SnapshotHandle& snapshot, std::size_t start, std::size_t count)` | 接受请求后返回 `Pending`，随后仍由 `tick()` 推进 |
| `SessionStatus present(const std::function<void(const CandidateViewPage&)>& callback)` | 有效页面在同步回调中提供，成功返回 `Ready` |
| `SelectionHandle selection(std::size_t slot) const` | 对应页内位置的选择句柄；提交时仍会验证有效性 |

按下面顺序组织一次完整编码查询：

1. `begin_input(code, epoch, q, text_count, mode)` 开始新输入。`code` 被复制到有界缓冲区；`q` 是本页希望取得的候选数，必须大于零且不超过 `snapshot_slots`。`text_count` 不超过 `q`，为 `0` 时先只取候选身份。`mode` 是 `Exact` 或 `BaseOnly`。

   `epoch` 的类型是 `MountEpoch`，应与打开共享资源时使用的[挂载代次](#资源读取与打开)一致。同一次挂载内的多次查询可以复用这个值，不能按按键或查询次数递增；传入与当前资源不一致的代次会返回 `Stale`。

2. 返回 `Pending` 后反复调用 `tick(budget)`；到 `StagingReady` 时，调用 `publish(staging())` 发布这一页。
3. 如需补取文字，调用 `request_text(published(), start, count)`，其中 `start` 是从零开始的页内位置。继续 `tick()`，直到文字准备完成。
4. 用 `present(callback)` 读取当前页，并在回调中复制你需要保留的数据。返回 `Ready` 才表示此次展示使用了有效页面。

<figure class="api-flow"><svg viewBox="0 0 370 370" role="img" aria-labelledby="core-flow-title core-flow-desc"><title id="core-flow-title">Core 候选页从查询到取字</title><desc id="core-flow-desc">输入事件发起查询，调度 tick 生成暂存页；publish 发布，再 request_text 和 tick 补取文字，最后 present 复制显示。</desc><defs><marker id="core-flow-arrow" markerWidth="7" markerHeight="7" refX="6" refY="3" orient="auto"><path d="M0 0 L6 3 L0 6" fill="none" stroke="currentColor"/></marker></defs><rect class="state" x="35" y="10" width="300" height="57" rx="9"/><text class="node-label" x="185" y="34">Pending</text><text class="node-note" x="185" y="54">begin_input 已接受查询</text><path class="rail" d="M185 67 V105" marker-end="url(#core-flow-arrow)"/><rect x="45" y="76" width="280" height="20" fill="var(--shell)"/><text class="action" x="185" y="91">tick(budget)</text><rect class="state" x="35" y="105" width="300" height="57" rx="9"/><text class="node-label" x="185" y="129">StagingReady</text><text class="node-note" x="185" y="149">候选身份已就绪</text><path class="rail" d="M185 162 V200" marker-end="url(#core-flow-arrow)"/><rect x="45" y="171" width="280" height="20" fill="var(--shell)"/><text class="action" x="185" y="186">publish(staging())</text><rect class="state" x="35" y="200" width="300" height="57" rx="9"/><text class="node-label" x="185" y="224">Ready</text><text class="node-note" x="185" y="244">publish 后的当前页</text><path class="rail" d="M185 257 V295" marker-end="url(#core-flow-arrow)"/><rect x="45" y="266" width="280" height="20" fill="var(--shell)"/><text class="action" x="185" y="281">request_text(...) → tick(budget)</text><rect class="state" x="35" y="295" width="300" height="57" rx="9"/><text class="node-label" x="185" y="319">StagingReady</text><text class="node-note" x="185" y="339">文字已补入当前页</text></svg><figcaption>框内为会话状态及其含义，箭头旁为宿主调用。最后用 present() 复制当前页；补取文字后不再次 publish。</figcaption></figure>

`Exact` 使用真实个人次数参与排序，`BaseOnly` 只用基础排序。个人状态不可用不能伪装为所有次数都为零；宿主可以明确选择基础模式，并在界面中如实表达状态。

`CandidateViewPage` 中的 `size` 是候选数，`has_more` 表示还有后续候选，`order_stale` 表示当前排序已过期。`text(slot)` 只在输出具有有效文字时返回非空视图。仅有 `RankItem` 或候选身份时，文字可能仍处于 `OutputStatus::Pending`。

> `CandidateViewPage` 的候选数组、文字池和 `string_view` 仅在同步 `present()` 回调期间有效。异步 UI 应复制文字，并把选择句柄与那一帧显示内容一起保存。

源码：[candidate_session.hpp](@source/integration/product/dependencies/core/include/hanime/candidate_session.hpp) · [输出状态](@source/integration/product/dependencies/core/include/hanime/bounded_output.hpp)。

## 从查询到提交的调用示例

下面从 `host_minimal` 提炼第一页的处理流程。先用该示例的 `Resources` 打开并校验文件，再创建 `NoLearning`、`DemoDocument` 和 `CandidateSession`，完成文字准入。会话配置至少允许一个候选，编码和文字容量需容纳资源中的数据；示例采用 `SessionConfig{4, 1, 1, 64, 64, 32}`。这些所有者必须晚于会话销毁，下面的控制对象又必须先于会话销毁。

```cpp
#include <hanime/candidate_session.hpp>
#include <string>

namespace docs_core {
using namespace hanime;
struct Frame { std::string text; SelectionHandle choice; OutputStatus output{OutputStatus::Pending}; };

// session 已完成资源校验和文字准入，且比这个控制对象活得更久。
class FirstCandidate {
 public:
  explicit FirstCandidate(CandidateSession& session) : session_(session) {}
  Frame frame;

  // 完整编码输入事件：先查身份，再按需取字。这里明确禁用个人排序。
  SessionStatus start(std::string_view code, MountEpoch epoch) {
    if (phase_ != Phase::Idle) return SessionStatus::Busy;
    frame = {};
    status_ = session_.begin_input(code, epoch, 1, 0, ScoringMode::BaseOnly);
    if (status_ == SessionStatus::Pending) phase_ = Phase::Rank;
    return status_;
  }

  // 调度循环：每次只推进一步；返回 Ready 后才能显示 frame。
  SessionStatus advance(const WorkBudget& budget) {
    if (phase_ == Phase::Idle) return status_;
    status_ = session_.tick(budget).status;
    if (status_ == SessionStatus::Pending || status_ == SessionStatus::WaitingState)
      return status_;
    if (status_ != SessionStatus::StagingReady) return stop(status_);
    if (phase_ == Phase::Rank) {
      status_ = session_.publish(session_.staging());
      if (status_ != SessionStatus::Ready) return stop(status_);
      status_ = session_.request_text(session_.published(), 0, 1);
      if (status_ != SessionStatus::Pending) return stop(status_);
      phase_ = Phase::Text;
      return status_;
    }
    // 取字完成更新的是已发布页，不需要再次 publish。
    Frame next;
    bool order_stale = false;
    status_ = session_.present([&](const CandidateViewPage& page) {
      order_stale = page.order_stale;
      if (page.size) {
        next.output = page.items[0].output;
        if (!order_stale && output_has_text(next.output)) {
          next.text = page.text(0);
          next.choice = session_.selection(0);
        }
      }
    });
    if (status_ == SessionStatus::Ready && order_stale)
      return stop(SessionStatus::NeedsRankRestart);
    if (status_ == SessionStatus::Ready) frame = next;
    return stop(status_);
  }

  // 选择事件：传入用户实际看到的那一帧，不能临时换成新页的同一位置。
  CommitResult choose(const Frame& shown) { return session_.confirm(shown.choice); }
  // 调度循环：Preparing 之后持续推进，Accepted 才表示接收成功。
  CommitResult advance_commit(const WorkBudget& budget) {
    auto result = session_.commit_result();
    if (result.status == CommitStatus::Preparing || result.status == CommitStatus::CallingSink)
      session_.tick(budget);
    return session_.commit_result();
  }
 private:
  enum class Phase { Idle, Rank, Text };
  CandidateSession& session_;
  Phase phase_{Phase::Idle};
  SessionStatus status_{SessionStatus::Idle};
  SessionStatus stop(SessionStatus status) { phase_ = Phase::Idle; return status_ = status; }
};
} // namespace docs_core
```

输入事件调用 `start("yuanxingji", 1)`；这里的 `1` 必须就是资源的挂载代次。调度器以 `WorkBudget{1, 17, 3}` 调用 `advance()`，仅在 `Pending` 或 `WaitingState` 时继续，`Ready` 后复制 `frame` 交给显示。没有可输出文字时，帧中没有可确认的选择，应显示相应输出状态。

用户选择时将那份帧交给 `choose()`。返回 `Preparing` 后调度 `advance_commit()`，直到 `Accepted` 或其他终态。若中途开始了新输入，再确认旧帧会得到 `Stale`：丢弃旧帧并刷新候选，不能自动确认新页。查询返回 `ReadError` 等错误时停止推进并处理资源问题；提交返回 `RetryableNoEffect` 或 `UnknownOutcome` 时按后面的规则处理，不自动重放。这个精简控制对象不处理翻页、输入取消或重试，完整调用可沿 `host_minimal` 的 `query()`、`display()`、`submit()` 阅读。

## 翻页、取消与过期

`request_range(direction, q, text_count)` 接受 `RangeDirection::First`、`After` 或 `Before`，继续使用当前逻辑输入。`restart_first(q, text_count, mode)` 重新从第一页排序；`resume_range()` 恢复被中断的那一个范围，不产生新的输入。操作同样通过 `tick()` 推进、再发布。

保存 `input_handle()` 后可用 `cancel_input(handle)` 取消该次输入。句柄关联的是创建它的输入；它不会因为保存得更久就适用于新查询。

| 会话结果 | 应如何处理 |
| --- | --- |
| `Pending`、`WaitingState` | 稍后继续推进；等待个人状态时也要给状态调度器工作机会 |
| `StagingReady` | 发布当前暂存页；完成按需取字后读取当前页 |
| `NoMatches`、`RangeExhausted` | 显示无匹配或翻页已到边界，保留应用需要的原始编码 |
| `NeedsRankRestart` | 当前排序依赖已变化，重新从第一页建立当前排序 |
| `Busy`、`NotReady` | 等待前一步完成，或补齐初始化；不要同时开始新操作 |
| `Stale`、`Cancelled` | 废弃旧结果，回到当前输入上下文 |
| `InvalidRequest`、`InvalidBudget`、`CapacityExceeded` | 修正参数或容量后再发起合法请求 |
| `ReadError`、`AllocationFailure` | 保留错误，停止使用该次结果，检查存储或内存配置 |

## 确认提交与重试

提交接口同样位于 `<hanime/candidate_session.hpp>`：

| 完整类型签名 | 返回值与作用 |
| --- | --- |
| `CommitResult confirm(const SelectionHandle& selection)` | 开始提交或返回该选择的已有结果 |
| `CommitResult commit_result() const` | 观察当前事务的状态、输出、学习通知和重试许可 |
| `CommitResult retry(const RetryPermit& permit)` | 使用一次允许的重试许可推进，没有许可不能自行重放 |
| `CommitResult cancel_commit(const CommitHandle& transaction)` | 取消指定事务，保留已经发生的结果 |

从当前显示页取得 `selection(slot)`，再调用 `confirm(selection)`。合法选择通常返回 `CommitStatus::Preparing`；继续 `tick(budget)` 并查看 `commit_result()`，直到出现最终结果。不要自己用屏幕上的文字代替引擎解析出的提交内容。

| `CommitStatus` | 含义与处理 |
| --- | --- |
| `Preparing`、`CallingSink` | 事务仍在进行；不要开始第二次提交或递归调用 |
| `Accepted` | 接收器已接受完整文字。重复确认已接受的选择不会再次写入 |
| `RetryableNoEffect` | 这次没有产生文本效果；排除原因后使用返回的 `RetryPermit` 调用 `retry()` |
| `UnknownOutcome` | 接收回调异常等原因使效果无法确定；禁止自动重放，先核对应用状态 |
| `OutputRestricted` | 当前输出形式不允许提交；换合法形式或取消，不能绕过限制拼接文字 |
| `Stale`、`Cancelled`、`Busy` | 分别重新选择当前候选、结束取消流程，或等待活动事务 |

`CommitResult::output` 给出文字解析情况，`learning` 给出学习通知状态，两者与正文是否接收是不同维度。`CommitHandle::learning_ticket()` 用于观察学习，不能用来重试正文。数据保存责任见[选词学习](@doc/learning)。

## 预算与生命周期

`WorkBudget{max_read_calls, max_read_bytes, max_work_items}` 限制每一步允许使用的读取次数、字节数和逻辑工作项。`SessionStep::cost` 返回本次实际消耗，可用于宿主统计。可选的 `stop_requested` 回调只在这次调用期间借用，不能指向已经失效的上下文。

引擎不负责给你的界面分配线程。回调用时也由宿主测量。缓冲区、页数和预算需要在目标硬件上共同验证；增加每步预算可能让一次界面响应等待更久。
