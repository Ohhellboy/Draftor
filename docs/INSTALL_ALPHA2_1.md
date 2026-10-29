> Historical Alpha 2.1 original-board documentation. Current Waveshare instructions are in [INSTALL](INSTALL.md); do not mix target images.

# Alpha 2.1 Installation Guide

**English** | [简体中文](INSTALL_ALPHA2_1.zh-CN.md)

This guide is for the ESP32-S3 N16R8 (Thick), with 16 MiB flash, 8 MiB OPI PSRAM and the project's specified [display and TF card wiring](HARDWARE.md).

The commands below are for macOS and Linux, using Python 3 and esptool 5.3.1. Windows users need to use their actual COM port and adjust the paths and virtual environment activation commands accordingly.

Follow the steps in order. Continue only after each step succeeds. If an error occurs, stop before running any further erase or write commands.

## 1. Choose an installation method

| What is currently on your device? | Which method should you use? |
| --- | --- |
| Alpha 1 | Back up your manuscripts and any data you need to keep, then follow the fresh-install procedure in this guide. Directly overwriting the old App is not recommended. |
| A new development board being installed for the first time | Prepare the TF resources, then follow the fresh-install procedure in this guide. |
| A compatible layout with two 4 MiB App partitions | Complete the preparations in sections 2 and 3, then update the App using section 6. Do not run the erase command in section 4. |

The Alpha 2.1 App is 3,893,280 bytes, which exceeds the capacity of the old 2 MiB App partitions. Devices using the old layout therefore need a fresh installation.

A fresh installation erases the old firmware, settings, Bluetooth pairing information and other data in the ESP32-S3's internal flash. Make your backups first.

This operation does not format the TF card, and installing Alpha 2.1 does not require formatting it. However, you should still back up the manuscripts on the TF card to a computer rather than keeping only the copy on the card.

Alpha 2 and Alpha 2.1 use compatible partitions and the same format4 runtime resources. Read the actual partition and boot records, then use section 6 to update the active App; do not run section 4’s erase command. Matching Alpha 2 dictionaries and the body font can stay on the TF card. The new menu and Find glyphs are built into the App.

## 2. Prepare files, tools and backups

### Download the files

Download these two attachments from the same version on the Alpha 2.1 release page, and extract them into the same folder:

```text
Draftor_v0.1.0-alpha.2.1_Firmware.zip  → Firmware/
Draftor_v0.1.0-alpha.2.1_Resources.zip → Resources/
```

These two packages are all you need for ordinary installation. Source and BuildInputs are for compiling the firmware yourself; you do not need to download them just to install it.

Use the installation guide in this version's Firmware package. Resources and BuildInputs may still include older instructions; do not mix installation steps from different versions.

### Back up your manuscripts

If your device runs an older version, back it up while it still works normally. A new development board has no old data, so you can skip this subsection.

Save the manuscripts you want to keep, export each one to your computer, and open the exported files to check that the text is complete. Exporting only the current manuscript does not back up the others.

Finish entering any text you want to keep, save it and wait for “已保存” (Saved on older firmware) on the screen. Exit USB transfer mode and turn off Wi-Fi if it is on. Close any serial monitor on your computer that is using the board's port, then power off the device before removing the TF card.

Back up the manuscripts and other files on the TF card to your computer, keeping the original directory structure. Also keep the old firmware and its matching resources. Do not replace the entire contents of the TF card with the downloaded Resources folder.

### Install the tools and identify the board

In a terminal, change to the folder containing Firmware/ and Resources/, then run:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install esptool==5.3.1
```

Replace /dev/tty.YOUR_DEVICE below with the serial port your board actually uses:

```sh
PORT=/dev/tty.YOUR_DEVICE
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-mac
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset flash-id
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset get-security-info
```

Check the results to confirm that this is the ESP32-S3 you intend to install and that its flash capacity is 16 MiB. Confirm the 8 MiB OPI PSRAM from the board model and specifications; flash-id alone cannot establish that.

If the board has **Secure Boot** or Flash Encryption enabled, stop. This guide does not cover those configurations. Do not use --force or change eFuses to bypass the restriction.

If the computer cannot put the board into download mode automatically, follow the board's instructions for its BOOT and RESET buttons, then repeat the checks above.

### Keep a full flash backup

For a device that already contains data, complete this step before erasing if you want to retain the option of restoring the old system and internal settings. It does not replace the manuscript exports and TF card backup above.

Use a new backup directory; do not overwrite an older backup:

```sh
mkdir private-backup-alpha2-1
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset read-flash \
  0x0 0x1000000 private-backup-alpha2-1/full-flash-before-alpha2.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 private-backup-alpha2-1/full-flash-before-alpha2.bin
