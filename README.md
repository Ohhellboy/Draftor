# Draftor Alpha 2: Faster Chinese Input and Wi-Fi Document Transfer

**English** | [简体中文](README.zh-CN.md)

[Release notes](docs/RELEASE_NOTES.md) · [Installation guide](docs/INSTALL.md)

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

Follow the [installation guide](docs/INSTALL.md) included in this version’s Firmware package.

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

The release firmware has passed a brief device check covering startup, Chinese input, candidate selection, saving and reopening a test manuscript.

See [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) and [docs/RESOURCE_STATUS.md](docs/RESOURCE_STATUS.md) for other known issues, resource sources and licensing information.

<!-- ALPHA_NAV_END -->

**A distraction-free local pocket text recording device**

![Draftor prototype running Chinese input](draftor-prototype.jpg)

---

## Current Status

### MCU Support

- ESP32-S3 N16R8
- Raspberry Pi Pico 2 W

The complete Alpha firmware currently targets ESP32-S3 N16R8 (Thick). Raspberry Pi Pico 2 W (Thin) has standalone ST7306 display demos, but no complete Alpha writing firmware in this release.

### Display Support

- [Osptek ST7306 black-white-red reflective TFT display](display/st7306/)

### Input Support

- Quanpin (Full Pinyin)
- Shuangpin (Double Pinyin)
- Wubi (Five-Stroke)
- English direct input

---

## WHAT IS DRAFTOR?

Draftor is a distraction-free local text input and recording device — a “typewriter” built around an MCU (Microcontroller Unit) and used with a Bluetooth keyboard.

By “distraction-free”, I mean offline, no AI, and limited editing features.

It provides offline text storage and transfer without relying on cloud text services.

---

## WHAT DOES DRAFTOR SUPPORT?

Draftor currently supports English direct input and three Chinese input methods: Quanpin (Full Pinyin), Shuangpin (Double Pinyin), and Wubi (Five-Stroke).

It supports Simplified Chinese, Traditional Chinese, and variant Chinese characters, with corresponding font resources.

It is compatible with standard English-layout keyboards for macOS and Windows.

It currently supports both the Raspberry Pi Pico 2 W and the ESP32-S3 N16R8. The former is the Thin version, while the latter is the Thick version. The Thin version is a simpler and more focused typing and recording platform. The Thick version provides more functions and therefore comes with more distractions.

Draftor currently supports the Osptek ST7306 black-white-red reflective TFT display. Its display driver has been adjusted from the official driver to make it more suitable for displaying text.

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
