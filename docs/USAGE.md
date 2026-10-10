# Draftor Usage & Interaction Guide / 操作手册

This guide explains how to type, edit documents, manage files, and transfer text using Draftor TF20 Alpha.

本指南介绍 Draftor TF20 Alpha 的文字输入、文档编辑、文件管理以及 USB 导入导出操作。

The instructions apply to the ESP32-S3 N16R8 (Thick) version of Draftor TF20 Alpha.

本指南适用于 ESP32-S3 N16R8（Thick）版本的 Draftor TF20 Alpha。

---

## 1. Keyboard Shortcuts / 核心快捷键

| Action / 操作 | Key / 按键 | Description / 说明 |
|---|---|---|
| **English / Chinese 输入切换** | `Caps Lock` | Switch between direct English input and bgIME Chinese input. / 切换英文直接输入与 bgIME 中文输入。 |
| **Save / 保存** | `Ctrl+S`, `Cmd+S` or `F6` | Save the current document. Wait for the save operation to finish. / 保存当前文档，等待保存操作完成。 |
| **Files / 文件管理** | `F2` | Open the document list. / 打开文件列表。 |
| **Layout / 横竖排切换** | `F4` | Switch between horizontal and vertical writing layouts. / 切换横排与竖排编辑。 |
| **Rename / 重命名** | `F2` → `R` | Select a document in the file list, then press R. / 在文件列表中选择文档，再按 R。 |
| **USB Menu / USB 菜单** | `F9` | Open the USB document transfer menu. / 打开 USB 文档交换菜单。 |
| **Export / 导出** | `F9` → `E` | Export the current document to a read-only USB drive. / 通过只读 USB 磁盘导出当前文档。 |
| **Inbox / 导入** | `F9` → `I` | Receive TXT files from a computer. / 从电脑接收 TXT 文件。 |
| **Confirm / 确认** | `Enter` | Confirm the current selection or action. / 确认当前选择或操作。 |
| **Cancel / 取消** | `Esc` | Cancel or return according to the current interface. / 根据当前界面取消操作或返回。 |

### Using Menus / 菜单操作

Draftor uses different key actions depending on the current interface. Follow the instructions shown on the screen when working with the file list, input method, or USB transfer menu.

草稿机的按键行为取决于当前界面。在文件列表、输入法候选区和 USB 菜单中，请以屏幕提示为准。

When entering Chinese text, finish or cancel the current composition before switching input modes or entering another operation that requires a completed input state.

中文输入尚未完成时，请先提交或取消当前编码，再切换输入模式或执行需要结束输入状态的操作。

---

## 2. Chinese Input & File Renaming / 中文输入与文件重命名

### 2.1 bgIME Chinese Input / 中文输入

Draftor uses bgIME (Big Goose Input Method Engine) for Chinese text input. The current Alpha supports:

草稿机通过 bgIME（大鹅输入法）输入中文。目前 Alpha 支持：

- **Quanpin / 全拼**
- **Xiaohe Shuangpin / 小鹤双拼**
- **Wubi 86 / 五笔86**

Type the input code, wait for the candidates to appear, and use the displayed selection controls to choose the intended character or word.

输入编码后，等待候选汉字或词语出现，再根据屏幕显示的候选编号或选择方式完成输入。

The candidate list and navigation behaviour may differ between input methods and horizontal or vertical layouts. Follow the current on-screen presentation.

不同输入方案以及横竖排布局下，候选显示和导航行为可能有所不同，请以当前屏幕为准。

**Esc / 取消输入**

During normal Chinese composition, `Esc` follows the current input-state cancellation sequence. It may return from candidate selection to raw-code editing before the uncommitted composition is discarded.

正文中文输入时，`Esc` 根据当前输入法状态逐级取消。它可能先退出候选选择、返回原始编码编辑，再取消尚未提交的编码，并不保证第一次按下就清空全部输入。

In the Rename dialog, `Esc` cancels the entire rename operation instead.

但在文件重命名界面，`Esc` 会直接取消整个重命名操作。

### 2.2 Renaming Documents / 文件重命名

Draftor supports Chinese filenames. Renaming a document does not modify its text content or automatically open another document.

草稿机支持中文文件名。重命名不会修改文档正文，也不会自动打开其他文档。

**Steps / 操作步骤**

