# Draftor R1 浏览器底层能力审计

日期：2026-11-19。仅源码与协议审查；未连接 USB、未读取设备、未擦除/写入真实 Flash。本审计不能替代目标板 DEVICE_PENDING 验收。

## 结论

**浏览器存在等价 ROM 协议路径，可做 SOFTWARE_READY_FOR_DEVICE_TEST 实现；无须加载 stub。** ESP Web Tools 10.5.2 的现成安装流程不能保持 Draftor 安全合同；建议只复用锁定 esptool-js 0.7.1 的 Web Serial/SLIP Transport，使用独立受控 ESP32-S3 ROM 命令层。

关键澄清：Python esptool 的 no-stub 慢读是 **ROM 0x0e READ_FLASH_SLOW，每命令固定64字节返回**，并非通过 SPI 控制器寄存器每次读4字节。浏览器可发送同一协议。设备时序/USB可靠性仍只能由下一阶段实机验证；不可从 mock 通过推断实机通过。

Fresh 本轮只模拟，真实 fresh 应硬禁用。发行 Python 安装器 `--no-stub erase-flash` 与其锁定 esptool 5.3.1 的能力不匹配：`erase_flash()` 标记为 stub-only。官方无 stub 区域擦除通过 `FLASH_BEGIN` 实现，理论可覆盖16MiB，但那是另一个需要独立真机验证的流程，不能默默替换后宣称发行 fresh 已经验收。

## 精确版本与本地证据

- ESP Web Tools **10.5.2**, commit `5925f9e20ed1d9a2c3f40760b5cb7879854d1736`；目录 `esp-web-tools/`。
- esptool-js **v0.7.1**, annotated tag object `aecaafdae3778599b59a6bf8697ed616df9cf75a`, commit `d5b779b0314bdec367125f4c7e3c6d2206b14311`；目录 `esptool-js/`。
- Python esptool **v5.3.1**, commit `0d2dfefe029eb48c23ddde61f9118b32d39dc7b9`；目录 `esptool-py/`。本目录从5.1.0检出后又获取并切换至5.3.1，最终全部Python结论以5.3.1为准。
- 上述目录只含上游源码，无项目设备数据。

## ESP Web Tools 为什么不能直接接入

`src/flash.ts` 第65行调用 `ESPLoader.main()`；其后按 `chipFamily` 加 `serialType` 选包，不证明微雪PCB。第176行附近按 `eraseFirst` 执行 `eraseFlash()`。第196行 `writeFlash()` 未传 `calculateMD5Hash`，不能形成独立写后内容核验。错误路径和完成路径均调用 `hardResetDevice()`，不保留 ROM 现场。`src/install-dialog.ts` 第355–375行对无 Improv 固件默认走清空安装。

