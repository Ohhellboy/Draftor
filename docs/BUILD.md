# Build / 源码构建

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Download / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Installation / 安装](INSTALL.md) · [Source Structure / 源码结构](SOURCE_INTEGRATION.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md)

Draftor Alpha includes the source code needed to build the ESP32-S3 writing firmware, along with the separately distributed project inputs. This guide covers firmware compilation, Host testing, and building bgIME Core on its own.

Draftor Alpha 提供 ESP32-S3 写作固件的源码，以及单独发行的配套构建输入。本指南介绍固件编译、电脑端（Host）测试，以及独立编译 bgIME Core 的方法。

The complete firmware currently targets **ESP32-S3 N16R8 (Thick)**. The standalone ST7306 display demos have their own [build instructions](../display/st7306/README.md).

当前完整固件针对 **ESP32-S3 N16R8（Thick）**。ST7306 独立屏幕示例有自己的[编译说明](../display/st7306/README.md)。

**Building firmware does not install it on a device.** Flashing and TF card preparation are covered separately in [Installation](INSTALL.md).

**编译不会自动将固件安装到设备上。** 烧录方法和 TF 卡准备步骤请参阅[安装指南](INSTALL.md)。

---

## 1. Build Requirements / 编译环境

The Alpha build uses the following tools and versions:

本次 Alpha 使用以下工具及版本：

| Tool / 工具 | Required version / 版本要求 |
|---|---|
| Python | Python 3 |
| CMake / CTest | 3.21 or later / 3.21 或更新版本 |
| C++ | C++17 |
| Arduino CLI | **1.4.1** |
| Arduino-ESP32 | **3.3.11** |
| ESP32 toolchain | **esp-x32/2601** |
| ESP-IDF components | Fixed versions supplied through Arduino-ESP32 3.3.11 / 使用对应的固定版本 |

The original build used the following upstream revisions:

原构建使用的主要上游版本包括：

- **ESP-IDF:** v5.5.5 · `b774170ff46`
- **Arduino:** `0d7a8614b`
- **TinyUSB:** `53f8c53c2`
- **Espressif lib-builder:** `ee57070`

Additional component versions are recorded in [`vendor/versions.txt`](../vendor/versions.txt).

其他组件的具体版本记录在 [`vendor/versions.txt`](../vendor/versions.txt)。

Install **Arduino-ESP32 3.3.11** using its [official release](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11) or package index. Do not substitute a newer SDK simply because it is available.

请从[官方版本页面](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11)或对应的软件包索引安装 **Arduino-ESP32 3.3.11**。不要直接用更新版本替代。

The SDK, compiler, and their caches are not included in the Draftor release archives. They must be installed separately.

SDK、编译器及其缓存不包含在 Draftor 发行压缩包中，需要另外安装。

The shell commands below use a POSIX-style terminal, such as macOS or Linux. Windows has not been separately validated with these instructions.

下列命令使用 macOS 或 Linux 常见的 POSIX Shell。当前文档尚未单独验证 Windows 环境下的完整执行过程。

---

## 2. Prepare the Source / 准备源码

Download the following archives from the [Alpha release](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1):

从 [Alpha 发行页面](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)下载：

- `Draftor_Alpha_Source.zip`
- `Draftor_Alpha_BuildInputs.zip`

Alternatively, you can check out the corresponding Git tag for the source tree.

也可以通过 Git 获取对应版本标签下的源码。

**Use Source and BuildInputs from the same release.**

**请使用同一发行版本的 Source 与 BuildInputs，不要混用。**

After extraction, the relevant directories should look like this:

解压后，主要目录结构如下：

```text
Draftor/
├── integration/
├── vendor/
├── release/
├── tools/
├── EXTERNAL_INPUTS.json
└── ...

BuildInputs/
├── inputs/
├── font-generation/
├── font-sources/
└── ...
```

The Source archive contains the integrated source tree, but some generated files and resource inputs are distributed separately in BuildInputs.