```

The resulting file should be 16,777,216 bytes, and the verification command must succeed. Save another copy in your backup location, keep it private and do not upload it to a public repository.

Do not erase the flash until you have backed up the manuscripts and data you need to keep.

## 3. Install dictionaries and the font on the TF card

### Confirm the TF card path

Connect the backed-up TF card through a card reader. Replace the path below with the TF card directory shown on your computer:

```sh
TF_ROOT=/Volumes/YOUR_TF_CARD
```

Use a directory you can open to view files, not a disk device name such as `/dev/disk...`.

First check whether the TF card already contains:

```text
HANIME/IME/B_SHORT_TEXT_20261018
```

Choose the next step according to what you find:

| Directory status | What to do next |
| --- | --- |
| The directory does not exist | Run the dictionary installation command below. |
| You have already installed this directory from this version's complete Resources package | Skip the dictionary installation command and continue with the font and resource checks. |
| A directory with the same name exists, but its origin is uncertain or the previous installation was interrupted | Back it up, rename it to a name that is not already in use, then install again. |

Do not combine dictionary files from different versions in one directory, or delete existing manuscripts, CURRENT or other document state files. You can retain complete resource directories from older versions.

### Install the dictionaries

Run this only when the target directory does not exist:

```sh
mkdir -p "$TF_ROOT/HANIME/IME"
python3 Firmware/tools/prepare_tf_resources.py --resources Resources \
  --target "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --write
```

The tool copies the resources, checks the files and writes the installation completion marker only after everything is complete. It will not overwrite an existing directory with the same name.

If it reports an error, resolve that error before writing the firmware.

### Install the font and check the resources

Prepare the font directory:

```sh
mkdir -p "$TF_ROOT/HANIME/IME/R1_API080"
```

If matched28.hbf1 is not already in the directory, run:

```sh
cp -n Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
```

Whether you have just copied the font or it was already there, run these checks:

```sh
cmp Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
python3 Firmware/check_install.py --firmware Firmware \
  --resources "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --mode fresh
```

If an existing font differs from this version's font, preserve the original file and investigate before overwriting it. Stop if the resource check fails as well; do not continue the installation with a mixture of files from different versions.

When you finish, the TF card should contain:

```text
HANIME/IME/B_SHORT_TEXT_20261018/
  shared.hns
  shared.hnv
  xiaohe.hnm
  xiaohe.hnv
  xiaohe_base.hbo
  xiaohe_base.hnv
  full_pinyin.hnm
  full_pinyin.hnv
  full_pinyin_base.hbo
  full_pinyin_base.hnv
  wubi86.hnm
  wubi86.hnv
  wubi86_base.hbo
  wubi86_base.hnv
  RESOURCE_MANIFEST.json
  CANDIDATE_READY.json

HANIME/IME/R1_API080/
  matched28.hbf1
```

After the checks complete, safely eject the TF card on your computer, then put it back into Draftor while the device is powered off.

No part of this process requires formatting the TF card.

## 4. Install Alpha 2.1 fresh

This section is for new development boards and for Alpha 1 devices whose backups are complete and which are ready for a fresh installation.

If you already have a compatible layout with two 4 MiB App partitions and only want to update the App, skip to section 6.

Reconnect the board, confirm that it is still the device and serial port checked earlier, and enter download mode.

### Erase internal flash

The command below erases all of the ESP32-S3's internal flash, including the old firmware and internal data. It does not format the TF card.

Run it only after confirming that your backups are complete:

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset erase-flash
```

Do not continue if erasing fails.

### Write the firmware

After erasing succeeds, write the four files from the same Firmware package:

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
```

In these commands, keep means retaining the flash parameters already in the images. You do not need to specify them separately.

After writing succeeds, verify the contents on the device:

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
```

Start the device only after all four files verify successfully.