1. Press `F2` to open the file list.

   按 `F2` 进入文件列表。

2. Select the document you want to rename and press `R`.

   选择需要重命名的文档，按 `R` 进入改名界面。

3. Edit the filename using English direct input or bgIME Chinese input.

   使用英文直接输入或 bgIME 中文输入编辑文件名。

4. When using Chinese input, select the intended characters or words first. They are committed to the temporary filename, not immediately written as the document's new name.

   输入中文时，先选词上屏。此时文字仅进入临时文件名，不会立即修改磁盘中的文档名称。

5. After confirming the complete filename, press `Enter` to perform the rename.

   检查完整文件名无误后，按 `Enter` 确认改名。

6. To cancel, press `Esc` before the rename is committed.

   如需取消，请在最终确认前按 `Esc`，原文件名保持不变。

**Important / 注意**

- Candidate selection and final filename confirmation are separate actions.

  中文选词与最终改名确认是两个独立操作。

- Letters such as `i`, `u`, `I`, and `U` are treated as filename input while the Rename dialog is active. They do not trigger file-list shortcuts.

  在改名界面中，`i`、`u`、`I`、`U` 等字母属于文件名输入，不触发文件列表快捷键。

- Pressing `Esc` in the Rename dialog cancels the entire operation, including any uncommitted Chinese composition.

  在改名界面按 `Esc` 会取消整个改名操作，包括尚未完成的中文编码。

- Renaming and opening are separate operations. To open a document, select it explicitly from the file list.

  改名与打开文档是独立操作。需要打开文档时，请返回文件列表进行选择。

---

## 3. USB Document Transfer / USB 文档导入与导出

Draftor supports offline TXT exchange with a computer through USB Mass Storage Class (MSC).

草稿机通过 USB MSC（大容量存储设备）与电脑交换 TXT 文档，不需要互联网或云端服务。

Export and Inbox are two separate modes:

导出和导入采用两种独立模式：

- **Export / 导出：** A read-only virtual USB drive containing a snapshot of the current document.

  **导出：** 向电脑提供一个只读虚拟 USB 磁盘，包含当前文档的文本快照。

- **Inbox / 导入：** A temporary writable USB drive used to receive TXT files before importing them as new drafts.

  **导入：** 向电脑提供一个临时可写 USB 磁盘，用于接收 TXT 文件，再由用户选择导入为新草稿。

**Always safely eject the USB drive on your computer before ending the transfer.**

**结束 USB 传输前，请务必在电脑上安全弹出磁盘。**

### 3.1 Export Mode / 导出模式

Export does not expose the device's internal TF card filesystem. Instead, Draftor presents a read-only virtual drive containing the current document as `DOCUMENT.TXT`.

导出模式不会直接开放设备内部 TF 卡的文件系统。草稿机会创建一个只读虚拟磁盘，将当前文档作为 `DOCUMENT.TXT` 提供给电脑。

**Steps / 操作步骤**

1. Connect Draftor's USB data port to your computer.

   将草稿机的 USB 数据接口连接到电脑。

2. Press `F9` to open the USB menu, then press `E`.

   按 `F9` 进入 USB 菜单，再按 `E` 选择导出。

3. Wait until the virtual USB drive is ready and appears on your computer.

   等待虚拟 USB 磁盘准备完成，并在电脑上显示。

4. Open the drive and copy `DOCUMENT.TXT` to your computer.

   打开磁盘，将 `DOCUMENT.TXT` 复制到电脑。

5. Safely eject the USB drive using your computer's operating system.

   在电脑上执行安全弹出磁盘。

6. Return to Draftor. Once the computer has released the drive, press `Esc` if needed to leave the USB interface.

   返回草稿机，确认电脑已经弹出磁盘后，按屏幕提示操作；如仍停留在 USB 界面，可按 `Esc` 返回。

**Important / 注意**

- The exported document is a snapshot of the current text. It is not a live, editable connection to the original document.

  导出的文字是当前文档的快照，不是与原文档实时同步的编辑连接。

- The virtual drive is read-only. Do not attempt to edit, rename, or delete files on it.

  虚拟磁盘为只读模式，不能在其中修改、重命名或删除文件。

- Editing the copied TXT on your computer does not change the original Draftor document.

  在电脑上修改复制出来的 TXT，不会改变草稿机中的原文档。

