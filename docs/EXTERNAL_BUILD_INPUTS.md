# External Build Inputs / 外部构建输入

**Version / 适用版本：Draftor Alpha · v0.1.0-alpha.1**

[Build Guide / 构建指南](BUILD.md) · [Source Structure / 源码结构](SOURCE_INTEGRATION.md) · [Resource Status / 资源与许可说明](RESOURCE_STATUS.md)

Some files required to build Draftor are distributed separately from the source code. They include generated font data, dictionary resources, test fixtures and an SDK build recipe.

Draftor 的部分构建输入没有直接放在源码树中，而是单独发行。这些文件包括生成的字体数据、词库资源、测试夹具及 SDK 构建配置。

The current Alpha requires **76 external project inputs**. All 76 are included in the matching `Draftor_Alpha_BuildInputs.zip` archive.

当前 Alpha 需要 **76 项外部项目输入**，均已包含在配套的 `Draftor_Alpha_BuildInputs.zip` 中。

The complete list of expected file paths and sizes is recorded in [EXTERNAL_INPUTS.json](../EXTERNAL_INPUTS.json).

完整文件路径与大小见 [EXTERNAL_INPUTS.json](../EXTERNAL_INPUTS.json)。

## 1. Installing the Inputs / 安装构建输入

Download `Draftor_Alpha_Source.zip` and `Draftor_Alpha_BuildInputs.zip` from the [same Alpha release](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1).

