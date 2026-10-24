# Resources & Licensing Status / 资源来源与许可情况

**Version / 适用版本：Draftor Alpha 2 · v0.1.0-alpha.2**

[Project](../README.en.md) / [项目主页](../README.md) · [Build / 源码构建](BUILD.md) · [License Inventory / 许可证清单](LICENSE_INVENTORY.md) · [Firmware Distribution / 固件分发](FIRMWARE_DISTRIBUTION.md)

Draftor includes Chinese dictionaries, character indexes, fonts and other resources developed or adapted from multiple sources. Different parts of these resources may be covered by different licenses.

Draftor 包含中文词库、字符索引、字体，以及来自不同来源、经过整理或改编的其他资源。这些材料的不同部分可能适用不同的许可证。

This document explains what is known about their origins, which materials are provided, and which redistribution questions remain unresolved.

本页说明这些资源目前已确认的来源、随发行提供的材料，以及尚未明确的再分发问题。

**Identifying a source is not the same as establishing permission to redistribute its derivatives. Equally, an unresolved licensing question does not, by itself, establish infringement.**

**确认材料的来源，不等于已经确认其派生作品的再分发权利；反过来，许可问题尚未明确，也不等于已经认定存在侵权。**

## 1. Current resource set / 当前资源

Alpha 2 uses format4 `B_SHORT_TEXT_20261018`, bundle `17304919664082782091`: 14 runtime files, 132,153,747 bytes, plus unchanged `matched28.hbf1` (2,464,108 bytes). It inherits the lexical identities and text content of the earlier B2 resource set; physical query, record and proof layout changed. The exact inventory/pins are [here](../release/alpha2/RESOURCE_MANIFEST.json).

本版使用 format4，共14个运行文件及原字体；继承原词条身份与文字，仅物理查询、记录及证明布局改变。原 B2 的242,793个继承身份、186个新增身份及4,157个接受映射目标属于既有来源记录，不是本轮新增词条。

Original materials and disclosures below are retained from Alpha 1. BuildInputs carries the corresponding fonts, dictionaries, source contributions and SDK/library materials needed to explain or redistribute the actual inputs. Current file mapping is in `RELEASE_PAIR.json`; old lists retained in provenance materials describe their original versions, not the new format4 byte layout.

以下详细来源与未决披露沿用 Alpha 1。BuildInputs保留原对应材料；历史清单描述当时版本，不能当作新format4的文件摘要。

## 2. Dictionary Sources / 词库来源

### Wordforms / 词形

The existing lexical identities include wordforms traced to several sources:

现有词条身份中的词形可以追溯到以下来源：

| Source / 来源 | Recorded entries / 记录数量 |
|---|---:|
| Rime `essay-simp` | 178,366 |
| Combined `essay` + `pinyin-simp` material | 61,593 |
| Rime `pinyin-simp` | 2,832 |
| Project compatibility entries / 项目兼容词条 | 2 |

The original Rime source records and available related materials are supplied with the release.

本次发行提供相应的 Rime 原始来源记录及现有配套材料。

The `essay-simp` material is associated with LGPL-v3, while `pinyin-simp` is associated with Apache-2.0. Their original notices and conditions remain applicable.

`essay-simp` 材料对应 LGPL-v3，`pinyin-simp` 对应 Apache-2.0。原有版权声明及许可条件继续适用。

**The origin of a written wordform does not automatically establish the origin or licensing terms of its pronunciation, character form or ranking weight.**

**确认词形来源，并不意味着已经确认该词的读音、字形或排序权重具有相同来源和许可条件。**

### Readings / 读音

The available pronunciation materials include the original Full Pinyin aligned tables and generator, together with fixed `pinyin-simp`, `luna-pinyin` and CC-CEDICT inputs.

现有读音材料包括原始全拼对齐表及生成程序，以及固定版本的 `pinyin-simp`、`luna-pinyin` 和 CC-CEDICT 输入。

These materials involve LGPL-v3, Apache-2.0 and CC BY-SA 4.0 terms, depending on their source.

根据具体来源，这些材料涉及 LGPL-v3、Apache-2.0 及 CC BY-SA 4.0 等许可。

