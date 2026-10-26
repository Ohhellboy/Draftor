# Draftor Alpha 2: Faster Chinese Input and Wi-Fi Document Transfer

**English** | [简体中文](RELEASE_NOTES_ALPHA2.zh-CN.md)

The main update in Alpha 2 is a noticeable improvement in Chinese input speed.

Candidates appear sooner after you type Pinyin, and selected words reach the document faster. These changes improve individual input responses and reduce some of the waiting when paging through candidates or typing continuously.

Alpha 2 also adds **local Wi-Fi document transfer**. You can connect to Draftor with your phone's browser to import and export documents. The phone connects directly to the ESP32-S3's Wi-Fi module, without relying on a nearby Wi-Fi network or any cloud text service.

## What changed

This round of performance work has four main parts:

1. **Block-based lookup:** reorganizes the input method's code index to reduce the data read while finding candidates.
2. **Less repeated lookup:** text locations that have already been found and verified can be passed directly to the text reader.
3. **Short text in the same block:** eligible complete short texts are read together with their shared entries, reducing additional reads and verification.
4. **Differential display updates:** the screen updates only the parts that have changed where possible, instead of sending the entire frame every time.

Together with other small adjustments, these changes reduce the wait between pressing a key and seeing the result.

This version continues to support Full Pinyin, Xiaohe Shuangpin, Wubi 86 and English input, with both horizontal and vertical writing.

## Input speed improvements

Here are several corresponding results from the Alpha 1 and cumulative Alpha 2 measurement builds, tested on the same ESP32-S3 prototype.

| Action | Alpha 1 | Alpha 2 |
| --- | --- | --- |
| First Xiaohe `wo`: complete candidates | 317.3 ms | **114.6 ms** |
| First Full Pinyin `shi`: complete candidates | 333.7 ms | **122.8 ms** |
| First Xiaohe `wo`: typed-code feedback | 55.3 ms | **9.6 ms** |
| Xiaohe phrase selected with Space: document display | 72.1 ms | **36.4 ms** |

The first two complete-candidate waits were reduced by about 64% and 63%.

Timing starts when the ESP32-S3 receives the relevant keyboard event and ends when the software finishes sending the corresponding typed-code, complete-candidate or document image to the display.

We also used the cumulative Alpha 2 build for a natural writing test of about 1,400 characters. Among valid samples, the median complete-candidate wait was **84.4 ms**, typed-code feedback took **17.8 ms**, and document display after selection took **46.5 ms**.

See [docs/PERFORMANCE.md](PERFORMANCE.md) for the full results and measurement method.

## Which files to download

To install on an ESP32-S3 N16R8 (Thick), download **Firmware** and **Resources** from the same version.

Follow the [installation guide](INSTALL.md) included in this version’s Firmware package.

If you want to build the firmware yourself, also download **Source** and **BuildInputs**, and follow [docs/BUILD.md](BUILD.md).

| File | Purpose |
| --- | --- |
| `Firmware.zip` | Firmware, boot files and partition files |
| `Resources.zip` | Matching dictionaries, indexes and font |
| `Source.zip` | Draftor, bgIME and related project-owned source |
| `BuildInputs.zip` | Generated files and dependency materials needed for the fixed build |

GitHub's automatically generated Source code archives are not complete installation packages.

## Upgrading from an earlier version

**A direct upgrade from Alpha 1 to Alpha 2 is not recommended.**

The new version changes the firmware partition layout. We recommend exporting your manuscripts and backing them up to a computer, then following the fresh-install procedure to erase the old system from the ESP32-S3's internal flash and install Alpha 2.

**Reinstallation erases the existing data in internal flash, but does not require formatting the TF card.** Install the new dictionaries that match the Alpha 2 firmware.

See [docs/INSTALL.md](INSTALL.md) for installation instructions.

## Things to keep in mind

Alpha 2 is still a test release. Local Wi-Fi document transfer remains experimental, and some usage scenarios have not been fully tested. This release does not include complete writing firmware for the Pico 2 W.

The release firmware has passed a brief device check covering startup, Chinese input, candidate selection, saving and reopening a test manuscript.

See [docs/KNOWN_ISSUES.md](KNOWN_ISSUES.md) and [docs/RESOURCE_STATUS.md](RESOURCE_STATUS.md) for other known issues, resource sources and licensing information.
