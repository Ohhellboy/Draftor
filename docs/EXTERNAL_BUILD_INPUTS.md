# Alpha 2.1 build inputs / Alpha 2.1 配套输入

`EXTERNAL_INPUTS.json` lists eleven required files. Install them into a fresh Source tree with `python3 -B tools/install_external_inputs.py --from ../BuildInputs/inputs`. The tool checks sizes before copying and refuses to overwrite existing inputs.

清单列出11个必需文件。按上述命令装入全新 Source 目录；安装器先检查尺寸，不覆盖现有文件。

The nine Alpha 2 inputs remain unchanged: composition and Latin font headers, UI14 data, the Wubi safety table, two body-font assets, rare-codepoint metadata, the upstream TinyUSB recipe and the fixed MSC-only archive. Alpha 2.1 adds `integration/product/src/ui16_font_data.cpp` for Find and `TestBuilds/menu-font/menu_font_data.hpp` for menus.

Alpha 2 的9项输入保持不变，本版增加 Find 文楷 UI16 数据和 MiSans 菜单字形表。菜单数据放在 TestBuilds 是既有构建约定；它仍是必需输入，不能因为该目录被 Git 忽略就漏包。

BuildInputs also retains original licenses, dictionary provenance, font sources and generators, corresponding SDK material and the optional format3 predecessor used by the format4 generator. These materials support reconstruction and attribution; the compiler does not consume all of them. The original MiSans TTF is not included. Its exact source identity and the separate status of generated menu tables are explained in [Font Distribution](FONT_DISTRIBUTION.md).

BuildInputs 另保留原许可、词库来源、字体来源及生成器、SDK对应资料和可选的format3前代输入。它们用于重建和署名，并非全部参与编译。包中不附带 MiSans 原 TTF；其来源身份及生成菜单表的分发状态见[字体分发说明](FONT_DISTRIBUTION.md)。

The TF runtime dictionaries and body font are the same as Alpha 2. Compiling Alpha 2.1 does not require regenerating them. Existing resource licensing questions remain recorded in [Resource Status](RESOURCE_STATUS.md).

TF运行词库和正文字体与Alpha 2相同，编译本版无需重建这些资源；既有许可未决事项继续见[资源说明](RESOURCE_STATUS.md)。
