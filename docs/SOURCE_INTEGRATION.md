# Source integration — v0.1.0-alpha.1

This release is an incremental commit on the existing public repository, based on main `7e3c8e2a6032ff6ea06335c41a1b4069922e98f7`. Its 12 existing commits remain ancestors. No empty-repository FIRST_COMMIT patch is applied, history rewritten or force-push used.

The original Alpha candidate has 991 files. All paths are included at the repository root. The only overlap with the 18-file public baseline is README.md: Alpha navigation is inserted while the complete original bilingual author text and prototype-photo link remain. The other 17 existing files, including all 15 display/st7306 files, HARDWARE.md and draftor-prototype.jpg, are retained byte-for-byte.

Only release documentation, notices and administrative/version metadata change relative to the 991-file candidate. Product/Core/driver/tool implementation, accepted resource bytes, generated font data, pins and firmware are unchanged. The existing display/st7306 tree keeps its independent MIT and upstream declarations; the new Alpha self-owned code uses root Apache-2.0 with third-party exceptions.

The updated Source ZIP contains the complete tracked integrated tree under `Draftor/`, including the pre-existing module, photo, original hardware overview and new Alpha navigation. It is intentionally larger than the old 991-file Source ZIP and is suitable for offline source browsing/building with the paired BuildInputs and documented fixed toolchain. Git history remains in the repository; `.git` and private preparation/audit records are excluded from the ZIP.

Resources, BuildInputs and Firmware ZIPs are the existing complete supplemental candidates, reused without repacking. Older administrative metadata embedded in them is historical; this release's root RELEASE_PAIR.json, PUBLICATION_PLAN.json and current license/status documentation provide the integration/version context. Original technical constants and existing file inventories remain applicable within their recorded scope.

The original ST7306 README references a DEVICE_SMOKE_TEST.md file already absent from the 18-file main baseline. This pre-existing link is recorded, not invented or represented as new device validation. The original module documentation remains unchanged.