### 3.2 Inbox Mode / 导入模式

Inbox receives TXT files through a temporary writable USB drive. Received files are not written directly into the active document.

Inbox 通过临时可写 USB 磁盘接收 TXT 文件。电脑写入的内容不会直接覆盖草稿机当前正在编辑的文档。

**Steps / 操作步骤**

1. Connect Draftor's USB data port to your computer.

   将草稿机的 USB 数据接口连接到电脑。

2. Press `F9` to open the USB menu, then press `I`.

   按 `F9` 进入 USB 菜单，再按 `I` 选择导入。

3. Wait for the Inbox drive to appear on your computer.

   等待 Inbox 暂存磁盘在电脑上出现。

4. Copy the desired `.txt` files into the Inbox drive.

   将需要导入的 `.txt` 文件复制到 Inbox 磁盘。

5. **Safely eject the Inbox drive on your computer.**

   **在电脑上安全弹出 Inbox 磁盘。**

6. Draftor will inspect the received files and display the available TXT documents. Use `↑` / `↓` to select the desired file.

   草稿机会检查接收到的文件，并显示可导入的 TXT 列表。使用 `↑` / `↓` 选择目标文件。

7. Press `Enter` to import the selected TXT as a new draft.

   按 `Enter`，将选中的 TXT 导入为新草稿。

8. After a successful import, choose what to do next:

   导入成功后，可以选择：

   - `Enter` — Open the newly imported draft.

     `Enter` — 打开刚导入的新草稿。

   - `Esc` — Return to the original document without switching to the imported draft.

     `Esc` — 返回原文档，不切换到新导入的草稿。

**Important / 注意**

- Import creates a new draft. It does not overwrite the original manuscript.

  导入会创建新草稿，不覆盖原稿。

- A successful import does not automatically change the active document (`CURRENT`). The active document changes only if the user explicitly chooses to open the newly imported draft.

  导入成功后，不会自动改变当前文档（CURRENT）。只有用户明确选择打开新稿时，才会切换当前文档。

- Inbox uses temporary RAM-backed storage. Files that have not been imported may be lost when the session ends or the device loses power.

  Inbox 使用临时内存存储。尚未导入的暂存文件可能在会话结束或设备断电后丢失。

- Do not disconnect the USB cable or interrupt power while the computer is writing to Inbox.

  电脑正在向 Inbox 写入文件时，不要拔出 USB 线缆或中断设备供电。

- For transfer testing, use disposable test files rather than your only copy of an important manuscript.

  测试传输功能时，请使用测试文件，不要直接使用重要文稿的唯一副本。

---

## 4. Troubleshooting / 异常处理

Draftor TF20 is an Alpha release. Some error conditions and recovery scenarios have not been fully tested.

Draftor TF20 仍属于 Alpha 版本，部分异常情况及恢复场景尚未完成充分测试。

If the device reports a fatal error, memory allocation failure, unexpected restart, or HALT message:

如设备出现致命错误、内存分配失败、异常重启或 HALT 停止提示：

1. **Stop the current operation.**

   停止继续输入或重复执行当前操作。

2. **Record the error.**

   记下屏幕上的错误信息、状态码及发生前的操作。有条件时，可以拍照或保存串口日志。

3. **Avoid repeated forced restarts.**

   不要在原因不明时反复强制重启，尤其是在保存或 USB 传输过程中。

4. **Preserve existing files.**

   排查问题前，不要随意格式化 TF 卡、删除文档目录或覆盖原有资源。

5. **Report the issue.**

   如果能够稳定复现，可以在 [Draftor GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) 中报告问题，说明硬件、固件版本、操作步骤和实际现象。上传日志前，请检查其中是否包含私人文字或设备识别信息。

   如果问题可以稳定复现，欢迎通过 GitHub Issues 提交报告。请尽可能提供复现步骤，并注意保护私人文档和设备信息。

For known limitations and the current testing scope, see [Known Issues / 已知问题](KNOWN_ISSUES.md).

现有功能限制及测试覆盖范围，请参阅[已知问题](KNOWN_ISSUES.md)。

---

**Draftor TF20 Alpha — v0.1.0-alpha.1**

[Project Home / 项目主页](https://github.com/Ohhellboy/Draftor) · [Download / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Installation / 安装](INSTALL.md) · [Hardware / 硬件](HARDWARE.md)
