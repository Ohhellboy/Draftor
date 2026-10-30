# Draftor Alpha 2.2: Waveshare Board Support

**English** | [简体中文](RELEASE_NOTES.zh-CN.md)

Alpha 2.2 adds support for the **Waveshare ESP32-S3-RLCD-4.2**, integrating the ESP32-S3, a 4.2-inch monochrome reflective LCD and a microSD card slot on one board. It reduces the wiring required to build a Chinese writing device.

The existing Alpha 2.1 writing features remain available, including Full Pinyin, Xiaohe Shuangpin, Wubi 86, horizontal and vertical writing, Find, document management and USB file transfer. The input engine and dictionary resources remain unchanged.

## What's new

**Waveshare monochrome display support.** Draftor now uses the ST7305 display controller, with monochrome markings for candidate focus, text selection and Find results. Chinese menu hints have also been adjusted to avoid overlapping status messages.

**Integrated microSD support.** The Waveshare port uses the board's native SDMMC interface and stores its manuscripts in a separate directory. It shares the existing Alpha 2.1 dictionaries and fonts while keeping the original Draftor document directory intact.

**Better support for multifunction Bluetooth keyboards.** The input handling now correctly receives separate keyboard and multimedia reports, addressing an issue where pressing a media key could interrupt subsequent typing. Standard F9 remains the USB transfer shortcut; some keyboards require Fn to send it.

**Document position restoration.** Newly imported manuscripts can reopen at the default position when no cursor position has previously been saved. Genuine read errors and damaged document records continue to be reported.

## Downloads

Alpha 2.2 provides the Waveshare firmware and updated source code. Dictionaries, fonts and build inputs are reused from Alpha 2.1.

| File | Purpose |
|---|---|
| [Alpha 2.2 Waveshare Firmware](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.2/Draftor_v0.1.0-alpha.2.2_WAVESHARE_RLCD42_Firmware.zip) | Board-specific firmware, boot files, partition layout and installation tools |
| [Alpha 2.1 Resources](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip) | Matching dictionaries, indexes and Chinese fonts |
| [Alpha 2.2 Source](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.2/Draftor_v0.1.0-alpha.2.2_Source.zip) | Draftor source and documentation |
| [Alpha 2.1 BuildInputs](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_BuildInputs.zip) | Fixed dependencies and generated inputs required to build the firmware |

For normal installation, use the matching Firmware and the prepared Resources. Source and BuildInputs are needed when building from source.

## Installation and upgrades

**First installation on a Waveshare board:** Follow the [installation guide](INSTALL.md) to prepare the microSD card and install the firmware. A first installation erases the board's internal Flash contents and settings. Back up anything you want to keep before proceeding.

**Updating a compatible Waveshare installation:** After verifying the partition layout and active application slot, you can update the application while retaining the existing microSD resources and manuscripts. Formatting the microSD card is unnecessary.

**Original ESP32-S3 + Osptek ST7306 hardware:** Continue using [Alpha 2.1](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2.1). Select the firmware for the correct display and storage hardware.

See the [installation guide](INSTALL.md) for flashing and the [user guide](USAGE.md) for keyboard shortcuts, Chinese input and document transfers.

## Notes

Alpha 2.2 remains a test release. Back up important manuscripts regularly. The Waveshare version has been used with USB power, a Logitech Bluetooth keyboard and a Linux host; wider hardware and battery testing remains to be done.

Local Wi-Fi transfer is still experimental. Document lists and trash operations may also take longer when many manuscripts are stored. See [Known Issues](KNOWN_ISSUES.md).

Licensing and source information for fonts, dictionaries and third-party components is available in [Resource Status](RESOURCE_STATUS.md). Earlier Alpha 2 input-performance results remain documented separately; no new performance claim is made for this hardware port.
