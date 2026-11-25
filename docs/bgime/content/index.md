# bgIME 嵌入式中文输入引擎

<p class="lead">bgIME（大鹅输入法）是面向资源受限设备的 C++17 中文输入引擎，支持全拼、小鹤双拼和五笔86。Core 与 Extension 可通过 CMake 构建为库，宿主按接入路径链接所需模块。</p>

引擎采用共享词条库与独立方案索引，以稳定词条身份关联词汇数据、输出形式和个人学习计数。Core 通过 `ResourceReader` 按偏移读取并验证外部只读资源，以有界工作预算推进可恢复的查询、排序和文字解析状态机。个人状态独立存放于可写文件。

宿主负责输入事件路由、任务调度、候选渲染与文本接收。确认提交的 UTF-8 文本通过 `TextCommitSink` 传入宿主；编辑器、显示驱动、输入设备及文稿保存不属于引擎。

<div class="hero-actions"><a class="primary" href="/bgime/quickstart/">构建与运行示例 →</a><a href="/bgime/api/">API 参考</a></div>

<figure class="architecture" aria-label="bgIME 模块与宿主接口关系">
<div class="architecture-host"><strong>宿主应用</strong><span>输入事件 · 任务调度 · 候选界面 · 文本接收</span></div>
<div class="architecture-direction">输入与控制调用 ↓　候选与状态返回 ↑</div>
<div class="architecture-paths">
<div class="architecture-node"><strong>Core 直接接入</strong><code>CandidateSession</code><span>完整编码查询与单候选提交</span></div>
<div class="architecture-node"><strong>Extension</strong><code>Composition → Submission</code><span>拼音解析、组合选择与提交；<code>DirectExact</code> 直接查询整段编码，不进行拼音切分</span></div>
<div class="architecture-node"><strong>Wubi 直接查询</strong><code>Wubi86Decoder → DirectQueryBinding</code><span>五笔编码规则与编辑；绑定 Core 候选会话</span></div>
</div>
<div class="architecture-direction">各路径使用 Core 查询与提交能力 ↓</div>
<div class="architecture-core"><strong>Core</strong><span>共享资源 · 候选查询 · 排序 · 输出解析 · 提交事务</span><span>候选返回宿主呈现，宿主确认选择后进入提交</span></div>
<div class="architecture-ports">
<div class="architecture-node"><strong>外部只读资源</strong><code>ResourceReader</code><span>Core 按偏移调用宿主读取接口；共享词库与方案索引独立于输入事件流</span></div>
<div class="architecture-node"><strong>文本接收</strong><code>TextCommitSink</code><span>提交 UTF-8 文本 → 宿主接收；成功结果为 <code>Accepted</code></span></div>
<div class="architecture-node"><strong>共享个人状态</strong><span>学习计数（Count）</span><code>BoundedUserState · StateV3</code><span>排序读取计数；接受后通知学习；通过宿主 <code>StateFilePort</code> 推进持久化</span></div>
</div>
<figcaption>Draftor 五笔使用 Extension 的 DirectExact 组合路径，并复用 Wubi 解码器校验输入；它不通过 DirectQueryBinding 驱动组合。文本接收、学习记录与持久化分别管理。Alpha 3 已使用共享学习，独立宿主仍需接入选词资格与状态调度。</figcaption>
</figure>

本文档适用于 **Draftor Alpha 3 中使用的 bgIME**。三个 Host 示例使用人工测试资源，可独立验证输入、候选与提交流程，无需开发板或正式词库。

## 能力边界

全拼与小鹤双拼支持逐键编辑、编码切分和多词组合提交。五笔86提供编码规则与直接查询适配，也可使用 Extension 的 `DirectExact` 组合路径。不同方案共享稳定词条身份及个人学习计数，记录条件与公开接口缺口见[选词学习](@doc/learning)。

引擎输出 UTF-8 字节串，不负责字形绘制、光标移动或文档保存。仓库未提供操作系统输入法安装包，也未提供适用于任意平台的完整界面；宿主需适配资源访问、调度和文本接收。

## 模块划分

源码保留 `HanIME` 命名。Host 示例是使用这些库的 C++17 命令行宿主。

| 模块 | 职责 | 接口与示例 |
| --- | --- | --- |
| Core | 共享词条与方案索引、候选查询与排序、输出解析、提交和个人记录 | [Core 示例](@doc/quickstart) |
| Extension | 全拼与小鹤双拼解析、编码切分、组合选择及提交适配；DirectExact 精确查询路径 | [组合输入示例](@doc/schemes) |
| Wubi | 五笔86编码编辑与校验、Core 直接查询绑定 | [Wubi 接口](@doc/api/wubi) |

Core 可独立使用，不要求经由 Extension 或 Wubi。五笔两条路径的依赖与调用差异见[输入方案与示例](@doc/schemes#五笔的两条接入路径)。

## 架构特性

- **资源与身份分离。** 方案索引将编码映射到共享词条身份；只读词库与可写个人状态按命名空间关联，不能按显示文字合并记录。
- **按需读取与验证。** `ResourceReader` 提供外部存储访问，资源打开、校验和挂载代次决定后续查询的有效性。正式资源使用配套身份描述与验证材料。
- **有界工作与可恢复状态。** `WorkBudget` 约束单步读取次数、字节数及逻辑工作量，宿主调度状态机至完成或终止。该预算不构成硬实时保证；缓冲区容量约束也不表示完全无动态分配。
- **独立的提交与保存状态。** 候选呈现、选择确认和文本提交各有状态与有效期。`Accepted` 表示文本接收器已接受；个人计数生效、持久化和宿主文稿保存需分别确认。

## Draftor 产品集成

[Draftor（草稿机）](https://github.com/Ohhellboy/Draftor) 将 bgIME 与键盘、编辑器、屏幕及 TF 卡连接。Alpha 3 微雪设备的集成还负责确认可学习的选词事件、调度后台保存及显示保存状态。

候选布局、快捷键和保存操作属于产品界面。其他宿主可采用不同交互，但接入时应遵守接口的数据有效期、串行调用和提交约定。

## 验证与支持范围

| 使用方式 | 已有验证范围 | 集成条件 |
| --- | --- | --- |
| Draftor Alpha 3 | 微雪 ESP32-S3、ST7305 4.2 英寸反射屏设备上的产品集成 | 资源、调度与学习代码依赖 Draftor 产品环境 |
| 三个 Host 示例 | macOS 上的 C++17 命令行程序，使用 POSIX 文件接口 | 覆盖基本输入、候选与提交；未完整演示 Alpha 3 学习接入 |
| 其他平台 | Core 可独立构建；未提供通用插件 ABI 或各平台安装包 | 需适配存储与调度并验证目标硬件；Windows 原生运行不在现有示例验证范围内 |

早期对照测试的版本、硬件和计时范围见[性能记录](@source/docs/PERFORMANCE.md)。Alpha 3 查询与取字耗时需结合目标设备、资源、存储及预算测量。

## 文档入口

<div class="route-grid"><a class="route-card" href="/bgime/quickstart/"><b>快速开始 →</b><p>Host 示例的构建与运行</p></a><a class="route-card" href="/bgime/integration/"><b>宿主集成 →</b><p>存储、输入、调度及提交接口</p></a><a class="route-card" href="/bgime/learning/"><b>选词学习 →</b><p>共享计数、排序与持久化</p></a><a class="route-card" href="/bgime/api/"><b>API 参考 →</b><p>接口声明、状态及生命周期</p></a></div>
