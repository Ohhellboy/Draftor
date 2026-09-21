# Draftor ST7306 minimal display demo

A standalone white / black / red geometric test pattern for the OSPTEK
YDP420H003-V3 ST7306 display, using the existing Draftor display driver.
Supported boards: **ESP32-S3 N16R8** and **Raspberry Pi Pico 2 W**.

The demo initializes the display, fills white, draws black and red blocks plus
an asymmetric orientation marker, sends one frame, then idles. It needs no
HanIME, fonts, editor, TF card, filesystem, keyboard or network.

## Wiring

GPIO numbers, not connector pin numbers:

| Signal | ESP32-S3 | Pico 2 W |
|---|---:|---:|
| SCK | 12 | 18 |
| MOSI | 11 | 19 |
| CS | 10 | 17 |
| D/C | 9 | 21 |
| RESET | 4 | 20 |
| MISO | not connected | 16 configured; no display readback |
| TE | unused | unused |

Use the display adapter's documented supply and ground connections. Use
display-only wiring; disconnect other Pico SPI peripherals or keep their CS
inactive. The demo does not manage shared peripherals.

## Build

Use the existing **Arduino-ESP32 3.3.11** or **Arduino-Pico 6.0.0** board core,
Arduino CLI, and Python 3. Do not upgrade the toolchain for this demo.
Run from this directory with fresh output directory names:

```sh
python3 scripts/stage_demo.py esp32s3 ../st7306-s3-demo
arduino-cli compile --clean --warnings all --jobs 4 \
  --fqbn 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=dio,PartitionScheme=default,USBMode=hwcdc,CDCOnBoot=default' \
  --build-path ../st7306-s3-build --output-dir ../st7306-s3-output \
  ../st7306-s3-demo/minimal_esp32s3

python3 scripts/stage_demo.py pico2w ../st7306-pico-demo
arduino-cli compile --clean --warnings all --jobs 4 \
  --fqbn 'rp2040:rp2040:rpipico2w:ipbtstack=ipv4only,flash=4194304_0' \
  --build-path ../st7306-pico-build --output-dir ../st7306-pico-output \
  ../st7306-pico-demo/minimal_pico2w
```

If your CLI uses a separate configuration, add `--config-file` followed by its
path to the compile command. The staging script copies only the selected four
driver files and its existing demo, and refuses to overwrite an output directory.
The core's Pico IPv4 default does not enable networking in the demo; no Wi-Fi
or Bluetooth library is included by the sketch.

## Flash ESP32-S3

After a successful local build, use the generated files from the ESP32-S3
output directory. The expected files and offsets are:

| File | Flash offset |
|---|---:|
| `minimal_esp32s3.ino.bootloader.bin` | `0x0` |
| `minimal_esp32s3.ino.partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xe000` |
| `minimal_esp32s3.ino.bin` | `0x10000` |

Use the connected **ESP32-S3** board's actual serial port, identified by unplugging
and reconnecting that board; `arduino-cli board list` can list ports. On macOS
this is usually `/dev/cu.usbmodem...` or `/dev/cu.usbserial...`; on Linux prefer
its matching `/dev/serial/by-id/...`. Do not select a Pico or another serial device.
Keep `--chip esp32s3`; the configuration is 16 MB flash, DIO, 80 MHz, OPI PSRAM.

With an existing esptool 5.x installation, replace `YOUR_S3_PORT` and run manually
from this directory:

```sh
esptool --chip esp32s3 --port YOUR_S3_PORT --baud 460800 \
  --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 ../st7306-s3-output/minimal_esp32s3.ino.bootloader.bin \
  0x8000 ../st7306-s3-output/minimal_esp32s3.ino.partitions.bin \
  0xe000 ../st7306-s3-output/boot_app0.bin \
  0x10000 ../st7306-s3-output/minimal_esp32s3.ino.bin
```

If automatic bootloader entry fails, hold BOOT while tapping RESET, release BOOT,
then run the same command. Flashing replaces the installed application. No
serial output is expected from this demo.

The unchanged `boot_app0.bin` helper comes from the installed ESP32 3.3.11 core's
`tools/partitions/boot_app0.bin`; retain the same four offsets.

## Flash Pico 2 W

Hold **BOOTSEL** while connecting the Pico 2 W over USB. Release BOOTSEL once 
the RP2350/Pico boot drive appears. Manually copy the generated
`../st7306-pico-output/minimal_pico2w.ino.uf2` onto that drive. It restarts automatically.
Use the matching RP2350/Pico 2 W board; no serial-port selection is needed.

## Verification status

Both platforms completed preprocessing, compilation, full firmware linking and
artifact generation with the fixed board-core versions. Arduino's library
selection listed only SPI in addition to the platform core.

| Platform | Compile | Link | Device smoke test |
|---|---|---|---|
| ESP32-S3 | PASS | PASS | PASS |
| Pico 2 W | PASS | PASS | Not performed |

The ESP32-S3 minimal demo has been flashed and visually checked on hardware.
The Pico 2 W demo has not been tested on hardware in this publication cycle.
See [DEVICE_SMOKE_TEST.md](DEVICE_SMOKE_TEST.md) for the test pattern and criteria.

The eight driver files are the same versions used by the reviewed Draftor 
builds, and the two example sketches contain only the minimal display test. 
SDKs, logs, ELF/map files, build caches and prebuilt firmware binariesare not 
included in this source folder.

Existing source notices are retained in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
and `licenses/`. Draftor-owned code and new files in this ST7306 module are
released under the MIT License provided in the module directory.