Source 包包含完整整合源码树，但部分生成文件及资源输入单独放在 BuildInputs 中。

The build uses **76 external project inputs**, listed in [`EXTERNAL_INPUTS.json`](../EXTERNAL_INPUTS.json). All 76 are provided in the matching BuildInputs archive.

构建需要 **76 项外部项目输入**，具体清单见 [`EXTERNAL_INPUTS.json`](../EXTERNAL_INPUTS.json)。本次配套 BuildInputs 已提供全部 76 项。

### Install the Build Inputs / 安装构建输入

Use a fresh, writable copy of the extracted Draftor source.

请使用一份全新、可写的 Draftor 源码副本。

Open a terminal and enter the source root:

打开终端，进入源码根目录：

```sh
cd "/absolute/path/to/Draftor"
```

Set `BUILD_INPUTS` to the directory containing the extracted `inputs/` folder:

将 `BUILD_INPUTS` 设置为解压后的 BuildInputs 目录：

```sh
export BUILD_INPUTS="/absolute/path/to/BuildInputs"
```

Install the supplied files:

安装配套输入：

```sh
python3 -B tools/install_external_inputs.py \
  --from "$BUILD_INPUTS/inputs"
```

The script checks the expected filenames and file sizes, then copies the 76 inputs into their original relative paths in the source tree.

脚本会检查所需文件名称与大小，再将 76 项输入复制到源码树中对应的位置。

**The installer refuses to overwrite existing files.** If the destination already contains these inputs, it will stop. Do not delete or replace files blindly; use a fresh source copy when preparing a new build.

**安装器不会覆盖已有文件。** 如果目标位置已经存在这些文件，程序会停止。不要为了让命令继续运行就随意删除文件；重新准备构建时，建议使用干净的源码副本。

If installation stops partway through, inspect the error before proceeding. The script does not provide an automatic rollback of partially copied files.

如果安装中途停止，请先检查错误。该脚本不提供已复制文件的自动回滚功能。

---

## 3. Build the ESP32-S3 Firmware / 编译 ESP32-S3 固件

### 3.1 Configure the Toolchain / 设置工具链

The build needs to know where Arduino CLI and the installed Arduino-ESP32 SDK are located.

编译程序需要知道 Arduino CLI 与 Arduino-ESP32 SDK 的实际安装位置。

From the Draftor source root, set:

在 Draftor 源码根目录中设置：

```sh
export HANIME_WORKSPACE="$PWD"

export DRAFTOR_SDK_SOURCE="/absolute/path/to/Arduino-data-directory"

export ARDUINO_CLI="/absolute/path/to/arduino-cli"
```

Replace the example paths with your actual installation paths.

请将示例路径替换成真实的安装路径。

`DRAFTOR_SDK_SOURCE` must point to the Arduino data directory containing `packages/esp32/`, not directly to the ESP32 board core or compiler executable.

`DRAFTOR_SDK_SOURCE` 应指向包含 `packages/esp32/` 的 Arduino 数据目录，而不是直接指向 ESP32 核心目录或编译器可执行文件。

For example, the expected directory contains:

例如，该目录下应能找到：

```text
packages/
└── esp32/
    ├── hardware/
    │   └── esp32/
    │       └── 3.3.11/
    └── tools/
        ├── esp32s3-libs/
        │   └── 3.3.11/
        └── esp-x32/
            └── 2601/
```

The build checks the installed SDK against the pinned versions. If the expected version or required components are missing, it stops rather than silently upgrading or substituting them.

构建脚本会检查 SDK 是否与固定版本匹配。如果版本或必要组件不符，程序会停止，而不是自动升级或替换。

### 3.2 Build the Application / 编译应用

Create a directory for temporary build files:

先准备临时文件目录：

```sh
mkdir -p TestBuilds/.tmp

export TMPDIR="$PWD/TestBuilds/.tmp"
export TMP="$PWD/TestBuilds/.tmp"
export TEMP="$PWD/TestBuilds/.tmp"
```

