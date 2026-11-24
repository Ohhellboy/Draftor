# 接口导览

先按需要完成的任务找到入口，再查看相应的参数与调用顺序。这里整理的是 Alpha 3 对应头文件和实际示例中的推荐路径，不将每个实现类都列为宿主必须调用的接口。

所有源码链接固定到 `f18939f8880e2278c231739d553fbf983c19e0f6`。公开类型仍使用 `hanime`、`hanime_extension` 和 `hanime::wubi` 命名空间。

## 按任务查找

| 任务 | 类型或接口 | 参考页 |
| --- | --- | --- |
| 打开词库与编码索引 | `ResourceReader`、`PagedSharedResource`、`PagedSchemeIndex` | [资源、候选与提交](@doc/api/core) |
| 查询完整编码、翻页和取字 | `CandidateSession` | [资源、候选与提交](@doc/api/core) |
| 接收最终文字 | `TextCommitSink::commit()` | [接入自己的程序](@doc/integration) |
| 逐键编辑全拼和小鹤双拼 | `Composition`、`SchemeRules` | [拼音与组合输入](@doc/api/composition) |
| 一次提交多个已选词 | `Submission` | [拼音与组合输入](@doc/api/composition) |
| 编辑五笔码并驱动查询 | `Wubi86Decoder`、`DirectQueryBinding` | [五笔接口](@doc/api/wubi) |
| 读取个人次数、接收学习通知 | `CountSource`、`BoundedUserState` | [选词学习](@doc/learning) |
| 持久化个人状态 | `StateFilePort`、`StateV3` | [选词学习](@doc/learning) |

## 公开入口与内部实现

`include/hanime/` 和 `include/hanime_extension/` 包含公开声明，但并不代表所有声明都属于同一代接入路径。例如，`core.hpp` 中仍保留 `ImeSession`、`UserStateStore` 等旧式同步接口。本版教程采用三个 Host 示例实际使用的分步资源、候选与组合接口，不混用旧接口的状态和保存约定。

`ExactRanker`、`OutputReader`、`CompositionCommitter` 等较低层的公开类型服务于需要自己组合查询或提交过程的使用方。第一次接入请优先使用会话和组合适配器；不要同时驱动适配器及其持有的内部查询。

`integration/product/src/` 是 Draftor 产品集成。`CandidateWindow`、`DeviceIme`、`LearningTrial` 和 `PersonalIndex` 可以帮助理解 Alpha 3，但不是一个通用 SDK 的安装入口。其中 `Composition::select_explicit()` 是私有函数，由友元 `CandidateWindow` 调用，不能作为普通宿主的公开 API。这个区别直接影响[学习事件资格](@doc/learning)。

## 共通的调用约定

**串行调用。** 同一个资源或会话由一个调度者推进。显示与提交回调可以把后续事件加入队列，但不能递归 `tick()`、销毁对象或在回调内重用借用缓冲区。

**所有者先创建、后销毁。** 文件读者、共享资源、方案索引、计数源和接收器应活得比借用它们的会话更久。`Submission` 要在 `Composition` 之后创建、之前销毁。

**区分待完成与失败。** `Pending`、`Working`、`Preparing` 意味着还要推进。`Ready` 在不同类型中分别可能表示资源可用或提交已准备好；提交完成要检查 `Accepted`，不能只看名字中有 `Ready`。

**句柄不延长操作权限。** 可以保存一个选择或事务句柄，但新输入、重试、资源更换或排序变化后，它可能过期。`Stale` 是要求重新核对当前上下文的结果，不是应被忽略的偶发错误。

**字节长度不等于字符数。** 文本与编码边界中的长度、偏移和容量以字节为单位。C++ 的 `std::string_view` 通常借用别处的内存；只有明确说明复制的接口才可在返回后释放输入数据。

## 公开头文件索引

| 头文件 | 用途 |
| --- | --- |
| [core.hpp](@source/integration/product/dependencies/core/include/hanime/core.hpp) | 基础资源、预算、方案索引和文本接收器声明 |
| [paged_shared_resource.hpp](@source/integration/product/dependencies/core/include/hanime/paged_shared_resource.hpp) | 共享资源的打开、校验和生命周期 |
| [candidate_session.hpp](@source/integration/product/dependencies/core/include/hanime/candidate_session.hpp) | 候选页、选择与单候选提交 |
| [bounded_output.hpp](@source/integration/product/dependencies/core/include/hanime/bounded_output.hpp) | 文字准入、输出形式与输出状态 |
| [extension.hpp](@source/integration/product/dependencies/extension/include/hanime_extension/extension.hpp) | 编码编辑、规划、候选及组合状态 |
| [submission.hpp](@source/integration/product/dependencies/extension/include/hanime_extension/submission.hpp) | 组合到提交事务的适配 |
| [wubi.hpp](@source/integration/product/dependencies/wubi/include/hanime_wubi/wubi.hpp) | 五笔解码器与直接查询绑定 |
| [bounded_user_state.hpp](@source/integration/product/dependencies/core/include/hanime/bounded_user_state.hpp) | 共享计数、学习记录、后台工作与关闭 |
| [state_v3.hpp](@source/integration/product/dependencies/core/include/hanime/state_v3.hpp) | 个人状态文件端口与分步持久化 |
| [composition_commit.hpp](@source/integration/product/dependencies/core/include/hanime/composition_commit.hpp) | 组合事务、学习通知与结果观察 |

模块没有承诺跨版本二进制 ABI。使用这一版源码时，按相同的配置重新编译模块和使用方；保留 Core、Extension 各自真实的版本号，见[版本说明](@doc/about)。
