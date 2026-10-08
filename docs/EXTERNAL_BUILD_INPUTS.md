# External inputs / 外部输入

All **76/76** entries of the original [EXTERNAL_INPUTS.json](../EXTERNAL_INPUTS.json) are provided unchanged in the same-release **Draftor_TF20_Alpha_BuildInputs.zip**, under `BuildInputs/inputs/<path>`. They are installed into a fresh source copy with the existing explicit installer. No UNKNOWN batch is filtered. The inherited manifest's old reason strings describe the previous preparation policy; this document and the [per-file disposition](EXTERNAL_INPUT_DISPOSITIONS.json) give the current plan.

| Target | Material readiness and limits |
|---|---|
| Tested-hardware app update / first installation | Original app, bootloader, partitions, boot_app0 and full TF resource layout prepared; installation actions not executed this round |
| S3 compilation | All 76 project inputs supplied; fixed official SDK/compiler installation is still required; previous clean build evidence reused |
| Portable 224 Host checks | All listed resource/fixture inputs supplied, including seven historical Host fixtures; no claim that all pass (16 inherited failures) |
| Internal historical 230 checks | Six private-workspace historical checks remain outside the public reproduction claim; see BUILD |
| Upstream recreation of every mixed font / lexicon | Not complete: unidentified legacy source inputs/transform chains remain; this does not remove existing generated or runtime bytes |

The seven `integration/evidence/` entries are synthetic/recorded Host fixtures, not device logs or the TF runtime dictionary. `reference/UI14_resources` and `reference/UI14_pins` are old Host comparison inputs, not the active runtime release. `step2f/source_v1_1.zip` is the exact historical Core source fixture, including its self-contained validation record. Nested records are identified in the file manifest and are not device/audit-chat logs.

Additional fixed original LXGW and Jigmo outlines, UI14 parent glyph dump, generator and parser are supplied beside the 76 inputs. Older Latin/composition outline identity, complete historical regeneration chain and dataset redistribution evidence remain unresolved. The supplied Step2A Wubi audit scripts are provenance source references with historical dependencies, not a claim of a complete portable B2-regeneration command.

The SDK recipe is supplied unchanged despite its unconfirmed distribution evidence; see RESOURCE_STATUS. SDK binaries/toolchain installation is an official fixed-version prerequisite, not a hidden private build input. Firmware distribution obligations are separately tracked in [FIRMWARE_DISTRIBUTION](FIRMWARE_DISTRIBUTION.md).

中文：全部76项已落实到同批附件；运行、生成输入、Host夹具和历史来源材料分开记账。提供材料不等于取得授权，S3可编译也不等于所有资源都能从已知上游重新生成。

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
