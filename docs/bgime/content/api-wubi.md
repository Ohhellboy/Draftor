# 五笔接口

五笔模块将编码规则与 Core 查询分开：`Wubi86Decoder` 维护编码，`DirectQueryBinding` 把编码计划交给一个 Core 候选会话。候选取字、翻页、显示和最终文字提交仍使用 Core。头文件见 [wubi.hpp](@source/integration/product/dependencies/wubi/include/hanime_wubi/wubi.hpp)，运行方法见[五笔示例](@doc/schemes)。

## 编码规则与生命周期

`wubi86_profile` 的方案名为 `wubi86`，字母表为 `a`–`y`，普通编码上限为四码，并启用满码唯一候选的提交请求。这是当前参考配置的规则，不应推广为所有五笔输入方案的固定规则。

`Decoder::plan()` 返回 `DirectQueryPlan`，包含借用的编码 `code`、修订号 `revision` 和是否满码 `full_code`。编码视图只在下一次成功编辑前有效。若使用绑定层，应通过绑定层编辑，不要绕过它直接修改解码器或会话输入。

## 创建并激活绑定

`DirectQueryBinding(decoder, session, index, shared, page_size)` 借用四个现有对象；默认页大小为 `3`，需要符合候选会话容量。它们都必须比绑定对象更早创建、更晚销毁。

先完成共享词库与方案索引的配对校验，再完成 `CandidateSession` 的文字准入，然后调用 `activate()` 并检查结果。绑定活动期间，不要在外部重新设置会话的输入或资源绑定。回调同样不能递归进入绑定对象。

## 编辑和推进

| 调用 | 参数、结果和后续动作 |
| --- | --- |
| `append(char)` | 传入由宿主路由的 ASCII 字符；大写折为小写。成功为 `Changed`，随后调用 `tick()` |
| `backspace()` | 删除最后一码。空输入可返回 `Unchanged`；成功编辑产生新的修订 |
| `cancel()` | 放弃当前编码和查询，不提交文字 |
| `leave()` | 明确离开输入方案并放弃当前输入，不隐式提交 |
| `tick(budget)` | 推进绑定和共享会话，返回 `Step`：其中 `shared` 是会话状态与消耗，`request` 是提交请求，另有对应选择和编码修订 |

`EditStatus::InvalidCode` 表示字符不在配置字母表中，`Full` 表示编码长度已满；这两种拒绝不会改变当前编码或修订。`Busy` 表示提交等活动尚未完成。`Stale`、`Closed` 或 `Exhausted` 需要宿主停止使用旧操作并检查当前生命周期。

## 显示、翻页与选择

从共享 `CandidateSession` 取得候选，使用 `request_text()` 请求文字，在 `present()` 回调内复制显示内容。翻页直接调用共享会话的 `request_range()`；不要为翻页偷偷发起一条不同编码的新输入。

用户确认当前候选时调用 `binding.confirm(selection, CommitReason::Explicit)`。接下来观察提交结果；需要重试时使用返回的 `RetryPermit` 调用绑定的 `retry()`。状态语义与 [Core 提交](@doc/api/core)相同。接受成功后绑定清空编码；重复确认已接受事务不会再次交付文字。

## 满码唯一候选请求

`Step::request == CommitReason::AutoUniqueFullCode` 是请宿主考虑提交的请求。当前首屏必须满足满码、只有一个候选、没有更多候选、排序仍有效且输出文字有效等条件。多个映射路径若指向同一词条身份，不会因此变成多个候选。

收到请求时，使用当次 `Step::selection` 和 `code_revision` 关联当前输入。由应用决定是否允许提交，允许后再调用 `confirm(selection, CommitReason::AutoUniqueFullCode)`。请求本身不会调用文本接收器。

实际 Draftor 还会检查自动提交资源、显示窗口、当前输入事件和编辑目标。它的产品入口见 [wubi_product.cpp](@source/integration/product/src/wubi_product.cpp) 和 [device_ime.cpp](@source/integration/product/src/device_ime.cpp)，不能用 Host 的最小条件替代全部产品检查。

## 学习范围

绑定只把提交和学习通知委托给 Core，不拥有第二份个人计数。`host_wubi86` 明确使用基础排序和禁用的学习通知源，没有个人状态文件。

Alpha 3 产品的五笔共享学习走 Draftor 的组合与选词资格路径。它不是单纯给这个绑定更换一个计数源就能完整复现的功能。相关责任和当前公开入口限制见[选词学习](@doc/learning)。
