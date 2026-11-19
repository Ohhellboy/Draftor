import { ESPLoader } from "../esploader.js";
import { ESP32ROM } from "./esp32.js";
import { MemoryMapEntry } from "./rom.js";
export declare class ESP32P4ROM extends ESP32ROM {
    CHIP_NAME: string;
    IMAGE_CHIP_ID: number;
    IROM_MAP_START: number;
    IROM_MAP_END: number;
    DROM_MAP_START: number;
    DROM_MAP_END: number;
    BOOTLOADER_FLASH_OFFSET: number;
    CHIP_DETECT_MAGIC_VALUE: number[];
    UART_DATE_REG_ADDR: number;
    EFUSE_BASE: number;
    EFUSE_BLOCK1_ADDR: number;
    MAC_EFUSE_REG: number;
    SPI_REG_BASE: number;
    SPI_USR_OFFS: number;
    SPI_USR1_OFFS: number;
    SPI_USR2_OFFS: number;
    SPI_MOSI_DLEN_OFFS: number;
    SPI_MISO_DLEN_OFFS: number;
    SPI_W0_OFFS: number;
    SPI_ADDR_REG_MSB: boolean;
    USES_MAGIC_VALUE: boolean;
    EFUSE_RD_REG_BASE: number;
    EFUSE_FORCE_USE_KEY_MANAGER_KEY_REG: number;
    EFUSE_FORCE_USE_KEY_MANAGER_KEY_SHIFT: number;
    FORCE_USE_KEY_MANAGER_VAL_XTS_AES_KEY: number;
    EFUSE_PURPOSE_KEY0_REG: number;
    EFUSE_PURPOSE_KEY0_SHIFT: number;
    EFUSE_PURPOSE_KEY1_REG: number;
    EFUSE_PURPOSE_KEY1_SHIFT: number;
    EFUSE_PURPOSE_KEY2_REG: number;
    EFUSE_PURPOSE_KEY2_SHIFT: number;
    EFUSE_PURPOSE_KEY3_REG: number;
    EFUSE_PURPOSE_KEY3_SHIFT: number;
    EFUSE_PURPOSE_KEY4_REG: number;
    EFUSE_PURPOSE_KEY4_SHIFT: number;
    EFUSE_PURPOSE_KEY5_REG: number;
    EFUSE_PURPOSE_KEY5_SHIFT: number;
    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT_REG: number;
    EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT: number;
    EFUSE_RD_REPEAT_DATA1_REG: number;
    EFUSE_SPI_BOOT_CRYPT_CNT_REG: number;
    EFUSE_SPI_BOOT_CRYPT_CNT_MASK: number;
    EFUSE_DOWNLOAD_MODE_XPD_ON_MASK: number;
    EFUSE_SECURE_BOOT_EN_REG: number;
    EFUSE_SECURE_BOOT_EN_MASK: number;
    PURPOSE_VAL_XTS_AES256_KEY_1: number;
    PURPOSE_VAL_XTS_AES256_KEY_2: number;
    PURPOSE_VAL_XTS_AES128_KEY: number;
    SUPPORTS_ENCRYPTED_FLASH: boolean;
    FLASH_ENCRYPTED_WRITE_ALIGN: number;
    USB_RAM_BLOCK: number;
    GPIO_STRAP_REG: number;
    GPIO_STRAP_SPI_BOOT_MASK: number;
    RTC_CNTL_OPTION1_REG: number;
    RTC_CNTL_FORCE_DOWNLOAD_BOOT_MASK: number;
    DR_REG_LPAON_BASE: number;
    DR_REG_PMU_BASE: number;
    DR_REG_LP_SYS_BASE: number;
    LP_SYSTEM_REG_ANA_XPD_PAD_GROUP_REG: number;
    PMU_EXT_LDO_P0_0P1A_ANA_REG: number;
    PMU_ANA_0P1A_EN_CUR_LIM_0: number;
    PMU_EXT_LDO_P0_0P1A_REG: number;
    PMU_0P1A_TARGET0_0: number;
    PMU_0P1A_FORCE_TIEH_SEL_0: number;
    PMU_DATE_REG: number;
    PMU_DATE_FLASH_FORCE_ON: number;
    UARTDEV_BUF_NO_USB_OTG: number;
    UARTDEV_BUF_NO_USB_JTAG_SERIAL: number;
    DR_REG_LP_WDT_BASE: number;
    RTC_CNTL_WDTCONFIG0_REG: number;
    RTC_CNTL_WDTCONFIG1_REG: number;
    RTC_CNTL_WDTWPROTECT_REG: number;
    RTC_CNTL_WDT_WKEY: number;
    RTC_CNTL_SWD_CONF_REG: number;
    RTC_CNTL_SWD_AUTO_FEED_EN: number;
    RTC_CNTL_SWD_WPROTECT_REG: number;
    RTC_CNTL_SWD_WKEY: number;
    MEMORY_MAP: MemoryMapEntry[];
    UF2_FAMILY_ID: number;
    EFUSE_MAX_KEY: number;
    KEY_PURPOSES: {
        0: string;
        1: string;
        2: string;
        3: string;
        4: string;
        5: string;
        6: string;
        7: string;
        8: string;
        9: string;
        10: string;
        11: string;
        12: string;
    };
    getPkgVersion(loader: ESPLoader): Promise<number>;
    getMinorChipVersion(loader: ESPLoader): Promise<number>;
    getMajorChipVersion(loader: ESPLoader): Promise<number>;
    getChipRevision(loader: ESPLoader): Promise<number>;
    getStubJsonPath(loader: ESPLoader): Promise<string>;
    getChipDescription(loader: ESPLoader): Promise<string>;
    getChipFeatures(loader: ESPLoader): Promise<string[]>;
    getCrystalFreq(loader: ESPLoader): Promise<number>;
    getFlashVoltage(loader: ESPLoader): Promise<void>;
    overrideVddsdio(loader: ESPLoader): Promise<void>;
    readMac(loader: ESPLoader): Promise<string>;
    getFlashCryptConfig(loader: ESPLoader): Promise<void>;
    getSecureBootEnabled(loader: ESPLoader): Promise<boolean>;
    /**
     * Get the UARTDEV_BUF_NO address based on chip revision
     * Variable .bss.UartDev.buff_uart_no in ROM .bss which indicates the port in use.
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     * @returns {number} The UARTDEV_BUF_NO address.
     */
    getUartdevBufNo(loader: ESPLoader): Promise<number>;
    /**
     * Check the UARTDEV_BUF_NO register to see if USB-OTG console is being used
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     * @returns {boolean} True if USB-OTG console is being used, false otherwise.
     */
    usesUsbOtg(loader: ESPLoader): Promise<boolean>;
    /**
     * Check the UARTDEV_BUF_NO register to see if USB-JTAG/Serial is being used
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     * @returns {boolean} True if USB-JTAG/Serial is being used, false otherwise.
     */
    usesUsbJtagSerial(loader: ESPLoader): Promise<boolean>;
    getKeyBlockPurpose(loader: ESPLoader, keyBlock: number): Promise<number | undefined>;
    isFlashEncryptionKeyValid(loader: ESPLoader): Promise<boolean>;
    /**
     * Function to be executed after chip connection
     * Sets ESP_RAM_BLOCK if USB OTG is used and disables watchdogs if needed
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     */
    postConnect(loader: ESPLoader): Promise<void>;
    /**
     * Disable watchdogs when USB-JTAG/Serial is used
     * The RTC WDT and SWD watchdog are not reset and can reset the board during flashing
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     */
    disableWatchdogs(loader: ESPLoader): Promise<void>;
    /**
     * Check SPI connection pin numbers
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     * @param {number[]} spiConnection - The SPI connection pin numbers.
     */
    checkSpiConnection(loader: ESPLoader, spiConnection: number[]): void;
    /**
     * Reset the chip using watchdog
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     */
    watchdogReset(loader: ESPLoader): Promise<void>;
    /**
     * Power on the flash chip by setting the appropriate registers
     * Required for ECO6+ when default flash voltage changed from 1.8V to 3.3V
     * @param {ESPLoader} loader - Loader class to communicate with chip.
     */
    powerOnFlash(loader: ESPLoader): Promise<void>;
}
