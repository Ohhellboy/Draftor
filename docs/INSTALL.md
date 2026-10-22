# Installing and upgrading Alpha 2 / Alpha 2 安装与升级

Target hardware: ESP32-S3 N16R8 (Thick), 16 MiB flash, 8 MiB OPI PSRAM and the [existing display/TF wiring](HARDWARE.md). Commands below use Python 3 and esptool 5.3.1 in a macOS/Linux shell. Set the actual serial port and TF mount path; do not copy another device's values. Windows users must use their actual COM port and adapt shell paths/activation commands.

适用硬件：ESP32-S3 N16R8（Thick）、16 MiB Flash、8 MiB OPI PSRAM及[原屏幕/TF接线](HARDWARE.md)。以下命令使用 macOS/Linux shell、Python 3 与 esptool 5.3.1；必须填写实际串口和 TF 挂载路径。Windows 用户需换为实际 COM 端口，并对应调整 shell 路径及环境激活命令。

Run each step only after the previous one succeeds. If a command fails, stop before any erase or write. The commands are separate instructions, not a script to run unattended.

每步成功后再继续；命令报错时先停止，不能继续清除或写入。以下为分步操作，不应整篇作为无人值守脚本执行。

## 1. Choose the installation route / 选择安装路线

| Current installation / 当前情况 | Route / 路线 |
| --- | --- |
| Alpha 1 / 旧版 Alpha 1 | **Back up, erase internal flash, then install Alpha 2 fresh. A direct upgrade is not recommended. / 先备份，清除内部 Flash 后全新安装；不建议直接升级。** |
| New board / 首次安装 | Prepare the TF resources, erase internal flash, then write the supplied boot/partition/App files / 准备 TF 资源，清除内部 Flash 后写入配套启动、分区和 App 文件 |
| Confirmed exact compatible dual-4 MiB layout / 已确认准确兼容双4 MiB布局 | Use the separate App-only route in section 6 / 使用第6节的独立 App-only 更新路线 |

The release App is **2,429,104 bytes**, labelled `DRAFTOR_ALPHA2_20261019_RELEASE`. It does not fit the old 2 MiB slots. The recommended Alpha 1 route is a fresh installation, not an automatic migration of internal data.

本版 App 为 **2,429,104 字节**，标签 `DRAFTOR_ALPHA2_20261019_RELEASE`，装不进旧2 MiB槽。Alpha 1 的推荐路线是全新安装，不是自动保留内部数据的迁移。

**Erasing internal flash removes the old firmware, partitions and all data/state stored there, including internal filesystem data, settings and pairing data. Back up important manuscripts and necessary data before running the erase command. It does not format the TF card, and TF formatting is not required. Keeping the TF card is not a substitute for a separate backup.**

**清除内部 Flash 会删除旧固件、分区及其中所有数据和状态，包括内部文件系统数据、设置、配对信息。执行清除命令前，必须备份重要文稿和必要数据。这不等于无损保留旧内部数据；TF 卡不需要格式化，保留 TF 卡也不能代替独立备份。**

## 2. Files, tools and backups / 文件、工具与备份

Download these two matching attachments from the [Alpha 2 release](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) once published, and extract them side by side. The link is pending while this is a local review copy.

发布后从 [Alpha 2 发行页](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2)取得以下同版附件，解压到同一父目录；本地待审期间下载链接尚未上线。

```text
Draftor_v0.1.0-alpha.2_Firmware.zip  -> Firmware/
Draftor_v0.1.0-alpha.2_Resources.zip -> Resources/
```

Source and BuildInputs are needed for building, not for ordinary installation. Use this updated guide in Firmware or Source; unchanged BuildInputs/Resources archives contain older guide copies. Their firmware inputs and resource payloads remain compatible.

普通安装不需要 Source 和 BuildInputs；自行构建时再取这两包。安装以新版 Firmware 或 Source 中的本指南为准，原样复用的 BuildInputs/Resources 内仍附有旧说明，配套输入和资源未变。

1. While the old firmware still runs, save important manuscripts, export them to a computer and **open the exported files to check the text**. Export each needed manuscript; a current-document export is not a backup of all drafts. Preserve native TF manuscripts, directory/state files and any other necessary card data separately. Keep the old firmware and matching resources.
2. Finish composition, wait for Saved, leave USB transfer and turn Wi-Fi off. Close any serial monitor. Power down before moving the TF card. Back up the card before installing resources; never replace the entire card with the Resources folder.
3. If you need the previous internal state for rollback, also retain a verified private full-flash backup using the commands below. This does not replace readable text exports or the TF backup.

1. 在旧固件仍可运行时，保存重要文稿，导出到电脑并**打开文件核对正文**。逐一导出需要保留的稿件，当前稿导出不等于全部稿件备份。另存 TF 原生文稿、目录/状态及其他必要文件，保留旧固件及配套资源。
2. 处理原码、等 Saved、退出 USB、关闭 Wi-Fi，关闭电脑串口监视器。关机后再移卡，安装资源前备份 TF；不要用 Resources 文件夹替换整张卡。
3. 如需恢复旧内部状态，再按下文读取并核验完整 Flash，私下保管；它不能代替可读正文和 TF 备份。

