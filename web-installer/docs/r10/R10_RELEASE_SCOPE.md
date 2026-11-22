# Draftor R10 限定公开范围 / Limited public release

固件固定为 v0.1.0-alpha.3 / WAVESHARE_RLCD42，App 4,021,152 bytes，SHA-256 `165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd`。

| 路径 / Path | 条件 / Conditions | 验收来源 / Evidence |
| --- | --- | --- |
| 首次安装 / First installation | 微雪 ST7305、16 MiB，准确 FactoryOnly_20260120 指纹及原允许可变区；清除全片后固定四份固件 / exact qualified factory image | R6 实机首装及启动；R10 从零捕获、保存回选及安装的软件集成 / inherited R6 hardware, R10 software integration |
| 重新安装 / Reinstallation | 准确 alpha.3 app0 同版本；仅活动 App / exact same version, active App only | R4 同版本写回实机；R10 持久包门槛 / inherited R4 hardware, R10 package integration |
| 离线恢复 Draftor / Offline Draftor recovery | R9 v1 完整包、原设备、16 MiB、安全状态合格 / valid package, original board, safe state | R9 普通用户断网重开、整片恢复、独立 Python 只读核验与正常使用；R10 复用写回核心 / inherited R9 hardware and user evidence |
| 离线恢复厂家 / Offline factory recovery | 准确厂家原状态完整包及原设备 / qualified factory backup and original board | 软件分支覆盖，共用已验收写回通道；未新增厂家恢复实机试验 / software coverage, shared qualified writer, not new factory hardware evidence |
| 在线备份与核验 / Online backup and verify | 同板检查、完整捕获或导入；在线整片恢复锁定 / no online full restore | R9 HTTPS 导入实机；从零完整捕获为 R10 合成协议测试 / inherited real import, synthetic full capture |

设备操作首批只支持 macOS 桌面 Google Chrome。实测基线：macOS 15.5 / arm64、Chrome 157.0.8103.50；不宣称每个版本组合都已实测。未知厂家或尾部、全空白、app1、未知固件、其他板型、安全未知/加密/安全启动、容量不符均拒绝。无强刷、改槽或任意镜像入口。

Device operations are limited to macOS desktop Google Chrome. Tested baseline: macOS 15.5 / arm64 and Chrome 157.0.8103.50; not every version combination is qualified. Unknown factory images/tails, blank boards, app1, unknown firmware, other PCBs, insecure/unknown security and incorrect capacity are refused. No force or arbitrary image/address option.

Windows、Edge、手机 USB、其他硬件、UART0 学习索引状态、新 TF 卡及大陆下载分别未验收。所有平台可以阅读和下载；不因未验收项扩大写入支持。TF 两包及其目录/摘要沿用既有结果。网页不操作 TF。完整 Flash 不含 TF 文稿，必须另行保管。

Windows, Edge, mobile USB, other hardware, UART0 LearningIndex state, a new TF card and mainland download performance remain unqualified. Reading and downloads are available separately from writing support. TF packages, paths and hashes are unchanged. The website never accesses TF, and full Flash backups do not include TF writing.

本轮没有连接或擦写用户设备，没有新增正常启动实测。R10 的新证据属于软件、浏览器合成端口及实际 HTTPS 发布检查；R4/R6/R9 实机证据按适用范围继承。

R10 does not connect or write to the user's physical device. New R10 evidence covers software, synthetic browser ports and actual HTTPS deployment; physical results are inherited within their qualified scope.
