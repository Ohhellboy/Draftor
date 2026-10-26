# Building Alpha 2.1 / Alpha 2.1 源码构建

Build the Chinese Find, menu and editor interface with the fixed inputs below. Product release names follow [VERSIONING.md](../VERSIONING.md); bgIME keeps its own versions.

使用以下固定输入编译中文查找、菜单和编辑器界面。整机版本遵循 [VERSIONING.md](../VERSIONING.md)，bgIME 独立维护版本号。

## Inputs / 输入

Unzip this version's Source and BuildInputs. `Draftor/` contains Git-tracked project source; `BuildInputs/inputs/` supplies only the eleven entries in `EXTERNAL_INPUTS.json`. Two added inputs are the WenKai UI16 bitmap and exact MiSans 14/16px menu tables. Check [Font Distribution](FONT_DISTRIBUTION.md) before redistributing generated tables. The remainder of BuildInputs holds original licenses, corresponding third-party sources, font/dictionary provenance and the explicit optional format3 input to the current resource generator. It is not an installed SDK or cache. Runtime resource generation is unnecessary for compiling this firmware.

解压同版 Source 与 BuildInputs。`Draftor/` 是拟提交的源码；`BuildInputs/inputs/` 对应清单中的11项配套输入。新增两项为文楷 UI16 位图和 MiSans 14/16px 菜单表；再分发前请阅读[字体分发说明](FONT_DISTRIBUTION.md)。其余内容为原许可、第三方对应源码、字体词库来源及可选资源生成所需的准确前代输入，不是整套已安装 SDK 或缓存。编译不需要重新生成词库。

Install **Python 3, Arduino CLI 1.4.1, Arduino-ESP32 3.3.11 and esp-x32 2601 (GCC 14.2.0)**. The SDK `versions.txt` must match `vendor/versions.txt` (ESP-IDF v5.5.5 b774170ff46; Arduino 0d7a8614b; TinyUSB 53f8c53c2). `DRAFTOR_SDK_SOURCE` points to the Arduino **data directory containing `packages/`**, not its parent. The build makes an owned USB overlay; installed tools/libraries are read-only inputs. CMake 3.20+ and a C++17 compiler are needed only for optional Host checks.

工具链固定为上述版本。`DRAFTOR_SDK_SOURCE` 指向包含 `packages/` 的 Arduino **data目录**。构建创建自身 USB 覆盖层，不修改原安装；可选 Host 检查另需 CMake 3.20+及 C++17 编译器。

An installed SDK may use symlinks for its versioned components. The build receipt records the resolved hardware, compiler and S3 library roots selected through `DRAFTOR_SDK_SOURCE`; dependency checks allow only these declared SDK components and this source tree.

已安装SDK可能用符号链接组织不同版本组件。回执会明确列出经此配置选中的硬件包、编译器及S3库的实际位置；依赖检查只接受这些已声明组件及本源码树。

```sh
cd Draftor
python3 -B tools/install_external_inputs.py --from ../BuildInputs/inputs
export DRAFTOR_SDK_SOURCE=/your/arduino/data
export ARDUINO_CLI=/your/bin/arduino-cli
python3 -B tools/build_candidate.py
```

Use a fresh extracted directory. Outputs are in `TestBuilds/s3/DISPLAY_TX_PROTO/dist/`; receipts are in `evidence/s3/`. No step connects to hardware or downloads a hidden input. The supplied TinyUSB archive is the exact fixed MSC-only component; its sources/overlay and upstream material remain in `vendor/` and BuildInputs. [External input details](EXTERNAL_BUILD_INPUTS.md).

使用全新解包目录；输出与回执分别位于上述目录。构建不会连接设备或暗中下载输入。TinyUSB 使用准确的 MSC-only 配套组件，同时保留源码、覆盖材料及对应上游资料。

## Active resources and configuration / 生效绑定与配置

`release/alpha2/pins/` is the canonical three-pin set. `build_s3.py` copies it to both the Arduino sketch and HanimePinned library, replacing historical local copies. After compilation it reads the actual `.d` dependencies and checks every consumed `release_pin.hpp`, `block_release_pin.hpp`, and `wubi_release_v1.hpp` against those canonical bytes. Arduino's exact leading `#line` directive is the only permitted normalization. The receipt is `PIN_INCLUDE_BINDING.json`; there is no manual JSON edit or hidden research directory.

