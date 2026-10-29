# Alpha 2.2 微雪安装指南

**简体中文** | [English and complete command reference](INSTALL.md)

仅适用 **Waveshare ESP32-S3-RLCD-4.2 / 16 MiB Flash / 8 MiB OPI PSRAM**。先看[当前验收范围](ALPHA2_2_VALIDATION.md)。原 Osptek 板继续使用[Alpha 2.1 安装指南](INSTALL_ALPHA2_1.zh-CN.md)与既有发行；不能混刷，Pico 没有完整写作固件。

发行包名明确包含 `WAVESHARE_RLCD42`。以包内 `FIRMWARE_MANIFEST.json` 和仓库[目标清单](../release/alpha2.2/WAVESHARE_RLCD42.json)为准，不能使用旧原机 App 摘要。Python 3.11+ 虚拟环境安装 esptool **5.3.1**；使用 `--no-stub --before no-reset --after no-reset` ROM 路径。

1. 保存文稿，正常弹出并结束传输，完全断电取出 TF，断开其他板。按住 BOOT 接 USB，两秒后松开。带电池时拔 USB 不一定断电。
2. 明确实物板型与串口，按英文页的 `read-mac` 命令读取本板 MAC；`PORT` 与 `MAC` 必须由现场确定，不能猜第一个端口。MAC 只确认芯片身份，不证明接线。
3. 保留并检查已有完整 Flash/NVS/配对备份和文稿/TF 副本；无需重做已有有效备份或整卡哈希。没有可用备份不能仅靠设置确认选项继续。备份和安装日志应私有保存。
4. 先运行英文页的默认只读计划。每次结果目录必须新建。安装器校验四件套、板型标签、16 MiB Flash、安全状态、实际分区和启动槽；不接受加密/Secure Boot，不改 eFuse、不 force。
5. 兼容更新用 `--mode update --execute --backup-confirmed`，可用 `--current-app` 增加旧 App 独立核验。实际 app0/app1 决定写址，不假定 app0。只写 App 并独立 verify，不动分区、启动文件、NVS、配对或 TF。失败立即停止，不自动重试。
6. 首装另用 `--mode fresh`。只有备份确认且确实要清除整片内部 Flash，才加 `--allow-erase`；这会清除旧固件、设置与配对。首装写四件套并分别校验；不得将此路径当作更新失败后的自动补救。新空板首装尚未实测，只有受控夹具覆盖。
7. `VERIFIED_BOOT_PENDING` 后断电，插回准确资源的 TF，松开 BOOT 正常启动。不要把下载模式的按键动作带入正常启动。

TF 继续引用准确 [Alpha 2.1 Resources](../release/alpha2.2/INPUT_REFERENCES.json)，format4、字体和 pins 不变，已有配套卡无需重铺。新卡只铺发布的共享资源；不覆盖未知旧资源/marker、不格式化、不移动旧文稿。微雪文稿根是 `/sd/WAVESHARE_RLCD42/HANIME_S3_TEST/v01`。首次让固件创建不存在的命名空间；未知或不完整目录会拒绝，不能预建任意空目录、强行接管或回退原机目录。

更新后原配对保留。测试键盘裸 F9 是 Consumer，Fn+F9 才是标准菜单键，不改变 Fn-Lock。导出只读快照，导入只有 512 KiB RAM Inbox，电脑从不直接挂载 TF；每次识别实际新卷，执行正常安全 eject 后再退出。仅 sync/umount 不能代替正常 SCSI eject。完整按键和异常处理见[使用手册](USAGE.zh-CN.md)。
