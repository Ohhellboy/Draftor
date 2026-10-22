# Draftor Alpha 2: Faster Chinese Input and Wi-Fi Document Transfer

The main update in Alpha 2 is a noticeable improvement in Chinese input speed.

Candidates appear sooner after you type Pinyin, and selected words reach the document faster. These changes improve individual input responses and reduce some of the waiting when paging through candidates or typing continuously.

Alpha 2 also adds **local Wi-Fi document transfer**. You can connect to Draftor with your phone's browser to import and export documents. The phone connects directly to the ESP32-S3's Wi-Fi module, without relying on a nearby Wi-Fi network or any cloud text service.

## What changed

This round of performance work has four main parts:

1. **Block-based lookup:** reorganizes the input method's code index to reduce the data read while finding candidates.
2. **Less repeated lookup:** text locations that have already been found and verified can be passed directly to the text reader.
3. **Short text in the same block:** eligible complete short texts are read together with their shared entries, reducing additional reads and verification.
4. **Differential display updates:** the screen updates only the parts that have changed where possible, instead of sending the entire frame every time.

Together with other small adjustments, these changes reduce the wait between pressing a key and seeing the result.

This version continues to support Full Pinyin, Xiaohe Shuangpin, Wubi 86 and English input, with both horizontal and vertical writing.

## Input speed improvements

Here are several corresponding results from the Alpha 1 and cumulative Alpha 2 measurement builds, tested on the same ESP32-S3 prototype.

| Action | Alpha 1 | Alpha 2 |
| --- | --- | --- |
| First Xiaohe `wo`: complete candidates | 317.3 ms | **114.6 ms** |
| First Full Pinyin `shi`: complete candidates | 333.7 ms | **122.8 ms** |
| First Xiaohe `wo`: typed-code feedback | 55.3 ms | **9.6 ms** |
| Xiaohe phrase selected with Space: document display | 72.1 ms | **36.4 ms** |

The first two complete-candidate waits were reduced by about 64% and 63%.

Timing starts when the ESP32-S3 receives the relevant keyboard event and ends when the software finishes sending the corresponding typed-code, complete-candidate or document image to the display.

We also used the cumulative Alpha 2 build for a natural writing test of about 1,400 characters. Among valid samples, the median complete-candidate wait was **84.4 ms**, typed-code feedback took **17.8 ms**, and document display after selection took **46.5 ms**.

See [docs/PERFORMANCE.md](PERFORMANCE.md) for the full results and measurement method.

## Which files to download

To install on an ESP32-S3 N16R8 (Thick), download **Firmware** and **Resources** from the same version.

If you want to build the firmware yourself, also download **Source** and **BuildInputs**, and follow [docs/BUILD.md](BUILD.md).

| File | Purpose |
| --- | --- |
| `Firmware.zip` | Firmware, boot files and partition files |
| `Resources.zip` | Matching dictionaries, indexes and font |
| `Source.zip` | Draftor, bgIME and related project-owned source |
| `BuildInputs.zip` | Generated files and dependency materials needed for the fixed build |

GitHub's automatically generated Source code archives are not complete installation packages.

## Upgrading from an earlier version

**A direct upgrade from Alpha 1 to Alpha 2 is not recommended.**

The new version changes the firmware partition layout. We recommend exporting your manuscripts and backing them up to a computer, then following the fresh-install procedure to erase the old system from the ESP32-S3's internal flash and install Alpha 2.

**Reinstallation erases the existing data in internal flash, but does not require formatting the TF card.** Install the new dictionaries that match the Alpha 2 firmware.

See [docs/INSTALL.md](INSTALL.md) for installation instructions.

## Things to keep in mind

Alpha 2 is still a test release. Local Wi-Fi document transfer remains experimental, and some usage scenarios have not been fully tested. This release does not include complete writing firmware for the Pico 2 W.

The release firmware has not yet been independently tested on a device.

See [docs/KNOWN_ISSUES.md](KNOWN_ISSUES.md) and [docs/RESOURCE_STATUS.md](RESOURCE_STATUS.md) for other known issues, resource sources and licensing information.

---

# 草稿机 Alpha 2：中文输入提速与 Wi-Fi 文稿传输