Some historical source-row identities are no longer available. The exact licensing treatment of certain mixed or derived resource files therefore remains unresolved.

部分历史来源行的原始身份记录已经不可得，因此某些混合或派生资源文件的具体许可适用范围仍未完全确认。

### Character Forms & Ranking Weights / 字形与排序权重

The resource records distinguish several types of character-form information, including simplified-character fallback, preserved S05 compatibility data, and project-owned compatibility mappings.

资源记录区分不同的字形信息，包括简体字回退、保留的 S05 兼容数据，以及项目自行建立的兼容映射。

The recorded Base weight data includes:

Base 排序权重的现有记录包括：

- 192,793 policy-assigned zero weights / 根据规则设置为零的权重
- 50,000 values from M0 / 来源于 M0 的权重值

The recorded Xiaohe Shuangpin weights include:

小鹤双拼的现有权重记录包括：

- 85,909 policy-assigned zero weights / 根据规则设置为零的权重
- 6,457 Pinyin-route maxima / 根据拼音路径取得的最大值
- 51,224 inherited values with recorded M0/M3 associations / 保留 M0/M3 来源关联的继承值

The original Essay ranking weights were not directly projected into these values.

这些数值并非直接由原始 Essay 词频权重整体映射得到。

Some early seed policies and upstream corpus-generation details are not fully documented. However, this does not mean that every inherited weight lacks a known source.

部分早期初始策略及上游语料生成细节尚未完整记录，但不能因此认定所有继承权重都没有可追溯来源。

---

## 3. Font Sources / 字体来源

The released fonts contain glyphs derived from identifiable upstream font projects.

本次发行使用的字体包含可追溯至不同上游字体项目的字形。

The recorded `matched28` font binding includes:

`matched28` 的已有字形来源记录包括：

- **20,206** glyphs associated with LXGW WenKai / 关联霞鹜文楷的字形
- **328** glyphs associated with Ysabeau / 关联 Ysabeau 的字形

The release also records the Latin98 font set, 256 non-CJK UI14 glyphs, and three composition-font profiles with corresponding font or generator sources.

此外，现有记录还包含 Latin98 字体集、256 个 UI14 非汉字字形，以及三套具有对应来源或生成记录的组合字体配置。

The relevant original fonts and Open Font License (OFL) notices are supplied.

相应原始字体及 Open Font License（OFL）声明随发行提供。

A separately traced rare-character contribution from Jigmo2 is associated with CC0, including the `u28fcd.hbf1` resource for U+28FCD.

另有一项可独立追溯的 Jigmo2 生僻字字形贡献，采用 CC0；其中包括用于 U+28FCD 的 `u28fcd.hbf1` 文件。

The available records establish provenance for the identified font portions. They should not be taken as proof that every historical mixed-font generation step can be recreated exactly.

现有记录可以说明已识别字体部分的来源，但不代表全部历史混合字体生成过程都能精确复现。

No font replacement or rerasterization is part of this documentation update.

本次文档更新不涉及替换字体或重新生成字形。

---

## 4. Wubi 86 / 五笔86

The current Alpha includes Wubi 86 input support.

当前 Alpha 已集成五笔86输入。

The available source materials include the fixed `rime-wubi` dictionary, its original authorship and license declarations, editable conversion inputs, conversion code, neutral mapping weights and corresponding bitmap bindings.

现有来源材料包括固定版本的 `rime-wubi` 词典、原始作者及许可证声明、可编辑的转换输入、转换代码、中性映射权重，以及配套的位图资源关联。

The original `rime-wubi` licensing declarations are retained, including the applicable LGPL material.

保留了 `rime-wubi` 原有的许可声明，包括相关 LGPL 材料。

Wubi-specific source materials and the shared lexical resources should be considered separately when evaluating licensing requirements. The shared resources may contain material from additional sources.

评估许可条件时，需要区分五笔专用来源材料与输入法共用词库。共用词库还可能包含其他来源的数据。

---

## 5. Resource Generation / 资源生成

The release provides available original inputs, editable tables, transformation policies and existing generators used in the project's resource preparation.

本次发行提供现有原始输入、可编辑的数据表、转换策略及资源生成程序。

