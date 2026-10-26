# Source integration / 源码入口

Alpha 2.1 combines Chinese Find, Chinese menus and the revised editor status display with the existing Alpha 2 engine and runtime resources. Product versions follow [VERSIONING.md](../VERSIONING.md).

Alpha 2.1 在既有Alpha 2引擎与运行资源上加入中文查找、中文菜单和编辑器状态栏。产品版本约定见上述文档。

| Entry / 入口 | Source / 实现 |
| --- | --- |
| bgIME Core / Extension / Wubi | `integration/product/dependencies/core`, `extension`, `wubi` |
| Body / 编辑器正文 | `integration/product/dependencies/common`, `platform/writing_app.cpp` |
| Find / 查找 | `dependencies/common/find_session.cpp`, `platform/writing_readers.cpp`, `src/find_display.cpp` |
| Menus and long names / 菜单与长名 | `platform/docmanager_display.hpp`, `menu_name_layout.hpp`, `menu_name_preview.hpp` |
| Editor status / 编辑器状态 | `platform/editor_hud.hpp`, `editor_geometry.hpp`, `device_main.cpp` |
| Display / 显示 | `platform/ST7306_OsptekLpm8.cpp`, `display_delta.hpp` |
| Catalog cache / 目录缓存 | `dependencies/common/src/hanime_document/document_catalog.hpp`, original 128-entry full metadata cache |
| Active resource pins / 生效资源绑定 | `release/alpha2/pins/` |
| Build / 构建 | `tools/build_candidate.py`, `integration/product/scripts/build_s3.py` |

Paths in the table are relative to `integration/product/` unless they start with `integration/`, `release/` or `tools/`. The bgIME guide points to the canonical engine; it does not contain another engine copy.

表中路径以`integration/product/`为起点，标明`integration/`、`release/`或`tools/`的除外。bgIME说明页链接到同一份权威实现，不另存引擎副本。