本次 Alpha 2 的主要更新，是让草稿机的中文输入速度明显提升。

输入拼音后，候选词更快出现；选中的字词，也更快地显示在正文中。这次优化不仅改善了单次输入的响应，还缩短了部分候选翻页和连续输入的等待。

此外，Alpha 2 加入了 **本地 Wi-Fi 传输功能**。用户可通过手机浏览器连接草稿机以导入、导出文稿。它通过 ESP32-S3 的Wi-Fi模块直连手机，不依赖所在场地的Wi-Fi网络和任何云端文本服务。

## 这次改了什么

本次性能优化主要包括四项：

1. **分块查询：**重新组织输入法的编码索引，减少寻找候选词时需要读取的资料。
2. **减少重复查询：**已经取得并验证的文字位置，可以直接交给后续取字过程使用。
3. **短文字同块读取：**让符合条件的完整短文字与共享词条一同取得，减少额外读取和验证。
4. **差量显示：**屏幕尽量只更新实际发生变化的部分，不再每次都重新传输整幅画面。

这些改进，加上其他细节调整，缩短了从按下按键到看到输入结果的等待时间。

本版继续支持全拼、小鹤双拼、五笔86和英文输入，以及横排、竖排写作。

## 输入速度改善

以下是同一台 ESP32-S3 原型机上，Alpha 1 与 Alpha 2 累计测量版本的几项对应测试结果。

| 操作 | Alpha 1 | Alpha 2 |
| --- | --- | --- |
| 小鹤首次 `wo`，完整候选 | 317.3 ms | **114.6 ms** |
| 全拼首次 `shi`，完整候选 | 333.7 ms | **122.8 ms** |
| 小鹤首次 `wo`，原码反馈 | 55.3 ms | **9.6 ms** |
| 小鹤词组空格选词，正文显示 | 72.1 ms | **36.4 ms** |

其中，前两项完整候选等待缩短了约64%和63%。

计时从 ESP32-S3 接收到对应的键盘输入事件开始，到原码、完整候选或正文的相应画面完成软件传输为止。

另外，我们还使用 Alpha 2 累计版进行了一次自然写作测试。在这次约1400字符的写作中，有效完整候选等待的中位数为 **84.4 ms**，原码反馈为 **17.8 ms**，选词后正文显示为 **46.5 ms**。

完整测试结果和测量方法见 [docs/PERFORMANCE.md](PERFORMANCE.md)。

## 下载哪些文件

ESP32-S3 N16R8（Thick）用户安装时，需要下载同一版本的 **Firmware** 和 **Resources**。

希望自行编译固件的用户，还需要 **Source** 与 **BuildInputs**，并参考 [docs/BUILD.md](BUILD.md)。

| 文件 | 用途 |
| --- | --- |
| `Firmware.zip` | 固件、启动文件及分区文件 |
| `Resources.zip` | 配套词库、索引和字体 |
| `Source.zip` | Draftor、bgIME及相关自有源码 |
| `BuildInputs.zip` | 固定构建所需的生成文件和相关依赖材料 |

GitHub自动生成的Source code压缩包，不等于完整安装包。

## 从旧版升级

**不建议直接从 Alpha 1 升级到 Alpha 2。**

由于新版本调整了固件分区，建议旧版用户先将文稿导出并备份到电脑，再按照全新安装流程，清除 ESP32-S3 内部Flash上的旧版系统并重新安装 Alpha 2。

**重新安装会清除内部Flash中的原有数据，但不需要格式化TF卡。** 新版词库应与Alpha 2固件配套安装。

具体安装方法见 [docs/INSTALL.md](INSTALL.md)。

## 仍需注意

Alpha 2 仍然是测试版本。本地Wi-Fi文稿传输仍属于实验功能，部分使用情境尚未充分测试。本版暂不提供Pico 2 W的完整写作固件。

发行固件尚未经过独立实机验证。

其他已知问题、资源来源及许可信息，见 [docs/KNOWN_ISSUES.md](KNOWN_ISSUES.md) 和 [docs/RESOURCE_STATUS.md](RESOURCE_STATUS.md)。
