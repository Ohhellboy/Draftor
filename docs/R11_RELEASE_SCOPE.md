# Draftor R11 Windows 优先公开测试 / Windows-first public beta

Windows 10/11 桌面 Chrome、Microsoft Edge 可以进入安装、备份和离线恢复。当前状态：WINDOWS_DEVICE_FIELD_TEST_PENDING / WINDOWS_DEVICE_PENDING。这表示 Windows 真实 USB 首装与离线恢复仍待收件人验证，不是代码锁写。macOS Chrome 继承 R4/R6/R9 的已验收能力。

实际入口按安全上下文、桌面环境、原生 Web Serial 与用户授权检查；操作系统和浏览器品牌不授予也不否决 Flash 权限。Edge 因策略缺失 Serial API 时请尝试 Chrome 或联系管理员，不绕过组织策略。手机和平板仅提供阅读和下载。

支持的设备仍只有实物确认的微雪 ESP32-S3-RLCD-4.2 / ST7305、16 MiB。正常安装只允许准确 FactoryOnly_20260120 指纹首装，或准确 v0.1.0-alpha.3 app0 同版本重装。未知厂家/尾部、未知固件、全空白、app1、其他板型、容量或安全状态异常拒绝，不自动改槽或强刷。

安装前须完整保存并回选私人恢复 ZIP，确认当前同板完整 Flash 一致，并保存准确离线 HTML。离线恢复只接受有效 R9 v1 包、原设备、受支持备份原状态及安全检查，允许当前内容因故障与备份不同；该豁免不适用于正常安装。在线恢复页只读。TF 不在 Flash 包内，必须单独保管，网页不操作 TF。

固件和资源未变。App 为 4,021,152 bytes，SHA-256 165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd。原 USB 303a:1001、芯片、安全、分区、OTA、厂商指纹、ROM MD5、一次互斥授权、保护区、写后校验及失败停止要求均保留；无 stub、自动复位或失败自动重试。

证据分开记录：R4 app0 重写、R6 准确厂家首装、R9 Draftor 原板完整恢复来自 macOS 实机。R10/R11 从零捕获及厂家恢复分支为独立合成协议覆盖；厂家恢复复用已验收 writer，不宣称厂家恢复新实机 PASS。R11 Windows runner 的真实浏览器进程使用合成端口，不是 USB 实机；Windows 10/11 具体硬件组合继续待现场验证。没有访问用户 Mac 上的微雪。

Windows Chrome and Edge are open for informed beta use. WINDOWS_DEVICE_FIELD_TEST_PENDING means physical USB first-install and offline recovery are still pending, not disabled by an OS gate. macOS Chrome retains inherited qualification. A secure desktop context, Web Serial and user permission are required. Edge policy restrictions are handled as capability/permission issues; use Chrome or contact the administrator. Do not bypass policy.

The device scope remains exact Waveshare ST7305 / 16 MiB, qualified FactoryOnly_20260120 first-install, and exact Alpha 3 app0 same-version reinstall. Unknown/blank/app1/security/capacity failures are refused. Installation requires an actually saved and reselected complete private ZIP matching the current board contents plus the saved offline tool. Offline restore is original-board only; online recovery stays read-only. TF is separate.

Windows hosted-runner browser tests are real Windows software tests with synthetic serial devices, not physical USB evidence. Windows 10/11 field results, other hardware, mobile USB, a new TF card and mainland download performance remain separate pending items. Current source, Windows guide and compatibility matrix accompany this release.
