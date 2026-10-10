# Draftor Known Issues & Limitations / 已知问题与限制

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Download / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Usage Guide / 操作手册](USAGE.md) · [Release Notes / 发行说明](RELEASE_NOTES.md)

Draftor is still an Alpha release. The main writing and Chinese input functions have passed selected tests on real hardware, but the device has not yet undergone comprehensive reliability testing.

Draftor 仍处于 Alpha 阶段。主要写作和中文输入功能已经通过部分实机测试，但尚未完成全面的可靠性验证。

This document covers three different kinds of limitations: current operating constraints, previously observed issues that are not fully explained, and conditions that have not yet been sufficiently tested. They are not all confirmed bugs in the current firmware.

本页记录三类情况：当前版本的使用限制、历史上出现过但尚未完全解释的问题，以及尚未充分测试的场景。**它们并不全是当前固件能够复现的故障。**

---

## 1. Before You Start / 使用前需要注意

### Power Supply / 供电

The standalone battery power module is still under development and is not included in this Alpha.

独立电池供电模块仍在开发中，不包含在本次 Alpha 中。

The current prototype requires an external power supply. Use a stable supply compatible with your ESP32-S3 board and check the wiring before powering on.

目前设备需要外部供电。请使用符合 ESP32-S3 开发板要求的稳定电源，并在通电前检查接线。

Unexpected power loss during writing or storage operations has not been fully tested. Do not assume that every interrupted operation can be recovered.

写作或存储过程中突然断电的恢复能力尚未得到充分验证。不要假定所有被中断的操作都能完整恢复。

### Document Backups / 文档备份

Draftor supports local document storage and TXT export, but the current Alpha should not be the only place where you keep important manuscripts.

草稿机支持本地保存和 TXT 导出，但目前不建议把 Alpha 设备作为重要文稿的唯一存储位置。

Save your work regularly, export important documents to your computer, and keep independent backups.

建议定期保存并导出重要文稿，在电脑或其他存储介质上保留独立备份。

### TF Card / TF 卡

Do not remove the TF card while the device is powered or performing storage operations.

设备仍在通电或进行存储操作时，不要直接取出 TF 卡。

If you need to change the card or prepare resources, finish the current operation, disconnect power, and follow the [Installation Guide](INSTALL.md).

如需更换 TF 卡或整理资源，请先结束当前操作并断电，再参照[安装指南](INSTALL.md)处理。

---

## 2. Current Functional Limits / 当前功能限制

### Chinese Input / 中文输入

bgIME supports Full Pinyin, Xiaohe Shuangpin, and Wubi 86, but some operations depend on the current composition state.

bgIME 支持全拼、小鹤双拼和五笔86，但部分按键行为取决于当前的编码输入状态。

- **Mode switching / 输入模式切换：** Finish or cancel an active Chinese composition before switching between Chinese and English input.

  尚有未提交的中文编码时，需要先完成或取消输入，再切换中英文模式。

- **Esc behaviour / Esc 行为：** `Esc` does not always clear everything in a single press. It follows the current composition and candidate-selection state.

  `Esc` 不一定一次清空所有编码，而是根据当前输入法状态逐级取消。

- **Rename / 文件重命名：** `Esc` cancels the entire rename operation, including uncommitted Chinese input.

  在文件重命名界面，`Esc` 会取消整个改名操作，包括尚未提交的中文编码。

These behaviours are part of the current interaction design, not necessarily input failures. See the [Usage Guide](USAGE.md).

这些属于当前交互设计，并不一定是输入法故障。具体操作请参考[操作手册](USAGE.md)。

### USB Document Transfer / USB 文档交换

USB Export and Inbox are separate transfer modes.

USB 导出与 Inbox 导入是两个独立的传输模式。

**Export / 导出**

Export provides a read-only virtual USB drive containing `DOCUMENT.TXT`, a snapshot of the current document. It does not expose the internal TF card filesystem or provide live document synchronization.

导出模式向电脑提供只读虚拟磁盘，其中的 `DOCUMENT.TXT` 是当前文档的快照。它不会开放内部 TF 卡文件系统，也不提供实时文档同步。

**Inbox / 导入**

Inbox uses temporary RAM-backed storage. Files copied into Inbox are not permanent drafts until they have been successfully imported.

Inbox 使用临时内存暂存文件。复制进去的 TXT 只有成功导入后，才会成为草稿机中的正式文档。

- Safely eject the drive on your computer before selecting files on Draftor.

  请先在电脑上安全弹出磁盘，再在草稿机上选择导入文件。

- Leaving the session or losing power may discard files that have not yet been imported.

  退出会话或设备断电时，尚未导入的暂存文件可能丢失。

- A successful import creates a new draft without automatically switching the active document.

  导入成功后会创建新稿，但不会自动切换当前编辑的文档。

