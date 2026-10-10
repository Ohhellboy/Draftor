# Firmware Recovery & Rollback / 固件恢复与版本回退

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Installation / 安装指南](INSTALL.md) · [Known Issues / 已知问题](KNOWN_ISSUES.md) · [Download Alpha / 下载](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)

## 1. Recovery and Rollback / 恢复与回退

Reinstalling firmware and rolling back to an earlier version are two different operations.

重新安装当前固件与回退到旧版本，是两种不同的操作。

- **Reinstallation / 重新安装：** Flash the current Alpha firmware again, using the matching hardware, partition layout and resources.

  重新写入当前 Alpha 固件，并确保硬件、分区布局及运行资源匹配。

- **Rollback / 版本回退：** Replace the current application with an earlier, compatible firmware version.

  将当前应用固件替换为经过确认、且与现有硬件及资源兼容的旧版本。

Neither operation automatically restores lost documents or repairs damaged storage.

这两种操作都不会自动恢复已经丢失的文档，也不能保证修复损坏的存储数据。

## 2. Available Firmware / 当前可用固件

The public **Draftor Alpha v0.1.0-alpha.1** release provides the ESP32-S3 firmware tested with a **20 MHz TF card SPI runtime clock**. TF card initialization uses **400 kHz**.

公开发行的 **Draftor Alpha v0.1.0-alpha.1** 提供经过实机测试的 ESP32-S3 固件。TF 卡 SPI 运行频率为 **20 MHz**，初始化频率为 **400 kHz**。

The tested application is identified as:

实测应用固件的文件名为：

`Draftor_Alpha_T06_TF20_R1.app.bin`

It is included in `Draftor_Alpha_Firmware.zip`, together with the corresponding first-installation files.

该文件包含在 `Draftor_Alpha_Firmware.zip` 中，同时提供首次烧录所需的配套文件。

An earlier **10 MHz** firmware version was retained during development, but its original application binary is **not provided as a separate downloadable rollback in this Alpha release**.

开发过程中曾保留一个 **10 MHz** 的旧版固件，但本次 Alpha 发行**没有单独提供可下载的旧版回退固件**。

Therefore, this release does not provide a ready-to-use public downgrade procedure.

因此，目前没有可供普通用户直接执行的公开降级流程。

## 3. Before Changing Firmware / 更换固件前

Before reinstalling or changing firmware:

重新安装或更换固件之前：

1. Save your current work and export important documents to your computer.

   保存当前文稿，并将重要文档导出到电脑备份。

2. Confirm the exact ESP32-S3 board, installed partition layout and firmware version.

   核对 ESP32-S3 开发板型号、现有分区布局和固件版本。

3. Confirm that the TF card resources match the firmware you intend to install.

   确认 TF 卡中的词库、字体及其他资源与目标固件版本匹配。

4. Follow the appropriate procedure in the [Installation Guide](INSTALL.md).

   按照[安装指南](INSTALL.md)中适用的步骤操作。

**Do not erase the entire flash, replace the partition table or format the TF card simply to attempt a rollback.** An application-only update is appropriate only after confirming partition and resource compatibility.

**不要仅仅为了尝试回退，就直接全片擦除 Flash、替换分区表或格式化 TF 卡。** 只有确认分区及资源兼容时，才可以采用单独更新应用固件的方法。

A newly compiled binary should not be assumed to be identical to a previously tested release. Do not substitute an arbitrary rebuild or mix dictionaries and fonts from different versions.

自行编译的新固件不能直接视为与此前实测版本相同。不要用未经确认的重新编译产物代替旧版固件，也不要混用不同版本的词库和字体。

## 4. If Something Goes Wrong / 如果设备出现问题

If Draftor fails to start, cannot load its resources, or behaves unexpectedly after a firmware change, first check the power supply, wiring, firmware version and TF card resources.

如果更换固件后无法启动、资源无法加载或出现异常行为，请先检查供电、接线、固件版本及 TF 卡资源。

See [Installation](INSTALL.md) for setup and flashing problems, and [Known Issues](KNOWN_ISSUES.md) for existing limitations.

安装及烧录问题请参阅[安装指南](INSTALL.md)；当前版本的已知限制请参阅[已知问题](KNOWN_ISSUES.md)。

If you need help with a reproducible problem, you can open an issue on [GitHub](https://github.com/Ohhellboy/Draftor/issues). Include the firmware version, hardware configuration and steps leading to the problem, but do not publish private documents or sensitive device logs.

如果问题可以复现，欢迎通过 [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues) 反馈。建议说明固件版本、硬件配置和复现步骤，但不要公开私人文稿或含敏感信息的设备日志。