Run commands from the directory containing `Firmware/` and `Resources/`. For example, set `PORT` to your board's serial port; `/dev/tty.YOUR_DEVICE` is a placeholder that must be replaced.

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

Confirm the intended ESP32-S3 and 16 MiB flash. The board specification must also confirm N16R8/8 MiB OPI PSRAM; flash-id alone cannot establish PSRAM. If flash encryption or Secure Boot is enabled, stop: these plain-image instructions do not cover that configuration. Do not use `--force` or change eFuses to bypass it. If automatic boot entry fails, use the board's documented BOOT/RESET sequence and retry the same identification checks.

确认是目标 ESP32-S3、16 MiB Flash，并按板卡规格确认 N16R8/8 MiB OPI PSRAM，不能只靠 flash-id 判断 PSRAM。若启用 Flash 加密或 Secure Boot，本指南的明文镜像流程不适用，应停止，不用 `--force` 或改 eFuse 绕过。不能自动进入下载模式时，按板卡说明使用 BOOT/RESET，再执行相同身份核对。

For an existing device, use a new empty private backup directory; never overwrite an older backup:

有数据设备请使用新的空备份目录，不覆盖旧备份：

```sh
mkdir private-backup-alpha2
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset read-flash \
  0x0 0x1000000 private-backup-alpha2/full-flash-before-alpha2.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 private-backup-alpha2/full-flash-before-alpha2.bin
```

The file must be 16,777,216 bytes and verification must succeed. Keep it private and separately copy it to your backup location. Do not proceed with erasure if readable manuscript backups or required data are missing.

完整备份必须为16,777,216字节且核验成功，另存到自己的备份位置，不公开上传。文稿或必要数据尚未备妥时，不能继续清除。

## 3. Install the matching TF resources / 安装配套 TF 资源

Mount the backed-up TF card through a card reader. Set `TF_ROOT` to its actual mount directory, not a raw disk device. This example path is a placeholder. The existing staging helper below only installs a **new** independent resource directory, verifies its contents and writes Ready last. It refuses to replace an existing directory.

用读卡器挂载已备份 TF，将 `TF_ROOT` 改为实际挂载目录，不能填原始磁盘设备。下例路径为占位值。既有安装工具只建立**新的**独立资源目录，核验内容并最后写 Ready，拒绝覆盖已存在目录。

```sh
TF_ROOT=/Volumes/YOUR_TF_CARD
mkdir -p "$TF_ROOT/HANIME/IME"
python3 Firmware/tools/prepare_tf_resources.py --resources Resources \
  --target "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --write
```

If this exact release directory is already present and its provenance is known, retain it and run the checker below before use. If its origin is uncertain or it is incomplete, back it up and rename it to a different unused name, then stage a fresh independent directory. Do not combine format2 files with format4 or invent a Ready marker. Do not delete documents, CURRENT, state files or old complete resource sets.

如果已有来源明确的本版准确目录，可复用并执行下文检查；如果来源不明或不完整，先备份并改为另一个未占用的目录名，再完整安装。不要混合 format2/format4，不伪造 Ready，不删除文稿、CURRENT、状态文件或旧完整配套。

Copy the font only if it is absent, then compare the bytes. A differing existing font is an installation conflict; preserve it and resolve the mismatch rather than overwriting it blindly.

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

The final card paths are:

```text
HANIME/IME/B_SHORT_TEXT_20261018/
  shared.hns, shared.hnv
  xiaohe.hnm, xiaohe.hnv, xiaohe_base.hbo, xiaohe_base.hnv
  full_pinyin.hnm, full_pinyin.hnv, full_pinyin_base.hbo, full_pinyin_base.hnv
  wubi86.hnm, wubi86.hnv, wubi86_base.hbo, wubi86_base.hnv
  RESOURCE_MANIFEST.json, CANDIDATE_READY.json
HANIME/IME/R1_API080/matched28.hbf1
```

There are 14 payload files (132,153,747 bytes), plus the manifest/Ready and 2,464,108-byte font. The shared bundle is `17304919664082782091`, format4. The checker verifies installation shape/pairing; compiled pins authenticate actual consumed bytes at runtime. Safely eject the TF card and put it back with the device powered off. **Do not format the card.**

14个资源文件合计132,153,747字节，另有清单/Ready和2,464,108字节字体。共享 bundle 为 `17304919664082782091`、format4。检查器核对安装结构及配对，固件可信绑定继续认证实际消费字节。安全弹出 TF，在设备断电时放回。**不格式化 TF。**

## 4. Fresh installation: erase, write and verify / 全新安装：清除、写入、核验

This section applies both to a new board and to an Alpha 1 device **after the backups in section 2 are complete and checked**. Reconnect the device, confirm the same port/identity and enter download mode. The next command erases all internal flash. The TF card is unaffected by this command; its separate backup remains necessary.

