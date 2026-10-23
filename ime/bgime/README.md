# bgIME

**Big Goose Input Method Engine · 大鹅输入法**

[简体中文](README.zh-CN.md) · [Draftor](../../README.md)

bgIME (大鹅中文输入法引擎, or Big Goose Input Method Engine) is a Chinese input engine for resource-constrained embedded devices.

The name “Big Goose” comes from the many ways of writing the Chinese character 鹅 (“goose”). The six characters 鹅, 鵝, 鵞, 䳗, 䳘 and 𨿍 (我 on the left, 隹 on the right) each have their own Unicode code point. The [*Dictionary of Chinese Character Variants*](https://dict.variants.moe.edu.tw/dictView.jsp?ID=52649) records 11 variant forms of 鵝.

bgIME has no ambition to include them all, but the diversity of writing represented by 鹅 inspired the engine's name. (For the record, this has absolutely nothing to do with the engine designer's experiences of being attacked by domestic geese and swans.)

Draftor (草稿机) is the current publicly released application built with bgIME. The available schemes support Full Pinyin (Quanpin), Xiaohe Shuangpin and Wubi 86. Full Pinyin and Xiaohe use Extension for input and composition; Wubi 86 has its own decoder and query adapter. All share Core's resource, candidate and commit services. English direct input, horizontal and vertical display, and document transfer are provided by the Draftor product layer.

This page describes bgIME's source layout and how to use it. The code remains in its original directories in the Draftor repository and has not yet been released as a separate distribution.

Core can be built independently. Turning it into a complete input method still requires scheme handling, matching resources and a host application.

## What changed with Draftor Alpha 2

[Draftor v0.1.0-alpha.2](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.2) is publicly available. This is a Draftor product release, not a separate bgIME release or a new “Alpha 2” Core API.

| Layer | Current version |
| --- | --- |
| Draftor product | `v0.1.0-alpha.2` |
| bgIME Core | `0.13.1-rc.1`; public API `0.13.0`; CMake library version `0.13.1` |
| Extension | `0.8.4-rc.1`; CMake project version `0.8.4` |

### Less data to read, less work to repeat

On a device with limited RAM and dictionaries stored on a TF card, finding a candidate is only part of the job. Its text must also be located, read and passed to the host. Alpha 2 integrates three related improvements:

1. **Block-based lookup:** Full Pinyin and Xiaohe code indexes are searched by block. Reading blocks as needed and reusing a valid working block reduces index I/O while finding candidates.
2. **Text handoff:** a default text location that has already been confirmed is passed directly to the next reading stage, reducing repeated location lookups and reads.
3. **Short text in the same format4 block:** eligible complete short default texts are obtained together with their shared lexical records, reducing additional accesses and verification work. Longer texts retain their complete read path.

Together, these changes reduce the work needed to fetch candidate text from storage and help shorten Chinese input waits. Resource validation, candidate validity and text commit rules remain in place. Draftor Alpha 2 enables the changes with matching build configuration and resources; independent integrations also need to wire up the corresponding configuration and resource set.

Alpha 2's **differential display updates** belong to Draftor's display implementation and combine with these changes to reduce whole-device waiting. **Wi-Fi document transfer** is a Draftor product feature. Neither is a bgIME Core feature.

### Reading the performance results

In one corresponding sample from the Draftor Alpha 1 and cumulative Alpha 2 measurement builds on the same ESP32-S3, the wait for complete candidates after the first Xiaohe `wo` fell from **317.3 ms** to **114.6 ms**. This is an **end-to-end device wait**, from receipt of an input event until the corresponding software LCD transfer finishes. It includes query, text reading and display work; it is not a standalone bgIME Core benchmark.

These finite examples do not establish the same percentage improvement for every input. The measurement builds are recorded separately from the release App with input observation disabled. See [Performance](../../docs/PERFORMANCE.md) for the results, timing definitions and remaining costs.

## Using bgIME in another application

### Source layout

| Part | Source | Role |
|---|---|---|
| Core | [dependencies/core](../../integration/product/dependencies/core/) | Shared lexical identities, resource access and validation, queries, ranking, candidates and text submission |
| Extension | [dependencies/extension](../../integration/product/dependencies/extension/) | Pinyin input, segmentation and composition handling, using the shared Core |
| Wubi | [dependencies/wubi](../../integration/product/dependencies/wubi/) | Wubi 86 decoding and direct-query adaptation |
| Host / Platform | [Core ports and Host adapters](../../integration/product/dependencies/core/include/hanime/), [Draftor platform](../../integration/product/platform/) | Resource I/O adapters; the host also supplies input events, scheduling, display and a destination for committed text |
| Draftor | [Product integration](../../integration/product/) | The existing keyboard, editor and interaction code, including horizontal and vertical candidate presentation |

The input schemes share Core's lexical resources and runtime services; they do not all use the same decoding or segmentation rules. Wubi is not routed through the Full Pinyin planner. For more context, see [Source Structure](../../docs/SOURCE_INTEGRATION.md).

