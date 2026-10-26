# bgIME

**Big Goose Input Method Engine · 大鹅输入法**

[简体中文](README.md) | **English** · [Draftor](../../README.en.md)

bgIME (Big Goose Input Method Engine) is a Chinese input engine designed for resource-constrained embedded devices.

It supports Full Pinyin, Xiaohe Shuangpin, and Wubi 86. Each input scheme has its own encoding rules, while sharing lexical resources, candidate lookup, and text submission through a common engine. Dictionaries can be stored on a TF card and read on demand, enabling Chinese input on microcontrollers with limited memory.

bgIME currently powers [Draftor](../../README.en.md), an open-source writing device. Its Core can be built independently, and the repository includes command-line examples for developers interested in integrating the engine into other projects.

## Why “Big Goose”?

The name “Big Goose” comes from the many ways of writing the Chinese character 鹅 (“goose”). The six characters 鹅, 鵝, 鵞, 䳗, 䳘 and 𨿍 (我 on the left, 隹 on the right) each have their own Unicode code point. The [*Dictionary of Chinese Character Variants*](https://dict.variants.moe.edu.tw/dictView.jsp?ID=52649) records 11 variant forms of 鵝.

bgIME has no ambition to include them all, but the diversity of writing represented by 鹅 inspired the engine's name. (For the record, this has absolutely nothing to do with the engine designer's experiences of being attacked by domestic geese and swans.)

## bgIME improvements in Draftor Alpha 2

[Draftor Alpha 2](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) is now available. This update improves bgIME's candidate lookup and text retrieval.

### Reducing data reads and repeated work

Microcontrollers have limited memory. To conserve RAM, bgIME stores dictionary data and indexes on a TF card and reads them as needed.

Chinese input broadly involves three steps: the user enters a code, the engine finds candidates, and their text is retrieved.

Alpha 2 improves three parts of this process:

1. **Block-based lookup:** Full Pinyin and Xiaohe Shuangpin use block-organized code indexes. The engine reads relevant blocks as needed and reuses valid data already in memory, reducing storage access during candidate lookup.

2. **Text-location handoff:** Shared lexical records contain the location of their default output text. Once this information has been read and verified, it can be passed directly to the text reader, reducing repeated lookups and reads.

3. **Co-located short-text reads (format4):** For eligible short texts, the default output text and its shared lexical record are read from the same data block, reducing additional accesses. Other outputs continue through the existing general text-reading path.

These changes shorten the process from entering a code to retrieving candidate text.

In Draftor, bgIME, the editor, and the display driver work together to complete the input process. Alpha 2 also uses differential display updates to reduce repeated transfers of unchanged screen regions.

### Measured response times

Alpha 1 and Alpha 2 were compared on the same ESP32-S3 prototype, with the following results:

| Input | Alpha 1 | Alpha 2 |
|---|---:|---:|
| First Xiaohe Shuangpin `wo`, complete candidates | 317.3 ms | **114.6 ms** |
| First Full Pinyin `shi`, complete candidates | 333.7 ms | **122.8 ms** |

Timing begins when the device receives the **keyboard event** and ends when the software completes the **display transfer** for the complete-candidate image. It includes lookup, text retrieval, and display.

Other results, including response times during natural writing, are available in the [performance report](../../docs/PERFORMANCE.md).

## Quick start

The bgIME source lives in the Draftor repository. Start with the command-line examples to explore candidate lookup and text submission, then integrate the engine into your own application as needed.

The following commands require CMake 3.20 or later and a C++17 compiler. The existing Host examples use POSIX file APIs and have been run on macOS.

### Run the Core example

From the repository root:

```sh
cmake -S ime/bgime/examples/host_minimal \
  -B build/host_minimal \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/host_minimal \
  --target bgime_host_minimal --parallel 2

./build/host_minimal/bgime_host_minimal \
  ./build/host_minimal/fixture
```

The example automatically generates a small test dictionary, then calls Core to look up candidates, select text, and submit the result.

Its Full Pinyin code `yuanxingji` and Xiaohe Shuangpin code `yrxkji` both resolve to 「原型机」, demonstrating how different input schemes can share the same lexical identity.

For keystroke handling, editing, and composition, see the other examples:

- [Full Pinyin and Xiaohe composition example](examples/host_composition/README.md): uses Core and Extension to demonstrate keystroke input, code editing, candidate selection, and composition submission.
- [Wubi 86 example](examples/host_wubi86/README.md): uses the Wubi decoder and Core to demonstrate keystroke input, candidate pagination, and text submission.

