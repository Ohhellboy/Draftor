# Draftor Alpha 3 — Waveshare: Learning from Word Selection, Seven Candidates, and Bluetooth Keyboards

[简体中文](RELEASE_NOTES.zh-CN.md) | **English**

Alpha 3 for the Waveshare **ESP32-S3-RLCD-4.2 (ST7305)** adds learning from word selection, improves the arrangement of Chinese candidates, and reworks Bluetooth keyboard connection. Keyboard connection is faster.

This first batch provides the Waveshare firmware and its matching learning index. Download and installation instructions are below.

## Frequently selected words move up

bgIME now learns from your word selections. A word that initially appears further down the list can move up after you select it repeatedly. Full Pinyin, Xiaohe Shuangpin, and Wubi 86 share these preferences: changing input schemes does not discard what the engine has learned about the same word.

Learning turns on automatically at startup, with a `Learning ON` message. Learning records are stored on the TF card and can be used again after saving and restarting.

To clear the learned preference for a displayed candidate, use `Ctrl+1` through `Ctrl+7` while the seven-candidate list is visible. The shortcut applies to the corresponding displayed candidate in the current row or column.

## Up to seven candidates in horizontal and vertical layouts

The main Chinese input view displays up to seven numbered candidates per row (horizontal) or column (vertical). Longer phrases take more space: up to **four two-character candidates** or **two three-character candidates** fit in a row. The Find interface continues to display up to six candidates per row.

Sometimes the first character of the next candidate peeks in at the end. It tells you there are more candidates on the next page; that partial hint cannot be selected until you turn the page. When the next row or column is already fully visible, the hint disappears.

## Pairing and switching Bluetooth keyboards

Draftor can remember up to three Bluetooth keyboards, but uses only one at a time. To pair a new keyboard, **press and hold its channel button** to enter pairing mode, then follow the instructions at the bottom of Draftor's screen. Enter the displayed PIN or confirm locally when prompted. If you mistype the PIN, restart the keyboard and enter pairing mode again.

Once paired, **briefly press the same channel button** to reconnect. You do not need to hold it and pair again every time.

To use a different paired keyboard, first turn off or disconnect the current one. Then turn on the next keyboard and briefly press its original channel button. Draftor will not take over an active keyboard connection merely because another keyboard wakes up nearby.

When all three saved places are occupied, you can still pair a fourth keyboard. Only after the new pairing and required confirmation succeed does it replace the earliest saved pairing. Briefly reconnecting or typing on a keyboard does not change that order.

## Saving your manuscript and learning records

Press `F6` or `Ctrl/Cmd+S` to save the current manuscript. Wait for saving to finish before continuing.

Before switching Draftor off, finish the current input and return to the manuscript, then press `Ctrl+F6`. Wait until the screen says `Saved; safe power off` before cutting power. This operation saves the manuscript and recent learning records, and pauses learning. If you decide to keep writing, press `Ctrl+F6` again to resume learning.

These are not the same operation: `F6` and `Ctrl/Cmd+S` save the manuscript, while `Ctrl+F6` combines manuscript saving, recent learning-data saving, and toggling learning on or off. The shortcut is admittedly doing too many jobs; its behaviour may be revised later.

## Display and existing functions

The Waveshare version defaults to a 180°-rotated display, including the manuscript, candidates, and menus. The firmware configures the onboard TF interface for 1-bit SDMMC at 40 MHz.

Horizontal and vertical writing, Chinese and English Find, local document management, and USB import/export are retained. Local Wi-Fi document transfer remains experimental. Please export and back up important manuscripts regularly, especially before updating firmware.

## Download and upgrade

This Alpha 3 batch is **only for the Waveshare ESP32-S3-RLCD-4.2 (ST7305)**. Get the firmware and matching resources from [GitHub Releases](https://github.com/Ohhellboy/Draftor/releases), and follow the [installation guide](INSTALL.md).

Compatible dictionaries and fonts may be retained, but the matching learning index is also required. You do not need to format an existing TF card solely to upgrade. Follow the guide to add the learning index and check manuscript and learning directories. **Updating the firmware alone will not migrate existing manuscripts.** Back up your writing and any learning records before updating.

Alpha 3 is being released in batches: **Waveshare → original ESP32-S3 N16R8 + Osptek ST7306 → Raspberry Pi Pico 2 W**. Each board has its own firmware image; do not cross-flash. Other variants will follow. Earlier versions remain available through [GitHub Releases](https://github.com/Ohhellboy/Draftor/releases).

Questions and suggestions are welcome through [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues).
