# Installation / 安装

Version `v0.1.0-alpha.1` pairs the existing TF20 build/resources as listed in [release assets](RELEASE_ASSETS.json).

Use the same-version Resources and Firmware ZIPs with the accepted [hardware](HARDWARE.md). These are local prepared materials, not a report of a new installation. Do not mix a new build with another resource generation or pins.

## TF resources

Resources ZIP has a `Resources/HANIME/IME/` tree. The fixed runtime roots on the card are `HANIME/IME/LEXICON_R3_B2_20261006` and `HANIME/IME/R1_API080/matched28.hbf1`; `/sd` is the firmware mount prefix and is not part of the offline card path. B2 includes all 14 data/authentication files, the original manifest/pins and a READY marker produced only after all staged payloads verified against the existing manifest. There must be no STAGING_INCOMPLETE marker. ZIP CRC does not replace runtime pin/authentication checks.

For an app-only update, retain the already-valid full installation. For a new offline installation, save/leave input and USB idle, remove all power, then use a card reader. Back up existing manuscripts and resource roots; stage the full same-version directory without overwriting an existing active installation. The supplied `tools/prepare_tf_resources.py` operates only on explicitly selected local directories, defaults to a check-only run, verifies all 14 payload digests and writes READY only after copied bytes verify. It does not enumerate cards or access firmware devices.

```sh
python3 -B tools/prepare_tf_resources.py --resources "$EXTRACTED_RESOURCES" --target "$NEW_LOCAL_RESOURCE_DIRECTORY"
# Add --write only when the selected new target directory is appropriate.
```

`EXTRACTED_RESOURCES` is the extracted `Resources` directory. The destination must be a new `HANIME/IME/LEXICON_R3_B2_20261006` directory. The tool stages B2; copy the unchanged `R1_API080/matched28.hbf1` separately, preserving an existing identical font and investigating any mismatch. Do not edit resource bytes or manufacture READY to bypass a failure. Safely eject before reinstalling the unpowered card.

## Application-only update

The existing partition layout must match `partitions.csv` / `partitions.bin` from the Firmware ZIP. App offset **0x10000**, capacity **0x200000**. The exact tested app is `Draftor_Alpha_T06_TF20_R1.app.bin` (2,061,808 bytes). Stop the UART reader before a user-initiated flash; select the actual current port.

```sh
python3 -m esptool --chip esp32s3 --port "$CONFIRMED_UART_PORT" --baud 460800 write-flash 0x10000 Draftor_Alpha_T06_TF20_R1.app.bin
```

## First installation on a blank matching board

All four original artifacts are supplied. Offsets and flash header options below come from the same tested build's flash_args, not guessed defaults. Board compilation uses QIO/OPI; the original flashing recipe requests DIO/80m/16MB. No merged device image is provided.

```sh
python3 -m esptool --chip esp32s3 --port "$CONFIRMED_UART_PORT" --baud 460800 write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 Draftor_Alpha_T06_TF20_R1.app.bin
```

This writes boot/partition/OTA initialization as well as the application and is for first installation; it is not the routine update command. The supplied partition table includes NVS/OTA/app/filesystem/coredump regions, but no existing device contents are supplied. No full erase command is needed here. esptool syntax must match the installed fixed tool version.

UART logging is 115200 baud; use one reader. Expected startup identity is `DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007`, resources `LEXICON_R3_B2_20261006`, runtime20MHz/init400kHz. Existing finite qualification is inherited; this preparation did not flash or verify a fresh blank board. Preserve [known limitations](KNOWN_ISSUES.md).

中文：应用更新与空板首次安装分别提供；本轮只核对原产物及偏移，没有访问设备。首次安装材料齐备不等于本轮完成新板实机验证。
