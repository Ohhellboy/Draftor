# 接口索引

本页按 Alpha 3 的公开头文件、模块依赖及操作类型索引接入接口。源码保留 `HanIME` 命名，对外名称为 bgIME。

所有源码链接固定到 `f18939f8880e2278c231739d553fbf983c19e0f6`。

## 模块与构建目标

| 模块 | 命名空间 | CMake 目标 |
| --- | --- | --- |
| Core | `hanime` | `HanIME::Core`；底层目标 `hanime_core` |
| Extension | `hanime_extension` | `HanIME::Extension`；依赖 Core |
| Wubi | `hanime::wubi` | Host 示例将 `wubi.cpp` 编入本地目标 `bgime_demo_wubi86`，链接 Core；未提供独立安装包 |

构建配置与宿主依赖见[宿主集成](@doc/integration#接入路径)。

## 按操作分类

| 任务 | 类型或接口 | 参考页 |
| --- | --- | --- |
| 打开词库与编码索引 | `ResourceReader`、`PagedSharedResource`、`PagedSchemeIndex` | [Core](@doc/api/core) |
| 查询完整编码、翻页和取字 | `CandidateSession` | [Core](@doc/api/core) |
| 接收最终文字 | `TextCommitSink::commit()` | [宿主集成](@doc/integration) |
| 逐键编辑全拼和小鹤双拼 | `Composition`、`SchemeRules` | [Composition / Submission](@doc/api/composition) |
| 一次提交多个已选词 | `Submission` | [Composition / Submission](@doc/api/composition) |
| 五笔直接查询 | `Wubi86Decoder`、`DirectQueryBinding` | [Wubi](@doc/api/wubi) |
| 读取个人次数、接收学习通知 | `CountSource`、`BoundedUserState` | [选词学习](@doc/learning) |
| 持久化个人状态 | `StateFilePort`、`StateV3` | [选词学习](@doc/learning) |

五笔组合路径的选型见[DirectQueryBinding 与 DirectExact](@doc/schemes#五笔接入路径)。

## 公开入口与内部实现

`include/hanime/` 和 `include/hanime_extension/` 包含公开声明，但并不代表所有声明都属于同一代接入路径。例如，`core.hpp` 中仍保留 `ImeSession`、`UserStateStore` 等旧式同步接口。本版教程采用三个 Host 示例实际使用的分步资源、候选与组合接口，不混用旧接口的状态和保存约定。

`ExactRanker`、`OutputReader`、`CompositionCommitter` 等较低层的公开类型用于自定义查询或提交流程。常规集成优先使用会话与组合适配器；不要同时驱动适配器及其持有的内部查询。

`integration/product/src/` 是 Draftor 产品集成。`CandidateWindow`、`DeviceIme`、`LearningTrial` 和 `PersonalIndex` 属于产品实现，不是通用 SDK 的安装入口。其中 `Composition::select_explicit()` 是私有函数，由友元 `CandidateWindow` 调用，不能作为普通宿主的公开 API。这个区别直接影响[学习事件资格](@doc/learning)。

## 通用调用约定

**串行调用。** 同一个资源或会话由一个调度者推进。显示与提交回调可以把后续事件加入队列，但不能递归 `tick()`、销毁对象或在回调内重用借用缓冲区。

**所有者先创建、后销毁。** 文件读者、共享资源、方案索引、计数源和接收器的生命周期应覆盖借用它们的会话。`Submission` 要在 `Composition` 之后创建、之前销毁。

**区分待完成与失败。** `Pending`、`Working`、`Preparing` 表示操作尚需推进。`Ready` 依类型表示资源可用、页面读取成功或提交已准备好；文本接收成功以 `Accepted` 为准。

**句柄有效期。** 新输入、重试、资源更换或排序变化可使选择或事务句柄过期。返回 `Stale` 后应重新核对当前上下文，不能复用旧选择。

**字节边界与借用数据。** 文本和编码的长度、偏移及容量均以字节为单位。`std::string_view` 的有效期以具体接口为准；只有明确复制输入的接口才允许在返回后释放输入数据。

## 公开头文件

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

## 版本兼容性

模块与宿主需使用相同配置重新编译；跨版本的二进制兼容性需要另行确认。Core、Extension 各自的版本号见[版本说明](@doc/about)。
