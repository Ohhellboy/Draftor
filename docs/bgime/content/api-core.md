# 资源、候选与提交

本页说明从打开资源到提交一个候选的 Core 路径。完整的可运行调用见 [host_minimal/main.cpp](@source/ime/bgime/examples/host_minimal/main.cpp)。需要逐键拼音或多个词组合时，在这些资源之上使用 [Composition 与 Submission](@doc/api/composition)。

## 资源读取与打开

`ResourceReader` 的 `size()` 返回文件字节数，`read(offset, destination, byte_count)` 按偏移写入调用者提供的缓冲区，完整读取成功才返回 `true`。`healthy()` 是不进行 I/O 的有效性检查。更复杂的读者可以实现 `read_budgeted()`：它返回 `Pending`、`Ready` 或 `Error` 及实际 `WorkCost`；在 `Ready` 前，目标缓冲区要继续有效且不能展示给用户。

电脑示例使用 [HostResourceFile](@source/integration/product/dependencies/core/include/hanime/host_resource_file.hpp)。构造时传入路径、单次读取上限及统计对象，并检查 `valid()`；统计对象也需要活得比文件读者更久。

| 调用 | 参数含义 | 返回值与下一步 |
| --- | --- | --- |
| `PagedSharedResource::open(reader, expected_namespace, expected_bundle_id, mount_epoch, config, error)` | 持有的资源读者、预期词条命名空间、预期包 ID、挂载代次、缓存配置及错误字符串指针。包 ID 为 `0` 时允许发现当前不可变资源包的 ID | `false` 时读取 `error` 并停止；`true` 后取得 `validation_cursor()`，用 `advance_validation(cursor, budget)` 校验到 `Ready` |
| `PagedSchemeIndex::open(reader, expected_registry_namespace, expected_shared_bundle_id, maximum_runtime_ref, config, error)` | 方案读者；命名空间和包 ID 必须与共享资源一致；最大运行期引用来自共享资源的词条数量 | 成功后继续 `advance_validation(budget)`，不能在 `InProgress` 时查询 |
| `ready()`、`info()` | 观察资源是否可用及其元数据 | 校验成功后核对 `info().scheme_id` 与所需输入方案一致 |

**挂载代次** `MountEpoch` 标识当前资源挂载周期。更换存储或资源时由宿主管理新代次及读者有效性，不能把旧候选带进新挂载。示例固定使用 `1`，因为它的一次进程运行只使用不可变测试文件。

校验只在 `InProgress` 时继续推进。其他非 `Ready` 状态需要处理错误；预算不足时调整预算，资源损坏或不匹配时更换正确资源，不要把失败当成空词库。共享校验光标与创建它的当前资源激活绑定。

源码：[ResourceReader 与 PagedSchemeIndex](@source/integration/product/dependencies/core/include/hanime/core.hpp) · [PagedSharedResource](@source/integration/product/dependencies/core/include/hanime/paged_shared_resource.hpp)。

## 创建候选会话与文字准入

`CandidateSession(index, shared, counts, sink, config)` 借用方案索引、共享资源、计数及提交通知源、文本接收器。上述对象都必须比会话活得更久。创建后检查 `valid()`，再完成文字准入：`begin_text_access()` → 多次 `advance_text_access(budget)` → `TextAccessStatus::Ready`。

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

按下面顺序组织一次完整编码查询：

1. `begin_input(code, epoch, q, text_count, mode)` 开始新输入。`code` 被复制到有界缓冲区；`q` 是本页希望取得的候选数，必须大于零且不超过 `snapshot_slots`。`text_count` 不超过 `q`，为 `0` 时先只取候选身份。`mode` 是 `Exact` 或 `BaseOnly`。

   `epoch` 的类型是 `MountEpoch`，应与打开共享资源时使用的[挂载代次](#资源读取与打开)一致。同一次挂载内的多次查询可以复用这个值，不能按按键或查询次数递增；传入与当前资源不一致的代次会返回 `Stale`。

2. 返回 `Pending` 后反复调用 `tick(budget)`；到 `StagingReady` 时，调用 `publish(staging())` 发布这一页。
3. 如需补取文字，调用 `request_text(published(), start, count)`，其中 `start` 是从零开始的页内位置。继续 `tick()`，直到文字准备完成。
4. 用 `present(callback)` 读取当前页，并在回调中复制你需要保留的数据。返回 `Ready` 才表示此次展示使用了有效页面。

`Exact` 使用真实个人次数参与排序，`BaseOnly` 只用基础排序。个人状态不可用不能伪装为所有次数都为零；宿主可以明确选择基础模式，并在界面中如实表达状态。

`CandidateViewPage` 中的 `size` 是候选数，`has_more` 表示还有后续候选，`order_stale` 表示当前排序已过期。`text(slot)` 只在输出具有有效文字时返回非空视图。仅有 `RankItem` 或候选身份时，文字可能仍处于 `OutputStatus::Pending`。

> `CandidateViewPage` 的候选数组、文字池和 `string_view` 仅在同步 `present()` 回调期间有效。异步 UI 应复制文字，并把选择句柄与那一帧显示内容一起保存。

源码：[candidate_session.hpp](@source/integration/product/dependencies/core/include/hanime/candidate_session.hpp) · [输出状态](@source/integration/product/dependencies/core/include/hanime/bounded_output.hpp)。

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
