# Input experience and measurements / 输入体验与测量

The cumulative update shortens complete waits, not only lookup time. These are retained observations on the ESP32-S3 N16R8 prototype, original ST7306 wiring/font, TF runtime 20 MHz, horizontal layout, original BLE keyboard and Wi-Fi never enabled during the paired comparison or FW1. The old arm is Alpha 1 with minimal passive timing adaptation; the new arm is the observed cumulative baseline. The release package is a new build of that business source with input observation disabled; it has no new device-performance claim.

累计收益针对完整等待，而非仅查词段。条件为同一 S3 N16R8、原 ST7306 接线/字体、TF20、H横排、原 BLE 键盘；对照和 FW1 全程 Wi-Fi 不启用。旧侧是 Alpha 1 最小测量派生版，新侧为带观察的累计基线。本包发行构建关闭输入观察，没有新的实机性能主张。

| Paired sample / 同轮配对样例 | Alpha measurement, ms | Cumulative, ms |
|---|---:|---:|
| XH first `wo`: complete candidates | 317.282 | 114.572 |
| PY first `shi`: complete candidates | 333.732 | 122.795 |
| XH first `wo`: final-key raw feedback | 55.307 | 9.625 |
| XH `wjdjle`: Space to accepted body display | 72.064 | 36.377 |
| First naturally typed XH long sequence: catch-up after last received key | 412.752 | 113.992 |

Times start at received input, not at physical key travel. Feedback, complete-candidate and accepted-body endpoints finish when the corresponding synchronous software LCD transfer returns, not when queued and not a photometric measurement of pixels. The displayed frame must belong to the corresponding input/body version. These manual sequences are finite examples, not a population benchmark or words-per-minute claim. Corrected typing and unavailable samples are not silently synthesized.

计时从设备收到输入开始，不含物理按键行程；三个端点分别对应原码、完整候选及已接受正文的同步画面传输返回，不能把入队当完成，也不是光学像素测量。帧必须属于对应输入/正文版本。手工序列是有限样例，不代表所有词或每分钟字数；误打修正及缺失样本没有补造。

## Separate free-writing session / 独立自由写作会话

| Endpoint / 端点 | Valid samples | Median ms | P95 ms |
|---|---:|---:|---:|
| Raw feedback / 原码反馈 | 3,811 | 17.762 | 29.674 |
| Complete candidates / 完整候选 | 2,397 | 84.442 | 142.029 |
| Selected body display / 选词后正文 | 874 | 46.5415 | 60.652 |
| Navigation display / 导航显示 | 79 | 83.872 | 186.100 |

This was one cumulative-version session, not another Alpha comparison. The actual exported body after save and after leaving/reopening the draft was identical: 3,977 UTF-8 bytes, 1,425 Unicode scalar values. The draft, identifiers and raw input logs are private and are not distributed. The observation span included thinking and pauses; it is not typing duration. Damaged or lost observation chains were excluded under the retained parser rules, not counted as successful zero-latency events.

这是累计版单臂会话，与前表对照分开。保存后及真正离开再重开后导出的实际正文均为3,977字节、1,425个Unicode标量值，逐字节相同。稿件、身份及原始输入日志不公开。观察跨度包含思考停顿，不当作打字时长；损坏/丢失观察链按既有解析规则排除，不补成零等待样本。

## Remaining costs / 仍保留的代价

Compared with the Alpha measurement arm, XH/PY activation was +1.373/+19.134 ms and the warm `wo` query-to-range substage was +6.193/+10.246 ms in the recorded cases. Two earlier hot-path examples (third `wo` and the long-z sequence's `zz` processing) remain on the ledger; missing observations did not close them. Occasional 55–64 ms waits, full synchronous redraw blocking and general/named-output costs remain relevant. No 64 ms optimization, new cache, wider inline text, altered service window or memory policy is part of Alpha 2 packaging.

相对 Alpha 测量臂，记录中的 XH/PY激活增加1.373/19.134 ms，热 `wo` 的查询到范围子段增加6.193/10.246 ms。早期第三次 `wo` 与长z串中 `zz` 处理两个慢例继续列账，缺少观察没有被写成关闭。55–64 ms等待、同步整屏阻塞及通用/命名输出成本仍在；本次封包未加入新优化、缓存或预算/内存政策变化。

The measured predecessor App is 2,441,680 bytes; its target static memory is 145,060 bytes. The new quiet-release build is listed separately in the distribution manifest. Against the measured Alpha arm, the whole integrated application adds 367,392 App bytes and 7,016 static bytes. Runtime dictionary files total 132,153,747 bytes, +5,213,496 over Alpha's 14 files. The synchronous display shadow uses 30,000 PSRAM bytes when available and retains full redraw fallback. These whole-version costs include platform/features and observation differences; they are not isolated costs of four algorithms. Five retained assertion signatures (ten paired failures) are historical open test evidence, not newly passing claims.

App与静态内存、TF文件和动态显示影子分别列账如上。整版差额包含平台/功能及观察差异，不能全部归因于四项优化。原有五类断言、两臂共十次失败继续作为历史未决证据，不改写成全部通过。
