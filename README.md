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

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for the full results and measurement method.

## Which files to download

To install on an ESP32-S3 N16R8 (Thick), download **Firmware** and **Resources** from the same version.

If you want to build the firmware yourself, also download **Source** and **BuildInputs**, and follow [docs/BUILD.md](docs/BUILD.md).

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

See [docs/INSTALL.md](docs/INSTALL.md) for installation instructions.

## Things to keep in mind

Alpha 2 is still a test release. Local Wi-Fi document transfer remains experimental, and some usage scenarios have not been fully tested. This release does not include complete writing firmware for the Pico 2 W.

The release firmware has not yet been independently tested on a device.

See [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) and [docs/RESOURCE_STATUS.md](docs/RESOURCE_STATUS.md) for other known issues, resource sources and licensing information.

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

完整测试结果和测量方法见 [docs/PERFORMANCE.md](docs/PERFORMANCE.md)。

## 下载哪些文件

ESP32-S3 N16R8（Thick）用户安装时，需要下载同一版本的 **Firmware** 和 **Resources**。

希望自行编译固件的用户，还需要 **Source** 与 **BuildInputs**，并参考 [docs/BUILD.md](docs/BUILD.md)。

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

具体安装方法见 [docs/INSTALL.md](docs/INSTALL.md)。

## 仍需注意

Alpha 2 仍然是测试版本。本地Wi-Fi文稿传输仍属于实验功能，部分使用情境尚未充分测试。本版暂不提供Pico 2 W的完整写作固件。

发行固件尚未经过独立实机验证。

其他已知问题、资源来源及许可信息，见 [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) 和 [docs/RESOURCE_STATUS.md](docs/RESOURCE_STATUS.md)。

<!-- ALPHA_NAV_END -->


**A distraction-free local pocket text recording device**<br>
一台无打搅本地口袋文本记录设备

![Draftor prototype running Chinese input](draftor-prototype.jpg)

---

## Current Status / 当前状态

### MCU Support / 单片机支持

- ESP32-S3 N16R8
- Raspberry Pi Pico 2 W

The complete Alpha firmware currently targets ESP32-S3 N16R8 (Thick). Raspberry Pi Pico 2 W (Thin) has standalone ST7306 display demos, but no complete Alpha writing firmware in this release.

当前完整 Alpha 固件适用于 ESP32-S3 N16R8（Thick）。Raspberry Pi Pico 2 W（Thin）目前公开了 ST7306 独立屏幕示例，本次发行不包含完整写作固件。

### Display Support / 屏幕支持

- [Osptek ST7306 black-white-red reflective TFT display](display/st7306/)

### Input Support / 输入支持

- Quanpin (Full Pinyin) — 全拼
- Shuangpin (Double Pinyin) — 双拼
- Wubi (Five-Stroke) — 五笔
- English direct input — 英文

---

## WHAT IS DRAFTOR? / 草稿机是什么？

Draftor is a distraction-free local text input and recording device — a “typewriter” built around an MCU (Microcontroller Unit) and used with a Bluetooth keyboard.<br>
一个无打搅本地文本记录设备，一台基于单片机的无线蓝牙「打字机」。

By “distraction-free”, I mean offline, no AI, and limited editing features.<br>
「无打搅」的意思是离线，无 AI，无复杂编辑功能。

It provides offline text storage and transfer without relying on cloud text services.<br>
它提供离线文本保存和传输，**不依赖云端**文本服务。

---

## WHAT DOES DRAFTOR SUPPORT? / 草稿机支持什么？

Draftor currently supports English direct input and three Chinese input methods: Quanpin (Full Pinyin), Shuangpin (Double Pinyin), and Wubi (Five-Stroke).<br>
草稿机目前提供英文和全拼、双拼、五笔三种中文输入法。

It supports Simplified Chinese, Traditional Chinese, and variant Chinese characters, with corresponding font resources.<br>
目前支持简体字、繁体字、异体字输出，并提供相应字库资源。

It is compatible with standard English-layout keyboards for macOS and Windows.<br>
兼容 macOS 和 Windows 标准英文键盘。

