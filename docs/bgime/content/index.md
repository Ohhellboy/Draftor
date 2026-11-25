<div class="eyebrow">BIG GOOSE INPUT METHOD ENGINE</div>

# 让中文输入，进入你的程序。

<p class="lead">bgIME（大鹅输入法）是一套面向嵌入式设备的中文输入引擎，支持全拼、小鹤双拼和五笔86。它负责查找候选词，并将用户选中的文字交给你的程序。</p>

<div class="hero-actions"><a class="primary" href="/bgime/quickstart/">运行第一个示例 →</a><a href="/bgime/api/">查阅接口</a></div>

<div class="flow" aria-label="输入处理流程"><div class="flow-title">从一次输入开始</div><div class="flow-steps"><div class="flow-step"><small>01 / 输入编码</small><code>yuanxingji</code><strong>你的输入事件</strong></div><span class="flow-arrow" aria-hidden="true">→</span><div class="flow-step"><small>02 / 查询与选词</small><code>原型机</code><strong>bgIME 候选</strong></div><span class="flow-arrow" aria-hidden="true">→</span><div class="flow-step"><small>03 / 提交文字</small><code>UTF-8</code><strong>你的应用程序</strong></div></div></div>

本文档适用于 **Draftor Alpha 3 中使用的 bgIME**。第一次使用时，可以先运行命令行示例，观察候选词如何产生、选中的文字如何提交，再把相应接口接入自己的设备。示例会生成小型测试词库，不需要先准备开发板或下载正式词库。

## 你可以用它做什么

你可以在写作设备或其他需要中文输入的程序中接收键盘编码、显示候选、让用户选择文字，再把结果插入文本。引擎通过 UTF-8 字节串交付文字；字形绘制、光标移动和文档保存由应用完成。

全拼和小鹤双拼可以进行逐键编辑、编码切分和多个选中词的组合提交。五笔86使用自己的编码规则。不同方案共享词条身份，因此同一个词的个人选词记录可以跨方案使用。Alpha 3 已实际使用这项学习能力，具体的记录条件和接入范围见[选词学习](@doc/learning)。

bgIME 不是操作系统输入法安装包，也不包含一个可直接移植到任意平台的完整界面。你需要为它提供资源读写、事件调度和接收文字的程序。

## 先认识三个模块

**宿主程序**是使用引擎的应用。下文中的 Host 示例，就是运行在电脑上的小型宿主程序。模块名称仍保留源码中的 `HanIME` 命名，不需要在接入时改名。

| 模块 | 它负责什么 | 从哪里开始 |
| --- | --- | --- |
| Core | 读取共享词条和方案索引、查找和排序候选、解析文字、完成提交与个人记录 | [Core 示例](@doc/quickstart) |
| Extension | 全拼和小鹤双拼的逐键编辑、切分、已选词组合，以及组合提交适配 | [组合输入示例](@doc/schemes) |
| Wubi | 五笔86编码编辑，以及把直接查询计划交给 Core 的适配接口 | [五笔接口](@doc/api/wubi) |

**候选**是引擎根据当前编码找到的词条；**提交**是应用确认接收这些文字的动作。候选显示出来、用户选中某个词，以及文字真正写入应用，是三个不同的阶段。

## 与 Draftor 的关系

[Draftor（草稿机）](https://github.com/Ohhellboy/Draftor) 是使用 bgIME 的开源写作设备。它把引擎与键盘、编辑器、屏幕和 TF 卡存储连接起来。Alpha 3 的微雪设备集成还负责确认可学习的选词事件、安排后台保存和显示保存状态。

草稿机的候选布局、快捷键和保存操作是一个应用案例。你的程序可以使用不同的界面和按键，只需遵守接口的数据有效期、串行调用和提交约定。

## 当前的使用范围

| 使用方式 | 已有范围 | 对接入者的影响 |
| --- | --- | --- |
| Draftor Alpha 3 | 微雪 ESP32-S3、ST7305 4.2 英寸反射屏设备上的产品集成 | 可参考实际资源、调度与学习调用，但这些代码依赖草稿机的产品环境 |
| 三个 Host 示例 | macOS 上的 C++17 命令行程序，使用 POSIX 文件接口 | 可学习基本输入、候选和提交；未完整演示 Alpha 3 学习接入 |
| 其他平台 | Core 可以独立构建；未提供通用插件 ABI 或各平台安装包 | 需要适配存储与调度，并自行验证目标硬件；Windows 原生运行不在现有示例的验证范围内 |

早期对照测试的版本、硬件和计时范围见[性能记录](@source/docs/PERFORMANCE.md)。Alpha 3 在目标设备上的查询和取字耗时，需要结合所用资源、存储和预算实际测量。

## 按你的任务继续

<div class="route-grid"><a class="route-card" href="/bgime/quickstart/"><span class="route-number">01 / FIRST RUN</span><b>准备并运行示例 →</b><p>获取源码、准备环境，并看懂基本输出。</p></a><a class="route-card" href="/bgime/schemes/"><span class="route-number">02 / EXAMPLES</span><b>选择输入方案 →</b><p>按需求运行组合或五笔示例，比较接入路径。</p></a><a class="route-card" href="/bgime/integration/"><span class="route-number">03 / INTEGRATION</span><b>接入自己的程序 →</b><p>连接输入事件、候选显示与文字接收。</p></a><a class="route-card" href="/bgime/api/"><span class="route-number">04 / REFERENCE</span><b>按任务找接口 →</b><p>查参数、返回值、调用顺序和有效期。</p></a></div>
