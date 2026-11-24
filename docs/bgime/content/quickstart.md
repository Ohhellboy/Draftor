# 快速开始

这一节带你运行 `host_minimal`。程序会用全拼编码 `yuanxingji` 和小鹤双拼编码 `yrxkji` 各查询一次「原型机」，把两次选中的文字交给自己的文本接收器。完成后，你会得到一个内容为「原型机原型机」的内存文档。

## 准备环境

需要 Git、CMake 3.20 或更新版本，以及支持 C++17 的编译器。现有 Host 文件适配器使用 POSIX 文件接口，示例在 macOS 15.5、Apple Silicon、Apple Clang 17 和 CMake 4.3.2 下有运行记录。其他 POSIX 环境需要自行验证；示例的 CMake 配置会拒绝非 UNIX 平台，不能直接把命令当作 Windows 原生构建步骤。

不需要 ESP32 SDK、开发板、字体或完整的正式词库。请在终端中运行命令，并使用 UTF-8 显示中文输出。

## 获取对应源码

在用于保存项目的目录执行以下命令。它会建立一个新的 `Draftor-bgime` 目录，并检出文档对应的发布标签。

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

标签检出后，Git 可能提示当前处于 detached HEAD 状态。这不影响编译。如果准备修改示例，可以先创建自己的分支。已有其他工作的仓库请保留原状态，使用上面的新目录运行教程。

也可以从 [Alpha 3 发布页](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.3)取得源码包；以下命令的工作目录始终是包含 `ime/` 和 `integration/` 的源码根目录。

## 编译并运行

选择尚未使用的构建目录。下面三条命令依次生成构建配置、编译程序和小型词库、运行示例。

```bash
cmake -S ime/bgime/examples/host_minimal \
  -B build/host_minimal -DCMAKE_BUILD_TYPE=Debug

cmake --build build/host_minimal \
  --target bgime_host_minimal --parallel 2

./build/host_minimal/bgime_host_minimal \
  ./build/host_minimal/fixture
```

CMake 会构建 `HanIME::Core`、POSIX Host 适配器和已有的测试资源生成器 `hanime_build_fixture`。生成器在 `build/host_minimal/fixture/` 下写出 `shared.hns`、`full_pinyin.hnm`、`xiaohe.hnm` 和清单。运行程序只读这些词库文件；唯一的命令行参数就是它们所在的目录。

程序按固定脚本运行，不等待你在终端中逐字输入。正常结束时退出码为 `0`；任何内置检查失败都会报告错误并以非零状态退出。也可以通过 CTest 运行同一个示例：

```bash
ctest --test-dir build/host_minimal --output-on-failure
```

## 看懂输出

完整输出还包含资源身份和过期选择检查。下面是需要先关注的几行：

```text
scheme=full_pinyin code=yuanxingji candidate[0]="原型机" ref=1 paths=2
scheme=xiaohe code=yrxkji candidate[0]="原型机" ref=1 paths=1
same stable identity across schemes: yes
learning: unavailable, count-source reads=0, no user state
final UTF-8 document="原型机原型机" sink_calls=2
demo checks: OK
```

`candidate[0]` 是第一页第一个候选。两个方案取得相同的稳定词条身份；`paths` 表示命中该身份的映射路径数，不是同文候选的数量。`sink_calls=2` 表示应用的文字接收器被成功调用了两次。

`learning: unavailable` 描述的是**这个示例的适配器**：它使用 `NoLearning` 和 `ScoringMode::BaseOnly`，不读个人次数，也不保存用户状态。Alpha 3 产品已启用共享选词学习，不能据此判断引擎没有学习能力。

## 沿着示例读代码

打开 [host_minimal/main.cpp](@source/ime/bgime/examples/host_minimal/main.cpp)，按以下顺序阅读：

1. `Resources` 打开共享词库和两个方案索引，检查命名空间、资源包配对和内容有效性。
2. `admit_text()` 检查会话配置的缓冲区是否足以读取资源中的文字。
3. `query()` 发起编码查询，发布候选页，再请求候选文字。
4. `display()` 在 `present()` 回调内把借用的文字复制到自己的字符串中。
5. `submit()` 用当前候选的选择句柄确认提交，推动会话直到文本接收器接受结果。

**句柄**是接口签发的对象，用来关联某一次输入、候选页或提交事务。不要只保存候选序号再在新页面上重用它；旧句柄会因输入或资源变化失效。

## 换成自己的项目时

把 `DemoDocument` 换成你的编辑器或文字接收器，把固定的 `query()` 调用换成输入方案模块产生的编码，再把 `display()` 中复制的文字交给界面绘制。文件路径、存储适配器、每步预算和页面容量也需要按目标平台配置。完整顺序见[接入自己的程序](@doc/integration)。

这个最小示例接收的是完整编码，不解析全拼音节，也不处理逐键组合输入。要查看这些过程，继续运行[全拼、小鹤双拼与五笔86示例](@doc/schemes)。

> 小型测试词库只包含程序演示所需的条目。它能证明接口如何工作，不能验证真实词库覆盖率、任意句子的切分质量或正式学习效果。实际输入需要[匹配版本的正式资源](@doc/resources)。

## 遇到问题

| 现象 | 检查与处理 |
| --- | --- |
| 找不到 `cmake` 或 C++ 编译器 | 先安装本机开发工具，再确认它们可从终端调用 |
| CMake 指向旧源码目录 | 更换一个未使用的 `-B` 目录，避免复用其他版本的缓存 |
| 无法打开 `shared.hns` | 确认已完成构建，并传入生成的 `fixture` 目录；它相对于当前工作目录解析 |
| 资源配对或验证失败 | 使用同一次构建生成的整套测试文件，不要混入正式资源或其他示例的文件 |
| 中文显示成方框 | 检查终端字体与 UTF-8 显示设置；引擎交付的是 UTF-8 字节，不负责终端字体 |
| 程序报告检查失败 | 保存完整命令、环境和输出，并附上源码提交反馈问题；不要把失败结果当成候选数据继续使用 |
