# R6 受控首次安装计划

2026-11-21。软件与实机结果分别记录在 R6_REPORT.md；本文件不是实机通过证明。继承 R3/R4 协议修复，固定 alpha.3 四文件，默认包继续锁写。兼容更新与首装仍使用同一个主按钮，内部权限互斥。

## 有限识别范围

- 固定 Draftor B 分区且 OTA 唯一有效：仅生成原有活动 App 兼容更新计划。首装测试构建不能写这种计划。
- 固定厂家表 SHA-256，并验证 [0,0x9000)、[0x10000,4548144)、[4548144,16777216) 的准确 ROM MD5：允许生成厂家初始化计划。中间 [0x9000,0x10000) 是该厂家布局的 NVS/phy_init，可因厂家正常启动而改变；这不是空白板，也不承诺保留该区。
- 全 FF：元数据为 FF 还不足够，必须有整片 16 MiB ROM MD5 证明。
- 其他布局、非白名单厂家程序、未知尾部、错误身份/容量/安全状态一律 STOP。没有“更新失败转首装”。板型必须由用户核实 ST7305 实物。

## 清除与写入范围

首装清除 **[0,0x1000000)** 的全部内部 Flash，含旧固件、内部文件、NVS 设置和配对；不访问 TF、eFuse。须先有本机完整可恢复备份及独立 TF 备份，确认停止写作、完全断电取出原 TF，并单独确认清除。

1. 写前重新检查身份、安全、16 MiB、分区/元数据指纹和完整厂家指纹。
2. ROM `FLASH_BEGIN`：`(16777216,16384,1024,0,0)`。这是官方 esptool 5.3.1 `cmds.erase_region()` 的 ROM 路径，不调用 stub-only `0xd0` / `0xd1`。确认整片 MD5 为全 FF 后才进入写文件阶段。
3. 按下表顺序仅允许四份 SHA-256 已锁定的文件。每文件一次 FLASH_BEGIN，1024 B DATA 块，末块 FF 填充；每文件 ROM MD5 通过后才进入下一文件。

| 文件 | 偏移 | 有效字节 | 传输结束（不含） | 擦除结束（不含） |
|---|---:|---:|---:|---:|
| bootloader.bin | 0x0 | 19968 | 0x5000 | 0x5000 |
| partitions.bin | 0x8000 | 3072 | 0x8c00 | 0x9000 |
| boot_app0.bin | 0xe000 | 8192 | 0x10000 | 0x10000 |
| app.bin | 0x10000 | 4021152 | 0x3e5c00 | 0x3e6000 |

4. 独立读取 4096 B 分区扇区及 8192 B OTA，分别核对 ROM MD5、准确发布字节及 app0 初始化状态；整片 MD5 必须等于“四文件+其余全 FF”的冻结目标。重新检查身份、安全。成功仅是 `VERIFIED_BOOT_PENDING`。
5. 关闭网页串口，独立 Python esptool 5.3.1 `verify_flash` 四偏移、全片 MD5、分区/OTA 慢读；通过后才由用户普通开机、插回允许使用的 TF 并验收写作。

任何错误、中断、取消、身份变化均停止后续命令。无自动重连、重试、换槽、stub、FLASH_END 或自动复位。清除后设备可能暂时不能启动；恢复也可能失败，不能声称备份消除全部风险。

## 厂家维护与恢复独立于网页验收

`tools/r6_maintenance.py` 使用官方 esptool 5.3.1 的 ROM `erase_region`、无压缩 `write_flash` 和 `verify_flash`，不改官方帧消费规则。沿用 R3 的状态/空 SLIP/长度检查，加入仅用于本次维护的精确出站字节范围许可。每步新建私有证据目录；已执行目录禁止自动重跑。`--before no-reset` 对应 connect(mode='no-reset')，不调用结束/复位 API。官方内部重试被终止异常及单次写入策略禁止。

- 厂家恢复：旧整片摘要必须仍等于指定备份；官方 ROM 全片区域擦除 → 全 FF 校验 → 0x0 写入 4548144 B 厂家镜像 → 全片预期摘要 → 新 Python 会话独立 verify。厂家普通开机需人工确认。
- 原状态恢复：仅从私有收据验证通过的 16777216 B 完整备份写入 0x0。一次 FLASH_BEGIN 已覆盖整片擦除，不额外擦除一遍；16384 个 1024 B DATA 块精确等于备份，Flash 参数全部 keep。独立整片 digest/分区/OTA 核验后由用户插原 TF、确认原键盘及原稿、新稿持久化。
- 真实错误不自动调用恢复。停止并报告，按用户已确认的恢复计划处理，必要的手动重入 ROM 集中说明；不得在恢复错误后再次盲刷。

## 复现（只操作软件）

`node tools/fetch-factory.mjs` 校验固定厂家夹具；`npm run test:r6` 运行无真实 USB 的协议测试。Python 测试：在安装准确 esptool 5.3.1 的环境运行 `python3 -B tests/r6-recovery.test.py`，只使用内存串口。

`npm run build:r6-locked` → dist-r6-locked（两类写入均 false）；`npm run build:first-install-test` → dist-first-install-test（仅首装实验 true）。它们不覆盖 R4 冻结构建；实验构建仅留本机，禁止公开部署。

依据：[准确 5.3.1 erase_region 源码](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/cmds.py#L1930)、[FLASH_BEGIN 源码](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/loader.py#L1095)、[S3 ROM 协议](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/serial-protocol.html)。