Each example uses its own small test dictionary. They provide a starting point for learning the interfaces and integration patterns; production applications need matching input resources.

### Build Core independently

To build only the bgIME Core static library, run the following commands from the repository root:

```sh
cmake -S integration/product/dependencies/core \
  -B build/bgime-core \
  -DHANIME_BUILD_HOST_ADAPTERS=OFF \
  -DHANIME_BUILD_TOOLS=OFF \
  -DHANIME_BUILD_FIXTURES=OFF \
  -DHANIME_BUILD_TESTS=OFF

cmake --build build/bgime-core --target hanime_core
```

This builds the `hanime_core` library target. Within the same CMake build, it can be referenced as `HanIME::Core`.

Core provides interfaces for lexical resource management, lookup, candidates, ranking, and text submission. The host application supplies storage access, input-event scheduling, and a destination for committed text.

See the [Core public headers](../../integration/product/dependencies/core/include/hanime/) for the API entry points.

## Source layout

bgIME separates shared input-method capabilities from scheme-specific input processing.

| Component | Source | Responsibility |
|---|---|---|
| Core | [dependencies/core](../../integration/product/dependencies/core/) | Shared lexical identities, resource access and validation, candidate lookup, ranking, and text submission |
| Extension | [dependencies/extension](../../integration/product/dependencies/extension/) | Full Pinyin and Xiaohe Shuangpin input rules, segmentation, and composition |
| Wubi | [dependencies/wubi](../../integration/product/dependencies/wubi/) | Wubi 86 decoding and query adaptation |
| Host / Platform | [Core interfaces](../../integration/product/dependencies/core/include/hanime/) · [Platform layer](../../integration/product/platform/) | Connecting resource I/O, input events, and text output to a device |
| Draftor | [Product integration](../../integration/product/) | Bluetooth keyboard input, editor, horizontal and vertical layout, and document management |

Full Pinyin, Xiaohe Shuangpin, and Wubi 86 can share Core's lexical resources while retaining their own input rules. Wubi uses its own decoder rather than Full Pinyin segmentation.

In Draftor, bgIME handles Chinese input, while the product combines it with keyboard input, editing, screen rendering, and local document transfer.

See [Source Integration](../../docs/SOURCE_INTEGRATION.md) for a more detailed source map.

## Versions and resources

### Current source versions

| Component | Version |
|---|---|
| [Draftor](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2.1) | `v0.1.0-alpha.2.1` |
| bgIME Core | `0.13.1-rc.1` |
| Core public API | `0.13.0` |
| Core CMake library version | `0.13.1` |
| Extension | `0.8.4-rc.1` |
| Extension CMake project version | `0.8.4` |

The Alpha 2 lookup and text-reading implementation described here corresponds to the source at the Draftor `v0.1.0-alpha.2` tag.

Core source directories and CMake targets continue to use the `HanIME` name.

### Dictionaries and indexes

Draftor Alpha 2 and Alpha 2.1 use the same **format4** resource set, stored at:

```text
HANIME/IME/B_SHORT_TEXT_20261018
```

The set includes shared lexical data and scheme indexes for Full Pinyin, Xiaohe Shuangpin, and Wubi 86.

Use the complete resource set that matches the firmware when installing and running it. Older documentation refers to the earlier B2 resource set, `LEXICON_R3_B2_20261006`; Alpha 2 uses the format4 resources above.

See the [resource manifest](../../release/alpha2/RESOURCE_MANIFEST.json) for the file set and version information, and the [build guide](../../docs/BUILD.md) for full firmware build instructions.

Core can be compiled independently without downloading the complete production dictionary. A running input method needs resources compatible with its chosen schemes.

## Licensing and development status

Project-owned bgIME source is released under **Apache License 2.0**. See [LICENSE](../../LICENSE) and [NOTICE](../../NOTICE). Fonts, dictionaries, and other third-party materials retain their respective licensing and attribution requirements.

Resource provenance and related licensing matters are documented in [Third-Party Notices](../../THIRD_PARTY.md), the [License Inventory](../../docs/LICENSE_INVENTORY.md), and [Resources & Licensing Status](../../docs/RESOURCE_STATUS.md).

A Chinese invention patent application has been filed concerning the shared stable lexical identity technology.

Some features are still in development:

- Core provides user-state and learning interfaces; user-frequency learning is still under development.
- Input schemes are integrated through the existing module interfaces. A general plugin ABI is not currently provided.
- The existing Host file adapters use POSIX APIs. Other platforms require corresponding platform adapters.

bgIME is a collaboration between me, Xiaojun and ChatGPT. Work on the engine continues, and suggestions are welcome—please leave a comment.