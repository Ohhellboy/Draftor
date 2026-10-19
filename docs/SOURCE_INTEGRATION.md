# Alpha 2 source integration / Alpha 2 源码整合

The local preparation starts from public `main` commit `9c91bf03177c08377e99d1b49faf2d7462e906cd`, retaining its history and Alpha 1 tag. Cumulative business source comes from the exact build used in the paired comparison and FW1. Packaging changes are explicit build paths, external input selection, generation entrypoints, three active format4 pins and documentation; no query, output, candidate, commit or document-storage semantics were changed. The later explicit author decision disables only input observation and changes the release build label.

本地副本保留原公开历史，继承准确实测累计业务源码。本轮仅处理构建路径、输入定位、生成入口、生效 pin与文档，不改查询、取字、候选、提交或文档语义。

| Entry / 入口 | Source / 实现 |
|---|---|
| Core and shared authority | `integration/product/dependencies/core/src/core.cpp`, `paged_shared_resource.cpp` |
| B block lookup | Core `base_order.cpp`, `base_order_internal.hpp` |
| Verified output handoff | Core `bounded_output.cpp`, `shared_structural.hpp` |
| Candidate row consumer | `integration/product/src/candidate_window.cpp`, `device_ime.cpp` |
| Complete presentation and body acceptance | `src/presentation_frame.hpp`, `platform/device_main.cpp`, `platform/writing_app.cpp` under product |
| Synchronous delta transport | `integration/product/platform/ST7306_OsptekLpm8.cpp`, `display_delta.hpp` |
| Active format4 trust material | `release/alpha2/pins/` and compiler `.d` receipt |
| Resource generation/validation | `tools/generate_format4.py`, `tools/resources/short_text.py`, `structural.py`, Core `verified_resource.cpp`, `packed_auth.cpp` |
| Host and S3 entry | `tools/check_alpha2.py`, `tools/build_candidate.py` |

`ime/bgime` links to the canonical Core instead of carrying a second copy. Source distribution is not a statement that external patent/source review has passed. The prior private review package and manuscripts are not part of the public tree.

bgIME入口不再复制一份Core；源码公开交付不等于外部专利/源码审查通过，原私下审查包及稿件不在这里。
