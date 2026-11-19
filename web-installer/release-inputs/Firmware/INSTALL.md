# Install Draftor Alpha 3 on Waveshare

**English** | [简体中文](INSTALL.zh-CN.md)

This guide is for **Waveshare ESP32-S3-RLCD-4.2**, 16 MiB Flash / 8 MiB OPI PSRAM. Check [device and installer qualification](ALPHA3_VALIDATION.md) before installing. The original Osptek board keeps its [Alpha 2.1 guide](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.3/docs/INSTALL_ALPHA2_1.md) and existing release; do not cross-flash targets. The Alpha 3 images for the original board and Pico 2 W are scheduled separately.

## Files and identity

Use `Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Firmware.zip`. Its `Firmware/` contains `app.bin`, `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `FIRMWARE_MANIFEST.json`, `install_firmware.py` and `check_install.py`. The [target manifest](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.3/release/alpha3/WAVESHARE_RLCD42.json) is authoritative; the old original-board digest is historical only. ELF and private device logs are not release assets.

The separate `app.bin` is a 4,021,152-byte application image. Its SHA-256 is `165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd`. Extract the firmware ZIP, then run the following commands from the directory containing `Firmware/`. Install Python 3.11+ and esptool **5.3.1** in a virtual environment (`python -m pip install esptool==5.3.1`). Use that environment's `python` below. Identify your intended serial port explicitly; never select an arbitrary first port. The ROM route uses `--no-stub --before no-reset --after no-reset` because USB-Serial/JTAG stub reads were unreliable in this setup.

Save documents and safely eject/exit any transfer. Disconnect power completely, remove the TF card, hold BOOT while attaching USB, then release BOOT after about two seconds. Disconnect other boards. Battery-equipped boards may remain powered after USB removal. Read the ROM MAC with the explicit port and record it privately:

```sh
python -m esptool --chip esp32s3 --port "$PORT" --before no-reset --after no-reset --no-stub read-mac
```

Set `PORT` to the actual port and `MAC` to the identity just checked against your physical board. ROM identity cannot prove wiring. The installer accepts no built-in MAC, guesses no port, rejects unknown security state, Secure Boot and Flash Encryption, and never forces eFuses.

## Backups and read-only plan

Keep verified existing Flash/settings/pairing backups and independent document/TF copies privately before a write. Do not overwrite a prior backup or treat a truncated read as complete. If a backup does not exist, acquire and verify it before using `--backup-confirmed`; that flag is an acknowledgement, not an automatic backup. Existing verified backups need not be recopied. Whole-card hashes are optional, not an installation gate. Never publish Flash/NVS or pairing material.

```sh
python Firmware/install_firmware.py --firmware Firmware --board WAVESHARE_RLCD42   --port "$PORT" --expected-mac "$MAC" --mode update --output private-plan
```

Default mode only checks files/device and reads actual partition/OTA metadata. Every output directory must be new and is private. The tool validates all four files and target label, fixed B partition bytes, security and 16 MiB Flash. Update selects the actual supported active app slot (app0 or app1); ambiguous, pending, rollback or incompatible OTA/layout states stop. A read-only plan does not authorize writing.

## Compatible update — App only

After the plan and backups pass, execute with a different output directory. Optionally provide the exact previous App to independently verify what is currently installed:

```sh
python Firmware/install_firmware.py --firmware Firmware --board WAVESHARE_RLCD42   --port "$PORT" --expected-mac "$MAC" --mode update --output private-update   --current-app previous-app.bin --execute --backup-confirmed
```

Omit `--current-app` only if no exact previous App is available; do not substitute another board's image. Update writes and independently verifies only the selected App. It does not change bootloader, partitions, OTA metadata, NVS, pairing or TF. Any failure is STOP: preserve the log and device state, no automatic retry or forced reboot.

## Fresh installation — separate destructive mode

Fresh installs use the same target's four files at `0x0`, `0x8000`, `0xe000`, `0x10000`. They erase **all internal Flash**, including old firmware, settings and pairing; they do not format TF. This route is not an update shortcut and was fixture-tested, not tested on a new empty board in this qualification.

First run the plan with `--mode fresh` and a new output directory. Only after checking backup and deliberate erase intent, use:

```sh
python Firmware/install_firmware.py --firmware Firmware --board WAVESHARE_RLCD42   --port "$PORT" --expected-mac "$MAC" --mode fresh --output private-fresh   --execute --backup-confirmed --allow-erase
```

Do not use fresh on an existing compatible board merely because update refuses an unknown state. Both modes finish with independent verification and remain in download mode (`VERIFIED_BOOT_PENDING`). No reset is issued.

## TF resources and first boot

The [TF manifest](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.3/release/alpha3/TF_RESOURCES.json) lists each path relative to the card root, its size and SHA-256. Before changing an existing card, back up its manuscript and learning directories to your computer. Keep their names and contents together.

1. On a new card, copy the contents of `Resources/HANIME/` from the [exact Alpha 2.1 Resources ZIP](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip) into `HANIME/` at the card root. This supplies the 14 format4 files and installation marker under `HANIME/IME/B_SHORT_TEXT_20261018/`, and the body font `HANIME/IME/R1_API080/matched28.hbf1`. An existing card can retain the matching files.
2. From `Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip`, copy `Resources/WAVESHARE_R7_L1_AREV1_RES_20261112/` to the card root. This supplies both `relations.bgr` and `relations.hnv`. Compare their sizes and digests with the TF manifest. If an existing file differs, preserve it and check the card and release before proceeding.
3. The firmware creates its manuscript namespace at `WAVESHARE_R7_L1_AREV1_DOC_TEST_20261112/`. Documents live under `HANIME_S3_TEST/v01/` inside it. Let the firmware create the directory and marker on first boot. Keep an existing valid namespace intact. The private learning directory is `WAVESHARE_R7_L1_AREV1_TEST_20261112/`. Preserve the whole directory, including both state banks and their auxiliary files. Firmware updates do not automatically migrate old documents or learning records. Keep your old card content and use normal document export/import to bring selected documents across.
4. Once firmware verification is complete, power off, insert the prepared card and start normally with BOOT released. Reuse your paired keyboard on its original channel. On a new empty board, pair the first keyboard as described in the [release notes](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.3/docs/RELEASE_NOTES.md). No physical blank-board first installation has been claimed for this release.

For real USB transfer, export is a read-only document snapshot; import is a **512 KiB RAM Inbox**, never the physical TF. Safely eject the actual newly enumerated volume using the host's normal eject action (SCSI eject), then follow the device's finish prompt. `sync`/unmount alone is insufficient. See [Usage](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.3/docs/USAGE.md). Unexpected save warnings, wrong data or verification failures require preserving the state, not formatting or repeatedly saving over evidence.
