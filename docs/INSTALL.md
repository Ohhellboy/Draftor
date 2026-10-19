# Installing and upgrading Alpha 2 / Alpha 2 安装与升级

**Local review copy; nothing has been published or flashed by this preparation.** Target: ESP32-S3 N16R8, 16 MiB flash, 8 MiB OPI PSRAM and the existing [hardware wiring](HARDWARE.md). The software does not qualify an untested board or battery arrangement.

**本地待审材料；本轮没有发布或刷写。** 目标为 ESP32-S3 N16R8、16 MiB Flash、8 MiB OPI PSRAM及[原接线](HARDWARE.md)。未据此确认其他开发板或电池方案。

## Choose the right route / 先区分布局

| Your installation / 当前情况 | Route / 路线 |
|---|---|
| Alpha 1 original 2 MiB App slots / 原 Alpha 1 的2 MiB应用槽 | **No App-only update. Migration pending confirmation. / 不能直接更新，迁移材料待确认。** |
| Exact compatible dual-4 MiB table / 准确兼容双4 MiB布局 | Check actual saved partition and OTA state, then App-only / 核对实际分区与活动槽后仅更新 App |
| New blank board / 首次空板安装 | New table + SDK startup files + App0 / 新分区、SDK启动文件及 App0 |

The release still requires slots larger than 2,097,152 bytes; the exact newly built App length is recorded in `Firmware/RELEASE_PAIR.json`. An arbitrary 4 MiB slot is not enough: offsets and the whole table must match. Do not copy an address from another person's session.

发行App仍需要超过旧2 MiB的槽，准确新构建长度见 `Firmware/RELEASE_PAIR.json`。仅有“4 MiB”字样也不够，整张分区表和偏移都要对应；不要照抄他人设备上的写入地址。

## Files and TF resources / 文件与 TF 资源

Download matching `Draftor_v0.1.0-alpha.2_Firmware.zip` and `Draftor_v0.1.0-alpha.2_Resources.zip`. Extract them to `Firmware/` and `Resources/`. Source and BuildInputs are needed only for building or obtaining corresponding third-party source. Future download links are listed in [Release Notes](RELEASE_NOTES.md).

安装使用同版 Firmware 与 Resources；构建或取得第三方对应源码时再下载 Source 与 BuildInputs。下载入口见[发行说明](RELEASE_NOTES.md)，本地预览中链接仍待发布。

Save and independently export important documents first, empty the composition, exit USB transfer, and turn Wi-Fi off. Shut down safely before removing the TF card. Copy only these package paths into the TF root:

先保存并另行导出重要文稿，清空原码、退出 USB、关闭 Wi-Fi；安全关机后才移卡。只把包内以下路径复制到 TF 根目录：

```
HANIME/IME/B_SHORT_TEXT_20261018/  # all 14 payload files + manifest + supplied Ready marker
HANIME/IME/R1_API080/matched28.hbf1
```

Keep `LEXICON_R3_B2_20261006` and every existing document, CURRENT and state file. Do not format the card. The old font path is deliberately retained; if the matching 2,464,108-byte font already exists, compare it with the package before reusing it, and never overwrite a differing file blindly. Copy into a new temporary resource directory, verify it, then rename it to the final independent directory; the supplied `CANDIDATE_READY.json` goes last. Never create a Ready marker to excuse missing files. JSON checks completeness; compiled pins still authenticate the actual consumed bytes.

保留旧 `LEXICON_R3_B2_20261006`、文稿、CURRENT及状态文件，不格式化。字形仍使用原路径；已存在的2,464,108字节字体须与包内文件一致后复用，遇到不同文件不要盲目覆盖。资源先复制到新的临时目录并核对，完成后改为目标独立目录，随包 Ready 标记最后放入。Ready 只说明安装完整，实际消费仍由固件中的可信绑定认证。

The included checker operates on ordinary host files only:

随包检查器只读电脑上的普通文件，不连接或写设备：

```sh
python3 Firmware/check_install.py --firmware Firmware   --resources Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode fresh
```

## Existing compatible 4 MiB layout / 已有兼容4 MiB布局

Before any write, retain a **current private backup** of flash, the partition table, both App slots and OTA selection, plus the TF documents and matching old resources. Do not upload that backup. Obtain the actual current table (`0x8000`, 4096 bytes) and OTA data (`0xe000`, 8192 bytes) with the normal board-specific backup procedure; the files in this release are not evidence of what is on your device. Check identity and flash/PSRAM configuration using that same established procedure.

写入前保留**当前设备的私有备份**：Flash、分区表、两个 App 槽、OTA选择状态，以及 TF 文稿和旧配套资源。不要上传备份。按开发板既有备份流程取得实际分区表（`0x8000`，4096字节）及 OTA 数据（`0xe000`，8192字节），核对设备身份和 Flash/PSRAM；发行包里的表不能代替现场读取。

```sh
python3 Firmware/check_install.py --firmware Firmware   --resources Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode update   --partition private-current-partition.bin --otadata private-current-otadata.bin
```

The checker derives the slot from the saved OTA entry, verifies its CRC and settled state, and reports `app_offset`. It currently accepts the measured, unambiguous one-valid/one-erased entry pattern; two populated entries, pending/rollback states, sequence ambiguity, encryption or a different partition map are refused. Resolve those cases using an appropriate device-specific procedure; do not edit the saved data to make the check pass.