However, full historical regeneration is not yet available for every mixed dictionary and font.

但部分历史混合词库及字体尚无法完全从原始材料重新生成。

In particular, the exact historical **M4 v0.1.1** generator revision is unavailable. An earlier **v0.1** revision has been identified, but the two are not treated as identical.

其中，历史 **M4 v0.1.1** 生成程序的准确版本目前不可得。已有较早的 **v0.1** 版本记录，但不能将两者视为相同。

This limitation concerns reconstruction of historical resource-generation processes. The current Alpha's generated runtime resources are supplied separately.

这一限制针对历史资源生成流程的复现，并不表示当前 Alpha 缺少运行所需的已生成资源。

For the available build inputs, see [External Build Inputs](EXTERNAL_BUILD_INPUTS.md).

已提供的构建输入详见[外部构建输入](EXTERNAL_BUILD_INPUTS.md)。

---

## 6. Firmware Dependencies / 固件依赖

The original tested firmware's link analysis identified contributing members from **71 libraries**.

原实测固件的链接分析识别出 **71 个库**的参与成员。

Corresponding fixed source materials, component records and notices are supplied for the identified scope, including applicable runtime exceptions and MPL-related materials.

对于已识别的组件范围，本次发行提供相应固定版本源码、组件记录及声明，其中包括适用的运行时例外条款和 MPL 相关材料。

These components do not share one universal license. Their applicable terms remain attached to their respective materials.

这些组件并不共用同一个许可证，各自适用的条款仍需分别遵守。

See [Linked Components](LINKED_COMPONENTS.json), [License Inventory](LICENSE_INVENTORY.md) and [Firmware Distribution](FIRMWARE_DISTRIBUTION.md).

具体见[链接组件清单](LINKED_COMPONENTS.json)、[许可证清单](LICENSE_INVENTORY.md)及[固件分发说明](FIRMWARE_DISTRIBUTION.md)。

---

## 7. Outstanding Licensing Questions / 尚未解决的许可问题

Three specific matters are identified in the current distribution record.

目前的发行记录中，以下三项具体问题仍待明确。

### A. Mixed Dictionary Licensing / 混合词库许可

The shared input-method resources combine contributions associated with LGPL-covered Essay/Luna materials and CC BY-SA 4.0 CC-CEDICT readings.

输入法共用资源包含来自适用 LGPL 的 Essay/Luna 材料，以及适用 CC BY-SA 4.0 的 CC-CEDICT 读音材料。

These contributions appear in shared HNS resources, scheme HNM resources and derived HBO data.

这些贡献涉及共用 HNS 资源、输入方案 HNM 资源以及派生 HBO 数据。

The remaining questions concern the exact scope of adaptation or collection, the appropriate file-level terms and the sufficiency of corresponding source materials.

尚待确认的问题包括改编或汇编的具体范围、各文件应适用的许可条件，以及配套来源材料是否充分。

The release supplies original data, source attribution, editable tables and available generators. Original author records, copyright notices, licenses, source links and modification notices should be retained.

发行包提供原始数据、来源署名、可编辑表格及现有生成程序。相关作者信息、版权声明、许可证、来源链接和修改说明应予保留。

CC BY-SA 4.0 has an official compatible-license mechanism involving GPLv3, but this does not automatically determine the license of every mixed resource or of bgIME Core.

CC BY-SA 4.0 存在涉及 GPLv3 的官方兼容许可机制，但这并不自动决定所有混合资源文件或 bgIME Core 的许可证。

References / 参考：

