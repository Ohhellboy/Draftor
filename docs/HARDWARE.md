# Draftor Hardware & Wiring / 硬件与接线

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Installation / 安装](INSTALL.md) · [Usage / 操作手册](USAGE.md) · [Original Hardware Overview / 原硬件概览](../HARDWARE.md) · [ST7306 Display Demo / 屏幕示例](../display/st7306/README.md)

This guide describes the hardware used by Draftor Alpha, including its main components, GPIO connections, power requirements, and USB interfaces.

本指南介绍 Draftor Alpha 使用的硬件，包括主要组件、GPIO 接线、供电要求及 USB 接口。

The complete Alpha firmware is intended for the **ESP32-S3 N16R8 (Thick)**. Raspberry Pi Pico 2 W (Thin) is supported by a separate display demo, not by the complete Alpha firmware.

本次完整 Alpha 固件适用于 **ESP32-S3 N16R8（Thick）**。Raspberry Pi Pico 2 W（Thin）目前提供独立屏幕示例，不包含完整 Alpha 写作固件。

**The standalone battery power module is still under development. The current prototype requires external power.**

**独立电池供电模块尚在开发中，目前原型机需要外部供电。**

---

## 1. Hardware Overview / 硬件概览

Draftor is built around a microcontroller, a reflective display, a Bluetooth keyboard, and a TF card.

草稿机的主要硬件包括单片机、反射式液晶屏、蓝牙键盘和 TF 卡。

The current Alpha configuration uses:

目前的 Alpha 硬件配置如下：

| Component / 部件 | Specification / 规格 |
|---|---|
| **MCU / 单片机** | ESP32-S3 N16R8 |
| **Flash / 闪存** | 16 MB |
| **PSRAM / 外部内存** | 8 MB, OPI PSRAM |
| **Display / 屏幕** | Osptek ST7306 reflective TFT LCD |
| **Resolution / 分辨率** | 300 × 400 pixels |
| **Display colours / 显示颜色** | Black, white and red / 黑、白、红 |
| **Input / 输入** | Bluetooth keyboard / 蓝牙键盘 |
| **Storage / 存储** | TF (microSD) card via SPI |
| **Programming / 烧录** | ESP32-S3 programming/UART interface |
| **Document transfer / 文档交换** | Native USB, Mass Storage Class (MSC) |
| **Power / 供电** | External supply compatible with the board / 适配开发板的外部电源 |

The display currently used by the project is the Osptek ST7306-based reflective panel. The independent display demo identifies the panel as **YDP420H003-V3**.

项目目前使用基于 ST7306 的鱼鹰反射屏。独立屏幕示例对应的面板型号为 **YDP420H003-V3**。

The current hardware configuration has been tested in selected scenarios. It should not be assumed that every ESP32-S3 development board, display adapter, or TF card module is directly compatible.

上述配置已经通过部分实机测试，但这不代表所有 ESP32-S3 开发板、屏幕转接板或 TF 卡模块都能直接兼容。

---

## 2. GPIO Connections / GPIO 接线

The following GPIO assignments are used by the current ESP32-S3 Alpha firmware.

以下 GPIO 分配对应当前 ESP32-S3 Alpha 固件。

**The numbers below are ESP32-S3 GPIO numbers, not physical connector pin numbers.**

**下表中的数字是 ESP32-S3 的 GPIO 编号，不是开发板排针上的物理位置编号。**

### 2.1 Display / 屏幕

| Signal / 信号 | ESP32-S3 GPIO | Description / 说明 |
|---|---:|---|
| **SCK** | GPIO 12 | SPI clock / SPI 时钟 |
| **MOSI** | GPIO 11 | Data from MCU to display / 单片机向屏幕发送数据 |
| **CS** | GPIO 10 | Chip select / 片选 |
| **D/C** | GPIO 9 | Data/command selection / 数据与命令切换 |
| **RESET** | GPIO 4 | Display reset / 屏幕复位 |

