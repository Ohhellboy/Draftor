# R1 只读兼容性与安装计划

准确发行：`v0.1.0-alpha.3`，tag 对应提交 `f18939f8880e2278c231739d553fbf983c19e0f6`。只有 WAVESHARE_RLCD42 / ST7305 可进入计划；芯片族不能证明 PCB。

## 写入前必须全部成立

1. 本地 4 份二进制和 4 份源清单与审阅锁文件的大小/SHA-256 一致；App、bootloader 的 ESP 镜像头、S3 chip id=9、App 标签、4 MiB 容量、固定分区字节及首装 OTA 初始化器一致。
2. 用户人工确认微雪实物，并给出先前私下核验的预期 MAC；使用明确用户选择的同一串口。R1 只覆盖 Espressif USB Serial/JTAG `303a:1001`。
3. 手动进入 ROM；单次 SYNC，消费 8 条响应。禁止加载/复用 stub，禁止 DTR/RTS 操作、自动重连或改变启动模式。
4. 完整 20 字节安全信息确认 S3、已知安全 flags、Secure Boot 和 Flash Encryption 均未启用。未知、受限下载、截断应答均 STOP。Flash ID 独立确认 16 MiB，不能以设置 ROM 参数冒充检测容量。
5. 实际读取 `0x8000 + 0x1000` 和 `0xe000 + 0x2000`；读取采用 ROM `0x0e`，每次 64 字节，并与独立 ROM MD5 交叉核验传输结果。
6. 执行前用户确认可恢复的完整 Flash/配对备份、准确原固件、TF 文稿与学习备份，TF 已取出，并确认完整计划。网页不创建备份，也不以勾选代替备份核验工作。
7. 再读同一设备身份、安全、Flash ID、分区及 OTA，要求与先前只读计划一致。可选准确旧 App 另行 MD5 比对；不匹配则 STOP。

「只读」是 Flash 只读：按上游 5.3.1 程序，初始化会写**易失** watchdog/SPI 控制寄存器及 ROM RAM 参数。这些操作不写 Flash、NVS 或 eFuse；具体序列见能力审计。

## 分流

| 实际状态 | 决定 | 可写范围 |
| --- | --- | --- |
| 固定 B 分区字节完全相符；OTA 唯一支持记录选择 app0 | 兼容更新计划 | 下表 app0 的 App 内容与覆盖扇区 |
| 固定 B 分区字节完全相符；OTA 唯一支持记录选择 app1 | 兼容更新计划 | 下表 app1 的 App 内容与覆盖扇区 |
| 分区/OTA 均为 FF，且全片 16 MiB 的独立 ROM MD5 与全 FF 镜像一致 | fresh 软件计划 | 模拟器可在单独确认后全片擦除并四文件安装；真实 fresh 禁止 |
| 只看到空白分区，但全片非 FF | STOP | 无 |
| 分区不符、2 MiB 槽、OTA pending/new/invalid/aborted/rollback、CRC 错、两个有效记录、没有有效记录 | STOP | 无 |
| 板型未确认、MAC 不符、非 S3、非 16 MiB、安全未知/启用/受限 | STOP | 无 |
| 资源下载失败或 SHA-256/镜像/容量校验失败 | STOP（不打开串口） | 无 |

OTA 规则逐字对应发行 `check_install.py.active_slot()`：只接受一个 `0 < seq < 0xffffffff`、CRC 正确、state 为 `2` 或 `0xffffffff` 的记录；另一个必须为全 FF 或准确的 `seq=0 + 28 字节 FF` 初始化哨兵。槽位为 `(seq - 1) % 2`。不猜测两个有效记录的优先级。

## 兼容更新的精确范围

| 项目 | app0 | app1 |
| --- | --- | --- |
| 允许的活动槽 | `[0x10000, 0x410000)` | `[0x410000, 0x810000)` |
| 实际 App 字节 | `[0x10000, 0x3e5ba0)` | `[0x410000, 0x7e5ba0)` |
| 1024 字节 ROM 写块补齐终点 | `0x3e5c00` | `0x7e5c00` |
| 覆盖扇区擦除终点（4 KiB 对齐） | `0x3e6000` | `0x7e6000` |
| App 字节数 | 4,021,152 | 4,021,152 |

App SHA-256：`165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd`。

更新不是「完全不擦除」：ROM `FLASH_BEGIN` 擦除本次 App 覆盖扇区；它**从不全片擦除**。后续 3927 个 1024 字节写块最后一块用 FF 补齐，仍限于活动槽。禁止修改 bootloader、partition、otadata、NVS/配对、未选 App、ime_ro、ffat、coredump；不访问 TF。

成功须在写 ACK 之外单独发送 ROM `0x13`，解析 **32 字节 ASCII MD5**，对照从已通过 SHA-256 的确切 App 计算出的 MD5。它等价于发行 Python `verify-flash` 的正常独立 digest 路径；不是整段 readback SHA-256。另复读分区/OTA，并对活动槽之外的所有 Flash 分段做写前/写后独立摘要比较。私人摘要不写入回执。

禁止发送 ROM `FLASH_END`，因为可能运行用户代码；禁止自动重启。最终仅为 `VERIFIED_BOOT_PENDING`，且 `tfResources=NOT_CHECKED`。不声称键盘、保存、启动或 TF 资源已实机通过。

## fresh 的软件模型

完整内部 Flash 清除 `[0, 0x1000000)` 必须有**独立**擦除确认和可恢复备份确认；未知/不兼容旧状态不能进入此分流。模拟计划的 4 个准确文件：

| 文件 | 偏移 | 字节数 |
| --- | --- | --- |
| bootloader.bin | `0x0` | 19,968 |
| partitions.bin | `0x8000` | 3,072 |
| boot_app0.bin | `0xe000` | 8,192 |
| app.bin | `0x10000` | 4,021,152 |

真实 fresh 没有驱动入口。发行脚本使用 `--no-stub erase-flash`，而 esptool 5.3.1 的 `erase_flash` 是 stub-only；本轮没有偷偷改成另一种全片擦除。ROM `FLASH_BEGIN` 区域擦除是后续可能采用的受控替代，须先修订安全合同并验证专用空板。

## 失败与回执

状态机：`IDLE → CONNECTING → CHECKING → PLAN_READY → RECHECKING → WRITING → VERIFYING → VERIFIED_BOOT_PENDING`。模拟 fresh 可在 WRITING 前出现 ERASING。任何失败进入终态 `STOP`；写入/擦除已开始则标记 `POSSIBLY_PARTIAL`。不重试、不改槽、不回退 fresh、不复位、不虚报完成。

串口异常不被吞掉；首次 SYNC 前仅允许有界排空 ROM 启动文本。开始协议后严格检查帧方向、opcode、长度、status、超时和 SLIP。写超时尝试中止在途流，后续命令全部禁止；在途写结果仍按未知处理。串口关闭设有超时，不无限等待。

默认交付构建即使只读计划通过也不允许真实写入。后续显式构建只解锁兼容更新，不解锁 fresh。只读计划、执行确认和驱动地址限制是分开的门槛。
