# 选词学习

**Draftor Alpha 3 已实际使用共享选词学习。** 引擎记录符合条件的选词次数，并在之后的查询中用它调整候选排序。全拼、小鹤双拼和五笔86共用同一份个人记录。

## 什么选择会被记录

用户明确选择**当前候选序列中并非首选的词**，并且应用成功接收正文后，这次选择才可能计入个人记录。这里的首选是全局排序中的第一个候选，不是每一页的第一个位置。选择首选项或由程序自动选择首选项，都不产生这项学习资格。

一次输入可以先选中多个词，再将它们作为一个组合提交。提交时只记录最终仍保留、学习资格有效的成员；同一选词动作与同一词条只计一次，不同的选词动作分别处理。查找与重命名目标不记录正文学习。

## 记录怎样跨方案使用

个人次数跟随词条身份。使用匹配的资源时，同一个词通过全拼、小鹤双拼或五笔86命中，都可以使用已有次数参与排序。切换输入方案不需要再为这个词建立一份记录。

文字相同但身份不同的两个词条分别计数。更换资源时，需要核对词条身份体系是否兼容；按屏幕文字匹配旧记录会混淆这些词条。三种方案各自的编码和候选集合仍然不同，因此共享次数后的候选顺序也可能不同。

## 在自己的程序中接入学习

引擎已有真实次数读取、提交后的学习通知、分步持久化和保存状态观察接口。Draftor 在此基础上接入了候选选择事件、个人索引、文件读写和前后台调度。其他宿主需要安排以下工作：

1. **确认有效的选词事件。** 将用户操作与当时显示的候选、输入修订及排序关联，再确定是否为明确选择非首选项。
2. **提供共享的个人存储。** 为三种方案使用同一个计数所有者，提供两个可写的个人状态文件，并配对正式资源与通用学习索引。
3. **持续推进和保存。** 在事件循环中安排记录与后台写入；退出前显式关闭，处理存储不可用、容量不足和保存失败。

当前公开的 `Composition::select()` 不签发 Alpha 3 所需的选择资格。签发入口 `select_explicit()` 是私有函数，只向 Draftor 的候选窗口友元开放。因此，其他宿主接入同样的学习规则前，还需要约定并提供可公开的资格入口及调用责任。

**目前尚无完整独立学习示例。** 三个 Host 示例展示基本输入、候选和提交，均关闭了学习。后续独立示例需要使用匹配的构建配置，覆盖显式非首选、自动首选不记录、跨方案共享、重复确认不重复计数、关闭重开和保存失败处理。省略 Alpha 3 的资格检查构建开关会改变记录筛选行为。

## 选词资格与输入修改

Draftor 的 `CandidateWindow` 在检查显示帧、输入修订、候选身份和当前排序后，调用 `Composition::select_explicit()`，并传入候选的全局位置。位置大于零时，选中的成员会携带 `SelectionLearningProof`，用于标识这次有资格参与学习的选择。

编辑输入、重新绑定资源和重新验证成员，对资格的影响不同：

| 操作 | 对成员与学习资格的影响 |
| --- | --- |
| `edit_raw()` | 与编辑区间重叠的成员被移除；位于编辑结束位置及其后的成员调整范围、标为待验证，并清除资格。不受影响的前缀成员保留原有资格。`DirectExact` 路径在编辑时不保留原有成员，包括在编码末尾追加的情况 |
| `rebind()` | 成功重新绑定资源后，将所有保留成员标为待验证，并清除它们原有的资格 |
| `revalidate()` | 重新检查成员的编码映射、词条身份和输出，保留成员携带的选择凭据。本身不清空资格，也不会创建或重新签发用户选择资格；已被编辑或 `rebind()` 清除的资格不会因此恢复 |

在空闲状态下，`edit_raw(begin, begin, {})` 是空操作，直接返回 `Ready`，不会修改成员。

