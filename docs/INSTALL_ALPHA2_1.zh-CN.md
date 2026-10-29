> Historical Alpha 2.1 original-board documentation. Current Waveshare instructions are in [INSTALL](INSTALL.md); do not mix target images.

# Alpha 2.1 安装指南

[English](INSTALL_ALPHA2_1.md) | **简体中文**

本指南适用于 ESP32-S3 N16R8（Thick）：16 MiB Flash、8 MiB OPI PSRAM，并使用项目规定的[屏幕和TF卡接线](HARDWARE.md)。

下面的命令适用于 macOS 和 Linux，使用 Python 3 与 esptool 5.3.1。Windows用户需要使用实际的COM端口，并相应调整路径和虚拟环境的启动命令。

请按步骤操作。每一步成功后再继续；出现错误时先停止，不要继续执行清除或写入命令。

## 1. 选择安装方式

| 你的设备目前是什么情况？ | 使用哪种方式？ |
| --- | --- |
| 正在使用Alpha 1 | 先备份文稿和需要保留的数据，再按照本指南全新安装。不建议直接覆盖旧App。 |
| 首次安装的新开发板 | 准备TF资源后，按照本指南全新安装。 |
| 已经使用与本版兼容的双4 MiB分区 | 完成第2、3节的准备后，使用第6节更新App。不要执行第4节的清除命令。 |

Alpha 2.1的App为3,893,280字节，已经超过旧版2 MiB应用分区的容量，因此旧版设备需要重新安装。

全新安装会清除ESP32-S3内部Flash中的旧固件、设置、蓝牙配对信息及其他内部数据。请先做好备份。

这项操作不会格式化TF卡，安装Alpha 2.1也不需要格式化TF卡。不过，TF卡中的文稿仍应另外备份到电脑，不能只保留卡上的一份。

Alpha 2 升级到 Alpha 2.1 时，分区和 format4 运行资源保持兼容。先读取实际分区及启动记录，再按第6节更新当前 App；不要执行第4节的擦除命令。已安装且身份匹配的 Alpha 2 词库和正文字体可以继续使用，新增菜单与查找字形已内置于 App。

## 2. 准备文件、工具和备份

### 下载文件

从Alpha 2.1发行页下载同一版本的两个附件，并解压到同一个文件夹：

```text
Draftor_v0.1.0-alpha.2.1_Firmware.zip  → Firmware/
Draftor_v0.1.0-alpha.2.1_Resources.zip → Resources/
```

普通安装只需要这两包。Source和BuildInputs供自行编译使用，不必为了安装而下载。

安装请以本版Firmware包中的指南为准。 Resources和BuildInputs中可能还附有旧版说明，不要交叉使用不同版本的安装步骤。

### 备份文稿

旧版设备请在仍能正常运行时完成备份。新开发板没有旧数据，可以跳过这一小节。

先保存需要保留的文稿，逐一导出到电脑，并打开导出的文件，确认文字完整。只导出当前稿件，不等于已经备份其他稿件。

把还要保留的输入完成并保存，等屏幕显示“已保存”（旧版显示 Saved）。退出USB传输模式；如果开启了Wi-Fi，也将它关闭。关闭电脑上占用开发板串口的监视窗口，然后关机取出TF卡。

将TF卡中的文稿和其他文件备份到电脑，保留原来的目录结构，同时保留旧固件和配套资源。不要用下载得到的Resources文件夹替换整张TF卡的内容。

### 安装工具并确认开发板

在终端中进入包含Firmware/和Resources/的文件夹，执行：

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install esptool==5.3.1
```

把下面的/dev/tty.YOUR_DEVICE替换为开发板实际使用的串口：

```sh
PORT=/dev/tty.YOUR_DEVICE
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-mac
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset flash-id
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset get-security-info
```

核对结果，确认连接的是你要安装的ESP32-S3，Flash容量为16 MiB。8 MiB OPI PSRAM需要根据板卡型号和规格确认，不能仅凭flash-id的输出判断。

如果开发板启用了 **Secure Boot（安全启动）**或 Flash Encryption（Flash加密），请停止。本指南不适用于这些配置，不要使用--force或修改eFuse来绕过限制。

如果电脑无法让开发板自动进入下载模式，请按板卡说明操作BOOT和RESET按键，再重新执行上面的检查。

### 保留完整Flash备份

已有数据的设备，如需保留恢复旧系统和内部设置的可能，请在清除前完成这一步。它不能代替前面的文稿导出和TF卡备份。

使用一个新的备份目录，不要覆盖旧备份：

```sh
mkdir private-backup-alpha2-1
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset read-flash \
  0x0 0x1000000 private-backup-alpha2-1/full-flash-before-alpha2.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 private-backup-alpha2-1/full-flash-before-alpha2.bin