依据：[flash.ts](https://github.com/esphome/esp-web-tools/blob/5925f9e20ed1d9a2c3f40760b5cb7879854d1736/src/flash.ts)、[install-dialog.ts](https://github.com/esphome/esp-web-tools/blob/5925f9e20ed1d9a2c3f40760b5cb7879854d1736/src/install-dialog.ts)。

## esptool-js 0.7.1 需要避开的高层默认

- `main()` 第1595行：读 MAC，并无条件 `runStub()`；不能使用。
- `connect()` 第678行：即使 attempts=1，也存在 `identifyChip` 失败后的断开重连；`_connectAttempt()` 第515行内部又循环5次sync。严格不重试应避开此路径。
- `getSecurityInfo()` 第800行：20字节失败后自动改为12字节重发。限定S3应只请求20字节；缺失/错误立即STOP。
- `readFlash()` 第1437行只发送0xd2 stub协议，没有no-stub慢读。
- `flashSpiAttach()` 第998行仅发4字节参数；S3 ROM需要8字节。
- 构造器 `FLASH_WRITE_SIZE=0x4000` 是stub块长；S3 ROM块长应为0x400。
- `flashBlock()` / `flashDeflBlock()` 默认最多3次重试；本项目应单发命令，任何错误停止。
- `flashMd5sum()` 第1405行把ROM的32字节ASCII摘要又hex编码，得到64字符错误结果；应按ASCII读取并严格校验32位hex。
- `readReg()` 直接取command返回的value，未检查ROM status。受控层应对所有寄存器读检查应答状态。
- `src/targets/esp32s3.ts` 的 `usesUsbJtagSerial()` 将旧UARTDEV值3当作JTAG；该旧值实际上属于USB-OTG。不要调用；以5.3.1的VID/PID判定为准。
- `Transport.readLoop()` 会吞下并继续 BufferOverrun/Framing/Break/Parity 错误；若合同要求任意读错STOP，应覆盖/包装使首个异常标记fatal并中止后续写命令。`Transport.write()` 的writer释放没有finally，发送异常后可锁住stream；受控封装需保证释放并停止，不能无限等disconnect。

依据：[esploader.ts](https://github.com/espressif/esptool-js/blob/d5b779b0314bdec367125f4c7e3c6d2206b14311/src/esploader.ts)、[webserial.ts](https://github.com/espressif/esptool-js/blob/d5b779b0314bdec367125f4c7e3c6d2206b14311/src/webserial.ts)、[esp32s3.ts](https://github.com/espressif/esptool-js/blob/d5b779b0314bdec367125f4c7e3c6d2206b14311/src/targets/esp32s3.ts)。

## 受控连接与目标识别

1. `navigator.serial.requestPort()` 必须由真实用户手势触发；不能在软件测试调用真实端口。
2. 建议R1硬件路径只接受 `port.getInfo()` 的 `usbVendorId=0x303a, usbProductId=0x1001`（Espressif USB Serial/JTAG）。这个条件只证明连接类型，**仍必须人工确认微雪PCB**。无VID/PID或外部UART/OTG应STOP并说明R1未覆盖，不能猜测。
3. 用户先手动进入ROM下载模式。Transport只open，不调用setSignals、不DTR/RTS reset、不自动changeBaud、不自动重试。直接在一个已知波特率连接；115200为保守初始值，USB/JTAG对该设置的实际影响需实测。
4. 单次SYNC opcode0x08，payload `07 07 12 20` 后接32个 `55`。处理首个响应再消费7个响应，共8个；不能把第2–8个当成下一命令回复。每个均需严格包头/长度/op/status检查；全零value是stub线索，保守实现可要求每个value非零，任意不一致STOP。
5. GET_SECURITY_INFO opcode0x14，空payload，S3应返回20字节数据，加4字节ROM尾部。严格命令层检查包头declared length与实际长度；返回方向=1，op与请求一致。依照 esptool 5.3.1 check_command()，尾部为 status、error reason、两字节 reserved，仅 status[0] 非零表示失败；成功的 `00 00 FF FF` 合法。空 SLIP 分隔帧跳过，非空截断或格式异常仍停止。一个命令在途，禁止并行设备命令。
6. 安全数据布局：flags u32LE、flash_crypt_cnt u8、key_purposes[7]、chip_id u32LE、api_version u32LE。chip_id必须9。flags bit0=SECURE_BOOT_EN，bit2=SECURE_DOWNLOAD_ENABLE；任何一项为1均STOP。Flash encryption enabled = popcount(flash_crypt_cnt)奇数；偶数仅说明当前未启用，仍需其值落在S3的3位crypt count已知范围0..7。可取更保守策略count必须0，但应注明比发行安装器更严格。
7. 已定义flags位0..10，mask=0x7ff；未知高位非0建议STOP。flags内其他已知位不是全部都意味着启用安全启动，不能伪称flags!=0就一定有加密；若产品选择全0接受集合，要说明这是保守限制。
8. api_version的语义是ROM API/ECO版本，并无此审计能证实的单个固定值白名单；不要凭猜测限制为某个值；检查正确S3 chip id、完整结构和已知flags。若无法解释布局则STOP。

依据：[Python loader.py security/sync/USB判定](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/loader.py#L1185)、[get-security-info加密计数判断](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/cmds.py#L2388)。

## S3 USB/JTAG WDT/SWD 精确序列

仅在chip_id=9、安全已知且非secure-download、确认ROM非stub、VID/PID=303a:1001之后执行。该步骤仅写易失控制寄存器，不擦除/写Flash；需在只读检查说明中区分“Flash只读”与“任何寄存器零写入”。

每个WRITE_REG opcode0x09 payload为四个u32LE `[address,value,0xffffffff,0]`，每个READ_REG opcode0x0a payload为u32LE地址。依次：

1. WRITE_REG(0x600080b0, 0x50d83aa1) — 解锁RTC WDT。
2. WRITE_REG(0x60008098, 0) — 禁用RTC WDT。
3. WRITE_REG(0x600080b0, 0) — 重锁。
4. WRITE_REG(0x600080b8, 0x8f1d312a) — 解锁SWD。
5. READ_REG(0x600080b4)，取得当前值 `v`。
6. WRITE_REG(0x600080b4, `(v | 0x80000000) >>> 0`) — 自动喂SWD。
7. WRITE_REG(0x600080b8, 0) — 重锁。

任一步失败立即STOP，不能开始Flash写入；不要自动reset。如果中途出错，为遵守“不继续设备操作”，不自动补救写寄存器。下阶段手动重入ROM处理。

5.3.1 `ESP32S3ROM._post_connect()` 调用 `disable_watchdogs()`；其USB判定继承 `loader.uses_usb_jtag_serial()`，按VID/PID。先前5.1的UARTDEV_BUF_NO逻辑不适用于当前准确版本。

依据：[S3寄存器定义及disable_watchdogs](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/targets/esp32s3.py#L310)。

## Flash只读准备、读取与独立校验

- SPI_ATTACH opcode0x0d：ROM payload 8字节 `[hspi_arg=0,is_legacy=0]`。
- Flash ID通过SPI控制器发0x9f只读JEDEC命令，继承esptool-js `runSpiflashCommand` 的寄存器布置，但受控命令层严格检查READ_REG状态。S3 base=0x60002000，USR=+0x18，USR1=+0x1c，USR2=+0x20，MISO_DLEN=+0x28，W0=+0x58，CMD=+0x00。读24bits；容量code `(id>>>16)&0xff` 0x18或0x38对应16MiB；全0、全FF、未知code拒绝。值0x38是否需要接受可取保守只0x18且说明范围。
- SPI_SET_PARAMS opcode0x0b，24字节LE词 `[0,0x1000000,0x10000,0x1000,256,0xffff]`，仅设置ROM RAM flash参数结构；必须先独立识别真实容量，不能先设置参数就声称测出16MiB。
- READ_FLASH_SLOW opcode0x0e：8字节 `[offset, n]`，`1<=n<=64`；每次返回64字节数据加ROM状态，即使n不足64也应返回固定64字节。只取前n字节。读取分区0x8000长度0x1000需要64条；OTA0xe000长度0x2000需要128条。所有长度/范围预先检查。
- 可对读取分区/OTA使用0x13 ROM MD5与本地MD5交叉验证，检测损坏/常量假数据；这是额外合理保障，不替代固定分区字节检查和OTA CRC。
- SPI_FLASH_MD5 opcode0x13：16字节 `[offset,size,0,0]`，返回32字节ASCII hex digest加ROM状态。必须解析ASCII，要求32位hex，并与从已SHA-256确认的本地原始bin计算出的MD5比对。
- Python `verify_flash()` 5.3.1本身优先使用独立ROM MD5；只有请求diff且digest不匹配才整段readback。因此单独发0x13与发行独立verify-flash等价，不必为写后核验强制读回全部4MiB。

依据：[ROM慢读](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/targets/esp32.py#L390)、[SPI_ATTACH/PARAMS/MD5](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/loader.py)、[verify_flash](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/cmds.py#L2278)。

## 无stub更新：精确写命令及结束语义

在全部预检/备份确认/只读计划通过、写入前再核对目标状态后：

1. 只接受安全计划返回的0x10000或0x410000；App长度<=0x400000，起始扇区对齐，所有实际擦除/补齐范围均包含于选定4MiB槽。更新不得调用全片erase API。
2. FLASH_BEGIN opcode0x02，S3 payload20字节，LE五词 `[erase_size,ceil(image_length/1024),1024,slot_offset,0]`。ROM upfront擦除本次App覆盖扇区；不能说完全没有erase。上游get_erase_size返回size，硬件以扇区处理；计划应明确最后4KiB扇区补齐范围。
3. FLASH_DATA opcode0x03，payload16字节头 `[1024,sequence,0,0]` 加1024字节数据，末块用0xff补齐。包头checksum为初值0xef逐byte XOR **仅数据块**（不含16字节头）。每块等ACK后才发下一块；失败不重试，停止并记partial状态。
4. 最后一块ACK后**不要发送FLASH_END(0x04)、FLASH_DEFL_END(0x12)、softReset、hardReset**。Python cmds.py第1510行明确ROM的finish会退出bootloader运行用户代码；no-reset写路径因此跳过它。
5. 保持ROM连接，独立发送0x13计算原始App准确长度的MD5，成功后复读分区/OTA，证明本次更新未改变它们。确认是否额外核验未选App/NVS取决于隐私/时间边界；至少所有可写地址范围应由命令审计证明未跨界。
6. 成功/失败都只释放串口，不切换DTR/RTS、不自动启动App。失败不能给成功回执，不能换槽/改fresh/自动重试。用户后续手动重启，属于下一阶段实机动作。

依据：[Python flash_begin/flash_block](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/loader.py#L1096)、[跳过ROM finish](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/cmds.py#L1510)、[no-reset处理](https://github.com/espressif/esptool/blob/0d2dfefe029eb48c23ddde61f9118b32d39dc7b9/esptool/cmds.py#L2448)。

## 自动测试至少应检查

- 完整SLIP mock（不只mock高层read/write）：sync八响应、特殊字节c0/db转义、单次发送、包头声明长度/op/status/截断/超时、错误后零后续Flash命令。
- 0x0e的固定64-byte返回和非64尾块，0x13 ASCII格式/32byte长度；捕获旧0.7.1双hex回归。
- SPI_ATTACH八字节、SET_PARAMS六词、security20byte/错误chip/未知flags/crypt count奇偶；Flash ID容量不可伪装为参数设置结果。
- WDT序列仅在正确S3+JTAG+安全已知后执行；没有setSignals、MEM_BEGIN/MEM_DATA/MEM_END、0xd0/0xd1/0xd2或FLASH_END。
- app0/app1计划到实际FLASH_BEGIN/FLASH_DATA一致；块补齐和扇区边界不越槽；独立MD5后才成功。
- 任一读写/流错误后STOP，不断开重连、不重试、不重置、不切换槽、不fallback fresh。
- fresh mock可实现擦除确认模型，但production transport明确拒绝全片擦除和fresh执行。

## DEVICE_PENDING

目标USB Serial/JTAG上ROM0x0e数据质量与耗时、SPI ID/安全信息的真实应答、WDT控制后长读稳定性、浏览器/OS串口断线表现、ROM0x13写后MD5真实性、现有备份恢复、实际app0/app1启动与文稿/配对保持、fresh空板首装。软件模拟不能将任何项改写为真机通过。
