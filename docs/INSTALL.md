# Installation / 安装

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Download / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Hardware / 硬件](HARDWARE.md) · [Usage / 操作手册](USAGE.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md)

This guide explains how to prepare the TF card, install Draftor Alpha on an ESP32-S3, and update an existing installation.

本指南介绍 Draftor Alpha 的 TF 卡准备、ESP32-S3 固件安装，以及已有设备的固件更新方法。

The current release supports **ESP32-S3 N16R8 (Thick)**. The complete Pico 2 W (Thin) firmware is not included.

本次完整固件适用于 **ESP32-S3 N16R8（Thick）**。Pico 2 W（Thin）的完整版本暂未包含在本次发行中。

**There are two installation procedures:**

**安装分为两种情况：**

- **First installation / 首次安装：** Prepare the TF card and flash the bootloader, partition table, and application onto a matching ESP32-S3 board.

  准备 TF 卡，并向匹配的 ESP32-S3 开发板写入引导程序、分区表和应用固件。

- **Application update / 应用更新：** Replace only the application firmware on a device that already has the correct partition layout and matching resources.

  对已经安装正确分区及配套资源的设备，仅更新应用固件。

**Do not use the first-installation command as a routine update.**

**不要把首次安装命令当作日常固件更新命令使用。**

---

## 1. Before You Start / 安装前准备

### 1.1 Hardware / 硬件

The current Alpha requires:

当前 Alpha 需要以下硬件：

| Component / 部件 | Requirement / 要求 |
|---|---|
| MCU / 单片机 | ESP32-S3 N16R8 · 16 MB Flash, 8 MB PSRAM |
| Display / 屏幕 | Osptek ST7306 reflective LCD |
| Storage / 存储 | TF (microSD) card and compatible SPI adapter |
| Keyboard / 键盘 | Compatible Bluetooth keyboard |
| Power / 供电 | Stable external power compatible with the development board |
| Connection / 连接 | Suitable USB cables for programming and data transfer |

The standalone battery power module is still under development. This Alpha requires external power.

独立电池供电模块仍在开发中。目前的 Alpha 需要外部供电。

**Check the board's voltage, power connections, and GPIO wiring before switching on the device.**

**通电前，请确认开发板的工作电压、供电方式和 GPIO 接线。**

The programming/UART connection and the native USB data connection serve different purposes. UART is used for programming and diagnostics; native USB is used for TXT export and Inbox import.

烧录／UART 接口与原生 USB 数据接口用途不同。前者用于固件烧录和串口诊断，后者用于 TXT 导入导出。

See [Hardware & Wiring](HARDWARE.md) for the actual pin assignments and power considerations.

具体引脚及供电注意事项见[硬件与接线说明](HARDWARE.md)。

### 1.2 Download the Release / 下载发行文件

Download the appropriate files from:

请从以下页面下载本次发行文件：

**[Draftor Alpha · v0.1.0-alpha.1](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)**

| File / 文件 | Purpose / 用途 |
|---|---|
| `Draftor_TF20_Alpha_Firmware.zip` | Firmware and first-installation files / 固件与首次烧录文件 |
| `Draftor_TF20_Alpha_Resources.zip` | Runtime dictionaries, indexes and fonts / 词库、索引及字体 |
| `Draftor_TF20_Alpha_Source.zip` | Source code and resource preparation tools / 源码与资源准备工具 |
| `Draftor_TF20_Alpha_BuildInputs.zip` | Additional inputs for compiling the firmware / 自行编译所需的配套输入 |

If you only want to install the released firmware, use the supplied Firmware and Resources archives. The Source archive also contains the optional resource preparation tool described below.

如果只是安装已经发布的固件，主要使用 Firmware 和 Resources 两个包。Source 包还提供下文介绍的资源准备工具。

BuildInputs is needed when building the firmware from source. It is not required simply to flash the supplied application binary.

BuildInputs 用于自行编译源码。如果只是烧录已经提供的固件，不需要单独执行构建流程。

**Use files from the same release. Do not mix files from different versions.**

**请使用同一版本的配套文件，不要混用不同版本的源码、词库和固件。**

The current archive and firmware filenames are part of the original build identity. Keep them unchanged when following the commands below.

目前的 ZIP 和固件文件名属于原构建标识。执行下列安装命令时，请保留其原始名称。

