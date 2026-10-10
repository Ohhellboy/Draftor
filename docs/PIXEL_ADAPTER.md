# Pixel Adapter / 像素适配器

Draftor includes a small, independently buildable pixel adapter for its 300 × 400 ST7306 display.

Draftor 提供一个可以独立编译的像素适配组件，用于 300 × 400 分辨率的 ST7306 屏幕。

The adapter handles two-bit pixel packing, colour conversion and pixel coordinates. It is part of the writing application's display code.

该组件负责双位像素打包、颜色转换及像素坐标映射，是草稿机写作程序显示代码的一部分。

Source / 源码：

- `integration/product/platform/st7306_osptek_lpm8_adapter.hpp`
- `tools/check_pixel_adapter.cpp`

## Build & Test / 编译与测试

The adapter can be checked on a computer using a C++17 compiler. No ESP32-S3 board, TF card resources or Arduino SDK is required.

可以使用支持 C++17 的编译器在电脑上运行检查，无需 ESP32-S3 开发板、TF 卡资源或 Arduino SDK。

From the repository root, run:

在仓库根目录执行：

```sh
mkdir -p TestBuilds

c++ -std=c++17 -Wall -Wextra -Werror \
  tools/check_pixel_adapter.cpp \
  -o TestBuilds/check_pixel_adapter

TestBuilds/check_pixel_adapter
```

The check verifies colour conversion round trips and ensures that pixel coordinates map to valid framebuffer byte positions.

该程序检查颜色转换能否正确往返，以及屏幕像素坐标是否映射到有效的帧缓冲区字节位置。

A successful run exits without an error message.

检查通过时，程序会正常退出，不输出错误信息。

## Scope / 功能范围

This adapter is not a complete ST7306 display driver. It does not initialize the display, handle physical wiring or provide Chinese input.

该适配器不是完整的 ST7306 屏幕驱动，不负责屏幕初始化、实体接线或中文输入。

Passing this check does not mean that the complete display, input method or Draftor firmware has been tested.

通过这项检查，并不代表屏幕实机、输入法或完整草稿机固件已经通过测试。

For the standalone display driver, wiring and hardware examples, see [ST7306 Display Demo](../display/st7306/README.md).

独立屏幕驱动、接线及实机示例请参阅 [ST7306 显示模块](../display/st7306/README.md)。