Do not disconnect the cable or interrupt power while the computer is writing to Inbox.

电脑正在写入 Inbox 时，请勿拔出 USB 线缆或中断电源。

### Fonts & Display / 字体与显示

Draftor includes font resources for Simplified Chinese, Traditional Chinese, and selected variant characters.

草稿机提供简体、繁体以及部分异体字的字体资源。

However, this does not mean that every Unicode character or every possible variant is supported.

但这不代表所有 Unicode 字符和异体字都已经得到支持。

Some complex characters may be difficult to distinguish at the small font sizes used by the interface. The 14px interface font has been checked using representative samples, not every possible character.

部分复杂字形在较小字号下可能不容易辨认。当前 14px 界面字体已经通过代表性样本检查，但尚未逐一验证所有字符。

---

## 3. Previously Observed Issues / 尚未完全解释的历史问题

Some issues were reported during earlier development and received targeted fixes. Later tests passed in selected scenarios, but not every original failure has been fully explained.

此前开发过程中曾出现一些问题，并已针对性修复。后续相关场景通过了测试，但部分历史故障的完整原因尚未查明。

### Enter Key Behaviour / Enter 按键

Earlier testing recorded an issue involving the first physical `Enter` press during certain interface or input-state transitions.

此前测试曾记录过特定界面或输入状态切换后，第一次按下实体 `Enter` 时的异常行为。

Related input-state handling has been improved, and subsequent targeted tests passed. However, the complete cause of the original report remains unresolved.

相关输入状态处理已经改进，后续定向测试也已通过，但最初故障的完整原因尚未彻底确认。

This does not mean the issue is currently reproducible. If you encounter a similar problem, please record the interface state and the exact sequence of key presses.

这不意味着问题在当前版本中仍能稳定复现。如果遇到类似情况，请记录当时的界面状态和具体按键顺序。

### Memory & Allocation / 内存与分配问题

Earlier development also encountered memory allocation failures under constrained conditions.

此前开发中也曾在内存受限的情况下出现分配失败。

Several fixes and internal memory optimizations have since been applied, followed by successful targeted tests. However, not all historical causes have been fully established, and extreme memory pressure has not been comprehensively tested.

此后已完成多项修复和内部内存优化，并通过了相关定向测试。不过，部分历史问题仍未完成全部归因，极限内存压力下的行为也尚未得到全面验证。

If the device reports an allocation failure or stops responding, do not repeatedly trigger the same operation. Record the displayed message and the actions leading to it.

如果出现内存分配失败或设备停止响应，请不要反复触发同一操作。建议记录屏幕信息及发生前的操作步骤。

### Host Regression Tests / 电脑端回归测试

The existing Host test suite retains **16 inherited failure cases** from earlier development. These are not newly introduced failures in the Alpha integration, nor are they marked as fixed.

现有电脑端（Host）测试中仍保留 **16 项历史继承失败**。它们不是 Alpha 整合时新增的失败，也没有被直接标记为已修复。

A Host test failure is not necessarily a reproducible fault on the current device. It does, however, mean that the release cannot claim that every test passes.

Host 测试失败不一定对应当前设备上可复现的故障，但这也意味着本版不能宣称全部测试通过。

---

## 4. Testing Coverage / 测试覆盖范围

### What Has Been Tested / 已验证内容

The Alpha firmware has passed selected tests on the ESP32-S3 device, including:

Alpha 已在 ESP32-S3 实机上通过部分测试，主要包括：

- Device startup and readiness / 设备启动与就绪
- Chinese candidate display, paging, and selection / 中文候选显示、翻页与选词
- Saving and reopening documents / 文档保存与重新打开
- USB TXT export / USB TXT 导出
- Inbox import as a new draft / Inbox 导入为新草稿

Wubi 86 and horizontal/vertical editor integration also have earlier software and device testing results.

五笔86以及横竖排编辑器整合也有此前的软件与实机测试记录。

These tests establish that the recorded operations worked in the tested conditions. They do not guarantee identical behaviour across all hardware configurations or long-term use.

这些测试证明了相应功能在指定条件下能够运行，并不保证所有硬件组合或长期使用场景都具有相同表现。

### What Has Not Been Fully Tested / 尚未充分验证的内容

- **Long-duration operation / 长时间运行：** Continuous operation over extended periods has not been comprehensively qualified.

  尚未完成充分的长时间连续运行验证。

- **Unexpected power loss / 意外断电：** Recovery from power interruption during different storage operations remains insufficiently tested.

  不同存储操作过程中意外断电的恢复能力尚未充分测试。

- **Extreme memory conditions / 极限内存工况：** Behaviour under severe memory pressure has not been fully characterized.

  严重内存压力下的行为尚未完成全面验证。

