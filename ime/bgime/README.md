> Alpha 2: current cumulative product build and active format4 bindings are documented in [the Build Guide](../../docs/BUILD.md). The standalone Core source remains here in `integration/product/dependencies/core`; the older fixture notes below describe their original test scope.
> Alpha 2累计构建与生效format4绑定见构建指南；下列旧回放资料保留原适用范围。

# bgIME

**Big Goose Input Method Engine · 大鹅输入法**

[简体中文](README.zh-CN.md) · [Draftor](../../README.md)

bgIME (大鹅中文输入法引擎, or Big Goose Input Method Engine) is a Chinese input engine for resource-constrained embedded devices.

The name “Big Goose” comes from the many ways of writing the Chinese character 鹅 (“goose”). The six characters 鹅, 鵝, 鵞, 䳗, 䳘 and 𨿍 (我 on the left, 隹 on the right) each have their own Unicode code point. The [*Dictionary of Chinese Character Variants*](https://dict.variants.moe.edu.tw/dictView.jsp?ID=52649) records 11 variant forms of 鵝.

bgIME has no ambition to include them all, but the diversity of writing represented by 鹅 inspired the engine's name. (For the record, this has absolutely nothing to do with the engine designer's experiences of being attacked by domestic geese and swans.)

Draftor (草稿机) is the current publicly released application built with bgIME. Its Chinese input supports Full Pinyin (Quanpin), Xiaohe Shuangpin and Wubi 86. English direct input is handled by Draftor's input routing, not by a separate bgIME English dictionary engine.

This page describes bgIME's source layout and how to use it. The code remains in its original directories in the Draftor repository and has not yet been released as a separate distribution.

Core can be built independently. Turning it into a complete input method still requires scheme handling, matching resources and a host application.

## Source layout

| Part | Source | Role |
|---|---|---|
| Core | [dependencies/core](../../integration/product/dependencies/core/) | Shared lexical identities, resource access and validation, queries, ranking, candidates and text submission |
| Extension | [dependencies/extension](../../integration/product/dependencies/extension/) | Pinyin input, segmentation and composition handling, using the shared Core |
| Wubi | [dependencies/wubi](../../integration/product/dependencies/wubi/) | Wubi 86 decoding and direct-query adaptation |
| Host / Platform | [Core ports and Host adapters](../../integration/product/dependencies/core/include/hanime/), [Draftor platform](../../integration/product/platform/) | Resource I/O adapters; the host also supplies input events, scheduling, display and a destination for committed text |
| Draftor | [Product integration](../../integration/product/) | The existing keyboard, editor and interaction code, including horizontal and vertical candidate presentation |

The input schemes share Core's lexical resources and runtime services; they do not all use the same decoding or segmentation rules. Wubi is not routed through the Full Pinyin planner. For more context, see [Source Structure](../../docs/SOURCE_INTEGRATION.md).

## Build and use

The current sources identify Core as **0.13.1-rc.1**, with public API **0.13.0** and CMake library version **0.13.1**. Extension is **0.8.4-rc.1**, with CMake project version **0.8.4**. These are component versions; **Draftor Alpha v0.1.0-alpha.1** is the product release.

### Core static library

Core has a standalone CMake build requiring **CMake 3.20 or later** and **C++17**. Start with [Build bgIME Core Separately](../../docs/BUILD.md).

The runtime-only instructions set these options to `OFF`:

- `HANIME_BUILD_HOST_ADAPTERS`
- `HANIME_BUILD_TOOLS`
- `HANIME_BUILD_FIXTURES`
- `HANIME_BUILD_TESTS`

When Core is configured as a top-level project, these development components otherwise default to enabled. The runtime-only build produces the `hanime_core` static library, exposed to CMake consumers as `HanIME::Core`. It does not produce a running input application or a Draftor firmware image.

### Standalone Host application

A Host consumer uses the existing [public Core interfaces](../../integration/product/dependencies/core/include/hanime/), supplies valid resources, advances the engine's work and receives committed text. Full Pinyin and Xiaohe composition use Extension; Wubi has its own decoder and query adapter. [Extension has a CMake entry](../../integration/product/dependencies/extension/CMakeLists.txt); the product's Wubi target is defined in [Draftor's product CMake](../../integration/product/CMakeLists.txt). The Wubi demo below defines a local target over the same Wubi source.

The [existing Host examples](../../integration/product/dependencies/core/examples/) have fixed version and resource assumptions. They are useful references, but are not a verified Quick Start for the current source versions.

Three standalone command-line demos are now available for these source versions:

- [Core Host demo](examples/host_minimal/README.md): Core code queries, candidate selection and text submission using complete fixture codes, without keystroke parsing.
- [Composition Host demo](examples/host_composition/README.md): Core + Extension, demonstrating Full Pinyin and Xiaohe keystroke input, editing, candidate selection and composition submission.
- [Wubi86 Host demo](examples/host_wubi86/README.md): Wubi86Decoder + DirectQueryBinding + Core, demonstrating keystroke input, candidate pagination, explicit submission and automatic submission requests when a complete four-letter code has a unique candidate.

All three have been tested on macOS with small, isolated test resources and need no complete B2 resource set. They are not installable system input methods. The Wubi codes are artificial fixture data, not a production Wubi code table. An automatic submission request does not commit text by itself; the Host must confirm it. This demo does not reproduce Draftor's complete autoSafe auto-commit policy.

### Draftor firmware

The complete firmware also builds the editor and device integration, with its own toolchain and external build inputs. Follow the [ESP32-S3 firmware build instructions](../../docs/BUILD.md) and use the matching Alpha resources.

## Resources and licenses

Project-owned bgIME source is released under **Apache-2.0**. See [LICENSE](../../LICENSE) and [NOTICE](../../NOTICE). Fonts, dictionaries and other third-party materials retain their respective licensing requirements.

The B2 resource set, `LEXICON_R3_B2_20261006`, accompanies Draftor Alpha. It is not needed to compile Core. A running consumer still needs valid resources for its chosen input schemes.

Resource provenance and unresolved licensing matters are documented in the existing [Third-Party Notices](../../THIRD_PARTY.md), [License Inventory](../../docs/LICENSE_INVENTORY.md) and [Resources & Licensing Status](../../docs/RESOURCE_STATUS.md).

The project has announced a Chinese invention patent application concerning the shared stable lexical identity technology. The application documents are not included in this repository.

## Current limits and next steps

- Internal `HanIME` names, namespaces and CMake targets are retained.
- There is no general input-scheme plugin ABI.
- Core includes user-state and learning interfaces, but the learning functionality is still under development. Draftor does not currently offer user-frequency learning.
- The standalone Host demos use small test resources; production dictionary coverage and complete product behavior have not been validated.
- The existing Host file adapters use POSIX APIs. Other platforms and toolchains need integration and testing before they can be listed as supported.

bgIME is a collaboration between me, Xiaojun and ChatGPT. Work on the engine continues, and suggestions are welcome—please leave a comment.
