# Draftor Alpha · v0.1.0-alpha.1

[Project Introduction](../README.md) · [简体中文](../README.zh-CN.md)

Draftor is an offline writing device built around a microcontroller. It uses a reflective LCD, a Bluetooth keyboard, and a TF (microSD) card for local text storage. It doesn't need an internet connection, AI, or cloud services.

It is a machine for writing, not another all-purpose PDA. It keeps the editing and file management functions needed for writing, without trying to do everything else.

The first public **Alpha** runs on **ESP32-S3 N16R8 (Thick)** and uses **bgIME (Big Goose Input Method Engine)**, a Chinese input method engine developed for Draftor.

It supports English, Full Pinyin (Quanpin), Xiaohe Shuangpin, and Wubi 86, with both horizontal and vertical writing layouts. You can save, reopen, and rename documents, export text as TXT files over USB, or import new drafts from a computer.

This is Draftor's first complete Alpha software release.

**The standalone battery power module is still under development and is not included in this release. The current prototype requires an external power supply.**

---

## 1. Download

**[Draftor Alpha · v0.1.0-alpha.1](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)**

This release includes all Draftor-owned source code, including bgIME Core, along with the complete runtime dictionaries, font resources, build materials, and device-tested firmware.

Four ZIP archives are available:

| File | Contents |
|---|---|
| `Draftor_Alpha_Source.zip` | Complete source tree, bgIME Core, display drivers, development tools, and documentation |
| `Draftor_Alpha_Resources.zip` | Chinese dictionaries, lookup indexes, bitmap fonts, and associated runtime resources |
| `Draftor_Alpha_BuildInputs.zip` | Required project build inputs, fixed-version upstream source materials, font sources, and license notices |
| `Draftor_Alpha_Firmware.zip` | Device-tested ESP32-S3 firmware and the associated files needed for initial flashing |

You can also browse the source code directly in this repository.

**Use files from the same release. Do not mix source code, resources, or firmware from different versions.** If you want to compile the firmware yourself, you will also need the specified toolchain versions.

## 2. Installation & Usage

If you want to run Draftor on your own hardware, these documents are a good place to start:

| Document | Contents |
|---|---|
| [Hardware & Wiring](HARDWARE.md) | Hardware requirements, GPIO connections, and power considerations |
| [Installation](INSTALL.md) | Preparing the TF card, flashing the firmware, and first-time setup |
| [Usage Guide](USAGE.md) | Chinese input, keyboard shortcuts, file management, and USB transfers |
| [Building from Source](BUILD.md) | Build environment, toolchain versions, and compilation instructions |
| [Known Issues](KNOWN_ISSUES.md) | Current limitations and unresolved problems |

### USB File Transfer

Draftor can exchange TXT files with a computer over USB, without installing a separate application or using the internet.

- **Export:** Press `F9` → `E`. A read-only USB drive appears on your computer, containing a snapshot of the current document as `DOCUMENT.TXT`.
- **Import:** Press `F9` → `I`. Copy TXT files to the temporary Inbox drive, safely eject it on your computer, then select the file you want to import on Draftor.

Importing creates a new draft. It does not overwrite the document you are currently editing or automatically switch to the imported document.

See the [Usage Guide](USAGE.md) for the complete procedure.

## 3. Hardware & Version

The complete Alpha firmware is intended for **ESP32-S3 N16R8 (Thick)**.

The software build baseline is `T06_R1`. The TF card uses an SPI clock of **20 MHz** during normal operation and **400 kHz** during initialization.

The firmware included in this release is the version tested on the actual device.

### What about Raspberry Pi Pico 2 W (Thin)?

A complete writing firmware for Pico 2 W (Thin) is not included in this Alpha release.

However, the previously published [ST7306 standalone display driver and demos](../display/st7306/) remain available for both ESP32-S3 and Raspberry Pi Pico 2 W.

You can use these demos to test the display, wiring, and basic rendering without installing the full Draftor system.

### This is an Alpha release

The main Chinese input, editing, save/reopen, and USB import/export workflows have passed limited tests on real hardware.

But this is not a stable release yet. Long-duration operation, unexpected power loss, extreme memory conditions, and some other scenarios have not been fully tested. There are also known issues that remain unresolved.

If you plan to use Draftor for important writing, use a stable external power supply compatible with your board, export your documents regularly, and keep backups.

See [Known Issues](KNOWN_ISSUES.md) for the current limitations.

---

## 4. Source Code & Licenses

New Draftor-owned source code, including bgIME Core, is released under the **Apache License 2.0**.

Copyright © 2026 **Ruotao Wang**.

The previously published [ST7306 display module](../display/st7306/) retains its original **MIT License** and third-party notices.

Third-party libraries, fonts, and dictionary data are not automatically covered by Draftor's Apache-2.0 license. Materials with established provenance retain their applicable copyright notices, licenses, and attribution. Some historical resources still have unresolved provenance or licensing questions.

Further details are available in:

[LICENSE](../LICENSE) · [NOTICE](../NOTICE) · [Third-party License Inventory](LICENSE_INVENTORY.md) · [Resource & License Status](RESOURCE_STATUS.md)

### Patent

Technology used by bgIME for organizing shared lexical resources through stable lexeme identities is the subject of a Chinese invention patent application.

The patent application documents are not included in this repository.

## 5. Source Structure

If you are interested in Chinese input methods or embedded development, these directories are a good starting point:

| Directory | Contents |
|---|---|
| `integration/product` | Writing application, editor, horizontal/vertical layouts, and bgIME Core |
| `integration/module`, `integration/shared` | USB document import and export |
| `integration/telemetry` | Input behaviour recording and performance analysis tools |
| `integration/tools`, `integration/fixtures` | Development tools and test fixtures |
| `display/st7306` | Standalone ST7306 display driver and demos |
| `vendor` | Third-party SDK adaptations and modifications |
| `release`, `tools` | Release configuration, build tools, and resource preparation |

The released source and companion materials include the project inputs used for the current ESP32-S3 build. The specified toolchain must still be installed separately.

Some historical mixed dictionaries and fonts cannot yet be fully regenerated from their original upstream materials. This is different from building the current firmware, and the release does not claim byte-for-byte reproduction of every historical artifact.

For more details, see:

[Build Guide](BUILD.md) · [Source Integration](SOURCE_INTEGRATION.md) · [Release Notes](RELEASE_NOTES.md)

---

## 6. Feedback & Contributions

Draftor is still under development.

If you are interested in Chinese input methods, offline writing devices, reflective displays, or text layout on microcontrollers, feel free to explore the source code, ask questions, or share your own experience.

If you try this Alpha and find something that doesn't work well, I'd like to hear about it. Feedback on Chinese input, document editing, and USB transfers would be particularly useful.

You can report bugs or suggest improvements through [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues), or submit a Pull Request.

There is still plenty of work to do. But now, at least, you can give it a try.

---

[Draftor Project](https://github.com/Ohhellboy/Draftor) · [Download Alpha](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Usage Guide](USAGE.md) · [Known Issues](KNOWN_ISSUES.md)
