# Rollback / 回退

Before installation, privately retain important plain-text exports, native TF manuscripts and state, the exact old firmware/resources, and a verified current full-flash backup when you need the previous internal state. Do not upload private backups.

安装前私下保留重要正文导出、TF 原生文稿及状态、准确旧固件/资源；需要恢复原内部状态时，还须保留经过核验的当时完整 Flash 备份。不要上传私人备份。

An App-only rollback is appropriate only if the partition table, selected slot and document-state compatibility remain unchanged. Restore the saved App to its confirmed original slot; retain the matching resources and OTA state. Do not blindly flash the Alpha 1 table onto a 4 MiB installation.

只有分区、活动槽及文档状态兼容条件未变时，才可将原 App 写回已确认的原槽，保持配套资源与 OTA 状态。不能盲写 Alpha 1 的旧分区表。

The recommended Alpha 1 to Alpha 2 or Alpha 2.1 fresh installation erases internal flash. It does not preserve the old internal data or state. An old App alone cannot restore them: recovery requires the correct complete backup and its original layout, or a clean old-version installation followed by a supported import of backed-up text. Keep TF copies separate; restoring flash alone does not restore later TF edits. Arbitrary cross-version native-document downgrade has not been fully qualified.

Alpha 1 到 Alpha 2 或 Alpha 2.1 的全新安装会清除内部 Flash，原内部数据和状态不会无损保留。仅有旧 App 不能恢复它们；需要准确完整备份及原布局，或全新安装旧版后通过支持的导入方式取回已备份正文。TF 另行备份，恢复 Flash 不能撤回之后的 TF 修改。任意跨版本原生文稿降级尚未全面验证。

See [Installation](INSTALL.md) for the backup and installation steps. There is no automatic migration or universal restore script.