The display uses an SPI connection. Display MISO is not connected in the current configuration, and the TE signal is not used.

屏幕通过 SPI 连接。当前配置不连接屏幕 MISO，也不使用 TE 信号。

The display's power and ground connections depend on the actual panel adapter. Check the adapter documentation before connecting them.

屏幕的供电及 GND 接法取决于实际使用的转接板。连接前，请先核对转接板的规格说明。

### 2.2 TF Card / TF 卡

| Signal / 信号 | ESP32-S3 GPIO | Description / 说明 |
|---|---:|---|
| **SCK** | GPIO 16 | SPI clock / SPI 时钟 |
| **MISO** | GPIO 18 | Data from TF card to MCU / TF 卡向单片机发送数据 |
| **MOSI** | GPIO 17 | Data from MCU to TF card / 单片机向 TF 卡发送数据 |
| **CS** | GPIO 15 | Chip select / 片选 |

The TF card uses a separate SPI signal assignment from the display.

TF 卡与屏幕使用不同的 SPI 信号引脚。

The current firmware uses a TF card SPI clock of **20 MHz** during normal operation and **400 kHz** during initialization.

当前固件的 TF 卡 SPI 运行频率为 **20 MHz**，初始化频率为 **400 kHz**。

Different TF card adapters, cards, and wiring arrangements may behave differently at this frequency. The published device tests do not establish compatibility with every combination.

不同 TF 卡模块、存储卡和接线方式在这一频率下可能有不同表现。现有实机测试并未覆盖所有组合。

**Use a TF card adapter compatible with the ESP32-S3 board and its logic levels. Do not assume that every module has the same power or signal requirements.**

**请使用与 ESP32-S3 开发板及其逻辑电平匹配的 TF 卡模块。不要假定所有模块的供电和信号要求相同。**

---

## 3. Power & Ground / 供电与接地

### 3.1 External Power / 外部供电

The current Alpha does not include the planned standalone battery power system.

当前 Alpha 不包含计划中的独立电池供电系统。

Power the prototype through an external supply suitable for your development board. Check the actual board's input specifications before making any connection.

请通过适合开发板的外部电源为原型机供电。接线前，应确认实际开发板允许的供电电压及输入方式。

**This document does not define a universal VCC connection for the display, TF adapter, or ESP32-S3 board.**

**本指南不为屏幕、TF 卡模块或 ESP32-S3 开发板规定通用 VCC 接法。**

Different adapters may include their own voltage regulation or level-shifting circuits. Their connector pins must be checked against their own documentation.

不同转接板可能内置稳压或电平转换电路，必须根据各自的规格说明确认供电引脚。

Do not connect a battery, charging board, UPS module, or additional power source to the prototype without first checking the complete power arrangement.

未确认完整供电连接关系前，不要随意接入电池、充电板、UPS 模块或其他附加电源。

### 3.2 Common Ground / 共地

The ESP32-S3, display adapter, and TF card adapter must share an appropriate common ground reference for their signal connections.

ESP32-S3、屏幕转接板和 TF 卡模块在进行信号连接时，需要建立正确的共地关系。

Connect GND according to the board and adapter documentation. Do not infer power pins from the GPIO table.

GND 应根据实际开发板与模块资料连接。不要根据 GPIO 信号表猜测电源引脚。

### 3.3 Using Two USB Connections / 同时连接两个 USB 接口

Some ESP32-S3 development boards provide separate programming/UART and native USB connections.

部分 ESP32-S3 开发板分别提供烧录／UART 和原生 USB 接口。

If both are connected at the same time, check how the board handles USB power. The accepted prototype arrangement used connections to the same computer or USB hub without an additional independent power source.

如果需要同时连接两种 USB 接口，请先确认开发板的 USB 供电路径。此前通过测试的原型配置使用同一台电脑或 USB Hub，没有额外叠加独立电源。