请从[同一个 Alpha 发行版本](https://github.com/Ohhellboy/Draftor/releases/tag/v0.1.0-alpha.1)下载 `Draftor_Alpha_Source.zip` 和 `Draftor_Alpha_BuildInputs.zip`。

The external files are located under:

构建输入位于：

```text
BuildInputs/
└── inputs/
    ├── integration/
    ├── release/
    ├── reference/
    └── vendor/
```

The directory structure under `inputs/` corresponds to the paths expected by the Draftor source tree.

`inputs/` 下的目录结构与 Draftor 源码所需的相对路径对应。

Use the provided installer in a **fresh source copy**:

请在**干净的源码副本**中运行安装脚本：

```sh
cd "/absolute/path/to/Draftor"

export BUILD_INPUTS="/absolute/path/to/BuildInputs"

python3 -B tools/install_external_inputs.py \
  --from "$BUILD_INPUTS/inputs"
```

The installer checks the expected filenames and file sizes before copying them. It does not overwrite existing destination files.

安装脚本会先检查文件名称和大小，再复制到源码树中对应的位置。脚本不会覆盖已有的目标文件。

If an input is missing, has an unexpected size or already exists at its destination, inspect the problem before proceeding. For complete build instructions, see [Build Guide](BUILD.md).

如果文件缺失、大小不符，或目标位置已经存在文件，请先检查原因。完整编译步骤见[构建指南](BUILD.md)。

## 2. What These Files Are For / 文件用途

The 76 inputs serve several different purposes:

这 76 项输入具有不同用途：

| Type / 类型 | Purpose / 用途 |
|---|---|
| Generated source inputs / 生成的源码输入 | Font data, Wubi tables and other files compiled into the application / 字体数据、五笔表及其他参与编译的生成文件 |
| Resource and build bindings / 资源与构建配对 | Dictionaries, indexes, manifests and related resource bindings / 词库、索引、清单及相关配对材料 |
| Host test fixtures / 电脑端测试夹具 | Reference resources and test data used by automated checks / 自动测试所需的参考资源和测试数据 |
| SDK build recipe / SDK 构建配置 | Fixed configuration used to build the required TinyUSB component / 用于构建指定 TinyUSB 组件的固定配置 |

Some entries under `integration/evidence/` and `reference/` are historical test inputs. They are not the active dictionaries installed on the TF card.

`integration/evidence/` 和 `reference/` 中的部分文件用于历史测试及对照，并不是当前需要安装到 TF 卡上的运行词库。

The historical `step2f/source_v1_1.zip` is also included as a Core test and source-reference fixture. It is not a substitute for the current bgIME runtime resources.

历史文件 `step2f/source_v1_1.zip` 作为 Core 测试及来源参考材料提供，不能代替当前 bgIME 的运行资源。

For actual TF card installation, use the matching Resources archive and follow [Installation](INSTALL.md).

如果需要准备真实设备的 TF 卡，请使用对应的 Resources 包，并参照[安装指南](INSTALL.md)。

## 3. Other Build Materials / 其他构建材料

BuildInputs also includes supporting materials outside the 76-file `inputs/` directory, including original font sources, font-generation tools and related source and license documents.

BuildInputs 在这 76 项 `inputs/` 文件之外，还提供原始字体材料、字体生成工具，以及相关源码和许可文档。

These materials help document and reproduce supported parts of the current build and resource-generation process.

这些材料用于说明和复现当前构建及部分资源生成过程。

The complete Arduino-ESP32 SDK and compiler toolchain must still be installed separately. They are not included in these 76 files.

完整的 Arduino-ESP32 SDK 和编译器仍需单独安装，不包含在这 76 项文件中。

## 4. Scope & Limitations / 适用范围与限制

The supplied inputs support the current ESP32-S3 build, together with the specified toolchain. They do not guarantee that every historical version of Draftor, every test or every dictionary-generation step can be reproduced.

这些输入配合指定工具链，可以用于构建当前 ESP32-S3 版本。但它们并不保证可以复现草稿机的所有历史版本、全部测试，或者每一次词库生成过程。

The public Host workflow provides 224 portable tests. Six additional historical tests are outside that workflow, and the existing test suite still contains known failures. See [Build Guide](BUILD.md) and [Known Issues](KNOWN_ISSUES.md).

公开 Host 测试流程提供 224 项可移植测试，另外六项历史测试不包含在这一流程中。现有测试仍保留部分已知失败。具体情况见[构建指南](BUILD.md)及[已知问题](KNOWN_ISSUES.md)。

Some older mixed font and dictionary generation steps cannot yet be fully reconstructed from the available original materials. The generated inputs and runtime resources required by the current Alpha are nevertheless provided.

部分历史混合字体和词库的生成过程尚无法从现有原始材料完整重建，但当前 Alpha 所需的生成输入和运行资源已经提供。

**Availability of a file does not establish its redistribution rights.** Some third-party resource and SDK-related licensing questions remain unresolved. Their known scope is documented in [Resource Status](RESOURCE_STATUS.md) and [Firmware Distribution](FIRMWARE_DISTRIBUTION.md).

**文件已经提供，不等于其再分发许可已经完全明确。** 部分第三方资源及 SDK 相关许可问题仍待确认。已知范围见[资源与许可说明](RESOURCE_STATUS.md)及[固件分发说明](FIRMWARE_DISTRIBUTION.md)。

## 5. Complete Input List / 完整构建输入清单

The following table lists the 76 required inputs, their expected relative paths, sizes, roles and locations in the BuildInputs archive.

下表列出全部 76 项输入，包括预期相对路径、文件大小、用途及其在 BuildInputs 中的位置。

| ID | Path | Bytes | Use | Supplied at |
|---|---|---:|---|---|
| E01 | `integration/product/platform/composition_font_data.hpp` | 62659 | COMPILED_GENERATED_INPUT | `BuildInputs/inputs/integration/product/platform/composition_font_data.hpp` |
| E02 | `integration/product/platform/latin_font.hpp` | 26193 | COMPILED_GENERATED_INPUT | `BuildInputs/inputs/integration/product/platform/latin_font.hpp` |
| E03 | `integration/product/assets/u28fcd.hbf1` | 148 | SINGLE_GLYPH_FONT_INPUT | `BuildInputs/inputs/integration/product/assets/u28fcd.hbf1` |
| E04 | `integration/product/assets/matched28.hbf1` | 2464108 | TF_FONT_OR_FONT_GENERATION_INPUT | `BuildInputs/inputs/integration/product/assets/matched28.hbf1` |
| E05 | `integration/product/assets/rare_codepoints.json` | 831 | TF_FONT_OR_FONT_GENERATION_INPUT | `BuildInputs/inputs/integration/product/assets/rare_codepoints.json` |
| E06 | `integration/product/src/wubi_auto_safe_v1.hpp` | 128605 | COMPILED_GENERATED_INPUT | `BuildInputs/inputs/integration/product/src/wubi_auto_safe_v1.hpp` |
| E07 | `integration/product/src/ui14_font_data.cpp` | 1548935 | COMPILED_GENERATED_INPUT | `BuildInputs/inputs/integration/product/src/ui14_font_data.cpp` |
| E08 | `integration/product/dependencies/release_resources/xiaohe_base.hbo` | 13873768 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/product/dependencies/release_resources/xiaohe_base.hbo` |
| E09 | `integration/product/dependencies/release_resources/xiaohe_base.hnv` | 449536 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/product/dependencies/release_resources/xiaohe_base.hnv` |
| E10 | `integration/product/dependencies/release_resources/full_pinyin_base.hbo` | 10102232 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/product/dependencies/release_resources/full_pinyin_base.hbo` |
| E11 | `integration/product/dependencies/release_resources/full_pinyin_base.hnv` | 327680 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/product/dependencies/release_resources/full_pinyin_base.hnv` |
| E12 | `integration/product/dependencies/release_resources/release_manifest.json` | 13467 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/product/dependencies/release_resources/release_manifest.json` |
| E13 | `integration/product/dependencies/core/resources/step2f/README.md` | 9012 | HOST_TEST_SOURCE_FIXTURE | `BuildInputs/inputs/integration/product/dependencies/core/resources/step2f/README.md` |
| E14 | `integration/product/dependencies/core/resources/step2f/candidate_adoption.json` | 13123 | HOST_TEST_SOURCE_FIXTURE | `BuildInputs/inputs/integration/product/dependencies/core/resources/step2f/candidate_adoption.json` |
| E15 | `integration/product/dependencies/core/resources/step2f/source_v1_1.zip` | 51746 | HOST_TEST_SOURCE_FIXTURE | `BuildInputs/inputs/integration/product/dependencies/core/resources/step2f/source_v1_1.zip` |
| E16 | `integration/product/dependencies/core/resources/step2f/full_pinyin.json` | 6819 | HOST_TEST_SOURCE_FIXTURE | `BuildInputs/inputs/integration/product/dependencies/core/resources/step2f/full_pinyin.json` |
| E17 | `integration/product/tests/data/ui14_expected.tsv` | 1565170 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/product/tests/data/ui14_expected.tsv` |
| E18 | `release/integration_headers/wubi_auto_safe_v1.hpp` | 128605 | COMPILED_GENERATED_INPUT | `BuildInputs/inputs/release/integration_headers/wubi_auto_safe_v1.hpp` |
| E19 | `integration/resources/legacy/shared.hns` | 29349067 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/legacy/shared.hns` |
| E20 | `integration/resources/legacy/xiaohe.hnm` | 2756648 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/legacy/xiaohe.hnm` |
| E21 | `integration/resources/legacy/full_pinyin.hnm` | 2080808 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/legacy/full_pinyin.hnm` |
| E22 | `integration/resources/staged/xiaohe_base.hbo` | 13873768 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/xiaohe_base.hbo` |
| E23 | `integration/resources/staged/xiaohe_base.hnv` | 449536 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/xiaohe_base.hnv` |
| E24 | `integration/resources/staged/shared.hns` | 29349067 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/shared.hns` |
| E25 | `integration/resources/staged/full_pinyin_base.hbo` | 10102232 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/full_pinyin_base.hbo` |
| E26 | `integration/resources/staged/full_pinyin_base.hnv` | 327680 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/full_pinyin_base.hnv` |
| E27 | `integration/resources/staged/xiaohe.hnm` | 2756648 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/xiaohe.hnm` |
| E28 | `integration/resources/staged/xiaohe.hnv` | 91136 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/xiaohe.hnv` |
| E29 | `integration/resources/staged/wubi86.hnm` | 1482792 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/wubi86.hnm` |
| E30 | `integration/resources/staged/full_pinyin.hnm` | 2080808 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/full_pinyin.hnm` |
| E31 | `integration/resources/staged/wubi86.hnv` | 50176 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/wubi86.hnv` |
| E32 | `integration/resources/staged/full_pinyin.hnv` | 68608 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/full_pinyin.hnv` |
| E33 | `integration/resources/staged/shared.hnv` | 58698752 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/shared.hnv` |
| E34 | `integration/resources/staged/wubi86_base.hbo` | 7369432 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/wubi86_base.hbo` |
| E35 | `integration/resources/staged/wubi86_base.hnv` | 239616 | HOST_RESOURCE_OR_BUILD_BINDING | `BuildInputs/inputs/integration/resources/staged/wubi86_base.hnv` |
| E36 | `integration/evidence/cross_scheme.tsv` | 409 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/evidence/cross_scheme.tsv` |
| E37 | `integration/evidence/real_samples.tsv` | 186 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/evidence/real_samples.tsv` |
| E38 | `integration/evidence/auto_safe_expectations.tsv` | 686669 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/evidence/auto_safe_expectations.tsv` |
| E39 | `integration/evidence/real_sample_identities.tsv` | 1543 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/evidence/real_sample_identities.tsv` |
| E40 | `integration/evidence/parent-r1/render/host_product_v_punctuation_set.ppm` | 360110 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/evidence/parent-r1/render/host_product_v_punctuation_set.ppm` |
| E41 | `integration/evidence/parent-r1/render/host_product_v_chinese_sentence.ppm` | 360110 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/evidence/parent-r1/render/host_product_v_chinese_sentence.ppm` |
| E42 | `integration/evidence/parent-r1/render/host_product_v_english_sentence.ppm` | 360110 | HOST_TEST_FIXTURE | `BuildInputs/inputs/integration/evidence/parent-r1/render/host_product_v_english_sentence.ppm` |
| E43 | `release/B2/xiaohe_base.hbo` | 13873768 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/xiaohe_base.hbo` |
| E44 | `release/B2/xiaohe_base.hnv` | 449536 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/xiaohe_base.hnv` |
| E45 | `release/B2/shared.hns` | 29349067 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/shared.hns` |
| E46 | `release/B2/full_pinyin_base.hbo` | 10102232 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/full_pinyin_base.hbo` |
| E47 | `release/B2/full_pinyin_base.hnv` | 327680 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/full_pinyin_base.hnv` |
| E48 | `release/B2/release_pin.hpp` | 7985 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/release_pin.hpp` |
| E49 | `release/B2/xiaohe.hnm` | 2756648 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/xiaohe.hnm` |
| E50 | `release/B2/xiaohe.hnv` | 91136 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/xiaohe.hnv` |
| E51 | `release/B2/wubi86.hnm` | 1482792 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/wubi86.hnm` |
| E52 | `release/B2/full_pinyin.hnm` | 2080808 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/full_pinyin.hnm` |
| E53 | `release/B2/release_manifest.json` | 13467 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/release_manifest.json` |
| E54 | `release/B2/wubi86.hnv` | 50176 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/wubi86.hnv` |
| E55 | `release/B2/full_pinyin.hnv` | 68608 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/full_pinyin.hnv` |
| E56 | `release/B2/shared.hnv` | 58698752 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/shared.hnv` |
| E57 | `release/B2/wubi86_base.hbo` | 7369432 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/wubi86_base.hbo` |
| E58 | `release/B2/wubi86_base.hnv` | 239616 | TF_RUNTIME_AND_BINDING | `BuildInputs/inputs/release/B2/wubi86_base.hnv` |
| E59 | `reference/UI14_pins/wubi_release_v1.hpp` | 3352 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_pins/wubi_release_v1.hpp` |
| E60 | `reference/UI14_pins/release_pin.hpp` | 5796 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_pins/release_pin.hpp` |
| E61 | `reference/UI14_pins/wubi_auto_safe_v1.hpp` | 128605 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_pins/wubi_auto_safe_v1.hpp` |
| E62 | `reference/UI14_resources/xiaohe_base.hbo` | 13739184 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/xiaohe_base.hbo` |
| E63 | `reference/UI14_resources/xiaohe_base.hnv` | 445440 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/xiaohe_base.hnv` |
| E64 | `reference/UI14_resources/shared.hns` | 29323042 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/shared.hns` |
| E65 | `reference/UI14_resources/full_pinyin_base.hbo` | 9828968 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/full_pinyin_base.hbo` |
| E66 | `reference/UI14_resources/full_pinyin_base.hnv` | 318464 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/full_pinyin_base.hnv` |
| E67 | `reference/UI14_resources/xiaohe.hnm` | 2727976 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/xiaohe.hnm` |
| E68 | `reference/UI14_resources/xiaohe.hnv` | 90112 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/xiaohe.hnv` |
| E69 | `reference/UI14_resources/wubi86.hnm` | 1482792 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/wubi86.hnm` |
| E70 | `reference/UI14_resources/full_pinyin.hnm` | 2023464 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/full_pinyin.hnm` |
| E71 | `reference/UI14_resources/wubi86.hnv` | 50176 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/wubi86.hnv` |
| E72 | `reference/UI14_resources/full_pinyin.hnv` | 66560 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/full_pinyin.hnv` |
| E73 | `reference/UI14_resources/shared.hnv` | 58646528 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/shared.hnv` |
| E74 | `reference/UI14_resources/wubi86_base.hbo` | 7369432 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/wubi86_base.hbo` |
| E75 | `reference/UI14_resources/wubi86_base.hnv` | 239616 | HOST_TEST_BASELINE | `BuildInputs/inputs/reference/UI14_resources/wubi86_base.hnv` |
| E76 | `vendor/CMakeLists.upstream.txt` | 3400 | SDK_BUILD_RECIPE | `BuildInputs/inputs/vendor/CMakeLists.upstream.txt` |

File-scope clarification: E03 u28fcd.hbf1 is the separately traced Jigmo2 U+28FCD derivative under CC0. The identical small_font.hpp generation input remains traced LXGW/OFL. UI14 generator/parser are self-owned Apache-2.0 code; these file-level conclusions do not settle the mixed font or whole firmware distribution conditions.