最终是否记录，还取决于提交时成员与选择凭据是否匹配。学习通知在正文被接收器接受后发出。相关实现见 [composition.cpp](@source/integration/product/dependencies/extension/src/composition.cpp)、[产品候选选择](@source/integration/product/src/candidate_window.cpp)、[组合成员筛选与提交](@source/integration/product/dependencies/core/src/composition_commit.cpp)和[输入目标决定](@source/integration/product/dependencies/extension/src/submission.cpp)。

## 个人记录绑定词条身份

接口以 `RegistryNamespaceId` 和完整的 `StableLexemeKey` 关联个人记录。前者标识词条身份体系的命名空间；后者是**稳定词条键**，用于在这个体系中识别同一个词。`RuntimeLexemeRef` 只是当前资源中的运行期位置，不能当作跨资源版本保存的个人身份。

更换命名空间或不兼容的资源后，宿主需要处理状态不兼容并保留原有记录。需要迁移时，先确定词条身份的对应关系。

`BoundedUserState` 是当前计数的统一所有者，同时提供排序读取和提交后的学习通知。`StateV3` 在文件端口之上维护持久状态。派生的个人索引用于快速定位受影响的词条和方案行，计数仍来自同一份个人状态。

## 打开与调度个人状态

先由宿主准备两个独立、可写的个人状态文件路径，再实现 `StateFilePort`；在 POSIX Host 上可参考 [HostStateFiles](@source/integration/product/dependencies/core/include/hanime/host_state_files.hpp)。个人状态应存放在只读词库包之外，与词库的 `shared.hns` 分开管理。随后按以下顺序打开并推进状态：

| 顺序或接口 | 参数、返回值与责任 |
| --- | --- |
| `StateV3(port, config)` | `port` 必须比状态对象更长寿；`config` 固定命名空间、状态 lineage、单次 I/O 和文件容量等。lineage 是状态存储谱系标识，应由宿主固定维护 |
| `begin_open(create_if_both_missing)` | 默认不创建；显式允许后，也只有两个文件都不存在时才创建新存储。损坏或不兼容时保留现有文件并处理错误 |
| `advance_open(state_budget)` | 推进到 `StateStatus::Ready` 后才能建立可用计数源；每步返回状态、I/O 成本与是否需要更多预算 |
| `BoundedUserState(state, namespace, config)` | 为同一份状态创建一个计数和学习所有者，检查 `valid()`。后端必须比它更长寿，它又必须比会话更长寿 |
| `begin_count(view, key)` → `advance_count(ticket, budget)` | 取得当前视图后读取完整稳定键的真实次数。返回 `Found`、`NotFound`、`Pending`、`Unavailable` 或 `Stale` |
| `advance_state(state_budget)` | 推进已接受学习意图、脏记录写入和维护。应用需要持续提供调度机会 |
| `foreground_advanced()`、`background_due()` | 在前台步骤间记录推进情况，判断应给后台的工作机会。保存耗时仍需在目标存储上测量 |

`StateFilePort` 提供按文件编号进行的 `open`、`size`、`read`、`write`、`sync`、`truncate` 和 `close`。读写结果要报告实际字节数，包括短读或短写；单次调用的重试次数需要有上限，以便宿主继续调度其他工作。`current()` 用于不做 I/O 的生命周期检查，TF 卡被移走时应让缓存访问也能发现失效。

`NotFound` 表示真实存储中没有该键，不能用于表示缓存未命中或存储离线；后两者需要等待或报告不可用。调用方应根据状态决定继续个人排序、刷新排序或明确转为基础模式。

## 记录结果与保存时机

接受正文与保存个人记录是两个不同结果。Core 在接受后通过 `CommitCountSource::notify_committed()` 或 `CompositionCountSource::notify_composition()` 通知同一个计数所有者；不要再手工补发一次通知或重放正文。组合的 `learning_ticket()` 可读取汇总和每个成员的结果；单候选事务可用 `observe_learning()` 观察对应凭据。