Do not assume that two USB cables, an external supply, and a battery can safely be connected together on every development board.

不要假定任意开发板都能够安全地同时连接两条 USB 线缆、外部电源及电池。

---

## 4. USB Interfaces / USB 接口

Draftor uses USB for two different purposes: programming and document transfer.

草稿机的 USB 主要有两种用途：固件烧录，以及文档交换。

| Interface / 接口 | Purpose / 用途 |
|---|---|
| **Programming / UART** | Flash firmware and read serial diagnostics / 烧录固件、读取串口日志 |
| **Native USB** | Export TXT and receive files through Inbox / TXT 导出及 Inbox 导入 |

### Programming & Serial Logs / 烧录与串口日志

Use the actual programming interface supported by your ESP32-S3 board.

请使用实际开发板提供的烧录接口。

The current firmware's UART diagnostic baud rate is **115200**.

当前固件的 UART 诊断波特率为 **115200**。

Only one program should read the UART port at a time. Close serial monitors before flashing.

同一时间只应有一个程序占用 UART 串口。烧录前，请先关闭串口监视器。

Serial port names differ between operating systems and development boards. Identify the actual connected port rather than copying an example from another machine.

不同操作系统及开发板的串口名称可能不同。请确认当前设备的真实端口，不要直接照搬其他电脑上的端口名称。

See [Installation](INSTALL.md) for the firmware flashing procedure.

固件烧录步骤见[安装指南](INSTALL.md)。

### Native USB & Document Transfer / 原生 USB 与文档交换

The native USB interface provides the document transfer connection.

原生 USB 接口用于文档交换。

Draftor supports two USB MSC modes:

草稿机提供两种 USB MSC 模式：

- **Export / 导出：** `F9` → `E` presents a read-only virtual USB drive containing the current document as `DOCUMENT.TXT`.

  按 `F9` → `E`，电脑上会出现只读虚拟磁盘，其中的 `DOCUMENT.TXT` 是当前文档的快照。

- **Inbox / 导入：** `F9` → `I` presents a temporary writable drive. Files can then be selected and imported as new drafts after the computer safely ejects the drive.

  按 `F9` → `I`，电脑上会出现临时可写磁盘。完成复制并安全弹出后，可以在草稿机上选择文件，导入为新草稿。

The internal TF card filesystem is not directly exposed to the computer through these modes.

这两种模式都不会直接将内部 TF 卡文件系统开放给电脑。

**Always safely eject the USB drive on the computer before ending a transfer.**

**结束传输前，请务必先在电脑上安全弹出 USB 磁盘。**

See the [Usage Guide](USAGE.md) for complete instructions.

完整操作步骤见[操作手册](USAGE.md)。

---

## 5. Bluetooth Keyboard / 蓝牙键盘

Draftor uses an external Bluetooth keyboard for text input.

草稿机使用外接蓝牙键盘输入文字。

The current project is designed around conventional English-layout keyboards, including keyboard layouts commonly used with macOS and Windows.

当前项目以常见的英文键盘布局为基础，包括 macOS 和 Windows 使用的标准英文键盘。

The firmware's printable-key mapping uses UK keyboard decoding in the current product implementation. Some punctuation and symbol keys may behave differently on other national keyboard layouts.

当前产品固件使用 UK 键盘映射处理可打印字符。其他国家或地区的键盘布局，尤其是标点和符号键，可能出现不同的输入结果。

Not every Bluetooth keyboard model or layout has been tested.

并非所有蓝牙键盘型号或布局都已完成兼容性测试。

For English/Chinese switching, candidate selection, document management, and layout shortcuts, refer to the [Usage Guide](USAGE.md).

中英文切换、候选选择、文件管理及横竖排快捷键，请参阅[操作手册](USAGE.md)。

---

## 6. TF Card & Storage / TF 卡与存储

Draftor stores its documents and Chinese input resources locally.

草稿机在本地保存文档与中文输入法资源。

