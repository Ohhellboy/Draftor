# 选词学习

**Draftor Alpha 3 已实际使用共享选词学习。** 当用户明确选择符合条件的候选，并且正文接收成功后，引擎记录对应词条的个人次数。之后查询同一词条时，这份记录可以影响候选排序；全拼、小鹤双拼和五笔86共用它。

三个 Host 示例主要说明基本输入、候选和提交，都关闭了学习。它们不是完整的 Alpha 3 学习接入示例。下面分别说明引擎已有能力、Draftor 的集成，以及其他宿主还需要补齐的部分。

## 个人记录绑定词条身份

记录以 `RegistryNamespaceId` 和完整的 `StableLexemeKey` 关联词条，而不是按屏幕文字或某种输入编码计数。**稳定词条键**用来在匹配的词条体系中识别同一个词；`RuntimeLexemeRef` 只是当前资源中的运行期位置，不能当作跨资源版本保存的个人身份。

这意味着同一个词通过三种方案命中时，可以共享次数；文字相同但词条身份不同的两个候选，则不能合并。更换命名空间或不兼容的资源后，需要显式处理状态不兼容，不能把旧次数按文字猜测着迁移。

`BoundedUserState` 是当前计数的统一所有者，同时提供排序读取和提交后的学习通知。`StateV3` 在文件端口之上维护持久状态。派生的个人索引用于快速定位受影响的词条和方案行，不是另一份独立计数来源。

## 什么选择会被记录

Alpha 3 的实际构建启用了选词资格检查：用户明确选择**当前候选序列中并非首选的词**，才会产生可用于学习的选择凭据。这里的首选是全局排序中的第一个候选，不是每一页的第一个位置。

Draftor 的 `CandidateWindow` 在检查显示帧、输入修订、候选身份和当前排序后，调用 `Composition::select_explicit()`，并传入候选的全局位置。后者在位置大于零时附带 `SelectionLearningProof`。自动选择首选项不会凭空产生学习凭据。

组合提交只记录最终仍保留、资格有效的成员。同一选词动作与同一词条不会因重复处理计数多次；不同的真实选词动作仍分别处理。编辑或重新验证会清除需要重新确认的资格。正文被接收器接受后才发出学习通知；查找与重命名目标不记录正文学习。

这些行为可从以下对应源码核对：[产品候选选择](@source/integration/product/src/candidate_window.cpp)、[选择资格的创建](@source/integration/product/dependencies/extension/src/composition.cpp)、[组合成员筛选与提交](@source/integration/product/dependencies/core/src/composition_commit.cpp)、[输入目标决定](@source/integration/product/dependencies/extension/src/submission.cpp)。

> 当前公开的 `Composition::select()` 不签发这项资格；`select_explicit()` 是私有接口，只向 Draftor 的候选窗口友元开放。普通宿主无法仅凭公开选择接口完整复现 Alpha 3 的事件资格。这是现有接入边界，不是学习能力缺失。

## 次数怎样影响排序

引擎支持基础排序与结合真实个人次数的排序。Alpha 3 产品选择 `score_policy.hpp` 中 `ARev1PolicyRevision` 所指的规则：先变换词条的基础权重，再增加有上限的个人次数奖励，并保留原始权重用于细化顺序。

在这条规则下，次数 `0` 和 `1` 没有个人奖励；从 `2` 起随次数增加奖励，到 `49` 及以上时奖励不再增长。奖励封顶不等于记录本身只允许保存到 `49`。即使有个人奖励，词条仍需匹配当前编码和输出条件；共享记录也不保证不同方案的候选顺序完全相同。

Core 中还保留较早的权重规则。不能因为 `RankRule` 的默认值或 `lambda` 字段存在，就把 Alpha 3 的产品策略概括成“基础分加次数乘系数”。实际配置见 [LearningTrial::configure](@source/integration/product/src/learning_trial.cpp)、[排序规则](@source/integration/product/dependencies/core/include/hanime/score_policy.hpp)和[Alpha 3 构建配置](@source/integration/product/scripts/build_s3.py)。

## 打开与调度个人状态

下面是已有公开能力的调用顺序，完整的独立学习示例尚未提供。先由宿主准备两个独立、可写的个人状态文件路径，再实现 `StateFilePort`；在 POSIX Host 上可参考 [HostStateFiles](@source/integration/product/dependencies/core/include/hanime/host_state_files.hpp)。不要把状态文件放进只读词库包，也不要混用词库的 `shared.hns` 和同后缀的个人状态文件。