```

生成的文件应为 16,777,216字节，验证命令也必须成功。把它另存到你的备份位置，私下保管，不要上传到公开仓库。

需要保留的文稿或数据尚未备份好时，不要继续清除Flash。

## 3. 将词库和字体安装到TF卡

### 确认TF卡路径

用读卡器连接已经备份好的TF卡。把下面的路径替换为电脑上实际显示的TF卡目录：

```sh
TF_ROOT=/Volumes/YOUR_TF_CARD
```

这里应填写可以打开、查看文件的目录，不是/dev/disk...这样的磁盘设备名。

接下来先查看TF卡上是否已有：

```text
HANIME/IME/B_SHORT_TEXT_20261018
```

根据实际情况操作：

| 目录情况 | 接下来怎么做？ |
| --- | --- |
| 没有这个目录 | 执行下面的词库安装命令。 |
| 已经从本版完整资源包安装过这个目录 | 跳过词库安装命令，继续检查字体和资源。 |
| 有同名目录，但不确定来源，或上次安装中断 | 先备份这个目录，再将其改成一个未使用的名字，然后重新安装。 |

不要把不同版本的词库文件合并到同一个目录，也不要删除已有文稿、CURRENT或其他文档状态文件。旧版的完整词库目录可以保留。

### 安装词库

仅在目标目录不存在时执行：

```sh
mkdir -p "$TF_ROOT/HANIME/IME"
python3 Firmware/tools/prepare_tf_resources.py --resources Resources \
  --target "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --write
```

工具会复制资源、检查文件，并在全部完成后写入安装完成标记。它不会覆盖已有的同名目录。

如果报错，请先处理错误，不要继续写入固件。

### 安装字体并检查资源

准备字体目录：

```sh
mkdir -p "$TF_ROOT/HANIME/IME/R1_API080"
```

如果目录中还没有matched28.hbf1，执行：

```sh
cp -n Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
```

无论字体是刚复制的还是之前已有，都要执行下面的核对：

```sh
cmp Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
python3 Firmware/check_install.py --firmware Firmware \
  --resources "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --mode fresh
```

如果已有字体与本版不同，先保留原文件并查清原因，不要直接覆盖。资源检查失败时也应停止，不能混用其他版本的文件继续安装。

完成后，TF卡应包含以下内容：

```text
HANIME/IME/B_SHORT_TEXT_20261018/
  shared.hns
  shared.hnv
  xiaohe.hnm
  xiaohe.hnv
  xiaohe_base.hbo
  xiaohe_base.hnv
  full_pinyin.hnm
  full_pinyin.hnv
  full_pinyin_base.hbo
  full_pinyin_base.hnv
  wubi86.hnm
  wubi86.hnv
  wubi86_base.hbo
  wubi86_base.hnv
  RESOURCE_MANIFEST.json
  CANDIDATE_READY.json

HANIME/IME/R1_API080/
  matched28.hbf1
```

检查完成后，在电脑上安全弹出TF卡，再将它插回已经断电的草稿机。

整个过程不需要格式化TF卡。

## 4. 全新安装Alpha 2.1

这一节适用于新开发板，以及已经完成备份、准备重新安装的Alpha 1设备。

已有兼容双4 MiB分区、只想更新App的用户，请跳到第6节。

重新连接开发板，确认仍使用前面检查过的设备和串口，并进入下载模式。

### 清除内部Flash

下面的命令会清除ESP32-S3的全部内部Flash，包括旧固件和内部数据。它不会格式化TF卡。

确认备份齐全后再执行：

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset erase-flash
```

清除失败时不要继续。

### 写入固件

清除成功后，写入同一个Firmware包中的四个文件：

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
```

这些命令中的keep表示保留镜像中原有的Flash参数，不需要另行填写。

写入成功后，核对设备中的内容：

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
```