The current Alpha expects the matching dictionary and font resources under `HANIME/IME/` on the TF card.

当前 Alpha 需要在 TF 卡的 `HANIME/IME/` 目录下安装配套词库和字体。

The required resource locations are:

必要的资源路径如下：

```text
HANIME/IME/LEXICON_R3_B2_20261006/
HANIME/IME/R1_API080/matched28.hbf1
```

These are resource directory names used by the released firmware, not instructions to rename the current Alpha version.

这些是本次固件实际使用的资源目录名，不代表需要将当前 Alpha 版本改名。

**Do not remove or replace the TF card while the device is powered.**

**设备通电期间，不要取出或更换 TF 卡。**

Before changing the card, finish input and USB operations, save important documents, and disconnect all power.

更换存储卡之前，请先结束输入和 USB 操作，保存重要文档，并断开全部电源。

For directory layout, resource preparation, and installation steps, see [Installation](INSTALL.md).

具体目录结构、资源准备和安装步骤见[安装指南](INSTALL.md)。

---

## 7. Raspberry Pi Pico 2 W (Thin)

The Raspberry Pi Pico 2 W is part of Draftor's wider development work, but the complete Alpha firmware in this release targets ESP32-S3 N16R8.

Raspberry Pi Pico 2 W 也是草稿机项目开发过的平台，但本次完整 Alpha 固件仅针对 ESP32-S3 N16R8。

A separate [ST7306 display driver and demonstration](../display/st7306/README.md) is available for Pico 2 W and ESP32-S3.

目前已提供适用于 Pico 2 W 和 ESP32-S3 的 [ST7306 独立显示驱动及示例](../display/st7306/README.md)。

The demo displays a black, white, and red test pattern. It does not require the Chinese input engine, editor, TF card, Bluetooth keyboard, or network connection.

该示例用于显示黑、白、红测试图案，不依赖中文输入法、编辑器、TF 卡、蓝牙键盘或网络连接。

**The Pico 2 W wiring is different from the ESP32-S3 GPIO mapping shown above.**

**Pico 2 W 的 GPIO 接线与上文 ESP32-S3 接线表不同。**

Use the wiring table in the [ST7306 demo documentation](../display/st7306/README.md) when testing the display on Pico 2 W. Do not use the ESP32-S3 Alpha flashing instructions on Pico 2 W.

使用 Pico 2 W 测试屏幕时，请按照 [ST7306 示例文档](../display/st7306/README.md)中的接线表操作。不要在 Pico 2 W 上执行 ESP32-S3 Alpha 固件的烧录命令。

The independent ESP32-S3 display demo has been visually tested on hardware. The Pico 2 W demo has been compiled and linked, but has not undergone a device smoke test in this publication cycle.

ESP32-S3 独立屏幕示例已经完成实机显示检查；Pico 2 W 示例已完成编译和链接，但本次发布流程尚未执行对应的实机烟测。

---

## 8. Before Powering On / 通电前检查

Before starting the device for the first time, check the following:

首次通电前，请确认以下事项：

- [ ] **Board / 开发板：** The board is an ESP32-S3 N16R8 matching this Alpha's hardware requirements.

  开发板型号符合当前 Alpha 的硬件要求。

- [ ] **Display / 屏幕：** ST7306 SPI signals are connected to the correct GPIOs.

  ST7306 屏幕的 SPI 信号已正确连接。

- [ ] **TF card / TF 卡：** The TF adapter uses the correct SPI pins, with a compatible power and logic interface.

  TF 卡模块使用正确的 SPI 引脚，供电及信号电平与开发板兼容。

- [ ] **Ground / 接地：** The MCU and peripheral modules have the required common ground connections.

  单片机与外设模块已经正确共地。

- [ ] **Power / 供电：** The external power source is suitable for the board, with no unverified additional power connections.

  外部电源符合开发板要求，没有未经确认的其他供电连接。

