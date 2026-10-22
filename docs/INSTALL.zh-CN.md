# Alpha 2 安装与升级

[English](INSTALL.md) | **简体中文**

适用硬件：ESP32-S3 N16R8（Thick）、16 MiB Flash、8 MiB OPI PSRAM及[原屏幕/TF接线](HARDWARE.md)。以下命令使用 macOS/Linux shell、Python 3 与 esptool 5.3.1；必须填写实际串口和 TF 挂载路径。Windows 用户需换为实际 COM 端口，并对应调整 shell 路径及环境激活命令。

每步成功后再继续；命令报错时先停止，不能继续清除或写入。以下为分步操作，不应整篇作为无人值守脚本执行。

## 1. 选择安装路线

| 当前情况 | 路线 |
| --- | --- |
| 旧版 Alpha 1 | **先备份，清除内部 Flash 后全新安装；不建议直接升级。** |
| 首次安装 | 准备 TF 资源，清除内部 Flash 后写入配套启动、分区和 App 文件 |
| 已确认准确兼容双4 MiB布局 | 使用第6节的独立 App-only 更新路线 |

本版 App 为 **2,429,104 字节**，标签 `DRAFTOR_ALPHA2_20261019_RELEASE`，装不进旧2 MiB槽。Alpha 1 的推荐路线是全新安装，不是自动保留内部数据的迁移。

**清除内部 Flash 会删除旧固件、分区及其中所有数据和状态，包括内部文件系统数据、设置、配对信息。执行清除命令前，必须备份重要文稿和必要数据。这不等于无损保留旧内部数据；TF 卡不需要格式化，保留 TF 卡也不能代替独立备份。**

## 2. 文件、工具与备份

发布后从 [Alpha 2 发行页](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2)取得以下同版附件，解压到同一父目录。

```text
Draftor_v0.1.0-alpha.2_Firmware.zip  -> Firmware/
Draftor_v0.1.0-alpha.2_Resources.zip -> Resources/
```

普通安装不需要 Source 和 BuildInputs；自行构建时再取这两包。安装以新版 Firmware 或 Source 中的本指南为准，原样复用的 BuildInputs/Resources 内仍附有旧说明，配套输入和资源未变。

1. 在旧固件仍可运行时，保存重要文稿，导出到电脑并**打开文件核对正文**。逐一导出需要保留的稿件，当前稿导出不等于全部稿件备份。另存 TF 原生文稿、目录/状态及其他必要文件，保留旧固件及配套资源。
2. 处理原码、等 Saved、退出 USB、关闭 Wi-Fi，关闭电脑串口监视器。关机后再移卡，安装资源前备份 TF；不要用 Resources 文件夹替换整张卡。
3. 如需恢复旧内部状态，再按下文读取并核验完整 Flash，私下保管；它不能代替可读正文和 TF 备份。

在含 `Firmware/` 与 `Resources/` 的目录执行，`PORT` 必须换成实际开发板串口，以下占位值不能直接使用。

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install esptool==5.3.1
PORT=/dev/tty.YOUR_DEVICE
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-mac
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset flash-id
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset get-security-info
```

确认是目标 ESP32-S3、16 MiB Flash，并按板卡规格确认 N16R8/8 MiB OPI PSRAM，不能只靠 flash-id 判断 PSRAM。若启用 Flash 加密或 Secure Boot，本指南的明文镜像流程不适用，应停止，不用 `--force` 或改 eFuse 绕过。不能自动进入下载模式时，按板卡说明使用 BOOT/RESET，再执行相同身份核对。

有数据设备请使用新的空备份目录，不覆盖旧备份：

```sh
mkdir private-backup-alpha2
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset read-flash \
  0x0 0x1000000 private-backup-alpha2/full-flash-before-alpha2.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 private-backup-alpha2/full-flash-before-alpha2.bin
```

完整备份必须为16,777,216字节且核验成功，另存到自己的备份位置，不公开上传。文稿或必要数据尚未备妥时，不能继续清除。

## 3. 安装配套 TF 资源

用读卡器挂载已备份 TF，将 `TF_ROOT` 改为实际挂载目录，不能填原始磁盘设备。下例路径为占位值。既有安装工具只建立**新的**独立资源目录，核验内容并最后写 Ready，拒绝覆盖已存在目录。

```sh
TF_ROOT=/Volumes/YOUR_TF_CARD
```

先判断目标目录状态，再决定是否执行安装命令：

| 目标目录实际情况 | 操作 |
| --- | --- |
| 目录不存在 | 执行下方新目录安装命令，再进行后面的检查。 |
| 已有来源明确的本版准确目录 | 跳过安装命令，直接进入[字体与配套检查](#字体与配套检查)。 |
| 来源不明或安装不完整 | 先备份并改为未占用的目录名，再按原流程安装完整的新目录。 |

“跳过安装”不等于跳过字体与配套检查。不要混合 format2/format4，不伪造 Ready，不删除文稿、CURRENT、状态文件或旧完整配套。

```sh
mkdir -p "$TF_ROOT/HANIME/IME"
python3 Firmware/tools/prepare_tf_resources.py --resources Resources \
  --target "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --write
