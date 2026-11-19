# Draftor Web Installer R1 — 软件交付报告

日期：2026-11-19。结论：**SOFTWARE_READY_FOR_DEVICE_TEST**。

已实现保留 v0.1 界面的静态网站、准确发行资产验证、受控浏览器 ROM 连接、只读安全计划、真实活动 App 槽选择、兼容更新、独立写后校验、首装模拟与失败回执。真实 USB、启动、配对保持及空板首装均为 **DEVICE_PENDING**；本轮没有访问 USB/真实 TF，没有擦写真实 Flash，没有部署、改 DNS、推送或合入正式分支。

默认交付站点锁住真实写入，只提供真实设备的只读计划和完整模拟器。后续经授权的专用构建可解锁**兼容更新**；真实 fresh 仍由状态机及驱动层阻止。这是软件阶段的交付边界，不是已经完成硬件资格验收。

## 输入、隔离与界面

- 仓库：[Ohhellboy/Draftor](https://github.com/Ohhellboy/Draftor)，准确发行 [v0.1.0-alpha.3](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.3)。tag 对象 `36366a585dc8151b28463d0d78d0d11ce5ccbacf`，提交 `f18939f8880e2278c231739d553fbf983c19e0f6`。
- 新建独立 checkout：`Draftor_Web_Installer_R1_20261119`；分支 `feature/web-installer-r1`。既有 `Draftor` 工作区的未提交修改未改动。
- 所有实现、文档、输入和软件证据位于 `web-installer/`；没有修改固件运行代码、bgIME 核心或发行二进制。
- 附件 Brief 被作为用户明确指定的任务规格执行；附件原 README 中关于未来 GitHub Pages/DNS 的设想不构成本轮部署授权。
- 保留素纸 `#F7F5EF`、墨色 `#292924`、朱色 `#A74539`、主版式、黑体「草稿机」回退、口号、中英切换、四个说明入口、GitHub 和一个主页安装入口。ST7306 继续不可选；没有引入未经批准的硬件照片。

## 发行完整性

固件 ZIP 为 2,913,395 字节，SHA-256 `626fb11222281dc4351d2de71ff9a36422f4aeca487e3475484b8bd55315699a`。与固定 tag 的 `WAVESHARE_RLCD42.json`、公开 MANIFEST、SHA256SUMS、包内 `FIRMWARE_MANIFEST.json`、`RELEASE_PAIR.json`、原安装脚本和 `CONTENTS.json` 交叉核对。构建还核验全部包内提取文件和对应公开源码 ZIP，共 59 项输入记录，见 `evidence/release-verification.json`。

| 二进制 | 字节数 | SHA-256 |
| --- | ---: | --- |
| app.bin | 4,021,152 | `165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd` |
| bootloader.bin | 19,968 | `31b3c1be45dc5a76aa85c82540d6787b675e711eaf11021a5eea7e36f469c6de` |
| partitions.bin | 3,072 | `a172a8ae4cd0db329d6de2635991e3f6e3feb374e0ee9dcab78d9d7b360df6c9` |
| boot_app0.bin | 8,192 | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |

App 未重新编译或替换。浏览器在请求串口前重新检查四份文件的大小、SHA-256、S3 镜像头、发行标签、槽容量及分区/初始化器规则；运行时只读取同源固定路径，拒绝重定向、截断、超长和下载失败。校验的是写入所使用的私有副本，避免异步哈希和调用方修改之间的竞态。

实际历史 Alpha 2.1 Resources ZIP 和 Alpha 3 LearningIndex ZIP 已下载并校验，18 个词库/认证页/标记/字体/学习索引文件逐项大小及 SHA-256 全部一致。`evidence/tf-resources.json` 记录的是**公开资源包**，不是实际 TF 卡。网页提供独立中英 TF 指导，不上传私人学习或文稿，不格式化 TF。

## 浏览器能力与采用方案

ESP Web Tools **10.5.2** 的现成流程不满足本合同：会进入加载 stub 的高层 `main()`、按芯片族选择固件、未识别固件时可默认擦除，并在结果处理路径复位。其 manifest/override 不能表达本项目的全部分区、OTA、精确活动槽及独立验证门槛。因此没有把该安装按钮直接嵌入主页面。依据：[锁定 flash.ts](https://github.com/esphome/esp-web-tools/blob/5925f9e20ed1d9a2c3f40760b5cb7879854d1736/src/flash.ts)、[install-dialog.ts](https://github.com/esphome/esp-web-tools/blob/5925f9e20ed1d9a2c3f40760b5cb7879854d1736/src/install-dialog.ts)。

采用 **esptool-js 0.7.1 的 SLIP 编码器 + 受控 Web Serial 流 + esptool 5.3.1 的 S3 ROM 协议适配**。不用 esptool-js 的 `main/runStub/readFlash/writeFlash/reset` 高层默认流程；修正/避开其 ROM MD5 ASCII 编码、stub 专用读取、ROM 块长和隐式重试行为。每个命令只发送一次，严格验证响应，串口 framing/parity 等错误立即停止。

无 stub 的读取并非浏览器缺失的能力：S3 ROM 提供 `0x0e READ_FLASH_SLOW`，每次返回 64 字节；`0x13` 返回 32 字节 ASCII MD5。浏览器发送与准确 Python 安装路径相同的命令，因此软件层面可实现等价只读数据与独立内容校验。正常 `verify-flash` 也使用独立 ROM MD5；本实现没有把写 ACK 当成验证，也没有声称已完成整段 Flash 的 SHA-256 readback。依据：[准确 Python ROM reader](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/targets/esp32.py)、[命令实现](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/loader.py)。

R1 限定原生 USB Serial/JTAG `303a:1001`，以 115200 波特率打开并保留同一端口，不更改信号、不重连/重置；实物板型与预期 MAC 另行确认。先在首次 SYNC 前有界排空 ROM 启动文本，然后只允许严格协议帧。S3 的 watchdog 和 SPI 寄存器初始化来自 5.3.1 的无 stub 流程，属于易失寄存器写，不是 Flash 或 eFuse 写。

**剩余硬门槛必须实机判定：** 目标 USB Serial/JTAG 上 64 字节读取的真实质量/耗时、完整安全应答、长操作稳定性、ROM digest 的实际结果、串口关闭后的启动行为。完整协议 mock 通过证明程序行为，不能把这些项写成 DEVICE_PASS。具体步骤见下一阶段动作单。

## 计划与数据保护

- 固定 B 分区：读取实际 4096 字节，前 3072 字节必须逐字节等于发行表；实际 OTA 8192 字节必须满足发行 Python 的唯一有效/唯一未用记录规则。pending、rollback、损坏和两个有效记录全拒绝。
- app0 只写 `0x10000`，app1 只写 `0x410000`。容量 `0x400000`；实际覆盖扇区分别截止 `0x3e6000` / `0x7e6000`，均为排他终点。计划明确显示 App 准确长度和扇区范围。
- 写入前再次核对同一身份、Flash、安全、分区、OTA 与原计划；备份、TF 取出和计划确认缺一不可。可选精确旧 App 内容比对。
- ROM `FLASH_BEGIN` 只擦除本次 App 覆盖的扇区；后续 3927 个 1024 字节块不跨槽。更新从不使用全片擦除，从不写入其他区域。
- 写完后独立 ROM MD5 校验 App，复读分区/OTA，对活动槽外全部 Flash 比较写前/写后 digest。日志不输出 MAC、私有 Flash 或这些私人摘要。
- 不发送 `FLASH_END`，不自动运行 App。失败保留 `STOP`；已经动过 Flash 的失败记为 `POSSIBLY_PARTIAL`。没有自动重试、换槽、全片擦除回退或成功误报。
- fresh 只有**全片 16 MiB 全 FF digest 证明**后才生成软件计划，不能只看空分区。模拟执行还要求独立全片擦除确认和可恢复备份确认。真实驱动没有 fresh/eraseAll 入口。

完整逐状态计划见 [INSTALL_PLAN.md](docs/INSTALL_PLAN.md)；机器可审阅的 app0、app1、fresh 和失败回执在 `evidence/plans/`。

## 首装阻碍与边界

准确发行脚本给所有操作加 `--no-stub`，但 esptool **5.3.1** 的 `erase_flash()` 明确为 **stub-only**。因此该脚本的 `--no-stub erase-flash` 不能作为已可执行的浏览器首装依据。这也说明原发行的 fresh 离线编排夹具不等于真实空板首装通过。依据：[准确 cmds.py](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/cmds.py)。

官方 ROM `FLASH_BEGIN` 区域擦除可作为后续受控替代方案，理论上能覆盖全片；本轮没有暗中替换为它，也没有启用 stub 或削减备份/空白证明门槛。真实 fresh 持续锁定，等待专用空板的协议和恢复验收。这不是已证实的浏览器根本能力缺失，因此总体结论是软件可进入**兼容更新实机验收**，而不是用危险的简化方案宣称已完成全部硬件安装。

## 自动验证证据

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| 发行文件、清单、包内提取文件与源码 ZIP | 59 项 PASS | `evidence/release-verification.json` |
| 软件安全、真实字节协议模拟及故障回归 | **90/90 PASS** | `evidence/unit-tests-final.tap` |
| OTA 与准确发行 Python 检查器对照 | **53/53 一致**（包含在上述测试） | `evidence/python-oracle.json` |
| 全协议 Web Serial mock 的 app0/app1 更新 | PASS，分别精确偏移，无 stub/reset/全片擦除 | `evidence/rom-app0.json`、`rom-app1.json` |
| Chromium 浏览器交互与静态布局 | **19/19 PASS** | `evidence/browser-tests.json` |
| 历史词库/字体与独立 LearningIndex | **18/18 PASS** | `evidence/tf-resources.json` |
| 站点 ZIP 全文件 SHA-256、锁写配置及必需资源 | PASS | `evidence/package-verification.json` |
| 真实硬件 / TF / 启动 / 配对保持 / 首装 | **DEVICE_PENDING** | 未执行 |

覆盖 App0/App1、未确认 PCB、错误 MAC/芯片/Flash、安全未知或启用、固定分区不符、OTA pending/rollback/双有效/CRC/序号边界、缺备份、计划变化、下载中断、文件篡改、流/ROM 错误、写中断、写后不一致、槽外改变、重复执行、异步副本竞态、取消、关闭超时，以及 fresh 独立门槛和擦除中断。浏览器测试先移除真实 Web Serial，所有安装行为都发生在合成 Flash。

浏览器中英主页面、移动端、app1 计划/完成、断线/校验失败截图在 `evidence/screenshots/`。另通过 Codex 浏览器手动完成 app1 软件模拟并看到 `VERIFIED_BOOT_PENDING` 与显式模拟标识。

## 交付及下一步

- `dist/` 与 `delivery/Draftor_Web_Installer_R1_site.zip`：默认锁写、可直接复制托管的完整静态站点。
- `delivery/Draftor_Web_Installer_R1_source.zip`：可复现源码、锁文件、输入和测试证据；站点内亦提供对应源码下载。
- `delivery/SHA256SUMS`：两个 ZIP 的最终摘要。
- [NEXT_DEVICE_TEST.md](NEXT_DEVICE_TEST.md)：一次授权后连续进行的备份、只读对照、兼容更新、独立核验和普通启动动作，不以生产设备试 fresh。

构建/运行说明见 README。未创建外部发布、PR 或合并动作。保留全部本轮交付与测试证据；没有清理任何历史工程或其他窗口文件。