---

## 2. Prepare the TF Card / 准备 TF 卡

Draftor stores documents and Chinese input resources on the TF card.

草稿机使用 TF 卡保存文档和中文输入法资源。

**If the card contains existing documents, back them up before changing its contents. Do not format or overwrite an existing card simply to install this release.**

**如果 TF 卡中已有文档，请先备份。不要为了安装新版而直接格式化或覆盖现有存储卡。**

Turn off the device and disconnect all power before removing or inserting the TF card.

取出或安装 TF 卡之前，必须先关闭设备并断开电源。

### 2.1 Resource Directory / 资源目录

Extract `Draftor_TF20_Alpha_Resources.zip` on your computer.

在电脑上解压 `Draftor_TF20_Alpha_Resources.zip`。

The archive contains a `Resources/HANIME/IME/` directory. On the TF card, the corresponding resource folders must be placed directly under `HANIME/IME/` at the card root.

压缩包中包含 `Resources/HANIME/IME/` 目录。复制到 TF 卡时，对应资源必须放在存储卡根目录下的 `HANIME/IME/` 中。

The required layout is:

TF 卡中需要保留以下目录结构：

```text
TF card root /
└── HANIME/
    └── IME/
        ├── LEXICON_R3_B2_20261006/
        │   ├── release_manifest.json
        │   ├── release_pin.hpp
        │   ├── CANDIDATE_READY.json
        │   └── [14 resource payload files]
        │
        └── R1_API080/
            └── matched28.hbf1
```

The release contains the complete runtime lexicon and the matching 28px font.

本次发行已经包含完整的运行词库及配套 28px 字体。

**Do not place an extra `Resources` folder at the TF card root.** The firmware expects the paths shown above.

**不要在 TF 卡根目录额外套上一层 `Resources` 文件夹。** 固件需要按照上述路径读取资源。

### 2.2 Copying Resources / 复制资源

For a new TF card, copy the complete release directory and matching font from the extracted Resources archive to their corresponding locations.

如果使用新的 TF 卡，可以将解压后的完整词库目录和配套字体复制到上述位置。

Keep the original directory names, filenames, resource contents, manifest and pin files unchanged.

请保留全部目录名、文件名、资源内容、manifest 和 pin 文件，不要自行修改。

The `CANDIDATE_READY.json` marker indicates a resource directory prepared and verified by the release process. Do not create or edit this marker manually to bypass an incomplete installation.

`CANDIDATE_READY.json` 是资源准备流程产生的就绪标记。不要为了跳过不完整的安装过程，自行创建或修改它。

If `STAGING_INCOMPLETE` is present, the resource directory is not ready for use.

如果资源目录中出现 `STAGING_INCOMPLETE`，表示准备过程尚未完成，不应直接用于运行。

### 2.3 Optional: Resource Preparation Tool / 可选：资源准备工具

The Source archive includes `tools/prepare_tf_resources.py`. It can prepare a new B2 lexicon directory and verify all 14 resource payloads against the release manifest.

Source 包提供 `tools/prepare_tf_resources.py`，可以准备新的 B2 词库目录，并根据发行清单验证全部 14 个资源文件。

The tool does not automatically detect TF cards or modify firmware. You must supply the source and destination paths yourself.

该工具不会自动寻找 TF 卡，也不会修改固件。资源来源路径和目标路径必须由用户明确指定。

Run the following from the extracted `Draftor/` source directory, replacing both example paths with real absolute paths:

在解压后的 `Draftor/` 源码目录中运行以下命令，并把示例路径替换为真实的绝对路径：

```sh
python3 -B tools/prepare_tf_resources.py \
  --resources "/path/to/extracted/Resources" \
  --target "/path/to/TF-card/HANIME/IME/LEXICON_R3_B2_20261006"
```

**This command only checks the source and planned destination. It does not copy files.**

**以上命令只执行检查，不会复制文件。**

Once the paths have been checked, the destination's parent directory `HANIME/IME/` exists, and the target release directory does not already exist, repeat the command with `--write`:

确认路径无误、目标卡中已经存在 `HANIME/IME/` 父目录，且目标版本目录尚不存在后，在同一命令末尾添加 `--write`：

```sh
python3 -B tools/prepare_tf_resources.py \
  --resources "/path/to/extracted/Resources" \
  --target "/path/to/TF-card/HANIME/IME/LEXICON_R3_B2_20261006" \
  --write
```

