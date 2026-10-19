# Rollback / 回退

Keep the exact previous App, resource directory, actual partition table, OTA selection and needed device state privately. An App-only rollback is applicable only while those layout/selection conditions remain compatible; restore the previous App to its confirmed original slot and keep its resources. Do not blindly flash Alpha 1's 2 MiB table over a 4 MiB installation.

回退需共同保留准确原App、资源、实际分区、OTA选择与必要状态。布局和选择条件仍兼容时才按原槽恢复App；不能盲写旧2 MiB分区表。

The current document codec is inherited, and same-version saved/reopened body equality was checked. Arbitrary cross-version downgrade and changed FFat migration are not qualified. Preserve plain-text exports as well as native manuscript storage. No universal erase/restore script is supplied. See [Installation](INSTALL.md).