- [ ] **Storage / 存储：** The TF card contains the matching dictionary and font resources.

  TF 卡已安装匹配的词库和字体资源。

- [ ] **USB / 接口：** The programming/UART and native USB functions have been identified correctly.

  已确认烧录／UART 与原生 USB 的实际接口。

After checking the hardware, follow the [Installation Guide](INSTALL.md) to prepare the resources and install the firmware.

确认硬件无误后，再根据[安装指南](INSTALL.md)准备资源并烧录固件。

---

## 9. Hardware Troubleshooting / 硬件问题排查

### Blank Display / 屏幕无显示

Check the display's power, ground, SCK, MOSI, CS, D/C, and RESET connections.

请检查屏幕供电、GND、SCK、MOSI、CS、D/C 和 RESET 接线。

Confirm that you are using the correct ST7306 panel and adapter. Do not assume another screen with the same resolution has an identical interface.

确认使用的是匹配的 ST7306 面板及转接板。不要假定其他相同分辨率的屏幕可以直接替换。

You can use the [standalone display demo](../display/st7306/README.md) to test the display independently of the writing application.

可以使用[独立屏幕示例](../display/st7306/README.md)，在不运行完整写作程序的情况下检查显示模块。

### TF Card Not Detected / TF 卡无法识别

Check the TF adapter's power, ground, and four SPI signal connections.

请检查 TF 卡模块的供电、GND 及四条 SPI 信号线。

If the card is detected but Chinese input resources are unavailable, check the directory structure and matching resource files described in [Installation](INSTALL.md).

如果存储卡能够识别，但中文输入法资源无法加载，请检查[安装指南](INSTALL.md)中的资源目录与配套文件。

Do not immediately format the card or replace the release resources when the cause is unknown.

原因不明时，不要立即格式化 TF 卡或替换发行资源。

### USB Connection Problems / USB 连接问题

Confirm whether you are using the programming/UART interface or native USB.

请先确认当前使用的是烧录／UART 接口，还是原生 USB 接口。

For firmware flashing, check the computer's serial port. For document transfer, confirm that the native USB connection is active and that Export or Inbox has been selected on Draftor.

烧录固件时应检查电脑串口；传输文档时应确认原生 USB 已连接，并已在草稿机上进入 Export 或 Inbox 模式。

### Unexpected Reset or Power Problems / 异常重启或供电问题

If the device repeatedly restarts or becomes unstable, check the external supply, wiring, and connected peripherals before changing the firmware.

如果设备反复重启或运行不稳定，请先检查外部电源、接线和外设，再考虑是否需要调整固件。

Do not connect additional power sources or change wiring while the device is powered.

设备通电期间，不要接入额外电源或更改接线。

If a problem can be reproduced, you can report it through [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues), including the board model, connected modules, power arrangement, and observed behaviour.

如果问题能够复现，欢迎通过 [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) 反馈，并尽可能说明开发板型号、连接模块、供电方式和实际现象。

---

## 10. Further Documentation / 更多文档

- [Installation / 安装指南](INSTALL.md)
- [Usage Guide / 操作手册](USAGE.md)
- [Building from Source / 源码构建](BUILD.md)
- [Known Issues / 已知问题](KNOWN_ISSUES.md)
- [ST7306 Display Demo / 独立屏幕示例](../display/st7306/README.md)

This document describes the hardware connections used by the current Alpha prototype. It is not a universal wiring specification for every ESP32-S3 board or ST7306 adapter.

本指南记录的是当前 Alpha 原型机采用的硬件连接方式，并不是适用于所有 ESP32-S3 开发板或 ST7306 转接板的通用接线规范。

The hardware is still evolving, especially its power system. Future revisions may use different connections or components.

草稿机的硬件仍在开发中，尤其是独立供电部分。后续版本可能调整连接方式或采用不同组件。

---

[Project / 项目主页](https://github.com/Ohhellboy/Draftor) · [Download Alpha / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Installation / 安装](INSTALL.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md)