It currently supports both the Raspberry Pi Pico 2 W and the ESP32-S3 N16R8. The former is the Thin version, while the latter is the Thick version. The Thin version is a simpler and more focused typing and recording platform. The Thick version provides more functions and therefore comes with more distractions.<br>
目前兼容 Raspberry Pi Pico 2 W 与 ESP32-S3 N16R8 单片机。前者为 Thin 版本，更纯粹的打字平台；后者是 Thick 版本，功能更多但诱惑更多。

Draftor currently supports the Osptek ST7306 black-white-red reflective TFT display. Its display driver has been adjusted from the official driver to make it more suitable for displaying text.<br>
草稿机目前使用鱼鹰 ST7306 黑白红三色 TFT 反射屏，驱动模块基于官方驱动调校，适合文本展示。

---

## WHY DO WE NEED A DISTRACTION-FREE TEXT RECORDING DEVICE? / 为什么需要无打搅记录设备？

Many people, including me, are easily distracted.<br>
很多人（包括我自己）容易分心。

The personal reasons for distraction are complex and cannot simply be reduced to “you are a useless guy.” Someone might be troubled by anxiety, PTSD, depression, or ADHD; someone else might simply be affected by an occasional low mood that occurs in summer.<br>
分心的个体原因挺复杂的，不是简单的「你这个人就是不行」可以概括。有些人长期陷于焦虑、抑郁或者注意力缺失的困境；有些人则时不时遭受夏日小情绪的困扰。

More importantly, there are too many things that distract us: WhatsApp, WeChat, email, social media, shopping websites, and god damn TikTok.<br>
更糟糕的是，很多东西让人分心。微信、WhatsApp、邮件、社交媒体、购物网站，以及该死的钉钉和抖音。

Writing can be a form of self-healing. It helps us rethink our behaviour and clear our minds. This is a process we have to do ourselves. Reading, studying, working, creating, and speaking cannot completely replace writing.<br>
写作是一种自愈的方法，它帮助人审视自己的行为，整理自己的想法。这个过程必须由自己完成。阅读、学习、工作、创作、闲聊，都无法完全替代写作。

Writing is also a way of learning and thinking through haptic feedback.<br>
写作还是一种通过触觉反馈学习和思考的方法。

We have to accept the limits imposed by millions of years of biological evolution. For many people, deep thinking is difficult to separate from tactile experience, even if they may not realize it. Even copying things out by hand better matches some people’s memory habits, including mine.<br>
我们不得不接受百万年生物演化的限制。有很多人无法脱离触觉完成深度认知过程，即便他们没意识到这一点。甚至抄写更符合一些人（包括我）的记忆习惯。

Friction is not always a bad thing.<br>
摩擦不一定是坏事。

I really enjoy the friction of an HB pencil on paper when I write in my notebook. It often makes me write a little more — authentic nonsense, handmade.<br>
我非常享受 HB 铅笔在纸面摩擦的反馈。它常常让我不由自主多记几笔（正宗废话，手工制造）。

Many people still prefer to do their own writing.<br>
很多人依然喜欢自己写作。

AI can indeed simulate a specific writing style extremely well, but that is not the same as personal expression. Writing experience can be digitized, but it is difficult to make it personal without lived experience in the real world.<br>
当今的 AI 确实可以完美模拟出具体的风格，但那不是个性的表达。写作经验可以被数据化，但很难脱离现实经验被个性化。

If you work with text — for example, as a novelist or an anthropologist — it may be easier to appreciate this.<br>
如果你是文本工作者，譬如文学创作者和人类学学者，可能更容易体会到这一点。

---

## WHY CHOOSE DRAFTOR? / 为什么选择草稿机？

### Because it supports Chinese input / 因为它有中文输入

WriterDeck is already a relatively established category of portable, distraction-free text recording devices in markets dominated by Latin-alphabet languages. Electronic typewriters with displays have existed for nearly half a century.<br>
WriterDeck（书写板？我不知道该怎么翻译了）是拉丁语系市场相对固定的离线便携文本记录产品品类，带液晶屏的打字机也快有半个世纪的历史。

However, most WriterDecks and thermal typewriters do not support Chinese input.<br>
但是大多数 WriterDeck 和热敏打字机不兼容中文。