The tool refuses to overwrite an existing release directory. It copies the B2 resources, verifies the copied payloads, and creates the READY marker after successful verification.

工具不会覆盖已有的同名版本目录。它会复制 B2 资源，检查复制后的内容，并在验证成功后生成 READY 标记。

**The tool prepares the lexicon only. It does not install the font.**

**该工具只准备词库，不负责安装字体。**

Copy `matched28.hbf1` separately from the Resources archive into:

还需要从 Resources 包中单独复制 `matched28.hbf1` 到：

```text
HANIME/IME/R1_API080/matched28.hbf1
```

If the destination already contains a file with the same name, do not overwrite it without checking that it is the correct matching font.

如果目标目录已经存在同名字体，请先确认是否与本版一致，不要直接覆盖。

Finally, safely eject the TF card from your computer.

完成后，在电脑上安全弹出 TF 卡。

---

## 3. First Installation / 首次固件安装

This procedure is intended for a matching ESP32-S3 N16R8 board that does not already have a known compatible Draftor installation.

本节适用于尚未安装匹配 Draftor 固件的 ESP32-S3 N16R8 开发板。

**Flashing firmware can replace existing programs and affect data stored in flash. Back up anything you need before proceeding.**

**烧录固件会写入开发板 Flash，可能覆盖原有程序或影响板载数据。操作前请备份需要保留的内容。**

The published files provide the original firmware and first-installation components. A separate full qualification run on a fresh blank board was not performed for this release.

本次已提供原实测固件及首次安装所需文件，但发行流程没有另外在新空板上完成一次全面实机验收。

### 3.1 Prepare the Computer / 准备电脑

You will need:

电脑端需要：

- Python 3
- A compatible Espressif `esptool` installation
- The extracted Firmware archive
- A confirmed programming/UART connection to the correct ESP32-S3 board

请先确认 Python 3 和 `esptool` 已正确安装，并将开发板通过烧录／UART 接口连接到电脑。

Check that esptool can be launched:

可以先检查 esptool 是否可用：

```sh
python3 -m esptool --help
```

Identify the correct serial port by checking the computer's available ports and confirming which port appears when the intended ESP32-S3 board is connected.

请通过电脑上的串口列表，并结合目标 ESP32-S3 开发板的连接与断开操作，确认实际烧录端口。

**Do not copy a serial-port name from another computer or an old development log.**

**不要照搬其他电脑或旧开发日志中的串口名称。**

If a serial monitor is running, close it before flashing.

如果串口监视器正在运行，请先关闭，避免与烧录工具争用端口。

### 3.2 Firmware Files / 固件文件

Extract `Draftor_TF20_Alpha_Firmware.zip`.

解压 `Draftor_TF20_Alpha_Firmware.zip`。

The first installation uses these four files:

首次烧录需要以下四个文件：

| File / 文件 | Flash offset / 写入偏移 |
|---|---|
| `bootloader.bin` | `0x0` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xe000` |
| `Draftor_Alpha_T06_TF20_R1.app.bin` | `0x10000` |

These offsets belong to the supplied firmware and partition layout. They must not be treated as universal settings for other ESP32-S3 projects.

这些偏移地址对应本次发行的固件与分区布局，不应直接套用到其他 ESP32-S3 工程。

The application partition starts at `0x10000` and has a capacity of `0x200000` bytes.

应用分区起始地址为 `0x10000`，容量为 `0x200000` 字节。

### 3.3 Flash the Device / 烧录设备

Open a terminal in the directory containing the four extracted firmware files. If they are in different directories, use their actual paths in the command.

打开终端，进入上述四个固件文件所在的目录。如果文件位于不同目录，请在命令中填写各自的实际路径。

Replace `YOUR_VERIFIED_UART_PORT` with the actual serial port you have confirmed.

将 `YOUR_VERIFIED_UART_PORT` 替换成已经核实的真实串口。

```sh
python3 -m esptool \
  --chip esp32s3 \
  --port "YOUR_VERIFIED_UART_PORT" \
  --baud 460800 \
  write-flash \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size 16MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0xe000 boot_app0.bin \
  0x10000 Draftor_Alpha_T06_TF20_R1.app.bin