```

### 字体与配套检查

字体只在缺失时复制，然后逐字节核对。已有字体不一致时保留原件、先解决配套冲突，不盲目覆盖。

```sh
mkdir -p "$TF_ROOT/HANIME/IME/R1_API080"
cp -n Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
cmp Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
python3 Firmware/check_install.py --firmware Firmware \
  --resources "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --mode fresh
```

最终 TF 卡路径如下：

```text
HANIME/IME/B_SHORT_TEXT_20261018/
  shared.hns, shared.hnv
  xiaohe.hnm, xiaohe.hnv, xiaohe_base.hbo, xiaohe_base.hnv
  full_pinyin.hnm, full_pinyin.hnv, full_pinyin_base.hbo, full_pinyin_base.hnv
  wubi86.hnm, wubi86.hnv, wubi86_base.hbo, wubi86_base.hnv
  RESOURCE_MANIFEST.json, CANDIDATE_READY.json
HANIME/IME/R1_API080/matched28.hbf1
```

14个资源文件合计132,153,747字节，另有清单/Ready和2,464,108字节字体。共享 bundle 为 `17304919664082782091`、format4。检查器核对安装结构及配对，固件可信绑定继续认证实际消费字节。安全弹出 TF，在设备断电时放回。**不格式化 TF。**

## 4. 全新安装：清除、写入、核验

本节适用于新板，也适用于**完成并核对第2节备份后的 Alpha 1 设备**。重新连接，确认同一串口/身份并进入下载模式。下一条命令会清除整个内部 Flash；不会格式化 TF，但 TF 仍应独立备份。

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset erase-flash
```

清除成功后写入同一 Firmware 包的四个文件；`keep` 保留原镜像头参数：

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.app.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.app.bin
```

| 配套文件 | 地址 | 字节 |
| --- | --- | ---: |
| `bootloader.bin` | `0x0` | 19,968 |
| `partitions.bin` | `0x8000` | 3,072 |
| `boot_app0.bin` | `0xe000` | 8,192 |
| `Draftor_v0.1.0-alpha.2.app.bin` | `0x10000` | 2,429,104 |

本包采用双4 MiB布局：

| 分区 | 偏移 | 容量 |
| --- | --- | --- |
| NVS | `0x9000` | `0x5000` |
| OTA 数据 | `0xe000` | `0x2000` |
| App0 | `0x10000` | `0x400000` |
| App1 | `0x410000` | `0x400000` |
| IME 保留区 | `0x810000` | `0x200000` |
| FFat | `0xa10000` | `0x5e0000` |
| 崩溃转储 | `0xff0000` | `0x10000` |

旧布局使用2 MiB应用槽，FFat始于 `0x410000`；新应用槽占用旧区域，FFat移到 `0xa10000`。单换分区表不能保留原内部文件系统，也不要把旧内部文件系统镜像直接填入新布局。

## 5. 首次启动

四个文件均核验成功后松开 BOOT，保持配套 TF 在设备中，复位启动。确认 Ready 和蓝牙键盘连接；清除内部配对状态后可能需要重新配对。新建可丢弃稿，输入短句、选词、保存，离开后从文件页重新打开，核对正文。键位见[操作手册](USAGE.md)。

若挂载、PSRAM、资源认证或保存报错，停止相关操作并保留备份；不伪造 Ready、不反复覆盖问题稿。需要恢复正文时，通过支持的导入功能新建文稿并核对，在此之前保留备份。同版本保存重开证据不能代替所有跨版本原生状态恢复验证。

## 6. 已有准确兼容4 MiB布局更新

此路线**不清除内部 Flash**，只向已确认的当前启动槽写 App。先完成第2节备份及第3节资源准备，再在下载模式读取实际分区与 OTA 字节，不能以包内文件代替现场状态：

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0x8000 0x1000 private-backup-alpha2/current-partition.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0xe000 0x2000 private-backup-alpha2/current-otadata.bin
python3 Firmware/check_install.py --firmware Firmware \
  --resources Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode update \
  --partition private-backup-alpha2/current-partition.bin \
  --otadata private-backup-alpha2/current-otadata.bin
```

检查器必须确认上表的完整布局并输出 `COMPATIBLE_FILES_ONLY`，校验 OTA CRC和稳定状态后给出 `app_offset`。只接受已有明确的一条有效/一条擦空记录；其他双记录、歧义、待确认或回滚状态会拒绝。拒绝时停止，不改数据绕过，并将结果与实际启动选择对应。

将以下占位值替换为**检查确认的** `app_offset`，不能一律套用 `0x10000` 或 `0x410000`：

```sh
APP_OFFSET=REPLACE_WITH_CHECKED_OFFSET
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.app.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.app.bin
```

此路线不写启动文件、分区、OTA、NVS或另一个 App 槽。完成后复位，并按第5节做简短保存/重开检查。回退条件见[回退说明](ROLLBACK.md)。