### Core static library

Building Core independently requires **CMake 3.20 or later** and **C++17**. From the repository root, the following commands build only the runtime library. No complete production dictionary download is needed:

```sh
cmake -S integration/product/dependencies/core -B build/bgime-core \
  -DHANIME_BUILD_HOST_ADAPTERS=OFF \
  -DHANIME_BUILD_TOOLS=OFF \
  -DHANIME_BUILD_FIXTURES=OFF \
  -DHANIME_BUILD_TESTS=OFF
cmake --build build/bgime-core --target hanime_core
```

The options and targets are defined in [Core's CMake entry](../../integration/product/dependencies/core/CMakeLists.txt).

When Core is configured as a top-level project, these development components otherwise default to enabled. The runtime-only build produces the `hanime_core` static library, exposed to CMake consumers as `HanIME::Core`. It does not produce a running input application or a Draftor firmware image.

### Standalone Host application

A Host consumer uses the existing [public Core interfaces](../../integration/product/dependencies/core/include/hanime/), supplies valid resources, advances the engine's work and receives committed text. Full Pinyin and Xiaohe composition use Extension; Wubi has its own decoder and query adapter. [Extension has a CMake entry](../../integration/product/dependencies/extension/CMakeLists.txt); the product's Wubi target is defined in [Draftor's product CMake](../../integration/product/CMakeLists.txt). The Wubi demo below defines a local target over the same Wubi source.

The [historical Host examples in the Core directory](../../integration/product/dependencies/core/examples/) have fixed version and resource assumptions. They are useful references, but are not a verified Quick Start for the current source versions.

The repository also provides three standalone command-line demos as integration references:

- [Core Host demo](examples/host_minimal/README.md): Core code queries, candidate selection and text submission using complete fixture codes, without keystroke parsing.
- [Composition Host demo](examples/host_composition/README.md): Core + Extension, demonstrating Full Pinyin and Xiaohe keystroke input, editing, candidate selection and composition submission.
- [Wubi86 Host demo](examples/host_wubi86/README.md): Wubi86Decoder + DirectQueryBinding + Core, demonstrating keystroke input, candidate pagination, explicit submission and automatic submission requests when a complete four-letter code has a unique candidate.

Their READMEs retain the scope of the existing macOS runs with small, isolated test resources, including the Alpha 1 source baseline stated there. This documentation update does not rerun those demos or relabel their old results as Alpha 2 validation. They need no complete production dictionary and do not establish full product behavior or provide installable system input methods. The Wubi codes are artificial fixture data, not a production Wubi code table. An automatic submission request does not commit text by itself; the Host must confirm it. This demo does not reproduce Draftor's complete autoSafe auto-commit policy.

### Draftor firmware

The complete firmware also builds the editor and device integration, with its own toolchain and external build inputs. Follow the [ESP32-S3 firmware build instructions](../../docs/BUILD.md) and use the Alpha 2 resources described below.

## Resources and licenses

Project-owned bgIME source is released under **Apache-2.0**. See [LICENSE](../../LICENSE) and [NOTICE](../../NOTICE). Fonts, dictionaries and other third-party materials retain their respective licensing requirements.

Draftor Alpha 2 uses the **format4 resource set `B_SHORT_TEXT_20261018`**, installed at `HANIME/IME/B_SHORT_TEXT_20261018` on the TF card. See the [published Build Guide](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.2/docs/BUILD.md) and [resource manifest](../../release/alpha2/RESOURCE_MANIFEST.json) for exact resource identities, active bindings and their relationship to the build.

**`LEXICON_R3_B2_20261006` is the B2 resource set associated with older material, including Alpha 1.** It remains relevant to historical builds and tests, but is not the current Alpha 2 runtime resource combination.

Compiling Core independently does not require a complete production dictionary. Running an input method does require resources that match the chosen schemes, shared lexical data and build configuration. Matching scheme names or text are not a reason to mix old B2 files with format4 files. The three Host demos use their own small test resources rather than this complete product set.

Resource provenance and unresolved licensing matters are documented in the existing [Third-Party Notices](../../THIRD_PARTY.md), [License Inventory](../../docs/LICENSE_INVENTORY.md) and [Resources & Licensing Status](../../docs/RESOURCE_STATUS.md).

The project has announced a Chinese invention patent application concerning the shared stable lexical identity technology. The application documents are not included in this repository.

## Current limits and next steps

- Internal `HanIME` names, namespaces and CMake targets are retained.
- There is no general input-scheme plugin ABI.
- Core includes user-state and learning interfaces, but the learning functionality is still under development. Draftor does not currently offer user-frequency learning.
- The standalone Host demos use small test resources; production dictionary coverage and complete product behavior have not been validated.
- The existing Host file adapters use POSIX APIs. Other platforms and toolchains need integration and testing before they can be listed as supported.

bgIME is a collaboration between me, Xiaojun and ChatGPT. Work on the engine continues, and suggestions are welcome—please leave a comment.