```

This command writes the bootloader, partition table, boot application helper, and Draftor application.

该命令会写入引导程序、分区表、启动辅助文件以及 Draftor 应用固件。

**Do not add a full-chip erase command.** A full erase is not required by this installation procedure and may destroy existing flash data.

**不要额外执行整片 Flash 擦除。** 本安装流程不要求整片擦除，额外擦除可能破坏原有数据。

If flashing fails, check the selected board, serial port, cable, power supply, and esptool output before trying again.

如果烧录失败，请先核对开发板型号、串口、线缆、供电及 esptool 错误信息，再决定是否重试。

Do not switch to another board or change flash offsets simply to make the command succeed.

不要为了让命令执行成功，就随意改用其他板型或修改烧录偏移地址。

---

## 4. Updating an Existing Installation / 更新已有固件

If your device already has the matching partition layout and complete compatible TF resources, you can update only the application.

如果设备已经具备匹配的分区布局和完整的配套 TF 资源，可以只更新应用固件。

**Do not use this method if you have not confirmed the installed partition layout and resource compatibility.**

**如果无法确认现有分区和资源是否匹配，请不要直接采用应用区更新方式。**

Before updating:

更新前请先：

1. Save the current document and back up important manuscripts.

   保存当前文档，并备份重要文稿。

2. Finish USB transfer and other storage operations.

   结束 USB 传输及其他存储操作。

3. Confirm that the installed partition table matches the supplied `partitions.csv` and `partitions.bin`.

   确认现有分区表与发行包提供的 `partitions.csv`、`partitions.bin` 匹配。

4. Confirm that the TF card already contains the matching resources and font.

   确认 TF 卡中已经安装了对应的词库和字体。

5. Stop the serial monitor and verify the programming port.

   关闭串口监视器，确认烧录端口。

Then flash the application only:

之后仅烧录应用固件：

```sh
python3 -m esptool \
  --chip esp32s3 \
  --port "YOUR_VERIFIED_UART_PORT" \
  --baud 460800 \
  write-flash \
  0x10000 Draftor_Alpha_T06_TF20_R1.app.bin