- [CC BY-SA 4.0 legal code](https://creativecommons.org/licenses/by-sa/4.0/legalcode)
- [Creative Commons compatible licenses](https://creativecommons.org/compatible-licenses/)

### B. Espressif Build Recipe / Espressif 构建配置授权

A fixed upstream build recipe is included in the BuildInputs materials:

BuildInputs 包含一份固定版本的上游构建配置：

- File / 文件：`components/arduino_tinyusb/CMakeLists.txt`
- Upstream / 上游：`esp32-arduino-lib-builder`
- Commit：`ee57070cc8571c488a64b8ef028684acdc6ff5e2`
- Git blob：`5107ca4a13033219917909ff99c84d3ad449cea0`
- Size / 大小：3,400 bytes

The file and its upstream identity have been recorded, but a redistribution grant specifically covering this material has not been established.

目前已经记录该文件及其上游版本身份，但尚未确认明确适用于这一材料的再分发授权。

The file is present in the supplied BuildInputs materials, including the original recipe and a copy within the fixed upstream source archive.

该文件已经包含在随发行提供的 BuildInputs 材料中，包括原始构建配置及固定上游源码归档中的副本。

**Availability of the file is not evidence of permission from Espressif.**

**文件已经提供，不等于取得了 Espressif 的再分发授权。**

### C. Historical Dictionary Reference Records / 历史辞典参考记录

A historical Host test fixture contains **18 short reference records** associated with Taiwan Ministry of Education dictionary materials:

一份历史 Host 测试夹具包含 **18 条**与台湾教育部辞典材料有关的简短参考记录：

- 14 from concise-dictionary materials / 14 条来自《国语辞典简编本》相关材料
- 4 from variant-character dictionary materials / 4 条来自异体字字典相关材料

The records consist of short linguistic relations or meaning conditions and project-authored notes. The recorded scope does not include copied full dictionary definition paragraphs, images or fonts.

现有记录属于简短的语言关系、义项条件以及项目自行撰写的说明，不包含整段复制的辞典释义、图像或字体。

It has not yet been established whether any of these records fall within protected expression or constitute adaptations subject to additional conditions.

目前尚未确认这些记录是否涉及受保护的表达，或者是否构成需要遵守额外条件的改作。

The concise dictionary's CC BY-ND 3.0 Taiwan and official usage conditions must be distinguished from the variant-character dictionary's separate rights notices.

《国语辞典简编本》的 CC BY-ND 3.0 台湾及官方使用条件，需要与异体字字典的独立权利声明区分处理。

The affected materials include historical `source_v1_1.zip` fixture copies in BuildInputs and corresponding `HOST_FIXTURE_SCOPE.json` records across the companion archives.

受影响范围包括 BuildInputs 中历史 `source_v1_1.zip` 测试材料的副本，以及多个配套发行包中的 `HOST_FIXTURE_SCOPE.json` 记录。

This issue concerns historical Host reference fixtures. It is not identified as a requirement to replace the current runtime dictionary payload.

这一问题涉及历史 Host 参考测试材料，现有记录并未将它认定为必须替换当前运行词库的原因。

References / 参考：

- [MOE concise dictionary usage conditions / 教育部简编本使用说明](https://language.moe.gov.tw/001/Upload/Files/site_content/M0001/respub/conciseddict_10312.pdf)
- [Variant dictionary copyright / 异体字字典版权说明](https://dict.variants.moe.edu.tw/page.jsp?ID=12)

---

## 8. Distribution & Attribution / 发行与署名

Draftor-owned source code is released under Apache-2.0. The independently published ST7306 display module retains its MIT license. Third-party resources retain their own applicable terms.

Draftor 自有源码采用 Apache-2.0；独立公开的 ST7306 显示模块继续使用 MIT 许可证；第三方资源则保留各自适用的许可条件。

The released archives include available source files, author records, license texts, attribution information and modification records for the identified materials.

对于已识别的材料，发行包提供现有源码、作者记录、许可文本、来源署名及修改记录。

Unresolved provenance or permission is recorded as unresolved. It is not treated as a public-domain declaration, an automatic Apache-2.0 grant, or a confirmed infringement finding.

来源或授权尚未明确的部分，继续按尚未确认处理。它既不意味着相关内容自动属于公有领域或适用 Apache-2.0，也不等于已经认定存在侵权。

For the detailed file inventory and licensing references, see:

完整文件清单及许可资料见：

[Resource Files / 资源文件清单](RESOURCE_FILES.json) · [License Inventory / 许可证清单](LICENSE_INVENTORY.md) · [Third-Party Notices / 第三方声明](../THIRD_PARTY.md) · [Firmware Distribution / 固件分发](FIRMWARE_DISTRIBUTION.md)
