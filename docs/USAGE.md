# Usage / 操作

| Action / 操作 | Key / 按键 |
|---|---|
| English / Chinese mode 英中切换 | Caps Lock |
| Save 保存 | Command/Ctrl+S (keyboard mapping), or F6 |
| Files 文件页 | F2 |
| USB menu | F9 |
| Export / Inbox from USB menu | E / I |
| Accept / cancel current modal | Enter / Esc |

Follow the visible menu hints for layout and file actions. 文件页动作和横竖排按当前屏幕提示进行，不从正文输入快捷键推定模态行为。

Chinese input uses the existing composition and numbered candidate rules. In Rename, first confirm the IME text into the temporary filename; only a subsequent final confirmation commits the name. Candidate selection must not rename the whole document. Esc clears composition and cancels the temporary filename. Rename keeps document identity and content; opening a file is a separate action.

改名框中 i/u/I/U 属于文件名输入。中文先选词提交到临时名称，再确认整个改名；Esc 取消不改原名。保存完成以 Saved 为准，不重复按键催促。

Export exposes a read-only TXT snapshot. Copy the intended test TXT, safely eject the disk on the computer, then exit the device UI. Inbox is a separate writable staging session: safely eject before import, choose the intended TXT, and publish as a **new** draft. Import does not automatically switch CURRENT or overwrite the original. Do not use formal manuscripts for testing.

导出后先安全弹盘再退出；Inbox 暂存后按提示选择 TXT 导入为新稿，不自动切 CURRENT，也不覆盖原稿。出现错误、重启、分配失败或停止提示时保留现场并停止当前场景。