```

This command writes the application at `0x10000`. It does not intentionally replace the bootloader or partition table.

该命令只向 `0x10000` 的应用分区写入固件，不主动替换引导程序或分区表。

A successful flash does not by itself prove that existing documents, resources, or other device state are compatible. Check the device after restarting.

烧录成功不等于原有文档、资源及其他设备状态已经自动验证。重启后仍需确认设备是否正常运行。

---

## 5. First Startup / 首次启动

After preparing the TF card and flashing the firmware:

完成 TF 卡准备及固件烧录后：

1. Disconnect power before installing the prepared TF card.

   先断电，再安装已经准备好的 TF 卡。

2. Check the display, TF adapter, and power connections.

   检查屏幕、TF 模块及供电接线。

3. Power on the ESP32-S3 using a suitable external supply.

   使用符合开发板规格的外部电源启动 ESP32-S3。

4. Wait for the device to finish initialization and display its ready state.

   等待设备完成初始化并进入可操作状态。

5. Connect a compatible Bluetooth keyboard using the device's pairing interface or prompts.

   根据设备实际显示的配对界面或提示，连接兼容的蓝牙键盘。

The firmware's internal build identity is:

固件内部构建标识为：

```text
DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007
```

Its paired runtime resource release is:

对应的运行资源版本为：

```text
LEXICON_R3_B2_20261006
```

If you use serial diagnostics, the UART logging baud rate is **115200**. Use only one serial reader at a time.

如果需要通过串口查看诊断信息，日志波特率为 **115200**。同一时间不要让多个程序占用串口。

### Basic Check / 简单功能检查

Once the device is ready, you can use a disposable test document to check the main functions:

设备就绪后，建议使用不重要的测试文档，简单确认以下功能：

- Enter a short English sentence / 输入一小段英文
- Switch to Chinese input and select a word / 切换中文输入并选词
- Press `F4` to switch the writing layout / 按 `F4` 切换横竖排
- Press `Ctrl+S`, `Cmd+S`, or `F6` to save / 使用保存快捷键
- Reopen the document from the file list / 从文件列表重新打开文档
- Try USB TXT export with `F9` → `E` / 尝试 USB TXT 导出

For complete keyboard and USB instructions, see the [Usage Guide](USAGE.md).

完整快捷键及 USB 操作请参阅[操作手册](USAGE.md)。

These checks help confirm that basic functions are available on your setup. They do not replace long-duration or power-loss testing.

上述检查仅用于确认基本功能在当前硬件上能够运行，不能代替长时间运行或断电恢复测试。

---

## 6. Troubleshooting / 常见问题

### Device Does Not Start / 设备无法启动

Check the power supply, wiring, board model, and flashing results. Confirm that you installed the firmware on an ESP32-S3 N16R8 with the intended partition layout.

请检查供电、接线、开发板型号及烧录结果，并确认固件安装在匹配的 ESP32-S3 N16R8 上。

Do not immediately format the TF card or erase flash when the cause is unknown.

原因不明时，不要立即格式化 TF 卡或擦除 Flash。

### Chinese Input or Fonts Are Missing / 中文输入或字体缺失

Check that both the lexicon directory and the font file are present at their expected locations.

请检查词库目录和字体文件是否都位于指定位置。

In particular, verify:

重点确认：

```text
HANIME/IME/LEXICON_R3_B2_20261006/
HANIME/IME/R1_API080/matched28.hbf1
```

The resource directory must contain the correct manifest, original files, and READY marker, without an unfinished staging marker.

词库目录需要包含对应的 manifest、原始资源文件和 READY 标记，同时不得留有未完成的暂存标记。

Do not edit the resource files or create missing validation markers by hand.

不要自行修改资源文件，也不要手工伪造缺失的验证标记。

### Flashing Fails / 烧录失败

Make sure esptool is targeting the correct serial port and chip. Close other applications using the port, check the USB cable, and read the reported error.

确认 esptool 使用了正确的串口与芯片类型。关闭占用串口的其他程序，检查 USB 线缆，并阅读具体错误信息。

If the board cannot enter programming mode automatically, consult its own BOOT/RESET instructions before attempting manual bootloader entry.

如果开发板无法自动进入烧录模式，请先查阅该板卡的 BOOT/RESET 操作说明。

### USB Drive Does Not Appear / USB 磁盘未出现

Draftor uses the native USB data connection for Export and Inbox. The programming/UART port is a separate interface.

Draftor 使用原生 USB 数据接口提供 Export 和 Inbox，烧录／UART 接口与之不同。

Confirm that you are using the correct data connection and that the device is ready. Finish any active Chinese composition before entering the USB transfer menu.

请确认连接的是正确的数据接口、设备已经就绪，并先完成或取消尚未提交的中文编码，再进入 USB 菜单。

See [USB Document Transfer](USAGE.md) for the full procedure.

完整步骤见[USB 文档交换操作](USAGE.md)。

### Unexpected Errors / 其他异常

If you encounter repeated restarts, allocation failures, or storage errors, stop the current operation and preserve the displayed information.

如果遇到反复重启、内存分配失败或存储错误，请停止当前操作并保留屏幕信息。

Do not repeatedly flash, format the card, or overwrite document directories in an attempt to recover without identifying the cause.

原因不明时，不要反复刷机、格式化存储卡或覆盖文档目录。

You can report reproducible issues through [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues). Please remove private text and device identifiers from logs before sharing them.

如果问题能够复现，可以通过 [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) 反馈。分享日志前，请检查并移除私人文字和设备识别信息。

---

## 7. Further Documentation / 更多文档

- [Hardware & Wiring / 硬件接线](HARDWARE.md)
- [Usage Guide / 操作手册](USAGE.md)
- [Building from Source / 源码构建](BUILD.md)
- [Known Issues / 已知问题](KNOWN_ISSUES.md)
- [Release Notes / 发行说明](RELEASE_NOTES.md)

Draftor Alpha has passed selected device tests, but this release has not been comprehensively qualified for every board, power configuration, or fresh installation scenario.

Draftor Alpha 已通过部分实机测试，但尚未针对所有开发板、供电组合及全新安装环境完成全面验证。

If you are new to ESP32 development, start with the hardware and TF card preparation sections. Check each step before flashing.

如果你第一次接触 ESP32，建议先从硬件接线与 TF 卡准备开始，确认条件满足后再进行烧录。

---

[Project / 项目主页](https://github.com/Ohhellboy/Draftor) · [Download / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Usage / 操作手册](USAGE.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md)