Then run:

执行：

```sh
python3 -B tools/build_candidate.py
```

The script prepares a project-owned copy of the required SDK components and applies Draftor's existing modifications there. It also rebuilds the required 25 TinyUSB source units if the matching archive is not already provided.

脚本会在项目自己的构建目录中准备所需的 SDK 组件，并应用 Draftor 已有的修改。如果没有现成的匹配归档，还会重新编译所需的 25 个 TinyUSB 源文件。

This process is designed not to modify the original Arduino-ESP32 installation.

该流程的设计目标是不修改原始 Arduino-ESP32 安装目录。

The actual firmware compilation is performed by `integration/product/scripts/build_s3.py`, which is called automatically.

实际固件编译由 `integration/product/scripts/build_s3.py` 执行，不需要单独调用。

### 3.3 Build Output / 编译产物

If the build succeeds, the application binary is generated at:

编译成功后，应用固件位于：

```text
TestBuilds/product-s3/dist/HanIME_S3_IME.ino.bin
```

The associated build files and logs are written under `TestBuilds/` and `evidence/product-s3/`.

相关构建文件和日志保存在 `TestBuilds/` 与 `evidence/product-s3/` 目录下。

The firmware uses the following release configuration:

本版固件采用以下主要配置：

| Setting / 配置 | Value / 值 |
|---|---|
| Target / 目标 | ESP32-S3 N16R8 |
| Release build | `DRAFTOR_RELEASE_BUILD=1` |
| USB document transfer | Enabled / 启用 |
| Telemetry profile | `0` |
| Capture gate | `1` |
| Output profile | `2` |
| TF SPI runtime | 20 MHz |
| TF SPI initialization | 400 kHz |
| Save substage diagnostics | Disabled / 关闭 |

The capture gate does not turn on the ordinary BASIC telemetry profile. The firmware retains selected safety and error-reporting mechanisms.

Capture gate 不会自动开启普通 BASIC 遥测模式。固件仍保留必要的安全检查及错误记录机制。

**A newly compiled binary is not necessarily byte-for-byte identical to the device-tested firmware supplied in `Draftor_Alpha_Firmware.zip`.**

**自行编译的固件不一定与 `Draftor_Alpha_Firmware.zip` 中经过实机测试的版本逐字节相同。**

The release's original application binary is the tested distribution artifact. A successful local compilation does not automatically qualify a replacement binary for use on the device.

发行包中提供的原始应用固件才是本次经过实机测试的版本。自行编译成功，并不代表新二进制文件已经完成相同的设备验证。

For flashing instructions, see [Installation](INSTALL.md).

烧录方式请参阅[安装指南](INSTALL.md)。

---

## 4. Host Tests / 电脑端测试

Draftor includes Host tests for its input methods, editor, document handling, USB components, and related behaviour.

Draftor 提供电脑端测试，覆盖输入法、编辑器、文档处理、USB 模块及相关行为。

The public source supports a **224-test portable subset** of the project's 230-test internal regression suite.

公开源码提供 **224 项可移植测试**，是项目原有 230 项内部回归测试的一部分。

Six historical tests depend on comparison materials or test environments that are not included in the public portable workflow. Their exclusion does not mean they have passed.

另外六项历史测试依赖未包含在公开可移植流程中的对照材料或测试环境。没有运行它们，并不代表它们通过。

### Normal Tests / 普通测试

After installing the required BuildInputs, run this command from the source root:

安装所需 BuildInputs 后，在源码根目录执行：

```sh
DRAFTOR_BUILD_JOBS=2 python3 -B integration/tools/host_driver.py \
  public_release \
  --public-validation \
  --release \
  --profile 0 \
  --capture 0 \
  --configure \
  --build \
  --ctest '.*' \
  --test-jobs 2 \
  --allow-fail
```

### ASan / UBSan Tests / 内存与未定义行为检查

To run the same portable tests with AddressSanitizer and UndefinedBehaviorSanitizer:

如需使用 AddressSanitizer 和 UndefinedBehaviorSanitizer 运行相同测试：

```sh
DRAFTOR_BUILD_JOBS=2 python3 -B integration/tools/host_driver.py \
  public_release \
  --public-validation \
  --release \
  --profile 0 \
  --capture 0 \
  --san \
  --configure \
  --build \
  --ctest '.*' \
  --test-jobs 2 \
  --allow-fail
```

These commands use `--capture 0` for the public Host test configuration. The actual ESP32-S3 firmware uses a different capture setting.

以上命令在公开 Host 测试中使用 `--capture 0`，与实际 ESP32-S3 固件的采集配置不同。

### Understanding the Results / 如何理解测试结果

**Not all Host tests currently pass.**

**当前并非所有 Host 测试都能通过。**

The project retains **16 inherited test failures** from earlier development. These have not been marked as fixed.

项目仍保留 **16 项历史继承失败**，尚未将其标记为已修复。

The `--allow-fail` option allows the test driver to preserve the complete results even if CTest reports failures. It does not turn failed tests into successful ones.

`--allow-fail` 允许测试驱动在 CTest 报告失败时继续保存完整结果，并不会将失败的测试算作通过。

Test logs and results are written under:

测试日志和结果保存在：

```text
integration/evidence/test_results/public_release/
```

The six tests outside the portable subset are:

不包含在公开可移植子集中的六项测试为：

- `ble_async_read_lifetime`
- `heap_diagnostic_integrity`
- `ble_notification_handoff`
- `heap_stop_scheduler`
- `descriptor_fallback_case2`
- `descriptor_lifecycle`

A successful Host build or test does not replace actual ESP32-S3 device validation.

Host 编译或测试的结果不能代替真实 ESP32-S3 设备上的验证。

For current limitations, see [Known Issues](KNOWN_ISSUES.md).

更多说明见[已知问题](KNOWN_ISSUES.md)。

---

## 5. Build bgIME Core Separately / 独立编译 bgIME Core

bgIME Core can also be compiled as a standalone C++ library, without building the complete writing application.

bgIME Core 也可以作为独立 C++ 库编译，不需要构建完整草稿机程序。

For a minimal runtime-only build, use:

如果只想编译最小运行时 Core，可以执行：

```sh
cmake -S integration/product/dependencies/core \
  -B TestBuilds/core \
  -DHANIME_BUILD_HOST_ADAPTERS=OFF \
  -DHANIME_BUILD_TOOLS=OFF \
  -DHANIME_BUILD_FIXTURES=OFF \
  -DHANIME_BUILD_TESTS=OFF

cmake --build TestBuilds/core -j2
```

This compiles the Core runtime without Host adapters, test fixtures, tools, or the full runtime dictionaries.

这会单独编译 Core 运行库，不包含 Host 适配层、测试夹具、工具或完整运行词库。

**It does not produce a complete Draftor firmware image.**

**这一操作不会生成完整的草稿机固件。**

The Core source is located under:

Core 源码位于：

```text
integration/product/dependencies/core/
├── include/hanime/
├── src/
├── examples/
└── tests/
```

The standalone build is useful if you want to study bgIME's architecture or integrate its runtime into another project.

如果希望研究 bgIME 架构，或者将它的运行时集成到其他项目，可以从这里开始。

---

## 6. Fonts, Dictionaries & Generated Resources / 字体、词库与生成资源

The release includes the generated inputs and runtime resources required for the current Alpha, together with available source materials and generation tools.

本次发行提供了当前 Alpha 需要的生成文件、运行资源，以及已有的来源材料和生成工具。

Font generation materials and original font sources are provided under:

字体生成材料及原始字体文件位于：

```text
BuildInputs/
├── font-generation/
└── font-sources/
```

You do not need to regenerate the fonts or dictionaries just to compile or install this release.

仅仅为了编译或安装本版 Alpha，**不需要重新生成字体或词库**。