- **Hardware variations / 硬件差异：** The release has not been validated against every ESP32-S3 board, TF adapter, keyboard, or power arrangement.

  本版尚未覆盖所有 ESP32-S3 开发板、TF 卡模块、蓝牙键盘及供电组合。

- **First-time installation / 空板首次安装：** The required flashing files are provided, but the publication process did not include a fresh qualification run on a new blank board.

  首次烧录所需的文件已经提供，但本次发布流程并未另外执行新空板的完整实机验收。

For installation and hardware compatibility, refer to [Installation](INSTALL.md) and [Hardware](HARDWARE.md).

安装与硬件适配细节请参阅[安装指南](INSTALL.md)及[硬件说明](HARDWARE.md)。

---

## 5. Not Included in This Alpha / 本次尚未包含的功能

Some features are under development or planned for later versions. They are not part of the current Alpha firmware.

部分功能正在开发或计划在后续版本中加入，目前不包含在 Alpha 固件中。

| Feature / 功能 | Current status / 当前状态 |
|---|---|
| **Standalone battery power / 独立电池供电** | Under development; external power required / 开发中，目前需要外部供电 |
| **Wi-Fi document transfer / Wi-Fi 文档传输** | Not included in this release / 本版未接入 |
| **Chinese input in Find / 查找界面中文输入** | Not included in this release / 本版未接入 |
| **Full interface localization / 完整界面中文化** | Not yet implemented / 尚未完成 |
| **Complete Pico 2 W (Thin) firmware / Pico 2 W 完整固件** | Not included; standalone display demos remain available / 本版未提供，独立屏幕示例仍可使用 |

These are release-scope limitations, not necessarily bugs.

这些是本次发行的功能边界，并不一定属于软件故障。

---

## 6. Source & Resource Limitations / 源码与资源限制

The released Source and BuildInputs archives contain the project inputs used for the current ESP32-S3 build. The specified toolchain must be installed separately.

本次发布的 Source 和 BuildInputs 包提供了当前 ESP32-S3 构建所需的项目输入，但仍需另外安装指定版本的工具链。

Not every historical dictionary or font-generation step can be reproduced from the currently available original upstream materials. This does not mean the complete runtime resource files are missing from the release.

部分历史词库或字体的原始生成过程尚未完全恢复，但这不代表本次发布缺少完整的运行资源文件。

Some third-party and mixed resources also have unresolved provenance or redistribution terms. These are documented separately and are not covered automatically by the Apache-2.0 license used for Draftor-owned source code.

部分第三方及混合资源的来源或再分发许可范围仍待明确。这些问题另有记录，不会因为 Draftor 自有源码采用 Apache-2.0 而自动获得授权。

See [Build Guide](BUILD.md) and [Resource & License Status](RESOURCE_STATUS.md).

详见[构建指南](BUILD.md)和[资源与许可说明](RESOURCE_STATUS.md)。

---

## 7. Reporting Problems / 问题反馈

If you encounter a problem, you can report it through [Draftor GitHub Issues](https://github.com/Ohhellboy/Draftor/issues).

如果发现问题，欢迎通过 [Draftor GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) 反馈。

A useful report should include:

建议尽量提供：

1. **Hardware / 硬件：** ESP32-S3 board, display, keyboard, TF card, and power arrangement.

   开发板、屏幕、键盘、TF 卡及供电方式。

2. **Firmware / 固件：** The firmware version or build identity shown by the device.

   设备使用的固件版本或构建标识。

3. **Steps / 操作：** What you were doing and the sequence of actions that led to the issue.

   出现问题之前的操作，以及能够复现问题的具体步骤。

4. **Expected vs. actual / 预期与实际：** What you expected and what actually happened.

   你希望设备怎样运行，以及实际发生了什么。

5. **Evidence / 现场信息：** A screen photo, error message, or serial log if available.

   如有条件，可以附上屏幕照片、错误提示或串口日志。

**Please check logs and screenshots before uploading them.** Chinese input and writing-device diagnostics may contain private text, document names, or device identifiers.

**上传截图或日志前，请先检查私人信息。** 中文输入和写作设备的诊断记录可能包含私人文字、文档名称或设备识别信息。

There is no need to share your manuscript to report a bug. A short, reproducible example is usually more useful.

报告问题不需要提供完整私人文稿。一个简短、能够复现问题的测试样例通常更有帮助。

---

Draftor is still under development. The Alpha can already be used for writing and experimentation, but it is not yet a fully qualified, finished product.

Draftor 仍在开发中。这个 Alpha 已经可以用于写作和实验，但还不是完成全面验证的最终产品。

[Project / 项目主页](https://github.com/Ohhellboy/Draftor) · [Release / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1) · [Usage / 操作手册](USAGE.md) · [Installation / 安装指南](INSTALL.md)
