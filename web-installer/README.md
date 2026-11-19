# Draftor Web Installer R1

本地隔离的软件实现。沿用附件 v0.1 的素纸朱印单页、内容层级、黑体标题回退和单一「连接设备并安装」入口。准确固件为 `v0.1.0-alpha.3`，只适用微雪 ESP32-S3-RLCD-4.2 / ST7305。

历史软件结论为 **SOFTWARE_READY_FOR_DEVICE_TEST**；R3 已完成同一微雪设备的 **READ_ONLY_DEVICE_PASS**。R4 的真实更新／普通启动结果以独立交付报告为准，不能从只读结果推断。默认构建继续锁写，真实 fresh 未验收。

R4／R5 增加独立的中英文 TF 下载入口，使用两份固定 GitHub Release 原包；不需要连接设备。见 [资源说明](docs/TF_RESOURCES_GUIDE.md) 和 [本轮构建／运行说明](docs/R4_R5_RUN.md)。

## 直接运行完整站点

解压交付的 `Draftor_Web_Installer_R1_site.zip`，在解压目录执行：

```sh
python3 -m http.server 4173 --bind 127.0.0.1
```

打开 `http://localhost:4173/`。不要直接使用 `file://`；Web Serial 和 Web Crypto 需要安全上下文。也可将站点目录的所有文件整体复制到任意 HTTPS 静态托管。本轮没有部署。

- 默认 `index.html`：真实设备只读检查与计划可用，**真实写入在构建时锁定**。不会自动连接任何串口。
- `simulation.html?scenario=app0` 或 `app1`：完整模拟更新。
- `simulation.html?scenario=fresh`：模拟证明整片空白、单独备份/擦除确认、四文件写入和独立校验。
- 负例：`partition`、`pending`、`unknown`、`flash8`、`security`、`disconnect`、`verify`。
- 每个模拟页明确标识合成 Flash，模拟器不调用 Web Serial。

运行时的 JS、CSS、4 份二进制、manifest、许可证和对应源码均自托管。没有 GitHub API、CDN、数据库、遥测或账号依赖。TF 资源下载是用户单独准备步骤，不是安装器运行条件。

## 从源码复现

Node.js 22+；Python 3.11+；浏览器测试默认使用已经安装的 Chrome。依赖由 `package-lock.json` 锁定，包括 esptool-js 0.7.1、spark-md5 3.0.2、esbuild 0.25.12、Playwright 1.56.1。

```sh
npm ci
npm run verify:release
npm test
npm run build
npm run test:browser
npm run preview
```

`npm run preview` 仅监听本机 `127.0.0.1:4173`。需要换端口时设置 `DRAFTOR_PORT`。自动浏览器测试在页面代码执行前移除真实 `navigator.serial`，仅测试模拟器或明确的取消选择夹具。没有测试会请求真实 USB。

如没有 Chrome，可安装 Playwright 的 Chromium，再设 `DRAFTOR_BROWSER=chromium`。Python 对照测试可用 `DRAFTOR_PYTHON=/path/to/python3` 指定解释器。

交付中保留了精确的原网页 ZIP、Brief、Firmware ZIP、LearningIndex ZIP、发行清单和发行 Python 安装脚本。可选重核历史 TF 原包（60,546,074 字节，未重复装入源码 ZIP）：

```sh
node tools/fetch-optional-inputs.mjs
python3 -B tools/verify-tf.py
```

该脚本只下载命名的公开固定版本，验证大小/SHA-256；不读取磁盘、串口或 TF。已经完成的 18 文件核验见 `evidence/tf-resources.json`。

## 实机阶段边界

仅在后续获得明确授权并完成 [NEXT_DEVICE_TEST.md](NEXT_DEVICE_TEST.md) 的前置条件后，才创建专用测试构建：

```sh
npm run build:device-test
node tools/serve.mjs dist-device-test
```

这只解锁兼容更新；真实 fresh 在适配器和状态机中都保持禁止。开关不是 URL 参数，不存在网页「强刷」或选择未知槽位的入口。默认站点发布包不含已解锁构建。

MAC 由使用者根据自己的私人备份/既往核验记录输入；网页与同一选定串口读到的 MAC 比较。实物微雪确认不能被 USB VID/PID 或 ESP32-S3 芯片族替代。R1 限定 `303a:1001` USB Serial/JTAG，其他串口路径停止。

写入不是原子 OTA 升级：按发行安装器合同覆盖**活动 App**，中断可能无法启动。因此可恢复备份是执行前硬门槛。通过后也只报告 `VERIFIED_BOOT_PENDING`，不会复位或声称已经可以写作。

## 审阅入口

- [REPORT.md](REPORT.md)：结论、证据、准确偏移和限制。
- [docs/INSTALL_PLAN.md](docs/INSTALL_PLAN.md)：全部分流和允许/禁止操作。
- [docs/CAPABILITY_AUDIT.md](docs/CAPABILITY_AUDIT.md)：ESP Web Tools、esptool-js、Python esptool 精确源码审计。
- `evidence/unit-tests-final.tap`、`evidence/browser-tests.json`：自动回归原始结果。
- `evidence/rom-app0.json`、`rom-app1.json`：实际 ROM 协议编码通过合成 Web Serial 流的证据。
- `evidence/python-oracle.json`：53 组 OTA 输入与发行 Python 规则逐项一致。
- `evidence/plans/`：app0、app1、fresh 和中断/校验失败的计划及回执。
- `public/docs/TF_GUIDE.html`：本地中英 TF 准备帮助及 18 文件校验清单。

新增 ROM 适配代码基于 GPL-2.0-or-later esptool 的协议流程；组合网页脚本按 GPL-3.0 提供对应源码。原有固件、依赖及素材保留各自许可，见 [docs/WEB_LICENSE.md](docs/WEB_LICENSE.md)。