| 学习或状态结果 | 对应用的含义 |
| --- | --- |
| `Pending` | 已有待处理意图，继续安排状态工作 |
| `RecordedDirty` | 已计入当前个人状态，但尚未确认持久保存 |
| `Persisted`、成员的 `persisted` | 对应记录已得到存储确认；正文保存仍由应用负责 |
| `PausedUnavailable`、`PausedCapacity` 等 | 此次记录因不可用或容量等原因暂停；显示实际状态，不能再次插入已经接受的正文 |
| `ClosedSaved` | 显式关闭已完成，可作为个人状态保存完成的依据 |
| `CloseUnsaved`、`Corrupt`、`TailUncertain`、`Incompatible` | 保存未完成、存储损坏或状态不兼容；保留文件，报告具体结果，避免自动覆写状态 |

应用准备退出或允许断电时，先停止产生新的学习事件，处理活动提交，调用 `BoundedUserState::begin_close()`，然后按预算推进 `advance_close()` 直到 `ClosedSaved`。保存应通过这条显式关闭流程完成；仅销毁对象不足以确认已持久化。打开、写入与关闭过程中出现的失败，需要结合实际存储处理。

Draftor 把这套生命周期与正文保存合并安排：学习开启时，完成当前输入后按 `Ctrl+F6` 请求保存正文和学习并暂停学习；只有正文、个人状态及相关后台工作均达到它要求的条件，才显示可以断电的提示。这是草稿机的界面选择，你的程序可使用菜单、按钮或其他退出流程。对应代码为 [daily_learning.cpp](@source/integration/product/src/daily_learning.cpp)。

## 次数怎样影响排序

引擎支持基础排序与结合真实个人次数的排序。Alpha 3 产品选择 `score_policy.hpp` 中 `ARev1PolicyRevision` 所指的规则：先变换词条的基础权重，再增加有上限的个人次数奖励，并保留原始权重用于细化顺序。

在这条规则下，次数 `0` 和 `1` 没有个人奖励；从 `2` 起随次数增加奖励，到 `49` 及以上时奖励不再增长。`49` 是奖励封顶的起点，个人次数仍可继续累积。词条仍需匹配当前编码和输出条件。

Core 中还保留较早的权重规则。接入 Alpha 3 学习时，应按产品配置设置 `RankRule`，使用上述规则。实际配置见 [LearningTrial::configure](@source/integration/product/src/learning_trial.cpp)、[排序规则](@source/integration/product/dependencies/core/include/hanime/score_policy.hpp)和[Alpha 3 构建配置](@source/integration/product/scripts/build_s3.py)。

## 正式资源与个人文件

Alpha 3 的 format4 共享词库与方案索引来自既有资源包，另配 format 301 的 `relations.bgr` 和验证页 `relations.hnv`。它们记录通用词条与方案行的位置关系，独立于个人数据，也不会包含某个用户的选词习惯。

产品将真实个人记录保存在两个状态文件 `a.hns`、`b.hns` 中，还使用 `a.hns.relations` 等派生个人索引文件。准确路径由 [r2_storage.hpp](@source/integration/product/src/r2_storage.hpp)和[平台初始化](@source/integration/product/platform/device_main.cpp)结合构建配置确定。独立宿主应为这些文件配置自己的固定路径。

派生索引依赖真实个人记录及匹配的通用资源。升级资源、卡片不可用或状态损坏时，需要执行相应的有效性与恢复检查。恢复派生索引时保留个人状态文件，避免丢失选词记录。资源下载和身份核对见[词库与配套资源](@doc/resources)。

参考：[BoundedUserState](@source/integration/product/dependencies/core/include/hanime/bounded_user_state.hpp) · [StateV3](@source/integration/product/dependencies/core/include/hanime/state_v3.hpp) · [学习凭据](@source/integration/product/dependencies/core/include/hanime/composition_commit.hpp) · [产品调度](@source/integration/product/src/learning_trial.cpp) · [产品输入与提交](@source/integration/product/src/device_ime.cpp)。
