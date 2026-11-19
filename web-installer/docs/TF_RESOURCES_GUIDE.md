# TF 卡资源准备 / TF resource guide

适用于微雪 ESP32-S3-RLCD-4.2 · 固件 v0.1.0-alpha.3。
For Waveshare ESP32-S3-RLCD-4.2 with firmware v0.1.0-alpha.3.

首次准备卡时下载两包；已有匹配资源可继续使用，不需要为了更新固件而重做 TF 卡。基础包沿用 alpha.2.1，两包共同对应 alpha.3。学习索引是公开通用资源，不是个人学习记录。
Download both archives for a new card. Keep matching resources on an existing card. The alpha.2.1 base archive and alpha.3 index together support alpha.3 firmware. The public index is not personal learning data.

| 原包 / Archive | 字节 / Bytes | SHA-256 |
| --- | ---: | --- |
| [Draftor_v0.1.0-alpha.2.1_Resources.zip](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip) | 60,546,074 | 460096846036bd01c941bc07ab98e6db4e130a247563e44fa3cb01d12a9c6636 |
| [Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.3/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip) | 1,831,828 | 05da4128d132c0673ae28bf9a1f35d3997a017cd1995d51d38051dc15f55c3eb |

下载来自固定 GitHub Release；速度和可用性取决于网络，不使用第三方镜像。保留原包文件名、README、NOTICE 和 LICENSE。解压后按发行 TF_RESOURCES.json 核对 18 个运行文件，共 138,323,109 字节（约 138.3 MB），不含文稿和个人学习数据。
Downloads use fixed GitHub Release assets. Network availability and speed vary. Keep original names and license notices. The runtime manifest lists 18 files totaling 138,323,109 bytes; ZIP sizes differ and personal files are excluded.

| 包内目录 / In ZIP | TF 根目录 / At card root |
| --- | --- |
| 基础包 / Base: Resources/HANIME/ | HANIME/ |
| 学习索引 / Index: Resources/WAVESHARE_R7_L1_AREV1_RES_20261112/ | WAVESHARE_R7_L1_AREV1_RES_20261112/ |

仅合并需要的资源子目录，不替换整个 HANIME。不要把 ZIP 外层文件夹或 Resources/ 作为额外父目录。HANIME 是固件兼容路径，不改名为 Draftor 或 bgIME。
Merge only the required resource subdirectories, without replacing the whole HANIME folder. Do not add a ZIP wrapper or Resources/ parent. Keep the firmware compatibility name HANIME.

- HANIME/IME/B_SHORT_TEXT_20261018/：14 个 format4 文件及 CANDIDATE_READY.json。
- HANIME/IME/R1_API080/matched28.hbf1：字体 / body font。
- WAVESHARE_R7_L1_AREV1_RES_20261112/：relations.bgr 与 relations.hnv 均需要 / both are required。

保留所有文稿、个人学习目录及其他文件。不格式化、不清空卡，不一律覆盖。发现同名资源有差异，先保留原件、核对版本，只处理发行清单明确的资源文件。
Keep all documents, personal learning directories and other files. Never format, clear or blanket-overwrite the card. Preserve mismatching same-name files, check versions, and change only listed runtime resources.

文稿路径：WAVESHARE_R7_L1_AREV1_DOC_TEST_20261112/HANIME_S3_TEST/v01/。
个人学习路径：WAVESHARE_R7_L1_AREV1_TEST_20261112/，与公开的 *_RES_* 资源目录不同。
保留这些完整目录、状态银行及合法标记。下载资源或更新不会自动迁移旧文稿和学习记录，不删除旧命名空间。
Documents and personal learning use the paths above. Preserve their full contents, state banks and valid markers. Downloads and updates do not migrate old state; keep old namespaces.

网页不访问、选择、格式化或写入 TF。缓存包和临时目录的校验不代表读取了实际卡。原 TF 备份的 18 项资源已匹配；若卡上资源自备份后未改变，可沿用，这不是本轮重新读卡的结论。
This site does not access, select, format or write TF. Archive verification does not inspect a physical card. Previously verified matching card resources may be reused if unchanged.

固件更新显示 VERIFIED_BOOT_PENDING 且所有独立写后检查成功后，完全断电、插回原 TF、松开 BOOT 普通启动。仍需实际验收键盘、中文和英文输入、旧稿抽查、新稿保存及重启后读取。
After verified firmware content and all independent post-write checks, power off fully, insert the prepared card and boot with BOOT released. Verify keyboard/input, an old document, a new save, and persistence after restart.

网页完整 18 项路径／长度／SHA-256 清单：docs/TF_GUIDE.html；机器清单：firmware/TF_RESOURCES.json。发行原文：docs/INSTALL.zh-CN.md / docs/INSTALL.md。
