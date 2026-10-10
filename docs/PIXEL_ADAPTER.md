# Standalone pixel adapter build / 独立像素适配器构建

The source is part of this single full-source tree. This is a resource-free component build example, not a separate publication phase. The maintained 300×400 two-bit pixel packing/color/coordinate adapter has no IME or wiring interface and is not a complete display initialization driver.

本节仅说明完整源码中的独立组件怎样编译。运行此检查不表示整机或 IME 通过。

```sh
mkdir -p TestBuilds
c++ -std=c++17 -Wall -Wextra -Werror tools/check_pixel_adapter.cpp -o TestBuilds/check_pixel_adapter
TestBuilds/check_pixel_adapter
```
