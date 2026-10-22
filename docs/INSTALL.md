# Installing and upgrading Alpha 2

**English** | [简体中文](INSTALL.zh-CN.md)

Target hardware: ESP32-S3 N16R8 (Thick), 16 MiB flash, 8 MiB OPI PSRAM and the [existing display/TF wiring](HARDWARE.md). Commands below use Python 3 and esptool 5.3.1 in a macOS/Linux shell. Set the actual serial port and TF mount path; do not copy another device's values. Windows users must use their actual COM port and adapt shell paths/activation commands.

Run each step only after the previous one succeeds. If a command fails, stop before any erase or write. The commands are separate instructions, not a script to run unattended.

## 1. Choose the installation route

| Current installation | Route |
| --- | --- |
| Alpha 1 | **Back up, erase internal flash, then install Alpha 2 fresh. A direct upgrade is not recommended.** |
| New board | Prepare the TF resources, erase internal flash, then write the supplied boot/partition/App files |
| Confirmed exact compatible dual-4 MiB layout | Use the separate App-only route in section 6 |

The release App is **2,429,104 bytes**, labelled `DRAFTOR_ALPHA2_20261019_RELEASE`. It does not fit the old 2 MiB slots. The recommended Alpha 1 route is a fresh installation, not an automatic migration of internal data.

**Erasing internal flash removes the old firmware, partitions and all data/state stored there, including internal filesystem data, settings and pairing data. Back up important manuscripts and necessary data before running the erase command. It does not format the TF card, and TF formatting is not required. Keeping the TF card is not a substitute for a separate backup.**

## 2. Files, tools and backups

Download these two matching attachments from the [Alpha 2 release](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) once published, and extract them side by side.

```text
Draftor_v0.1.0-alpha.2_Firmware.zip  -> Firmware/
Draftor_v0.1.0-alpha.2_Resources.zip -> Resources/
```

Source and BuildInputs are needed for building, not for ordinary installation. Use this updated guide in Firmware or Source; unchanged BuildInputs/Resources archives contain older guide copies. Their firmware inputs and resource payloads remain compatible.

1. While the old firmware still runs, save important manuscripts, export them to a computer and **open the exported files to check the text**. Export each needed manuscript; a current-document export is not a backup of all drafts. Preserve native TF manuscripts, directory/state files and any other necessary card data separately. Keep the old firmware and matching resources.
2. Finish composition, wait for Saved, leave USB transfer and turn Wi-Fi off. Close any serial monitor. Power down before moving the TF card. Back up the card before installing resources; never replace the entire card with the Resources folder.
3. If you need the previous internal state for rollback, also retain a verified private full-flash backup using the commands below. This does not replace readable text exports or the TF backup.

Run commands from the directory containing `Firmware/` and `Resources/`. For example, set `PORT` to your board's serial port; `/dev/tty.YOUR_DEVICE` is a placeholder that must be replaced.

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install esptool==5.3.1
PORT=/dev/tty.YOUR_DEVICE
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-mac
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset flash-id
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset get-security-info
```

Confirm the intended ESP32-S3 and 16 MiB flash. The board specification must also confirm N16R8/8 MiB OPI PSRAM; flash-id alone cannot establish PSRAM. If flash encryption or Secure Boot is enabled, stop: these plain-image instructions do not cover that configuration. Do not use `--force` or change eFuses to bypass it. If automatic boot entry fails, use the board's documented BOOT/RESET sequence and retry the same identification checks.

For an existing device, use a new empty private backup directory; never overwrite an older backup:

```sh
mkdir private-backup-alpha2
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset read-flash \
  0x0 0x1000000 private-backup-alpha2/full-flash-before-alpha2.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 private-backup-alpha2/full-flash-before-alpha2.bin
```

The file must be 16,777,216 bytes and verification must succeed. Keep it private and separately copy it to your backup location. Do not proceed with erasure if readable manuscript backups or required data are missing.

## 3. Install the matching TF resources

Mount the backed-up TF card through a card reader. Set `TF_ROOT` to its actual mount directory, not a raw disk device. This example path is a placeholder. The existing staging helper below only installs a **new** independent resource directory, verifies its contents and writes Ready last. It refuses to replace an existing directory.

```sh
TF_ROOT=/Volumes/YOUR_TF_CARD
```

Check the target directory before running the installation command:

| Target directory | Action |
| --- | --- |
| Does not exist | Run the new-directory installation command below, then continue with the checks. |
| This exact release directory is present and its provenance is known | Skip the installation command and go directly to [font and pairing checks](#font-and-pairing-checks). |
| Origin is uncertain or installation is incomplete | Back it up and rename it to a different unused name, then install a complete new directory using the existing procedure. |

Skipping installation does not skip the font and pairing checks. Do not combine format2 files with format4 or invent a Ready marker. Do not delete documents, CURRENT, state files or old complete resource sets.

```sh
mkdir -p "$TF_ROOT/HANIME/IME"
python3 Firmware/tools/prepare_tf_resources.py --resources Resources \
  --target "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --write
```

### Font and pairing checks

Copy the font only if it is absent, then compare the bytes. A differing existing font is an installation conflict; preserve it and resolve the mismatch rather than overwriting it blindly.

```sh
mkdir -p "$TF_ROOT/HANIME/IME/R1_API080"
cp -n Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
cmp Resources/HANIME/IME/R1_API080/matched28.hbf1 \
  "$TF_ROOT/HANIME/IME/R1_API080/matched28.hbf1"