## 5. Start and check the device

Make sure the matching TF card is inserted, release the BOOT button and reset the board to start it.

Wait for the writing screen, then connect the Bluetooth keyboard; the bottom line shows “已连接” when it is ready for input. Erasing internal flash also removes the old pairing information, so you may need to pair the keyboard again.

First create a disposable test manuscript, type a short passage in Chinese, select words and save it. Leave that manuscript, reopen it from the file list and check that the text is still correct. See the [usage guide](USAGE.md) for the keys.

If storage mounting, PSRAM, dictionary checks or saving report an error, stop the relevant operation and keep your backups. Do not repeatedly overwrite a manuscript that has a problem.

To restore an old manuscript, use the existing import function to create a new manuscript, then open it and check its contents. Keep the backups on your computer until you have confirmed the restoration. An old internal-filesystem backup cannot be written directly to Alpha 2.1's new partitions.

## 6. Existing compatible 4 MiB layout: update only the App

This section applies only to devices with a compatible layout containing two 4 MiB App partitions. It does not erase internal flash and writes only to the App partition currently in use.

Complete the backups in section 2 and the TF resource preparation in section 3 first. With the device in download mode, read its actual partition table and boot records:

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0x8000 0x1000 private-backup-alpha2-1/current-partition.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0xe000 0x2000 private-backup-alpha2-1/current-otadata.bin
python3 Firmware/check_install.py --firmware Firmware \
  --resources Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode update \
  --partition private-backup-alpha2-1/current-partition.bin \
  --otadata private-backup-alpha2-1/current-otadata.bin
```

Do not substitute a partition file from the downloaded package for the file just read from the device.

When the check succeeds, the tool outputs:

```text
COMPATIBLE_FILES_ONLY
```

It also provides app_offset, the location where the App should be written this time. Confirm that it matches the boot partition the device actually uses before continuing.

The checker currently accepts only one unambiguous combination of boot records: one valid record and one erased record. If it reports an incompatible layout or cannot determine the boot location, stop rather than guessing an address.

Replace REPLACE_WITH_CHECKED_OFFSET below with the app_offset confirmed by the check. Do not simply copy 0x10000 or 0x410000 from someone else's device.

```sh
APP_OFFSET=REPLACE_WITH_CHECKED_OFFSET
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.1.app.bin
```

This update route does not write the boot files, partition table, OTA data, NVS or the other App partition.

After verification succeeds, start the device as described in section 5 and make a short save-and-reopen check. If you need to restore an older version, read the [rollback guide](ROLLBACK.md) first.

## Appendix: files and partitions

This section is a reference for checking files and partitions. For ordinary installation, follow the steps above; you do not need to adjust these values yourself.

### Firmware files

Release App label:

```text
DRAFTOR_ALPHA2_20261019_RELEASE
```

| File | Write address for a fresh installation | File size |
| --- | --- | --- |
| bootloader.bin | 0x0 | 19,968 bytes |
| partitions.bin | 0x8000 | 3,072 bytes |
| boot_app0.bin | 0xe000 | 8,192 bytes |
| Draftor_v0.1.0-alpha.2.1.app.bin | 0x10000 | 3,893,280 bytes |

### TF resources

This version has 14 dictionary and index files totalling 132,153,747 bytes, plus a resource manifest, an installation completion marker and a 2,464,108-byte font file.

Resource identifiers:

```text
bundle: 17304919664082782091
format: 4
```

The installation checker checks whether the files are complete and their versions match. The firmware still authenticates the resource content it actually reads at runtime.

### Internal flash partitions

| Partition | Start address | Capacity |
| --- | --- | --- |
| NVS | 0x9000 | 0x5000 |
| OTA data | 0xe000 | 0x2000 |
| App0 | 0x10000 | 0x400000 |
| App1 | 0x410000 | 0x400000 |
| IME reserved area | 0x810000 | 0x200000 |
| FFat internal filesystem | 0xa10000 | 0x5e0000 |
| Coredump area | 0xff0000 | 0x10000 |

The old layout had 2 MiB App partitions and FFat starting at 0x410000. The new layout enlarges the App partitions and moves FFat to 0xa10000. You therefore cannot preserve the old internal filesystem just by replacing the partition table, or write an old filesystem image directly to the new location.
