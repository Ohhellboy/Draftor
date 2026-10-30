> Historical Alpha 2.1 original-board documentation. Current Waveshare instructions are in [INSTALL](INSTALL.md); do not mix target images.

# Draftor Alpha 2.1: Chinese Find & Interface Update

**English** | [简体中文](RELEASE_NOTES.zh-CN.md)

Alpha 2.1 adds a Find interface with Chinese input and tidies up Draftor’s Chinese menus and editor status display. Find follows the writing direction, while the filename, input mode, save status and connection status are easier to read at a glance.

## What changed

- **Find Chinese or English text in a manuscript.** Press `Ctrl+F` or `Cmd+F` and enter a query. After completing any Chinese composition, use `Enter` for the next match or `Shift+Enter` for the previous one, with wraparound at the document boundaries. Typing in the query field leaves the manuscript unchanged.
- **Manage manuscripts with Chinese prompts.** The file list, trash, rename screen, statistics, help and USB/Wi-Fi pages now use Chinese labels. Long filenames can be expanded, and Shift selection in the rename field lets you replace or delete part of a name.
- **Keep the writing screen clear.** The filename occupies the top line. The bottom line shows `En／双／全／五` for the input mode, followed by save and Bluetooth status on the right. In horizontal writing, the filename moves to the former mode line and the text area extends upwards. Composition, candidates, errors and pairing codes still appear when needed.

To support these interface changes, this release also fixes several selection, vertical Find hint and menu-return details, and avoids repeated glyph measurements when laying out long names. A change to the link order of string implementations also reduces firmware size. See the [user guide](USAGE.md) for the controls.

## Which files to download

For an ESP32-S3 N16R8 (Thick), download **Firmware** and **Resources** from the same release. To compile the firmware yourself, also download **Source** and **BuildInputs**.

| File | Purpose |
| --- | --- |
| `Firmware.zip` | App, boot and partition files, installation guide and offline checker |
| `Resources.zip` | Matching TF dictionaries, indexes and body font |
| `Source.zip` | Draftor, bgIME, related source and documentation |
| `BuildInputs.zip` | Generated glyphs, fixed dependencies and source materials needed to build |

GitHub’s automatic Source code archives are repository snapshots. Follow the guide in Firmware to install the matching attachments, or the [build guide](BUILD.md) to compile them.

## Upgrading from an earlier version

**From Alpha 2:** this release keeps the dual 4 MiB App layout and the same format4 TF resources. Save and back up manuscripts, then read and check the device’s partition table and boot records. If they are compatible, follow the [installation guide](INSTALL_ALPHA2_1.md) to update only the active App. This interface update does not require reinstalling dictionaries or formatting the TF card.

**From Alpha 1, an old 2 MiB layout, or on a new board:** the App does not fit the old slots, so use the separate fresh-install procedure. That procedure erases internal flash; back up the data you need first. It does not require formatting the TF card.

## Things to keep in mind

Alpha 2.1 is still a test release, and local Wi-Fi transfer remains experimental. Export important writing regularly and keep independent backups.

With a large library, the file list and some document-management operations can become slow. Export completed writing for archiving; moving manuscripts to the trash does not reduce the combined catalog size. See [Known Issues](KNOWN_ISSUES.md) for the limits.

See [Resources and Licensing](RESOURCE_STATUS.md) for font and dictionary sources. Earlier Chinese input measurements remain results for [Alpha 2](RELEASE_NOTES_ALPHA2.md), with full details in the [performance report](PERFORMANCE.md).