| 顺序或接口 | 参数、返回值与责任 |
| --- | --- |
| `StateV3(port, config)` | `port` 必须比状态对象更长寿；`config` 固定命名空间、状态 lineage、单次 I/O 和文件容量等。lineage 是状态存储谱系标识，应由宿主固定维护 |
| `begin_open(create_if_both_missing)` | 默认不创建；显式允许后，也只有两个文件都不存在时才创建新存储。其他损坏或不兼容状态不能当作新用户清空 |
| `advance_open(state_budget)` | 推进到 `StateStatus::Ready` 后才能建立可用计数源；每步返回状态、I/O 成本与是否需要更多预算 |
| `BoundedUserState(state, namespace, config)` | 为同一份状态创建一个计数和学习所有者，检查 `valid()`。后端必须比它更长寿，它又必须比会话更长寿 |
| `begin_count(view, key)` → `advance_count(ticket, budget)` | 取得当前视图后读取完整稳定键的真实次数。返回 `Found`、`NotFound`、`Pending`、`Unavailable` 或 `Stale` |
| `advance_state(state_budget)` | 推进已接受学习意图、脏记录写入和维护。应用需要持续提供调度机会 |
| `foreground_advanced()`、`background_due()` | 在前台步骤间记录推进情况，判断应给后台的工作机会；它不是毫秒级保存保证 |

`StateFilePort` 提供按文件编号进行的 `open`、`size`、`read`、`write`、`sync`、`truncate` 和 `close`。读写结果要报告实际字节数，不能隐藏短读短写，也不能在一次接口调用中偷偷进行无限重试。`current()` 用于不做 I/O 的生命周期检查，TF 卡被移走时应让缓存访问也能发现失效。

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
| `CloseUnsaved`、`Corrupt`、`TailUncertain`、`Incompatible` | 不能宣告保存成功；保留现场，报告失败或不兼容，避免自动覆写状态 |

应用准备退出或允许断电时，先停止产生新的学习事件，处理活动提交，调用 `BoundedUserState::begin_close()`，然后按预算推进 `advance_close()` 直到 `ClosedSaved`。析构不会代替你刷盘。打开、写入与关闭过程中出现的失败需要结合实际存储处理，不能仅凭“析构已经执行”判断安全。

Draftor 把这套生命周期与正文保存合并安排：学习开启时，完成当前输入后按 `Ctrl+F6` 请求保存正文和学习并暂停学习；只有正文、个人状态及相关后台工作均达到它要求的条件，才显示可以断电的提示。这是草稿机的界面选择，你的程序可使用菜单、按钮或其他退出流程。对应代码为 [daily_learning.cpp](@source/integration/product/src/daily_learning.cpp)。

## 正式资源与个人文件

Alpha 3 的 format4 共享词库与方案索引来自既有资源包，另配 format 301 的 `relations.bgr` 和验证页 `relations.hnv`。它们记录通用词条与方案行的位置关系，独立于个人数据，也不会包含某个用户的选词习惯。

产品创建两个权威状态文件 `a.hns`、`b.hns`，还使用 `a.hns.relations` 等派生个人索引文件。准确路径由 [r2_storage.hpp](@source/integration/product/src/r2_storage.hpp)和[平台初始化](@source/integration/product/platform/device_main.cpp)结合构建配置确定。独立宿主应选自己的固定路径，不照抄源码中的历史设备目录名。

派生索引依赖权威记录及匹配的通用资源。升级资源、卡片不可用、状态损坏时，要沿用对应的有效性与恢复检查；不要删除权威文件来“修好”索引。资源下载和身份核对见[词库与配套资源](@doc/resources)。

## 独立接入还缺少什么

引擎已经提供真实计数、通知凭据、分步持久化、记录观察和保存关闭接口；Draftor 已把它们用于三种输入方案，并提供了产品所需的选择资格、个人索引、文件适配和调度。

目前没有一个独立 Host 示例同时覆盖 Alpha 3 的真实选词资格、跨方案记录、重开后读取与保存失败处理。尤其在启用 Alpha 3 资格检查的构建下，普通 `select()` 不会产生所需凭据；直接省略产品构建开关会采用不同的记录筛选行为，也不能算复现 Alpha 3。

若要将学习带入另一个宿主，下一步应先明确可公开的事件资格接口与调用责任，再补一个使用匹配构建配置、独立文件端口的学习示例。这个示例应覆盖显式非首选、自动首选不记录、跨方案共享、重复确认不重复计数、关闭重开和失败恢复。现有产品代码可作为依据，但不能把私有友元调用包装成已经公开的通用接入承诺。

参考：[BoundedUserState](@source/integration/product/dependencies/core/include/hanime/bounded_user_state.hpp) · [StateV3](@source/integration/product/dependencies/core/include/hanime/state_v3.hpp) · [学习凭据](@source/integration/product/dependencies/core/include/hanime/composition_commit.hpp) · [产品调度](@source/integration/product/src/learning_trial.cpp) · [产品输入与提交](@source/integration/product/src/device_ime.cpp)。
