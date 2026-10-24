# Draftor Alpha 2 User Guide

**English** | [简体中文](USAGE.zh-CN.md)

This guide covers the ESP32-S3 N16R8 (Thick) version of Draftor: entering text, renaming manuscripts, and transferring manuscripts over USB or local Wi-Fi. If you have not installed the firmware yet, start with the [installation guide](INSTALL.md).

## 1. Common keys

| Action | Keys | Notes |
|---|---|---|
| Switch between English and Chinese input | `Caps Lock` | Switch between direct English input and Chinese input. |
| Save the current manuscript | `Ctrl+S`, `Cmd+S` or `F6` | Wait for saving to finish after pressing the key. |
| Open the file list | `F2` | Browse manuscripts and select one to open or rename. |
| Switch between horizontal and vertical layout | `F4` | Change the direction in which text is arranged. |
| Rename a manuscript | `F2` → select a manuscript → `R` | Select a manuscript in the file list, then press `R`. |
| Open the USB transfer menu | `F9` | Press `E` in the menu to export, or `I` to import. |
| Open the local Wi-Fi page | `F10` | Show the information needed to join the hotspot and open the transfer page. |
| Confirm a menu selection or action | `Enter` | Follow the instructions on the current page. |
| Cancel or go back | `Esc` | Its effect depends on the screen; see below. |

In the table, `F9` → `E` means press `F9`, then press `E`, rather than holding both keys at once.

The same key may have different effects while writing, selecting candidates, browsing files or using a transfer menu. Follow the instructions on the current page.

## 2. Entering text and renaming manuscripts

### Chinese input

Draftor uses bgIME (大鹅输入法), with support for full Pinyin, Xiaohe double Pinyin and Wubi 86. Press `Caps Lock` to switch between Chinese input and direct English input.

For Chinese input, type the code for the current input scheme, wait for candidates to appear, then select a character or phrase using the number or selection instructions shown on screen. Candidate order and paging controls vary with the input scheme and horizontal or vertical layout; follow the instructions in the candidate area.

Before switching between Chinese and English, or starting an action that requires you to finish the current input, complete your selection or cancel the unfinished input.

### Cancelling unfinished input

When entering Chinese in the manuscript body, `Esc` backs out in stages: it may first leave candidate selection and return to code editing, then cancel the uncommitted input. The letters you typed may therefore remain after the first press.

**In the rename screen, `Esc` works differently: it cancels the entire rename operation and leaves the original filename unchanged.**

### Saving and changing the layout

Press `Ctrl+S`, `Cmd+S` or `F6` to save the current manuscript, and wait for saving to finish before continuing. Press `F4` to switch between horizontal and vertical layout.

### Renaming a manuscript

Draftor supports Chinese filenames. Renaming does not change the manuscript body or automatically open another manuscript.

1. Press `F2` to open the file list, then select the manuscript to rename.
2. Press `R` to open the rename screen.
3. Enter or edit the filename. You can type in English or use bgIME for Chinese.
4. For Chinese input, select the characters or phrases first. The selected text goes into the filename field; the new name has not yet been saved.
5. Once the whole filename is correct, press `Enter` to complete the rename.

**Selecting Chinese text and confirming the rename are two separate steps.** Pressing `Esc` before the final confirmation discards the rename, including any unfinished Chinese input, and keeps the original filename.

In the rename field, letters enter the filename instead of triggering the file list’s letter shortcuts. To open the manuscript after renaming it, return to the file list and select it.

## 3. Importing and exporting manuscripts over USB

Connect Draftor’s USB data port to a computer to exchange TXT manuscripts, much as you would copy files to or from a USB drive. No network connection is needed.

USB transfer has two modes:

| Mode | What appears on the computer | Purpose |
|---|---|---|
| Export | A read-only drive containing the current manuscript as `DOCUMENT.TXT` | Copy text from Draftor to the computer. |
| Import (Inbox) | A temporary writable drive | Receive TXT files from the computer, then import them on Draftor as new manuscripts. |

