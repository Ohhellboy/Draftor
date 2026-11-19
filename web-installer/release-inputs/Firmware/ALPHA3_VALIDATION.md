# Waveshare Alpha 3 qualification / 实机验收

The released ESP32-S3 application is 4,021,152 bytes, SHA-256 `165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd`. The target is the Waveshare ESP32-S3-RLCD-4.2 with ST7305 and 180° rotation. [Firmware manifest](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.3/release/alpha3/WAVESHARE_RLCD42.json)

| 检查 / Check | 结果 / Result |
| --- | --- |
| 兼容更新 / Compatible update | Same-board identity, live partition table and OTA selection checked. Verified backup before writing the active App. A separate readback matched the distributed App byte for byte. Flash outside the active slot and saved pairing data matched the pre-install backup. |
| 普通写作 / Normal writing | After a normal boot, the operator entered English and Chinese text using one registered keyboard and observed “已保存，已连接”. No slot or attempt number appeared. The operator accepted this exact App. |
| 键盘轮换 / Keyboard rotation | Three registered keyboards previously entered text and switched on the R3 BLE baseline. The subsequent HUD edit removed the visible numbers. The final build changed the SDK source-path mapping at compile time. The exact final App received the single-keyboard writing check above. |
| 全新空板 / Fresh empty board | Installer orchestration tested offline for the fresh and both active-slot update paths. The three generic startup files match the preceding public Waveshare distribution. Physical blank-board first installation remains untested. |
| 资源 / Resources | Verified the 14 pinned format4 files and body font from the historical asset. The two generic learning files match the compiled relation digest and Merkle root and can be regenerated from the public inputs. |

[安装](INSTALL.zh-CN.md) · [Installation](INSTALL.md) · [资源清单](https://github.com/Ohhellboy/Draftor/blob/v0.1.0-alpha.3/release/alpha3/TF_RESOURCES.json)