四个文件都验证成功后，再启动设备。

## 5. 启动并检查

确认配套TF卡已经插好，松开BOOT按键，复位启动开发板。

等待设备进入写作画面，再连接蓝牙键盘；连接就绪时底栏显示“已连接”。清除内部Flash后，原来的配对信息也会消失，可能需要重新配对。

先新建一篇不重要的测试稿，输入一小段中文，选词并保存。离开这篇稿件，再从文件列表中重新打开，确认文字仍然正确。具体键位见[操作手册](USAGE.zh-CN.md)。

如果出现存储挂载、PSRAM、词库检查或保存错误，请停止相关操作，保留备份，不要反复覆盖有问题的稿件。

需要恢复旧文稿时，通过已有的导入功能建立新稿，并打开检查内容。确认恢复无误之前，继续保留电脑上的备份。旧版内部文件系统的备份不能直接写入Alpha 2.1的新分区。

## 6. 已有兼容4 MiB分区：只更新App

这一节仅适用于与本版兼容的双4 MiB分区设备，不清除内部Flash，只写入当前使用的应用分区。

先完成第2节的备份和第3节的TF资源准备。设备进入下载模式后，读取它实际使用的分区表和启动记录：

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0x8000 0x1000 private-backup-alpha2-1/current-partition.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0xe000 0x2000 private-backup-alpha2-1/current-otadata.bin
python3 Firmware/check_install.py --firmware Firmware \
  --resources Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode update \
  --partition private-backup-alpha2-1/current-partition.bin \
  --otadata private-backup-alpha2-1/current-otadata.bin
```

不要用下载包中的分区文件替代刚从设备读取的文件。

检查通过后，工具会输出：

```text
COMPATIBLE_FILES_ONLY
```

同时给出app_offset，即本次应写入App的位置。确认它与设备实际使用的启动分区一致后，再继续。

目前的检查器只接受能够明确判断的一种启动记录组合：一条有效记录和一条空记录。如果提示不兼容，或者无法确定启动位置，请停止，不要猜测地址。

把下面的REPLACE_WITH_CHECKED_OFFSET替换为检查确认的app_offset。不要直接套用别人设备上的0x10000或0x410000。

```sh
APP_OFFSET=REPLACE_WITH_CHECKED_OFFSET
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
```

这条更新路线不写入启动文件、分区表、OTA数据、NVS或另一个App分区。

验证成功后，按第5节启动设备，做一次简短的保存和重开检查。需要恢复旧版时，请先阅读[回退说明](ROLLBACK.md)。

## 附录：文件与分区参考

这一节供核对文件和分区使用。普通安装按前面的步骤操作即可，无需自行调整这些数值。

### 固件文件

发行App标签：

```text
DRAFTOR_ALPHA2_20261019_RELEASE
```

| 文件 | 全新安装时的写入地址 | 文件大小 |
| --- | --- | --- |
| bootloader.bin | 0x0 | 19,968字节 |
| partitions.bin | 0x8000 | 3,072字节 |
| boot_app0.bin | 0xe000 | 8,192字节 |
| Draftor_v0.1.0-alpha.2.1.app.bin | 0x10000 | 3,893,280字节 |

### TF资源

本版有14个词库和索引文件，总计132,153,747字节，另有资源清单、安装完成标记，以及2,464,108字节的字体文件。

资源标识如下：

```text
bundle: 17304919664082782091
format: 4
```

安装检查器用于检查文件是否齐全、版本是否配套；固件运行时仍会验证实际读取的资源内容。

### 内部Flash分区

| 分区 | 起始地址 | 容量 |
| --- | --- | --- |
| NVS | 0x9000 | 0x5000 |
| OTA数据 | 0xe000 | 0x2000 |
| App0 | 0x10000 | 0x400000 |
| App1 | 0x410000 | 0x400000 |
| IME保留区域 | 0x810000 | 0x200000 |
| FFat内部文件系统 | 0xa10000 | 0x5e0000 |
| 崩溃转储区域 | 0xff0000 | 0x10000 |

旧版的应用分区为2 MiB，FFat从0x410000开始；新版扩大了应用分区，FFat移动到0xa10000。因此，不能只替换分区表来保留旧内部文件系统，也不能把旧文件系统镜像直接写入新位置。
