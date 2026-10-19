# Known issues, limits and privacy / 已知问题、限制与隐私

Alpha 2 is usable for writing and experimentation, with a finite paired comparison and one natural writing/save/reopen check. Keep independent exports of important manuscripts. Current limitations, historical observations and untested conditions are separated below.

Alpha 2 已有有限对照及一次自然写作、保存重开核验。重要文稿仍请独立导出备份；以下区分当前限制、历史现象与未覆盖条件。

## Current limits / 当前限制

- **2 MiB slots cannot accept this App.** Old-layout migration is pending; use [Installation](INSTALL.md), not the old App-only command. / 旧2 MiB槽装不下本版，迁移待确认。
- Resources must be the exact format4 set; format2 files/Ready markers do not make a mixed directory compatible. / 不混用旧新资源或伪造 Ready。
- Some activation/warm-query costs and synchronous full-screen blocking remain; see [Performance](PERFORMANCE.md). / 激活、热查询子段及同步整屏仍有代价。
- No learned frequency/personal dictionary, abbreviated or mixed Pinyin, Chinese Find entry, complete UI localization, selectable new fonts or complete Pico writing firmware is added. / 未新增学习、简拼混拼、中文查找、完整汉化、字体切换或 Pico 整机固件。
- Experimental F10 local Wi-Fi transfer and QR pages exist in the fixed image; Wi-Fi starts off, Esc returns to writing without turning it off, and F10 then O turns it off. The performance and FW1 evidence covers Wi-Fi off. / F10实验热点/二维码已在固件；Esc不关热点，F10后O关闭，本轮性能证据覆盖关闭状态。

## Serial diagnostics / 串口诊断

The exact tested build has `DRAFTOR_AUTH_OBSERVE=1`, telemetry profile 0 and capture 1. `src/service_endpoints.cpp::snapshotServiceEndpoint` copies up to 63 composition bytes into the existing RAM ring; `formatServiceEndpoint` emits them as `raw=%s`. `ui_trace.cpp` selects these records and `platform/device_main.cpp` drains them through the idle UART logger automatically. This happens despite the generic boot trace line saying `raw_text=0`: that line does not describe the AUTH endpoint overlay. No interactive enable command is required, and the ordinary diagnostic command parser returns immediately in this capture configuration.

准确镜像默认启用 AUTH 观察：组成码最多63字节进入原内存环，再自动由串口输出。通用启动行的 `raw_text=0` 不涵盖这条 AUTH 路径，不能据此声称没有原码记录；本配置也不靠用户手动开启。

This observation path contains no persistent input-log file writer. It does not send an observation stream to a cloud service. A serial-connected computer can record input codes and timing/context information, so keep that connection private and inspect logs before sharing. This statement does not imply that saved manuscripts or ordinary device state disappear on reset. The packaged image preserves this behavior; a quieter firmware would be a separately built and separately validated version.

此观察路径没有持久输入日志文件写入，也没有把观察流发送到云端；接入串口的电脑可以保存组成码及时间/上下文，分享前要清理私人信息。这不表示已保存文稿或普通设备状态会随复位消失。本包保留实测行为，静默版本需要单独构建与验证。

## Historical observations still open / 历史未彻底解释的现象

An intermittent first `n` in a longer Pinyin input was reported in earlier sessions; the latest finite session does not establish a general fix. Earlier display splitting/distortion under a different, unadopted coordination candidate is not a confirmed current format4/synchronous-delta fault. Both remain historical observations, not claims that every current input fails. Retained candidate/session assertion failures and missing hot-path observations are listed in the technical performance notes; packaging did not weaken those tests.

此前曾报告较长拼音中首次 `n` 未及时出现，后续有限会话不能作为普遍修复结论。旧未采纳协同候选上的分半/扭曲也不等于当前版已复现故障。旧断言失败及热路径缺项继续列账，本轮未放宽断言。

## Not fully qualified / 尚未充分覆盖

Long continuous use, unexpected power interruption during every storage phase, extreme memory pressure, different cards/keyboards/power arrangements, fresh-board installation and arbitrary cross-version downgrade remain incompletely qualified. Current software support uses the original external-power prototype; this release does not make a new claim about battery hardware. USB save/export/reopen evidence is retained, but Wi-Fi/QR was not requalified by the latest performance or free-writing sessions.

长期连续使用、存储各阶段断电、极限内存、不同卡/键盘/供电、新空板及任意降级尚未全面覆盖。软件范围沿用原外部供电原型，不推断电池硬件的新状态。USB保存导出重开有既有证据，本轮性能及自由写作没有重新验收 Wi-Fi/二维码。

Report a short reproducible example, version and hardware details via [Issues](https://github.com/Ohhellboy/Draftor/issues). You do not need to upload a manuscript, full serial log or flash backup. Third-party provenance questions remain in [Resource Status](RESOURCE_STATUS.md).

反馈时提供简短复现、版本及硬件信息即可，不需上传私人稿、整段串口或 Flash备份。[资源来源未决事项](RESOURCE_STATUS.md)另列。
