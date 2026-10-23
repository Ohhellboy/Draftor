# Existing Wi-Fi evidence / 既有 Wi-Fi 验证范围

Alpha 2 inherits the local Wi-Fi transfer implementation and its earlier software and device checks. This release preparation does not create a new Wi-Fi acceptance task or repeat hotspot, QR, browser upload/download or coexistence tests. The separate brief check for the release App covers startup, Chinese input, selection, saving and reopening only.

Alpha 2 直接继承本地 Wi-Fi 传输实现及此前软件、实机验证。本次发行收尾不新建 Wi-Fi 验收任务，不重跑热点、扫码、网页导入导出或共存测试；发行 App 的独立短检查只覆盖启动、中文输入、选词、保存和重开。

## Retained results / 沿用结果

- The Wi-Fi integration/STARTUP2 software checks covered document snapshots, creation of a new document without selecting or overwriting CURRENT, UTF-8/size rejection, transaction receipts, failure cleanup and USB exclusion. The STARTUP2 regression ran 13 cases in both normal and ASan/UBSan builds; earlier unchanged shared-contract results are retained separately.
- The retained STARTUP2 device report records 37 completed hotspot start/stop cycles with no recorded startup failure. The author confirmed phone connection, the local web page, downloading and opening text, one successful new-document import, editing with the hotspot active, and the USB exclusion prompt. Those cycles were not all identical transfer sessions.
- QR1 software checks covered the common QR library, the two connection/address pages, session ownership and actual display buffers. Product checks passed 21 cases in both normal and sanitizer builds; 10 QR images from the display buffers were independently decoded. These software results do not themselves establish every phone camera's scanning behavior.

- Wi-Fi 集成/STARTUP2 软件验证覆盖正文快照、新建而不选择或覆盖 CURRENT、UTF-8/大小拒绝、事务回执、失败清理及 USB 互斥；STARTUP2 普通版与 ASan/UBSan 各13项回归通过，未改共享合同的旧结果单独继承。
- 既有 STARTUP2 实机报告记录37次完整热点启停、未记录启动失败；作者确认手机连接、本地网页、下载并打开正文、一次成功新稿导入、热点开启时编辑及 USB 互斥提示。37轮不等于每轮都完成相同传输动作。
- QR1 软件验证覆盖通用二维码库、连接/网址两页、会话归属及实际屏幕缓冲；产品普通版与 sanitizer 各21项通过，屏幕缓冲生成的10张二维码独立解码通过。这些软件结果本身不能代替所有手机摄像头的实际扫码表现。

## Specific limits retained / 保留的具体缺项

The retained device report did not fully establish the before/after CURRENT identity, precise snapshot boundaries during concurrent editing, interrupted-upload receipt recovery, every input-method/layout combination, all boundary-size or malformed uploads on hardware, or general client compatibility. A Pi-side connection attempt failed while the phone worked; its cause was not established. The QR1 software report did not record a phone-scanning device result. These gaps remain recorded; they are not new test requirements for this release-preparation round.

既有实机报告未完整确认导入前后 CURRENT 身份、并发编辑下的精确快照边界、中断上传的回执恢复、每个输入法/布局组合、设备上的全部大小边界和非法上传、广泛客户端兼容性。Pi 辅助连接曾失败而手机可用，根因未定；QR1 软件报告未记录手机扫码实机结果。以上缺项继续记录，不自动转成本轮新增测试要求。

The subsequent Alpha/cumulative performance comparison and FW1 writing session kept Wi-Fi off. They do not replace the earlier Wi-Fi evidence. The exact observer-disabled release App has completed its separate brief startup, Chinese input, selection, save and reopen check with Wi-Fi kept off. See [Usage](USAGE.md) for current keys and [Known Issues](KNOWN_ISSUES.md) for other limitations.

之后的 Alpha/累计性能对照与 FW1 写作保持 Wi-Fi 关闭，不代替此前无线证据。关闭输入观察的准确发行 App 已完成启动、中文输入、选词、保存和重开的单独短检查，全程保持 Wi-Fi 关闭。操作键位见[使用说明](USAGE.zh-CN.md)，其他限制见[已知问题](KNOWN_ISSUES.md)。
