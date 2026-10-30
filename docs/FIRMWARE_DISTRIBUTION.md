# Alpha 2.2 firmware and packages / 固件与配套包

The only proposed new device firmware is WAVESHARE_RLCD42. Its authoritative [manifest](../release/alpha2.2/WAVESHARE_RLCD42.json) binds the production commit, App, private ELF identity and exact four files; [qualification](ALPHA2_2_VALIDATION.md) determines release readiness. The original board stays on the existing Alpha 2.1 release. [Historical Alpha 2.1 packaging](FIRMWARE_DISTRIBUTION_ALPHA2_1.md) is retained, not reused as the Waveshare authority.

Public Firmware includes the four files, explicit-target installer, manifest, guides and applicable notices. Clean Source is a Git-tracked archive. Existing [Alpha 2.1 BuildInputs and Resources](../release/alpha2.2/INPUT_REFERENCES.json) are referenced unchanged, not regenerated or silently replaced. Private ELF, Flash/NVS, manuscripts, serial identity, raw logs and SDK/cache are excluded.

The production image is not rebuilt after its device qualification for document-only changes. Source release/tag commits may add docs; the manifest keeps the exact production commit. The final finite device round passed with the exact manifest App; the GitHub release page determines actual publication status. Source-only updates do not imply another tested binary.

公开目标、资源和许可证分别见 [INSTALL](INSTALL.zh-CN.md)、[RESOURCE_STATUS](RESOURCE_STATUS.md)、[FONT_DISTRIBUTION](FONT_DISTRIBUTION.md) 和 [THIRD_PARTY](../THIRD_PARTY.md)。旧版权来源未决项如实保留，不把本次封包解释为新增许可。
