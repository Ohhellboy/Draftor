# Build Draftor Alpha 3 for Waveshare

本批目标为微雪 ESP32-S3-RLCD-4.2、ST7305、180°，16 MiB Flash、8 MiB OPI PSRAM。构建以 [Alpha 3 目标清单](../release/alpha3/WAVESHARE_RLCD42.json)中的产品源码提交为准。运行时 TF 文件见 [TF 清单](../release/alpha3/TF_RESOURCES.json)。固件构建无需连接设备或准备 TF。

Use Python 3.11+, Arduino CLI 1.4.1, Arduino-ESP32 3.3.11, esp-x32 2601 (Xtensa GCC 14.2.0) and the ESP32-S3 libraries that come with that core. The SDK `versions.txt` must match the [pinned version file](../vendor/versions.txt), including ESP-IDF 5.5.5. Use the ESP32 Arduino package from [Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html). Set `ARDUINO_CLI` to the installed executable and `DRAFTOR_SDK_SOURCE` to the Arduino data directory that contains `packages/`. The usual source is a local installed SDK; do not set either variable to a serial device.

## Build

Use a fresh checkout of this tag or extract the Source ZIP. Verify the pinned [external input archive and 11 input files](EXTERNAL_BUILD_INPUTS.md). Extract the archive beside the source tree and run from the folder containing `tools/`.

```sh
python3 -B tools/install_external_inputs.py --from ../BuildInputs/inputs
python3 -B tools/build_candidate.py --target WAVESHARE_RLCD42 --rotation 180
```

先配置上面的两个环境变量，再执行命令。原样输入包括修改过的 TinyUSB 组件、字形表和编译用头文件。构建工具在本地工作树中准备 SDK 覆盖层，使用冻结的菜单字形、三份 format4 pin、学习索引 pin 与产品编译参数。对开发机源目录和 Arduino SDK 目录应用前缀映射。本版保留内部 R3 硬件标签；用目标清单里的完整 App SHA-256 辨认实际发行文件。

The full build emits the App, bootloader and partition table under `TestBuilds/s3/WAVESHARE_RLCD42_ROT180/dist/`, with the linked ELF and map in the build outputs. The compiler command, stack gate and dependency receipts are in `evidence/s3/WAVESHARE_RLCD42_ROT180/`. These local receipts can contain the developer’s own paths. The startup initializer comes from the installed Arduino core at `tools/partitions/boot_app0.bin`. [Build identity](../release/alpha3/BUILD_IDENTITY.json)

The downloadable App is the specific recorded build. Independently linked ELF files may have different debug metadata and therefore a different embedded ELF digest. Confirm the board, resource pins and build settings when comparing a new build. The source tree retains historical tools and target records; the Alpha 2.2 build and qualifications are available in the [preceding tag](https://github.com/Ohhellboy/Draftor/tree/v0.1.0-alpha.2.2).

To recreate the 3,587,304-byte learning lookup and its 117,760-byte proof from the published, extracted format4 resources, run the following command. It checks the input hashes, generated output digests and Merkle root before saving the results.

```sh
python3 -B tools/generate_learning_index.py --resources ../Resources/HANIME/IME/B_SHORT_TEXT_20261018 --output generated-index
```
