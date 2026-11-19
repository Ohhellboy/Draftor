# R4 / R5 本地构建与范围

本轮继续使用 feature/web-installer-r1，保留 R3 的 ROM 状态字节与空 SLIP 处理。R5 只修改静态页面、文案、样式和说明，不改变串口、计划或写入代码。

## 默认站点

Node.js 22+，依赖使用已有 package-lock.json。新环境先运行 `npm ci`，再运行 `npm run build`。`node tools/serve.mjs dist` 仅监听 localhost，默认 4173，可用 DRAFTOR_PORT 更改端口。直接解压默认站点 ZIP 时也可使用 `python3 -m http.server 4173 --bind 127.0.0.1`。

默认 BUILD.json 的 hardwareUpdateWrites 和 hardwareFreshWrites 均为 false。TF 下载使用普通固定链接，不先 fetch ZIP 到内存，不要求 Web Serial；首页与完整帮助均保留中英文。准确大小、SHA-256 和路径见 TF_RESOURCES_GUIDE.md。下载依赖外部 GitHub Release 的网络可用性，烧录运行资源仍同源自包含。

## 仅本地设备验收

本轮 Brief 已授权一次原微雪 app0 的兼容更新。`npm run build:device-test` 生成独立 dist-device-test，hardwareUpdateWrites=true、hardwareFreshWrites=false。用 `node tools/serve.mjs dist-device-test` 运行。该目录只用于已授权的本地设备验收，不进入默认站点 ZIP，不公开发布。

R4 固定实际源码及两个构建摘要后再连接设备。保留已有备份、同板身份与当次元数据复核、旧 App 比对、前后保护区 MD5 及独立 esptool 5.3.1 verify-flash。保护摘要覆盖活动槽外 [0,0x10000) 与 [0x410000,0x1000000)，并不逐字节验证整片 Flash。唯一计划写入 alpha.3 App 的 [0x10000,0x3e5ba0)，1024 B 传输填充结束 0x3e5c00，4096 B 扇区结束 0x3e6000。

不加载 stub、不全片擦除、不写其他分区、不发送 FLASH_END 或自动复位。覆写活动 App 不是原子 OTA；异常后停止且不自动重刷。写后内容通过仍需人工普通启动、旧稿抽查和新稿保存／重启读取。

## 交付与证据

新交付位于 delivery/R4_R5_20261119，不覆盖历史包。默认站点含准确固件、同源依赖、许可及对应源码；源码包保留准确 Firmware / LearningIndex 输入、构建脚本、锁文件和历史相关测试。60.5 MB 基础资源原包可按固定链接重新取得，大小与摘要见资源说明，不重复嵌入源码包。

evidence/R4_R5_20261119 仅放脱敏验证记录。真实 Flash、MAC、配对、个人文稿、私人摘要和原始串口记录只留本机私有证据目录，不打包或提交。默认包与专用构建分别登记 BUILD.json 和 bundle SHA-256，实际写入结果与人工反馈单独列在交付 REPORT.md 中。
