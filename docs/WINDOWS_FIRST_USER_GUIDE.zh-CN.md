[当前安装步骤与故障处理](https://draftor.tcssn.org/guide.html)

# Windows 首位用户操作说明

Windows 10/11 桌面 Chrome 或 Microsoft Edge 现已测试性开放。Windows 真实 USB 路径仍待现场验证（WINDOWS_DEVICE_FIELD_TEST_PENDING）。页面会检查实际能力和设备状态；不要以按钮可点击当成设备已通过。无需 Python、命令行、Codex 或浏览器扩展。

## 准备

保存文稿并结束 USB 传输。彻底断电（含电池）后取出 TF；另行保管 TF 文稿和学习目录。只连接这块微雪 ESP32-S3-RLCD-4.2 / ST7305。用可靠的 USB 数据线，按住 BOOT 接电脑，约两秒后释放，保持 ROM 下载模式。关闭串口监视器及其他安装标签。

## 找到正确设备

在 Windows“设备管理器”的“端口（COM 和 LPT）”查看插入该板后出现的 USB 串行设备；浏览器中名称可能是 USB Serial Device (COMx)、USB 串行设备 (COMx) 或 USB JTAG/serial debug unit。COM 数字可能变化，不是板子身份。只选择这块板对应的 USB 项，不猜选蓝牙或其他串口。网站仍核验 USB VID/PID、ESP32-S3 身份和完整安全状态。

没有出现设备时先检查数据线、USB 口、供电、BOOT 步骤，以及是否被其他程序占用。Windows 10 及以上的 USB Serial/JTAG 驱动一般可随联网自动安装；若有黄色警告，只按 Espressif 官方说明处理。不要随意把串行接口替换成 JTAG 的 WinUSB 驱动，也不需要安装整套开发工具。参考：[Espressif USB Serial/JTAG 官方说明](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_serial_jtag.html)。

Edge 没有串口 API 或被公司策略阻止时，可尝试最新版桌面 Chrome；受管理的电脑请联系管理员允许 Serial API。不要关闭浏览器安全设置或绕过组织策略。参考：[Microsoft Edge 串口策略](https://learn.microsoft.com/en-us/deployedge/microsoft-edge-policies/defaultserialguardsetting)。

## 安装与自己的恢复包

打开 https://draftor.tcssn.org/first-install.html，点“连接设备并安装”。阅读 Windows 测试提示，按实际条件勾选，再在浏览器串口选择框选对应 USB 设备。取消选择不会开始写入。

只读检查通过后，页面会自动给出准确厂家首装或 Alpha 3 app0 同版本重装计划。陌生固件、app1、空白板或不合格状态应停止；不要寻找强刷入口。

没有旧文件也能使用“读取完整 Flash（只读）”。等待全部读取结束，在电脑私人文件夹保存生成的 ZIP；等浏览器显示下载完成，再用“回选”选择同一个完整 ZIP，不要解压或改名混淆。只有页面显示与当前同板完整 Flash 一致才算通过。下载失败或中断不算保存；保留原文件，先解决下载问题。

再下载离线恢复 HTML，确认保存到相同私人目录；显示扩展名，确保是 .html 而非 .html.txt。将 ZIP 和 HTML 一起备存。工具下载失败时不要安装。恢复包包含设置、配对和可能的内部文稿，不能上传；TF 文稿不在此包内。

阅读计划、影响和覆盖范围后自主确认。首装会清除全部 16 MiB 内部 Flash；同版本重装只写已确认的活动 App。写入和校验时保持 USB、供电和页面。失败立即停止，保存脱敏回执，不自动再刷、改槽、擦除或复位。

## 真正离线的恢复

只有需要恢复时才操作。先关闭所有网站安装页。电脑断网后，在 Windows Chrome/Edge 按 Ctrl+O，打开已保存的 Draftor_Recovery_R11_Offline.html，再选择自己的完整私人 ZIP。无需启动本地服务或安装软件。页面会检查 FILE 安全上下文和 Web Serial；若提示能力缺失，尝试另一支持浏览器或联系支持，不要绕过检查。

确认 TF 已取出、原板 ROM 模式和实际覆盖影响，再授权一次完整恢复。它会回到备份时状态，覆盖全部内部 Flash；备份后的内部修改会丢失。若备份是厂家状态，恢复后是厂家演示，不是自动装 Draftor。失败时保留现场和回执，不连续恢复。

内容核验完成后先关闭连接；再彻底断电，插回已准备的 TF，松开 BOOT 普通启动。原 Draftor 备份使用键盘原通道回连，不重置配对；厂家首装按提示配对。检查显示、中英文输入、旧稿、新稿保存与重启读取。网页内容校验不能代替这些正常使用检查。

## 首位愿意配合的收件人反馈

依次反馈 Windows 版本和浏览器版本、USB/COM 显示、只读计划、恢复 ZIP 与 HTML 保存/回选、安装结果，以及 TF、键盘和写作结果。不要发送私人 ZIP、MAC 或文稿。真实离线恢复另由愿意配合者确认范围后验收，不为收集证据故意断电或破坏内容。真实故障保留回执，使用已保存工具或受控支持恢复。

TF 两份资源和目录说明沿用官网；下载资源不等于备份 TF。不要格式化或覆盖含个人文稿的卡。大陆下载链路尚未因此获得验收。