三份生效 pin 以 `release/alpha2/pins/` 为准，自动复制到 sketch 与 Core 库，并检查编译器真实 `.d` 包含来源。仅允许去掉 Arduino 添加的准确首行 `#line`，不靠手改清单配对。

| Setting / 设置 | Fixed value / 固定值 |
|---|---|
| B block / 分块 | `DRAFTOR_BLOCK_BACKEND=1` |
| Text handoff / 取字衔接 | `HANIME_TEXT_CHAIN=1` |
| Shared short text / format4 | `DRAFTOR_SHARED_TEXT_VARIANT=3` |
| Synchronous delta display / 同步差量显示 | `DRAFTOR_DISPLAY_TRANSACTION=1` |
| Resident location table / 完整定位驻留 | `DRAFTOR_TEXT_RESIDENT=0` |
| Old input/display coordination / 旧协同机制 | `DRAFTOR_INPUT_PRESENT_COORD=0` |
| AUTH / CP / display detail | `0 / 0 / 0` |
| Telemetry profile / capture | `0 / 1` — ordinary input logging muted, AUTH observer disabled |
| SHA batched-final experiment | `HANIME_SHA256_BATCHED_FINAL=0` |
| Output profile / TF runtime | `2 / 20000000 Hz` |
| USB / release build | `1 / 1` |
| Flash / PSRAM | `16M / opi` |

The board uses the exact custom dual-4 MiB partition CSV in `integration/product/partitions/B.csv`. Core CMake version is 0.13.1, Core release identifier 0.13.1-rc.1, API 0.13.0 and Host 0.8.4-rc.1; these are independent of the Draftor release number.

分区取准确双4 MiB CSV；Core、API、Host版本与整机 Alpha 2.1 版本号分别保留。

## Source map and optional checks / 源码与可选检查

Canonical Core is `integration/product/dependencies/core`; `ime/bgime/` is an entry guide, not another Core. Product display uses `integration/product/platform/ST7306_OsptekLpm8.cpp`, which includes the cumulative changes. The independent MIT display demo remains a separate module, not the full firmware's driver source.

Core 只有一个权威实现；整机显示驱动与独立 MIT 演示分开，不升级演示来替代整机实现。

```sh
python3 -B tools/check_alpha2.py
python3 -B tools/generate_format4.py --help
# Optional, potentially slow resource rebuild; never needed merely to compile:
python3 -B tools/generate_format4.py   --input-format3 ../BuildInputs/resource-generation/format3_input   --output generated-format4
```

The resource tool inherits the full identity/default/general-output comparison and proof generation, with explicit input/output paths. It produces only the selected format4 release; it does not rerun tree or layout comparisons. Original dictionary/font reconstruction limits remain in [Resource Status](RESOURCE_STATUS.md). Historical CMake replay suites require their own legacy fixtures and are not substituted for this packaged build or advertised as all passing.

资源工具继承生成时的全量身份、默认/通用文字比较及证明生成，只生成选定的 format4，不重跑历史选型。原始词库/字体重建的既有限制见[资源说明](RESOURCE_STATUS.md)。历史回放测试需要各自旧资料，不冒称已在本包全部通过。

## String implementation at link time / 字符串链接

The build extracts `string-inst.o` from the pinned compiler’s `esp32s3/no-rtti/libstdc++.a` and places it immediately after the Arduino core archive. The standard SDK archives and the later libstdc++ link remain in place. The build records the archive/member identity in `MATTER_TEMPLATE_PROVIDER.json`; no prebuilt object is shipped in Source. Runtime licensing is listed in [Third-Party Notices](../THIRD_PARTY.md).

构建从固定工具链的上述库中提取字符串实现，放在 Arduino core 库之后。SDK 库和后续 libstdc++ 链接仍保留，提取对象的身份写入构建回执；Source 不附带预编译对象。运行库许可见[第三方声明](../THIRD_PARTY.md)。
