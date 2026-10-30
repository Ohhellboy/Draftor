# Draftor

[简体中文](README.md) | **English**

Draftor is an open-source, portable Chinese writing device built around a microcontroller. It uses a Bluetooth keyboard and supports offline writing and local document storage.

It aims to provide a distraction-free writing experience: no internet connection is needed, there are no AI writing features, and the device keeps to basic editing and document transfer.

## Latest update: Alpha 2.2 — Waveshare Board Support

Alpha 2.2 adds support for the **Waveshare ESP32-S3-RLCD-4.2**, which combines an ESP32-S3, a 4.2-inch monochrome reflective LCD and a microSD card slot on one board. There are fewer separate modules to wire together. With a Bluetooth keyboard and a prepared microSD card, Draftor can be used for offline Chinese writing.

Full Pinyin, Xiaohe Shuangpin, Wubi 86, horizontal and vertical writing, Chinese Find and document management are all carried over. This update also improves multifunction Bluetooth keyboard compatibility, adjusts candidate focus and Chinese menu hints for the monochrome display, and fixes an unnecessary warning when reopening certain imported manuscripts.

[Download Alpha 2.2](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2.2) · [Release notes](docs/RELEASE_NOTES.md) · [Installation](docs/INSTALL.md) · [User guide](docs/USAGE.md) · [Original-board Alpha 2.1](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2.1)

![Draftor prototype running Chinese input](draftor-prototype.jpg)

## Features and hardware

| Feature | Support |
|---|---|
| Chinese input | Full Pinyin, Xiaohe Shuangpin and Wubi 86 |
| English input | Direct English input |
| Character display | Simplified, Traditional and variant Chinese characters using the supplied fonts |
| Writing layout | Horizontal and vertical |
| Document management | Local saving, Chinese file menus, renaming, trash and help |
| Find in a manuscript | Chinese and English search, forward/backward navigation and wraparound in both layouts |
| Document transfer | USB import and export; experimental local Wi-Fi transfer |

The complete ESP32-S3 writing firmware currently supports two hardware configurations:

**Waveshare ESP32-S3-RLCD-4.2:** An integrated board with an ST7305 monochrome reflective display and microSD slot, using Alpha 2.2.

**Original ESP32-S3 N16R8 + Osptek ST7306:** A separate black-white-red reflective display and microSD module, continuing with Alpha 2.1.

The repository also provides standalone ST7306 display examples for Raspberry Pi Pico 2 W. See [Hardware](docs/HARDWARE.md) for connections and requirements.

## Getting started

### Install the firmware

Choose the firmware that matches your board.

The Waveshare board uses [Alpha 2.2 Firmware](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.2/Draftor_v0.1.0-alpha.2.2_WAVESHARE_RLCD42_Firmware.zip). The original ST7306 setup continues using [Alpha 2.1](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2.1).

The Waveshare version reuses the [Alpha 2.1 dictionaries, indexes and fonts](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip). An existing microSD card prepared with the matching resources can be reused.

The installation guide covers first installation and updates to compatible Draftor partitions. First installation erases the device's internal Flash, so back up anything you want to keep. The microSD card does not need formatting.

### Write and transfer documents

The [user guide](docs/USAGE.md) covers Chinese input, writing layouts, Find, document management, USB transfers and local Wi-Fi transfers.

Alpha 2.2 is still a test release, and local Wi-Fi transfer remains experimental. Back up important manuscripts regularly. See [Known Issues](docs/KNOWN_ISSUES.md).

### Build from source

Use Alpha 2.2 **Source** with the matching Alpha 2.1 **BuildInputs**. Follow the [build guide](docs/BUILD.md) to prepare the toolchain and select the correct hardware target. The device also requires the matching microSD resources.

GitHub's automatically generated Source code archives are repository snapshots. For firmware installation, use the files provided on the release page.

## Chinese input: bgIME

Draftor's Chinese input is provided by **bgIME — the Big Goose Input Method Engine**.

bgIME is designed for embedded devices with limited memory. It supports Full Pinyin, Xiaohe Shuangpin, and Wubi 86. Each scheme has its own encoding rules, while sharing lexical resources, candidate lookup, and text submission.

Its Core can be built independently, and the repository includes command-line examples for other projects to explore and integrate. Draftor combines the engine with Bluetooth keyboard input, an editor, screen rendering, and document transfer.