本节适用于新板，也适用于**完成并核对第2节备份后的 Alpha 1 设备**。重新连接，确认同一串口/身份并进入下载模式。下一条命令会清除整个内部 Flash；不会格式化 TF，但 TF 仍应独立备份。

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset erase-flash
```

After a successful erase, write these four files from the same Firmware package. `keep` preserves the supplied image headers.

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

| Supplied file / 配套文件 | Address / 地址 | Bytes / 字节 |
| --- | --- | ---: |
| `bootloader.bin` | `0x0` | 19,968 |
| `partitions.bin` | `0x8000` | 3,072 |
| `boot_app0.bin` | `0xe000` | 8,192 |
| `Draftor_v0.1.0-alpha.2.app.bin` | `0x10000` | 2,429,104 |

The package supplies a dual-4 MiB layout:

| Partition / 分区 | Offset / 偏移 | Size / 容量 |
| --- | --- | --- |
| NVS | `0x9000` | `0x5000` |
| OTA data | `0xe000` | `0x2000` |
| App0 | `0x10000` | `0x400000` |
| App1 | `0x410000` | `0x400000` |
| IME reserved | `0x810000` | `0x200000` |
| FFat | `0xa10000` | `0x5e0000` |
| Coredump | `0xff0000` | `0x10000` |

The old layout used 2 MiB slots and started FFat at `0x410000`; the new App slots overlap old areas and FFat moves to `0xa10000`. Merely replacing the partition file cannot preserve the old internal filesystem. Do not copy an old internal-filesystem image into the new layout.

旧布局使用2 MiB应用槽，FFat始于 `0x410000`；新应用槽占用旧区域，FFat移到 `0xa10000`。单换分区表不能保留原内部文件系统，也不要把旧内部文件系统镜像直接填入新布局。

## 5. First start / 首次启动

After all four files verify, release BOOT and reset the board with the prepared TF card inserted. Confirm Ready and normal BLE keyboard connection; internal pairing data may need to be established again after erasure. In a new disposable draft, type a short Chinese sentence, select words, save, leave that draft and reopen it from the file list to check the text. See [Usage](USAGE.md) for the keys.

四个文件均核验成功后松开 BOOT，保持配套 TF 在设备中，复位启动。确认 Ready 和蓝牙键盘连接；清除内部配对状态后可能需要重新配对。新建可丢弃稿，输入短句、选词、保存，离开后从文件页重新打开，核对正文。键位见[操作手册](USAGE.md)。

If mounting, PSRAM, resource authentication or saving fails, stop the relevant operation and keep backups intact. Do not fabricate Ready or repeatedly overwrite a problematic document. Restore required text through the supported document import path, as new documents; keep backups until the recovered text is checked. Same-version save/reopen evidence does not prove every cross-version native-state restore.

若挂载、PSRAM、资源认证或保存报错，停止相关操作并保留备份；不伪造 Ready、不反复覆盖问题稿。需要恢复正文时，通过支持的导入功能新建文稿并核对，在此之前保留备份。同版本保存重开证据不能代替所有跨版本原生状态恢复验证。

## 6. Existing exact compatible 4 MiB layout / 已有准确兼容4 MiB布局更新

This route **does not erase internal flash** and writes only the App to the confirmed currently selected slot. First make the backups in section 2 and install/retain the matching TF resources in section 3. Obtain the actual current partition and OTA bytes while the device is in download mode; package defaults cannot substitute for them:

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

The checker must report `COMPATIBLE_FILES_ONLY` for the exact table above. It verifies OTA CRC/settled state and reports `app_offset`. It accepts the known unambiguous one-valid/one-erased record pattern; other populated/ambiguous/pending/rollback states are refused. Stop if refused, rather than editing the data to pass. Match the result to the actual boot selection.

检查器必须确认上表的完整布局并输出 `COMPATIBLE_FILES_ONLY`，校验 OTA CRC和稳定状态后给出 `app_offset`。只接受已有明确的一条有效/一条擦空记录；其他双记录、歧义、待确认或回滚状态会拒绝。拒绝时停止，不改数据绕过，并将结果与实际启动选择对应。

Set `APP_OFFSET` to that **checked** value. The example placeholder below must be replaced; the address is not universally `0x10000` or `0x410000`.

将以下占位值替换为**检查确认的** `app_offset`，不能一律套用 `0x10000` 或 `0x410000`：

```sh
APP_OFFSET=REPLACE_WITH_CHECKED_OFFSET
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.app.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.app.bin
```

Do not write bootloader, partitions, OTA data, NVS or the other App slot on this route. Then reset and perform the short save/reopen check in section 5. For rollback conditions, see [Rollback](ROLLBACK.md).

此路线不写启动文件、分区、OTA、NVS或另一个 App 槽。完成后复位，并按第5节做简短保存/重开检查。回退条件见[回退说明](ROLLBACK.md)。