python3 Firmware/check_install.py --firmware Firmware \
  --resources "$TF_ROOT/HANIME/IME/B_SHORT_TEXT_20261018" --mode fresh
```

The final card paths are:

```text
HANIME/IME/B_SHORT_TEXT_20261018/
  shared.hns, shared.hnv
  xiaohe.hnm, xiaohe.hnv, xiaohe_base.hbo, xiaohe_base.hnv
  full_pinyin.hnm, full_pinyin.hnv, full_pinyin_base.hbo, full_pinyin_base.hnv
  wubi86.hnm, wubi86.hnv, wubi86_base.hbo, wubi86_base.hnv
  RESOURCE_MANIFEST.json, CANDIDATE_READY.json
HANIME/IME/R1_API080/matched28.hbf1
```

There are 14 payload files (132,153,747 bytes), plus the manifest/Ready and 2,464,108-byte font. The shared bundle is `17304919664082782091`, format4. The checker verifies installation shape/pairing; compiled pins authenticate actual consumed bytes at runtime. Safely eject the TF card and put it back with the device powered off. **Do not format the card.**

## 4. Fresh installation: erase, write and verify

This section applies both to a new board and to an Alpha 1 device **after the backups in section 2 are complete and checked**. Reconnect the device, confirm the same port/identity and enter download mode. The next command erases all internal flash. The TF card is unaffected by this command; its separate backup remains necessary.

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset erase-flash
```

After a successful erase, write these four files from the same Firmware package. `keep` preserves the supplied image headers.

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.app.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  0x0 Firmware/bootloader.bin \
  0x8000 Firmware/partitions.bin \
  0xe000 Firmware/boot_app0.bin \
  0x10000 Firmware/Draftor_v0.1.0-alpha.2.app.bin
```

| Supplied file | Address | Bytes |
| --- | --- | ---: |
| `bootloader.bin` | `0x0` | 19,968 |
| `partitions.bin` | `0x8000` | 3,072 |
| `boot_app0.bin` | `0xe000` | 8,192 |
| `Draftor_v0.1.0-alpha.2.app.bin` | `0x10000` | 2,429,104 |

The package supplies a dual-4 MiB layout:

| Partition | Offset | Size |
| --- | --- | --- |
| NVS | `0x9000` | `0x5000` |
| OTA data | `0xe000` | `0x2000` |
| App0 | `0x10000` | `0x400000` |
| App1 | `0x410000` | `0x400000` |
| IME reserved | `0x810000` | `0x200000` |
| FFat | `0xa10000` | `0x5e0000` |
| Coredump | `0xff0000` | `0x10000` |

The old layout used 2 MiB slots and started FFat at `0x410000`; the new App slots overlap old areas and FFat moves to `0xa10000`. Merely replacing the partition file cannot preserve the old internal filesystem. Do not copy an old internal-filesystem image into the new layout.

## 5. First start

After all four files verify, release BOOT and reset the board with the prepared TF card inserted. Confirm Ready and normal BLE keyboard connection; internal pairing data may need to be established again after erasure. In a new disposable draft, type a short Chinese sentence, select words, save, leave that draft and reopen it from the file list to check the text. See [Usage](USAGE.md) for the keys.

If mounting, PSRAM, resource authentication or saving fails, stop the relevant operation and keep backups intact. Do not fabricate Ready or repeatedly overwrite a problematic document. Restore required text through the supported document import path, as new documents; keep backups until the recovered text is checked. Same-version save/reopen evidence does not prove every cross-version native-state restore.

## 6. Existing exact compatible 4 MiB layout

This route **does not erase internal flash** and writes only the App to the confirmed currently selected slot. First make the backups in section 2 and install/retain the matching TF resources in section 3. Obtain the actual current partition and OTA bytes while the device is in download mode; package defaults cannot substitute for them:

```sh
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0x8000 0x1000 private-backup-alpha2/current-partition.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset read-flash \
  0xe000 0x2000 private-backup-alpha2/current-otadata.bin
python3 Firmware/check_install.py --firmware Firmware \
  --resources Resources/HANIME/IME/B_SHORT_TEXT_20261018 --mode update \
  --partition private-backup-alpha2/current-partition.bin \
  --otadata private-backup-alpha2/current-otadata.bin
```

The checker must report `COMPATIBLE_FILES_ONLY` for the exact table above. It verifies OTA CRC/settled state and reports `app_offset`. It accepts the known unambiguous one-valid/one-erased record pattern; other populated/ambiguous/pending/rollback states are refused. Stop if refused, rather than editing the data to pass. Match the result to the actual boot selection.

Set `APP_OFFSET` to that **checked** value. The example placeholder below must be replaced; the address is not universally `0x10000` or `0x410000`.

```sh
APP_OFFSET=REPLACE_WITH_CHECKED_OFFSET
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 --after no-reset write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.app.bin
python3 -m esptool --chip esp32s3 --port "$PORT" --after no-reset verify-flash \
  "$APP_OFFSET" Firmware/Draftor_v0.1.0-alpha.2.app.bin
```

Do not write bootloader, partitions, OTA data, NVS or the other App slot on this route. Then reset and perform the short save/reopen check in section 5. For rollback conditions, see [Rollback](ROLLBACK.md).