检查器从已保存 OTA 记录校验 CRC、稳定状态并推导活动槽，输出 `app_offset`。目前覆盖已测的“一条有效、一条擦空”明确状态；双记录、待确认/回滚、序号歧义、加密或不同布局会拒绝。此时需另行确认实际启动选择，不得修改记录以绕过检查。

After matching the result to the device's running slot, the App-only command template is:

把结果与设备实际启动槽对应后，App-only 命令模板为：

```sh
python3 -m esptool --chip esp32s3 --port YOUR_PORT --baud 460800   write-flash CHECKED_APP_OFFSET Firmware/Draftor_v0.1.0-alpha.2.app.bin
python3 -m esptool --chip esp32s3 --port YOUR_PORT   verify-flash CHECKED_APP_OFFSET Firmware/Draftor_v0.1.0-alpha.2.app.bin
```

Replace the placeholders with the confirmed values; the template is deliberately not runnable unchanged. Do not write partition, OTA, bootloader or another slot during this route. The slot is **not always `0x10000` or always `0x410000`**. Restore the saved original App in the same slot if returning to the exact prior installation, while preserving its partition/OTA and matching resources. A changed partition or document state needs separate recovery planning.

占位参数必须换成已确认值，此模板不能原样执行。此路线不写分区、OTA、启动文件或另一槽。地址**既不总是 `0x10000`，也不总是 `0x410000`**。恢复准确原状态时，将原 App 写回同槽，并保留原分区/OTA及配套资源；分区或文档状态改变时需单独处理。

## Old 2 MiB layout / 旧2 MiB迁移

The original table has App0 at `[0x10000,0x210000)`, App1 at `[0x210000,0x410000)` and FFat at `[0x410000,0xff0000)`. The new table is:

旧表 App0、App1、FFat 范围如上；新表为：

| Partition / 分区 | Offset / 偏移 | Size / 容量 |
|---|---:|---:|
| NVS | `0x9000` | `0x5000` |
| OTA data | `0xe000` | `0x2000` |
| App0 | `0x10000` | `0x400000` |
| App1 | `0x410000` | `0x400000` |
| IME reserved | `0x810000` | `0x200000` |
| FFat | `0xa10000` | `0x5e0000` |
| Coredump | `0xff0000` | `0x10000` |

New App0 overlaps old App1; new App1 and the IME reservation overlap old FFat. FFat's starting address changes. Copying a new partition table is therefore **not** a complete safe migration. This package supplies the target table and overlap analysis, but not a qualified automatic data/state migration. The old-layout route remains **pending author confirmation and separate validation**. Keep using Alpha 1 until that route is provided; do not erase the chip or try the fresh-install command on a used device.

新 App0 占用旧 App1，新 App1 与 IME 保留区占用旧 FFat，FFat 起点也改变。只换分区表不构成安全迁移。本包给出目标布局及重叠分析，尚未提供经过确认的数据/状态自动迁移流程。旧布局路线仍**待作者确认及单独验证**；在此之前继续使用 Alpha 1，不全片擦除，也不对有数据设备套用空板命令。

## First installation / 首次安装

Only for a genuinely blank compatible board with no state to preserve. The checker above validates the host file combination. It does not mean a fresh board was qualified in this preparation. Use the supplied bootloader from the same release build, SDK `boot_app0.bin`, matching partitions and App. Do not use a private full-flash image. After hardware/identity checks, the separate command is:

仅适用于确为空白、无待保留状态的兼容开发板。上述离线检查验证文件组合，本轮没有新空板实机验收。启动文件来自准确构建/SDK，不使用私人整片备份。核对硬件与身份后，独立命令为：

```sh
python3 -m esptool --chip esp32s3 --port YOUR_PORT --baud 460800 write-flash   0x0 Firmware/bootloader.bin 0x8000 Firmware/partitions.bin   0xe000 Firmware/boot_app0.bin 0x10000 Firmware/Draftor_v0.1.0-alpha.2.app.bin
```

There is no `erase-flash` step. Confirm each written file using `verify-flash`, then start with the prepared TF inserted and check Ready before writing. If TF, PSRAM or resource validation fails, stop and correct the installation; do not manufacture Ready files or repeatedly save over a problem. Follow [Usage](USAGE.md) for saving and exporting your first draft.

这里没有全片擦除步骤。逐项 `verify-flash` 后，插入已准备 TF 启动，先确认 Ready 再写作。挂载、PSRAM或资源认证失败时应先处理安装问题，不伪造 Ready、不反复覆盖保存。首篇稿件的保存和导出见[操作手册](USAGE.md)。

## Document compatibility / 文档兼容范围

The cumulative source retains the document codec, storage model and atomic publication contracts. FW1 compared the actual saved and reopened body successfully in this same version. That does not establish arbitrary cross-version downgrade compatibility. Preserve exports and exact prior firmware/resources/state together; do not promise rollback solely from possession of an old `.bin`.

累计源码保留文档编解码、存储及原子发布合同；FW1 已核验同版本保存/重开的实际正文相同。任意跨版本降级尚未据此证明。回退要共同保留导出稿及准确原固件、资源、状态，不能仅靠一个旧 `.bin` 保证。