[Explore bgIME, its examples, and integration guide](ime/bgime/README.en.md)

---

## WHY DO WE NEED A DISTRACTION-FREE TEXT RECORDING DEVICE?

Many people, including me, are easily distracted.

The personal reasons for distraction are complex and cannot simply be reduced to “you are a useless guy.” Someone might be troubled by anxiety, PTSD, depression, or ADHD; someone else might simply be affected by an occasional low mood that occurs in summer.

More importantly, there are too many things that distract us: WhatsApp, WeChat, email, social media, shopping websites, and god damn TikTok.

Writing can be a form of self-healing. It helps us rethink our behaviour and clear our minds. This is a process we have to do ourselves. Reading, studying, working, creating, and speaking cannot completely replace writing.

Writing is also a way of learning and thinking through haptic feedback.

We have to accept the limits imposed by millions of years of biological evolution. For many people, deep thinking is difficult to separate from tactile experience, even if they may not realize it. Even copying things out by hand better matches some people’s memory habits, including mine.

Friction is not always a bad thing.

I really enjoy the friction of an HB pencil on paper when I write in my notebook. It often makes me write a little more — authentic nonsense, handmade.

Many people still prefer to do their own writing.

AI can indeed simulate a specific writing style extremely well, but that is not the same as personal expression. Writing experience can be digitized, but it is difficult to make it personal without lived experience in the real world.

If you work with text — for example, as a novelist or an anthropologist — it may be easier to appreciate this.

---

## WHY CHOOSE DRAFTOR?

### Because it supports Chinese input

WriterDeck is already a relatively established category of portable, distraction-free text recording devices in markets dominated by Latin-alphabet languages. Electronic typewriters with displays have existed for nearly half a century.

However, most WriterDecks and thermal typewriters do not support Chinese input.

Devices such as the AlphaSmart, Freewrite, Brother EP-22, and Canon TypeStar 5 were not designed around Chinese input. Sharp, Casio, and other manufacturers did produce word processors capable of handling Chinese, but their Chinese input methods no longer match contemporary input habits.

Some require character-by-character input, which is very different from how people normally use modern Chinese input methods.

### Because it is designed for long-form text

Chinese PDAs were personal digital terminals, not dedicated long-form writing devices.

Popular e-readers such as Kindle and iReader are designed primarily for reading, not writing.

Some of these devices have awkward input interfaces, while others cannot provide the fast candidate response that Chinese input requires.

Friction is not always a bad thing, but it is not necessarily a good thing either.

### Old equipment comes with other costs

Old laptops, tablets, mobile phones, and Chinese word processors can be very good for text input and editing, provided that you can accept their limitations: weight, battery life, file transfer, and other practical restrictions.

I like the fold-out keyboard on the Nokia 6822, for example, but its screen is too small and, for my purposes, it lacks usable Wi-Fi, Bluetooth file transfer, and a practical USB interface.

### Functions are distractions

Many of the devices mentioned above run Linux, OS X, Windows, iOS, BBOS, Symbian OS, or other operating systems.

They have many functions. Some can even run video games.

Those functions are potential sources of distraction.

For someone who can spend five minutes playing with a toggle switch, those functions are enough to kill writing.

### Cost matters too

Here, by “cost”, I also mean the time and effort required to modify and adapt equipment.

Some classic devices are now expensive collectibles rather than practical tools.

### Faster than handwriting

I make my own A5 notebooks and take notes in pencil.

But writing by hand is hard on the hands, and speed is also a real problem.

Friction might be a good thing, but tenosynovitis is not.

---

In short, if you need a distraction-free long-text recording device with Chinese input methods, do not want to spend too much time adapting old equipment, and want something faster than handwriting, Draftor may be a good choice.

**Source code and documentation will be added here progressively as the project develops.**

## Development and licensing

Project-owned Draftor source is released under [Apache License 2.0](LICENSE). The standalone ST7306 display module retains its MIT license. Fonts, dictionaries, and other third-party materials retain their respective licensing and attribution requirements.

[Build guide](docs/BUILD.md) · [bgIME input engine](ime/bgime/README.en.md) · [ST7306 display module](display/st7306/) · [Third-party notices](THIRD_PARTY.md) · [Resources and licensing](docs/RESOURCE_STATUS.md)

Questions, problems, and suggestions are welcome through [GitHub Issues](https://github.com/Ohhellboy/Draftor/issues).