Devices such as the AlphaSmart, Freewrite, Brother EP-22, and Canon TypeStar 5 were not designed around Chinese input. Sharp, Casio, and other manufacturers did produce word processors capable of handling Chinese, but their Chinese input methods no longer match contemporary input habits.<br>
那些知名的记录设备不是按照中文输入设计的，譬如 AlphaSmart、Freewrite、兄弟 EP-22 和佳能 TypeStar 5。而像夏普、卡西欧在 1980s 前后推出的中文处理器，它们的中文输入方式已经不符合今天的中文输入习惯。

Some require character-by-character input, which is very different from how people normally use modern Chinese input methods.<br>
有的设备只支持单个汉字的输入方式，这几乎已经被淘汰了。

### Because it is designed for long-form text / 因为它为长文本输入设计

Chinese PDAs were personal digital terminals, not dedicated long-form writing devices.<br>
中文掌上电脑（譬如曾经的商务通）是个人数据终端，而非长文本写作设备。

Popular e-readers such as Kindle and iReader are designed primarily for reading, not writing.<br>
当下流行的 Kindle、掌阅之类的电纸书也是为了阅读而非写作设计的。

Some of these devices have awkward input interfaces, while others cannot provide the fast candidate response that Chinese input requires.<br>
有些设备的文本输入交互设计得相当诡异，有些则受限于低刷新率的屏幕，达不到中文候选词的高速要求。

Friction is not always a bad thing, but it is not necessarily a good thing either.<br>
摩擦不总是坏事，但也不一定是好事。

### Old equipment comes with other costs / 老设备有其他成本

Old laptops, tablets, mobile phones, and Chinese word processors can be very good for text input and editing, provided that you can accept their limitations: weight, battery life, file transfer, and other practical restrictions.<br>
老笔记本电脑、老平板、老手机、老中文处理机在中文输入和编辑上，都很好，但前提是你得接受它们的重量、续航和其他限制。

I like the fold-out keyboard on the Nokia 6822, for example, but its screen is too small and, for my purposes, it lacks usable Wi-Fi, Bluetooth file transfer, and a practical USB interface.<br>
我喜欢 Nokia 6822 的折叠键盘，但它屏幕太小，没有可用的 Wi-Fi 功能和蓝牙文件传输功能。

### Functions are distractions / 功能即干扰

Many of the devices mentioned above run Linux, OS X, Windows, iOS, BBOS, Symbian OS, or other operating systems.<br>
上述提到的很多设备，基于 Linux、OS X、Windows、iOS、BBOS、Symbian OS 或者其他操作系统研发。

They have many functions. Some can even run video games.<br>
它们功能繁复，自带游戏。

Those functions are potential sources of distraction.<br>
成为潜在的干扰源。

For someone who can spend five minutes playing with a toggle switch, those functions are enough to kill writing.<br>
对一个切换按钮都能玩 5 分钟的人来说，这些功能足以杀死写作。

### Cost matters too / 成本高昂

Here, by “cost”, I also mean the time and effort required to modify and adapt equipment.<br>
这里说的成本，包括了改造和适应旧设备的时间成本。

Some classic devices are now expensive collectibles rather than practical tools.<br>
某些经典型号可能价格昂贵，它们按收藏品销售，而不是以老工具定价。

### Faster than handwriting / 比手写快

I make my own A5 notebooks and take notes in pencil.<br>
我自制 A5 大小的笔记本，并用铅笔记录。

But writing by hand is hard on the hands, and speed is also a real problem.<br>
但是手写除了费手，在速度上也确实是个问题。

Friction might be a good thing, but tenosynovitis is not.<br>
摩擦可能是好事，但腱鞘炎从来都不是。

---

In short, if you need a distraction-free long-text recording device with Chinese input methods, do not want to spend too much time adapting old equipment, and want something faster than handwriting, Draftor may be a good choice.<br>
总之，如果你需要一台无打搅便携中文输入设备，并且不想在旧器材上花费太多时间，还希望比手写更快，草稿机可能会是一个好选择。

**Source code and documentation will be added here progressively as the project develops.**<br>
**欢迎收藏。草稿机项目的开发、源代码和文档将在此逐步更新。**