Some historical mixed font and dictionary generation processes cannot yet be reproduced completely from the available original upstream inputs. The current generated files are nevertheless included.

部分历史混合字体及词库的完整上游生成过程尚未全部恢复，但当前版本所需的生成文件已经提供。

This distinction matters: building the current application is not the same task as reconstructing every historical resource-generation step.

需要区分：编译当前程序，与重新复现全部历史资源生成过程，是两件不同的事。

More details:

详细资料见：

[External Build Inputs / 外部构建输入](EXTERNAL_BUILD_INPUTS.md) · [Resource Status / 资源与许可说明](RESOURCE_STATUS.md)

---

## 7. Troubleshooting / 常见问题

### Missing External Inputs / 缺少构建输入

If `install_external_inputs.py` reports missing or size-mismatched files, check that Source and BuildInputs came from the same release and that `BUILD_INPUTS` points to the correct directory.

如果安装脚本提示缺少文件或大小不符，请确认 Source 与 BuildInputs 属于同一发行版本，并检查 `BUILD_INPUTS` 是否指向正确目录。

If destination files already exist, use a clean source copy rather than overwriting them without checking.

如果目标文件已经存在，请优先使用干净的源码副本，不要直接覆盖。

### SDK Version Mismatch / SDK 版本不匹配

The SDK preparation script checks the installed Arduino-ESP32 version and required components against the release's pinned versions.

SDK 准备脚本会检查已安装的 Arduino-ESP32 及其组件是否符合本版固定要求。

If the check fails, verify `DRAFTOR_SDK_SOURCE`, the installed board package, and the required compiler before retrying.

如果检查失败，请核对 `DRAFTOR_SDK_SOURCE`、板卡包版本和编译器。

Do not modify the global SDK installation simply to bypass the checks.

不要为了绕过检查而直接修改全局 SDK 安装目录。

### Build Failure / 编译失败

Check the build output and logs for the first actual error. Common areas to check are missing project inputs, mismatched SDK versions, incorrect environment paths, and unavailable build tools.

编译失败时，应先找到日志中的第一个实际错误。重点检查项目输入、SDK 版本、环境变量路径和构建工具是否齐备。

If the SDK preparation was interrupted or left an incomplete build directory, preserve the error output and use a fresh build copy rather than attempting to repair it by overwriting unknown files.

如果 SDK 准备过程中断，或留下不完整的构建目录，请先保留错误信息，再使用新的工作副本重新准备。

### Host Test Failures / Host 测试失败

Some inherited failures are already known. They should be distinguished from new errors or unexpected changes in the test results.

部分历史测试失败已经记录。应将这些已有失败与新出现的错误区分开来。

Do not assume that a zero exit code from a command using `--allow-fail` means every test passed.

使用 `--allow-fail` 时，不要仅凭命令最终退出码为零就认定全部测试通过。

---

## 8. Further Documentation / 更多文档

- [Installation / 固件安装](INSTALL.md)
- [Hardware & Wiring / 硬件接线](HARDWARE.md)
- [Usage Guide / 操作手册](USAGE.md)
- [External Inputs / 外部构建输入](EXTERNAL_BUILD_INPUTS.md)
- [Resource Status / 资源与许可说明](RESOURCE_STATUS.md)
- [Known Issues / 已知问题](KNOWN_ISSUES.md)
- [ST7306 Standalone Driver / 独立显示驱动](../display/st7306/README.md)

Draftor is still under development. The source code is available for study, modification, and experimentation under its applicable licenses.

Draftor 仍在开发中。相关源码已经公开，可以在各自许可条件下研究、修改和实验。

The provided firmware has passed selected device tests, but building your own binary does not automatically provide the same validation.

本次发行固件已通过有限的实机测试，但自行构建的新固件不因此自动获得相同的验证结论。

---

[Project / 项目主页](https://github.com/Ohhellboy/Draftor) · [Download Alpha / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Installation / 安装](INSTALL.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md)
