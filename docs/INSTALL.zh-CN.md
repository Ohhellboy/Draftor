# 草稿机 Alpha 3 微雪安装指南

**简体中文** | [English](INSTALL.md)

本批适用于微雪 ESP32-S3-RLCD-4.2，ST7305 屏幕，16 MiB Flash，8 MiB OPI PSRAM，默认旋转 180°。原版 Osptek 和 Pico 2 W 的 Alpha 3 固件另批提供。[目标清单](../release/alpha3/WAVESHARE_RLCD42.json) · [验收范围](ALPHA3_VALIDATION.md)

## 准备设备和备份

下载并解压 `Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Firmware.zip`。其中 `Firmware/app.bin` 仅含 App，共 4,021,152 字节，SHA-256 为 `165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd`。首次安装使用同包的通用 bootloader、分区表与 OTA 初始数据。

先在原固件保存文稿及学习记录，正常结束传输，断电后取出 TF。将自己的文稿、学习目录和现有设备 Flash（包括设置和配对）的可恢复备份私下保存并核验。`--backup-confirmed` 表示这一步已完成。带电池时拔 USB 可能仍有供电。核对实物板型，断开其他开发板，按住 BOOT 接入 USB，约两秒后松开。

安装 Python 3.11+，在虚拟环境运行 `python -m pip install esptool==5.3.1`。在含有 `Firmware/` 的目录执行以下命令。将 `PORT` 设为本机实际串口，读出并核对本板 MAC，再把它设为 `MAC`。串口、MAC 与结果目录只在本机使用。

```sh
python -m esptool --chip esp32s3 --port "$PORT" --before no-reset --after no-reset --no-stub read-mac
python Firmware/install_firmware.py --firmware Firmware --board WAVESHARE_RLCD42 --port "$PORT" --expected-mac "$MAC" --mode update --output private-plan
```

## 兼容更新

上面的计划只读检查固件、芯片、16 MiB Flash、安全状态、板上实际分区表和 OTA 状态。安装器只接受兼容布局与能明确选择的活动 App；可能是 app0 或 app1。检查输出，再执行一次更新，结果写入另一个新目录。有原 App 准确文件时可加 `--current-app previous-app.bin` 进行额外比对。

```sh
python Firmware/install_firmware.py --firmware Firmware --board WAVESHARE_RLCD42 --port "$PORT" --expected-mac "$MAC" --mode update --output private-update --execute --backup-confirmed
```

更新只写活动 App 并独立校验，保留已有 NVS、键盘配对和 TF 内容。遇到分区、安全状态或校验异常就保留现场和私有日志。

## 全新安装

首次安装另用 `--mode fresh`。先执行只读计划。选择执行后将**擦除整片内部 Flash**，旧固件、内部设置及配对均会被清除。确认已备份，再主动加上擦除参数。更新失败时不要转入此流程。本版首装验证为离线夹具和通用构建文件核对；全新空板首装尚未实测。

```sh
python Firmware/install_firmware.py --firmware Firmware --board WAVESHARE_RLCD42 --port "$PORT" --expected-mac "$MAC" --mode fresh --output private-fresh-plan
python Firmware/install_firmware.py --firmware Firmware --board WAVESHARE_RLCD42 --port "$PORT" --expected-mac "$MAC" --mode fresh --output private-fresh --execute --backup-confirmed --allow-erase
```

## TF 资源与普通启动

1. 新卡从[指定的 Alpha 2.1 资源包](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip)将 `Resources/HANIME/` 下的内容放入卡根的 `HANIME/`。其中 `HANIME/IME/B_SHORT_TEXT_20261018/` 提供 14 个 format4 文件和安装标记，`HANIME/IME/R1_API080/matched28.hbf1` 是正文字体。已有卡可以沿用匹配的文件。
2. 从 `Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip` 将 `Resources/WAVESHARE_R7_L1_AREV1_RES_20261112/` 复制到卡根。目录内须同时有 `relations.bgr` 和 `relations.hnv`。按[TF 清单](../release/alpha3/TF_RESOURCES.json)核对路径、大小和 SHA-256。发现同名文件不符，请保留原件核对。无需格式化 TF。
3. 文稿位于卡根 `WAVESHARE_R7_L1_AREV1_DOC_TEST_20261112/HANIME_S3_TEST/v01/`，学习记录位于 `WAVESHARE_R7_L1_AREV1_TEST_20261112/`。升级前分别完整备份。保留已有合法标记；首次由固件创建文稿命名空间及学习目录。更新不会自动迁移旧文稿或学习，旧文稿可通过正常导出、导入流程转移。
4. 安装器显示 `VERIFIED_BOOT_PENDING` 后，断电、插回准备好的 TF、松开 BOOT 正常启动。兼容更新后可用已配键盘原信道普通唤醒。新设备按[发行说明](RELEASE_NOTES.zh-CN.md)配对第一把键盘。