**For both import and export, safely eject the drive on the computer before leaving Draftor’s USB screen.**

### Exporting to a computer

1. Connect Draftor to the computer with a USB data cable.
2. On Draftor, press `F9`, then `E`.
3. Wait for the new USB drive to appear on the computer, then open it.
4. Copy `DOCUMENT.TXT` to the computer.
5. Safely eject the drive on the computer.
6. Return to Draftor and follow the on-screen instructions to exit; if the USB screen is still open, press `Esc` to go back.

The computer sees a text copy of the current manuscript, not the entire TF card. The drive is read-only: files on it cannot be edited, renamed or deleted.

**The exported file does not stay in sync with Draftor.** Editing the copied TXT file on the computer does not change the original manuscript on Draftor.

### Importing from a computer

1. Connect Draftor to the computer with a USB data cable.
2. On Draftor, press `F9`, then `I`.
3. Wait for the Inbox drive to appear on the computer, then copy the `.txt` files you want to import onto it.
4. Once copying is complete, **safely eject the Inbox drive on the computer**.
5. Draftor checks the received files and lists the TXT files available to import. Use `↑` and `↓` to select a file, then press `Enter` to import it.
6. After a successful import, press `Enter` to open the new manuscript, or `Esc` to return to the original manuscript without switching to the new one.

**Importing creates a new manuscript and does not overwrite your original.** The manuscript you are editing changes only when you explicitly choose to open the new one.

Inbox is only a temporary receiving area. After copying files into it, you still need to complete the import on Draftor; otherwise, files not yet imported may be lost when you leave the transfer mode or the device loses power. Do not unplug the cable or turn off the device while the computer is writing to Inbox.

Keep the original files for important manuscripts on your computer. Do not use the temporary inbox as your only backup.

## 4. Transferring manuscripts over local Wi-Fi

Draftor can create its own Wi-Fi hotspot so a phone browser can open the local transfer page to import or export manuscripts. You do not need the venue’s Wi-Fi network or a cloud account.

### Connecting a phone

Wi-Fi is off by default at startup. First finish or cancel any Chinese input and wait for saving to finish. If a USB transfer is active, leave it as described in the previous section.

1. Press `F10` to open the local hotspot page.
2. On the connection QR-code page, scan the code with your phone to join Draftor’s hotspot.
3. Press `Tab` to switch to the URL QR-code page, then use your phone to open the local transfer page.
4. Import or export manuscripts in the phone browser.

If scanning the QR codes is inconvenient, press `P` to view the manual connection information, then connect using the hotspot details and address shown on screen.

Manuscripts imported over Wi-Fi are also saved as new manuscripts. They do not directly overwrite or switch the manuscript currently being edited.

### Returning to writing and turning off the hotspot

Press `Esc` to return to writing, **but the hotspot stays on**.

When you have finished transferring and want to turn off the hotspot, press `F10` to return to the Wi-Fi page, then press `O`.

USB and Wi-Fi manuscript transfer cannot be used at the same time. Turn off the Wi-Fi hotspot before switching to USB.

## 5. If something goes wrong

If you see a save error, an unexpected restart, a memory error or a stop message such as `HALT`, stop typing or repeating the current operation.

Note the message on screen and what you were doing before the error; take a photo if convenient. When the cause is unclear, avoid repeatedly forcing a restart, especially during saving or transfer. Do not format the TF card, delete manuscript directories or overwrite existing resources to troubleshoot the problem.

To report a problem, use [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) and describe your board model, firmware version, the steps you took and what happened. **You do not need to upload private manuscripts, complete input logs or Flash backups.** Before sharing screenshots or diagnostic information, check whether they contain private text or device information.

Alpha 2 is still a test release. Regularly export important manuscripts and keep a separate backup. See [Known Issues](KNOWN_ISSUES.md) for other limitations and privacy information.

---

[Project home](../README.en.md) · [Release notes](RELEASE_NOTES.md) · [Installation](INSTALL.md) · [Hardware wiring](HARDWARE.md)
