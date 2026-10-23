# Known issues, limits and privacy / 已知问题、限制与隐私

Alpha 2 is usable for writing and experimentation, with a finite paired comparison and one natural writing/save/reopen check. Keep independent exports of important manuscripts. Current limitations, historical observations and untested conditions are separated below.

Alpha 2 已有有限对照及一次自然写作、保存重开核验。重要文稿仍请独立导出备份；以下区分当前限制、历史现象与未覆盖条件。

## Current limits / 当前限制

- **2 MiB slots cannot accept this App.** Alpha 1 users should back up manuscripts and necessary data, erase internal flash, and follow the [fresh-install procedure](INSTALL.md). The TF card does not need formatting. / 旧2 MiB槽装不下本版；Alpha 1 用户请先备份，清除内部 Flash 后全新安装，不格式化 TF 卡。
- Resources must be the exact format4 set; format2 files/Ready markers do not make a mixed directory compatible. / 不混用旧新资源或伪造 Ready。
- Some activation/warm-query costs and synchronous full-screen blocking remain; see [Performance](PERFORMANCE.md). / 激活、热查询子段及同步整屏仍有代价。
- No learned frequency/personal dictionary, abbreviated or mixed Pinyin, Chinese Find entry, complete UI localization, selectable new fonts or complete Pico writing firmware is added. / 未新增学习、简拼混拼、中文查找、完整汉化、字体切换或 Pico 整机固件。
- Experimental F10 local Wi-Fi transfer and QR pages exist in the fixed image; Wi-Fi starts off, Esc returns to writing without turning it off, and F10 then O turns it off. The performance and FW1 evidence covers Wi-Fi off. / F10实验热点/二维码已在固件；Esc不关热点，F10后O关闭，本轮性能证据覆盖关闭状态。

## Serial diagnostics / 串口诊断

The author requested removing serial composition-code and input-timing observation from the release. Its build sets `DRAFTOR_AUTH_OBSERVE=0`, telemetry profile 0 and capture 1: `ui_trace.cpp::traceUi` returns before recording input events, `service_endpoints.cpp` is excluded by its existing guard, and `device_main.cpp::idleDiagnostics` returns without draining an input stream. The ordinary diagnostic command parser is also disabled in this configuration. The target check rejects linked observer symbols and SW/input-text format strings.

作者已决定发行版关闭组成码与输入计时观察。沿用现有编译开关关闭采集/排出路径，并检查实际链接镜像中没有对应观察符号和格式字符串；没有重写输入法业务。

Startup/build identity, connection, memory/health and error diagnostics remain on the serial port; it is not a silent port. No persistent input-log writer is introduced. Review diagnostic output before sharing it. The earlier measured baseline did emit composition codes automatically and remains privately retained as comparison evidence; its image is not substituted for this release App.

串口仍保留启动标识、连接、内存/运行状态及错误诊断，不是完全静默；没有新增持久输入日志。分享诊断前仍应检查内容。此前实测基线确实默认输出组成码，作为原证据单独保留，不拿它替代这份发行App。

The exact observer-disabled release App has passed a brief device check covering startup, Chinese input, candidate selection, saving and truly reopening the same test manuscript after leaving it. Reopened text was checked visually by the author; this was not an exported-byte comparison. Earlier performance measurements still describe the measured business baseline; no new performance test was run.

关闭输入观察的准确发行App已通过一次简短实机检查，覆盖启动、中文输入、选词、保存，以及离开后真正重开同一测试稿。重开文字由作者目视核对，本次没有进行正文导出字节比较。此前性能数据仍对应累计测量基线，本轮没有重新测试性能。

## Historical observations still open / 历史未彻底解释的现象

An intermittent first `n` in a longer Pinyin input was reported in earlier sessions; the latest finite session does not establish a general fix. Earlier display splitting/distortion under a different, unadopted coordination candidate is not a confirmed current format4/synchronous-delta fault. Both remain historical observations, not claims that every current input fails. Retained candidate/session assertion failures and missing hot-path observations are listed in the technical performance notes; packaging did not weaken those tests.

此前曾报告较长拼音中首次 `n` 未及时出现，后续有限会话不能作为普遍修复结论。旧未采纳协同候选上的分半/扭曲也不等于当前版已复现故障。旧断言失败及热路径缺项继续列账，本轮未放宽断言。

## Not fully qualified / 尚未充分覆盖

Long continuous use, unexpected power interruption during every storage phase, extreme memory pressure, different cards/keyboards/power arrangements, fresh-board installation and arbitrary cross-version downgrade remain incompletely qualified. Current software support uses the original external-power prototype; this release does not make a new claim about battery hardware. Existing USB save/export/reopen evidence and earlier Wi-Fi/QR software and device evidence are retained; the latest performance and free-writing sessions kept Wi-Fi off. See the [inherited Wi-Fi evidence and remaining gaps](WIFI_STATUS.md).

长期连续使用、存储各阶段断电、极限内存、不同卡/键盘/供电、新空板及任意降级尚未全面覆盖。软件范围沿用原外部供电原型，不推断电池硬件的新状态。USB保存导出重开及此前 Wi-Fi/二维码的软件、实机证据继续沿用；后续性能和自由写作会话保持 Wi-Fi 关闭。具体范围见[既有 Wi-Fi 证据与缺项](WIFI_STATUS.md)。

Report a short reproducible example, version and hardware details via [Issues](https://github.com/Ohhellboy/Draftor/issues). You do not need to upload a manuscript, full serial log or flash backup. Third-party provenance questions remain in [Resource Status](RESOURCE_STATUS.md).

反馈时提供简短复现、版本及硬件信息即可，不需上传私人稿、整段串口或 Flash备份。[资源来源未决事项](RESOURCE_STATUS.md)另列。
