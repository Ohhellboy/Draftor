# Hardware connections / 硬件连接

[Original project hardware overview](../HARDWARE.md) · [Independent ST7306 demos](../display/st7306/README.md). The full Alpha firmware below targets the ESP32-S3 Thick version; the separate Pico Thin display demo is not a firmware substitute.

Target: ESP32-S3, 16 MB flash, OPI PSRAM, 300×400 ST7306 Osptek reflective display, SPI TF adapter and Bluetooth keyboard. Pins and product configuration are inherited unchanged from TF20. This describes the accepted hardware, not arbitrary board compatibility.

| Signal / 信号 | GPIO |
|---|---:|
| Display SCK / 时钟 | 12 |
| Display MOSI / 数据 | 11 |
| Display CS / 片选 | 10 |
| Display DC | 9 |
| Display RESET / 复位 | 4 |
| TF SCK / 时钟 | 16 |
| TF MISO | 18 |
| TF MOSI | 17 |
| TF CS / 片选 | 15 |

Use the board and adapter specifications to confirm voltage, ground and power compatibility. Programming/UART and native USB are separate ports: UART is used at 115200 baud for logs; native USB provides the data connection for Export/Inbox. Enumerate current ports instead of copying historical identifiers. Only one reader may own the UART. Confirm the board's supply arrangement before connecting both USB ports; the accepted arrangement used one computer/Hub without an additional supply.

电压与供电连接按实际板卡规格确认，信号共地；不据 GPIO 表猜测电源接法。烧录／UART 日志口与原生 USB 数据口分开，端口须现场枚举。双线供电需核对板卡实际拓扑，不同时随意加电池／其他电源。

TF runtime is 20 MHz; initialization remains 400 kHz. Do not remove the card with any supply connected. Save and leave input/USB activity idle before a separately authorized offline card operation. Preserve documents and the verified resource installation. See [installation](INSTALL.md); the companion Firmware ZIP includes original first-install artifacts; this round did not test a new blank-board installation.
