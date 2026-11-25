# 快速开始

Host 示例使用 C++17 和 POSIX 文件适配器，构建时生成独立的人工测试资源。三个示例互相独立，按所需接入路径选择，不要求依次运行。

## 示例选型

| 接入需求 | 对应示例 |
| --- | --- |
| 完整编码查询与单候选提交 | 本页的 `host_minimal` |
| 全拼或小鹤双拼的逐键输入与多词组合 | 获取源码后运行[组合示例](@doc/schemes#全拼与小鹤双拼) |
| 五笔86的逐键查询与提交请求 | 获取源码后运行[五笔示例与路径选择](@doc/schemes#五笔86) |

`host_minimal` 分别以全拼编码 `yuanxingji` 和小鹤双拼编码 `yrxkji` 查询「原型机」，通过文本接收器提交两次，最终内存文档为「原型机原型机」。

## 环境要求

构建依赖 Git、CMake 3.20 或更新版本及支持 C++17 的编译器。现有 Host 文件适配器使用 POSIX 文件接口，示例在 macOS 15.5、Apple Silicon、Apple Clang 17 和 CMake 4.3.2 下有运行记录。其他 POSIX 环境需要自行验证。示例的 CMake 配置会拒绝非 UNIX 平台；在 Windows 原生环境运行前，需要适配文件接口和构建配置。

不需要 ESP32 SDK、开发板、字体或完整的正式词库。终端应使用 UTF-8 显示中文输出。

## 获取源码

以下命令创建 `Draftor-bgime` 目录，并检出对应的发布标签：

```bash
git clone --branch v0.1.0-alpha.3 --depth 1 \
  https://github.com/Ohhellboy/Draftor.git Draftor-bgime
cd Draftor-bgime
git rev-parse HEAD
```

最后一条命令应输出：

```text
f18939f8880e2278c231739d553fbf983c19e0f6
```

标签检出后的 detached HEAD 状态不影响编译；修改示例前可创建工作分支。使用独立目录，避免改变已有工作的仓库状态。

也可以从 [Alpha 3 发布页](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.3)取得源码包；以下命令的工作目录始终是包含 `ime/` 和 `integration/` 的源码根目录。

## 编译与运行

使用新的构建目录，依次配置、编译程序与测试资源，再运行示例：

```bash
cmake -S ime/bgime/examples/host_minimal \
  -B build/host_minimal -DCMAKE_BUILD_TYPE=Debug

cmake --build build/host_minimal \
  --target bgime_host_minimal --parallel 2

./build/host_minimal/bgime_host_minimal \
  ./build/host_minimal/fixture
```

CMake 会构建 `HanIME::Core`、POSIX Host 适配器和已有的测试资源生成器 `hanime_build_fixture`。生成器在 `build/host_minimal/fixture/` 下写出 `shared.hns`、`full_pinyin.hnm`、`xiaohe.hnm` 和清单。运行程序只读这些词库文件；唯一的命令行参数就是它们所在的目录。

这里指定的可执行目标会自动带上测试词库依赖。省略 `--target` 表示构建该示例工程的默认目标；三个工程的依赖差别见[构建目标说明](@doc/schemes#默认构建与指定目标)。

程序按固定脚本运行，不接收终端交互输入。正常结束时退出码为 `0`；任何内置检查失败都会报告错误并以非零状态退出。也可以通过 CTest 运行同一个示例：

```bash
ctest --test-dir build/host_minimal --output-on-failure
```

## 输出说明

关键输出如下；完整输出还包含资源身份和过期选择检查：

```text
scheme=full_pinyin code=yuanxingji candidate[0]="原型机" ref=1 paths=2
scheme=xiaohe code=yrxkji candidate[0]="原型机" ref=1 paths=1
same stable identity across schemes: yes
learning: unavailable, count-source reads=0, no user state
final UTF-8 document="原型机原型机" sink_calls=2
demo checks: OK
```

`candidate[0]` 是第一页第一个候选。两个方案取得相同的稳定词条身份；`paths` 表示命中该身份的映射路径数，不是同文候选的数量。`sink_calls=2` 表示应用的文字接收器被成功调用了两次。

`learning: unavailable` 表示本示例禁用了学习：它使用 `NoLearning` 和 `ScoringMode::BaseOnly`，不读个人次数，也不保存用户状态。Alpha 3 产品已启用共享选词学习；记录条件和所需接入工作见[选词学习](@doc/learning)。

## 示例结构

[host_minimal/main.cpp](@source/ime/bgime/examples/host_minimal/main.cpp) 的调用顺序如下：

1. `Resources` 打开共享词库和两个方案索引，检查命名空间、资源包配对和内容有效性。
2. `admit_text()` 检查会话配置的缓冲区是否足以读取资源中的文字。
3. `query()` 发起编码查询，发布候选页，再请求候选文字。
4. `display()` 在 `present()` 回调内复制借用文字到宿主字符串。
5. `submit()` 用当前候选的选择句柄确认提交，推动会话直到文本接收器接受结果。

选择句柄绑定对应输入及候选页的有效期。候选序号不能替代选择句柄；输入或资源变化可使旧句柄失效。

## 后续步骤

宿主集成需替换 `DemoDocument`，实现编辑器的文本接收接口；以输入方案模块生成的编码调用 `query()`，以 `display()` 复制的文字更新渲染层。文件路径、存储适配器、每步预算和页面容量按目标平台配置，调用顺序见[宿主集成](@doc/integration)。

最小示例只接收完整编码，不解析全拼音节或处理逐键组合输入。逐键输入使用[组合或五笔示例](@doc/schemes)。

> 小型测试词库只包含程序演示所需的条目，用于验证接口调用和输出。实际输入需要[匹配版本的正式资源](@doc/resources)，并另行测试词库覆盖、句子切分和学习效果。

## 故障排查

| 现象 | 检查与处理 |
| --- | --- |
| 找不到 `cmake` 或 C++ 编译器 | 先安装本机开发工具，再确认它们可从终端调用 |
| CMake 指向旧源码目录 | 更换一个未使用的 `-B` 目录，避免复用其他版本的缓存 |
| 无法打开 `shared.hns` | 确认已完成构建，并传入生成的 `fixture` 目录；它相对于当前工作目录解析 |
| 资源配对或验证失败 | 使用同一次构建生成的整套测试文件，不要混入正式资源或其他示例的文件 |
| 中文显示成方框 | 检查终端字体与 UTF-8 显示设置；引擎交付的是 UTF-8 字节，不负责终端字体 |
| 程序报告检查失败 | 保存完整命令、环境和输出，并附上源码提交反馈问题；不要把失败结果当成候选数据继续使用 |
