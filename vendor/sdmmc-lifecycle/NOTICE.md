# SDMMC 生命周期来源与修改

来源为 Espressif ESP-IDF v5.5.5，固定提交 `b774170ff46` 的 `components/fatfs/vfs/vfs_fat_sdmmc.c`，Apache-2.0，版权所有 2015–2024 Espressif Systems (Shanghai) CO LTD。原文件留作审计参考，不参加编译。

`integration/product/platform/board_sdmmc.cpp` 是微雪目标专用的分阶段挂载实现：沿用原版 host、card、diskio、FAT/VFS API，去掉格式化/分区、SDSPI 和全局 SDK context 注册；记录已获取的阶段，最后一次 owner 分配成功才发布。失败按 FAT→VFS→diskio→host→card 顺序回滚。已释放的句柄不保留；清理错误锁存并停止本次启动的挂载重试。FatFs 或 host 拒绝释放时，仍被引用的对象仅保留一份，隔离到复位，不暴露给产品。

固定 SDK 的 `esp_vfs_fat_unregister_path` 可能先释放 FAT context 再报错，状态机按已消费处理；不会用错误码推断对象仍然有效。没有调用 SDK 的 `esp_vfs_fat_sdmmc_mount` 或 `esp_vfs_fat_sdcard_unmount`，也没有修改安装 SDK 或原目标 SD 实现。

测试编译实际生产挂载实现，只有 Host 测试可替换两次任务内分配以注入故障。底层 SDK 的内部 assert、硬件中断和电气错误不被概括为“所有 OOM/实机错误均可恢复”。
