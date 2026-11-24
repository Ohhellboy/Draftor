# 拼音与组合输入

`Composition` 管理原始编码、候选和已经选择的词；`Submission` 把这些词组成的完整文字交给应用。使用这组接口时不需要同时驱动一个 `CandidateSession`。可运行示例见 [host_composition/main.cpp](@source/ime/bgime/examples/host_composition/main.cpp)。

## 创建输入对象

`Composition(index, shared, rules, config)` 借用已打开且校验完成的方案索引和共享资源。全拼选择 `SchemeRules::FullPinyin`，小鹤双拼选择 `SchemeRules::R1Xiaohe`。索引的方案名必须与规则匹配。

另一个构造形式接受 `SchemePolicy`，用于明确给出规则和资源方案名。`DirectExact` 代表直接编码查询，不运行拼音切分规划；Draftor 的五笔产品集成使用这个入口。当前可选规则为全拼、小鹤双拼和直接编码查询。

`Config` 中的 `raw_bytes`、`members`、`page_items` 分别约束原始编码字节数、已选词数量和页内候选数量。`word_bytes`、`name_bytes`、`text_bytes` 与 `request_bytes` 约束输出及请求缓冲区。容量超限会返回错误，不会替你截断中文。`learning_counts` 是借用的计数源，`apply_learning` 控制是否用它参与排序；设置方法与产品边界见[选词学习](@doc/learning)。

构造之后检查 `valid()`。资源对象和借用的计数源要晚于输入对象销毁；若使用 `StorageBackend` 自定义分配器，其分配和释放约定也必须配对。公开声明见 [extension.hpp](@source/integration/product/dependencies/extension/include/hanime_extension/extension.hpp)。

## 编辑编码并发起查询

| 调用 | 参数及结果 |
| --- | --- |
| `edit_raw(begin, end, replacement)` | 用 `replacement` 替换半开字节区间 `[begin, end)`。尾部插入时两个边界相等；退格可替换相应区间为空串。成功会复制编码并改变输入修订；活动操作期间可返回 `Busy` |
| `query_next(output, context_revision)` | 为当前未选部分规划下一段候选；`output` 默认为默认输出形式，`context_revision` 由宿主用于区分输出上下文。通常返回 `Working` |
| `operation()` | 取得刚发起操作的句柄；后续 `tick()` 与 `cancel()` 使用同一个句柄 |
| `tick(operation, budget)` | 在指定预算内推进操作，返回 `Step`，其中有状态、Core 工作量、局部工作量及输出状态 |
| `cancel(operation)` | 取消这个操作，让宿主可以继续编辑；旧句柄不能取消替代它的新操作 |
| `cancel_input(input_id)` | 明确放弃指定输入会话；不同于只取消当前查询 |

`RawInput::raw` 是借用的原始编码视图，`InputStamp` 标识它所属的输入和修订。跨事件保存前复制字符串；延后使用时间戳或规划结果时，用 `input_is_current()`、`plan_is_current()` 重新检查。

输出上下文变化后，应取消当前操作，以新的 `context_revision` 重新发起查询。不要让仍引用旧上下文的规划结果授权一次新的选择。

## 取得候选、文字与选择

查询到 `Status::PageReady` 后，调用 `page()` 读取候选数组、范围和 `has_more`。`page().items` 包含 `RankItem` 身份和排序信息，不包含可直接显示的候选文字。

先取得 `selection(slot)`，再调用 `read_text(selection, output)`，通过当次 `operation()` 与 `tick()` 推进到完成，读取 `last_text()`。它是借用的字符串视图，应在下一次修改前复制。选择候选则调用 `select(selection, output)`，并继续推进操作，使这个词进入已选部分。

`snapshot()` 返回原始编码、已选文字、成员范围、焦点和组合状态。所有页面、快照和文字视图都借用输入对象的缓冲区，只在下一次修改前有效。旧页号或旧候选位置不能替代当前选择句柄。

| `CompositionState` | 对宿主的含义 |
| --- | --- |
| `Idle` | 没有已选组合；仍需依据原始编码判断是否有输入 |
| `PartialSelected` | 已有部分词，但仍有未选编码，继续查询下一段 |
| `NeedsRecheck` | 输入或资源等上下文发生变化，已选部分需要重新验证 |
| `AllSelected` | 整个原始编码已被有效成员覆盖，可以准备组合提交 |

`request_page(direction)` 翻阅当前范围的候选，`retract_last()` 撤回最后一个已选词，`reopen_segment(index)` 重新打开指定成员，`revalidate()` 重新检查已选意图。活动操作之间需要遵守同样的串行和句柄约定。`ranking_is_current()` 可在延后的 UI 选择前检查排序依赖是否仍有效。

公开的 `select()` 完成选择，但不会签发 Alpha 3 产品的显式非首选学习资格。记录学习还需要满足[选词学习](@doc/learning)中的资格与提交条件。

## 准备并提交整个组合

构造 `Submission(composition, shared, sink, config, counts)`。`counts` 是可选的组合学习通知源；默认配置禁用学习。`CompositionConfig` 限定最大成员数、形式名称、单词文字和总文字容量。`Submission` 必须先于其借用的输入对象、共享资源和最终接收器销毁。

| 顺序与调用 | 参数、返回值和处理 |
| --- | --- |
| `prepare(target)` | 默认目标为 `InputTarget::Body`；另有 `Find`、`Rename`。捕获当前 `AllSelected` 组合，不做 I/O。成功返回 `core.status == Preparing` 及 `SubmissionHandle`；未完整选择返回 `InvalidRequest` |
| `advance(handle, budget)` | 解析并验证完整输出。反复推进到 `Ready`，保留这一次返回的事务信息；错误时按 `core.status` 处理 |
| `prepared(handle, callback)` | 在同步回调内查看 `PreparedCompositionView`，包含成员及完整 UTF-8 文字；视图只在回调中有效，不会调用文本接收器 |
| `confirm(handle)` | 确认当前准备好的事务，由接收器接受一次完整组合。检查 `core.status`，成功为 `Accepted` |
| `retry(handle)` | 仅在可重试且确定没有文本效果时调用；成功开始重试后使用返回的新句柄，旧尝试的句柄不能继续控制新尝试 |
| `cancel(handle)`、`close()` | 前者取消指定当前尝试，后者关闭整个适配器；都不能消除已经接受或效果不确定的历史事实 |

准备与就绪阶段会暂时占用输入对象，避免组合在提交期间被改动。正常接受后输入被清空。`UnknownOutcome` 会隔离这个输入会话，即使销毁适配器也不能把“可能已经写入”的事实变成可安全重放。

`prepare()` 的目标也参与学习决定：`Find` 和 `Rename` 不进行正文学习。`set_record_enabled(bool)` 只影响未来事务，已经捕获的记录决定和后台工作不会因此被删除或关闭。

源码：[submission.hpp](@source/integration/product/dependencies/extension/include/hanime_extension/submission.hpp) · [submission.cpp](@source/integration/product/dependencies/extension/src/submission.cpp) · [Core 组合结果类型](@source/integration/product/dependencies/core/include/hanime/composition_commit.hpp)。

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
