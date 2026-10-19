# Alpha 2 external inputs / Alpha 2 配套输入

`EXTERNAL_INPUTS.json` is the authoritative install list. Run `tools/install_external_inputs.py --from ../BuildInputs/inputs`; it validates all sizes before copying and refuses overwriting existing inputs. None are fetched silently.

准确安装清单位于仓库根，安装前统一检查尺寸、不覆盖现有文件、不暗中下载。

The nine inputs are the composition font header, Latin font header, UI14 font source, Wubi generated safety table, two font assets, rare-codepoint metadata, upstream TinyUSB CMake source list and the exact MSC-only TinyUSB archive. They are retained generated/original inputs, not nine newly licensed works. Font/data provenance and unresolved D01/D04/D08 issues are inherited from Alpha 1.

9项为组合字体、拉丁字体、UI14生成源码、五笔生成表、两项字体资产、生僻字元数据、TinyUSB上游构建列表及准确 MSC-only库；对应原来源和既有未决事项，不另行推定许可。

The bundle also carries original font sources/recipes, dictionary contributions, licenses and corresponding SDK/library sources retained from Alpha 1, plus the minimal format3 predecessor input for `generate_format4.py`. These are for provenance, redistribution and optional resource generation; they are not all read by the S3 compiler. No installed SDK, old build cache, private flash image or previous research stage is included.

另附原字体/配方、词库贡献记录、许可及 SDK/库对应源码，以及 format4 生成所需的最小前代输入。它们用于来源、再分发与可选资源重建，并非每项都参与编译；不附已安装SDK、旧缓存、私人Flash或研究stage。

The actual S3 dependency receipt is produced by the public build. For the fixed input identity and pin set see [RELEASE_PAIR.json](../RELEASE_PAIR.json); licensing detail is in [Resource Status](RESOURCE_STATUS.md).
