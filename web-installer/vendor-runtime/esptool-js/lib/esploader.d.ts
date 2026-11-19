import { Transport } from "./webserial.js";
import { ROM } from "./targets/rom.js";
import { ResetStrategy } from "./reset.js";
import { LoaderOptions } from "./types/loaderOptions.js";
import { FlashOptions } from "./types/flashOptions.js";
import { After, Before } from "./types/resetModes.js";
import { FlashFreqValues, FlashModeValues, FlashSizeValues } from "./types/arguments.js";
import { SecurityInfo } from "./types/securityInfo.js";
/**
 * Callback function type for handling packets received during flash memory read operations.
 * @callback FlashReadCallback
 * @param {Uint8Array} packet - The data packet received from the flash memory.
 * @param {number} progress - The current progress of the read operation in bytes.
 * @param {number} totalSize - The total size of the data to be read in bytes.
 */
export type FlashReadCallback = ((packet: Uint8Array, progress: number, totalSize: number) => void) | null;
/** Espressif USB vendor ID used by native USB-Serial/JTAG and USB-OTG. */
export declare const ESPRESSIF_VID = 12346;
/** Default USB product ID for Espressif USB-Serial/JTAG. */
export declare const USB_JTAG_SERIAL_PID = 4097;
export declare class ESPLoader {
    ESP_RAM_BLOCK: number;
    ESP_FLASH_BEGIN: number;
    ESP_FLASH_DATA: number;
    ESP_FLASH_END: number;
    ESP_MEM_BEGIN: number;
    ESP_MEM_END: number;
    ESP_MEM_DATA: number;
    ESP_WRITE_REG: number;
    ESP_READ_REG: number;
    ESP_SPI_ATTACH: number;
    ESP_CHANGE_BAUDRATE: number;
    ESP_FLASH_DEFL_BEGIN: number;
    ESP_FLASH_DEFL_DATA: number;
    ESP_FLASH_DEFL_END: number;
    ESP_SPI_FLASH_MD5: number;
    ESP_GET_SECURITY_INFO: number;
    ESP_ERASE_FLASH: number;
    ESP_ERASE_REGION: number;
    ESP_READ_FLASH: number;
    ESP_RUN_USER_CODE: number;
    ESP_IMAGE_MAGIC: number;
    ESP_CHECKSUM_MAGIC: number;
    ROM_INVALID_RECV_MSG: number;
    DEFAULT_TIMEOUT: number;
    ERASE_REGION_TIMEOUT_PER_MB: number;
    ERASE_WRITE_TIMEOUT_PER_MB: number;
    MD5_TIMEOUT_PER_MB: number;
    CHIP_ERASE_TIMEOUT: number;
    FLASH_READ_TIMEOUT: number;
    MAX_TIMEOUT: number;
    WRITE_BLOCK_ATTEMPTS: number;
    WRITE_BLOCK_RETRY_DELAY_MS: number;
    SPI_ADDR_REG_MSB: boolean;
    CHIP_DETECT_MAGIC_REG_ADDR: number;
    DETECTED_FLASH_SIZES: {
        [key: number]: string;
    };
    chip: ROM;
    IS_STUB: boolean;
    FLASH_WRITE_SIZE: number;
    secureDownloadMode: boolean;
    private securityInfoCache;
    transport: Transport;
    private baudrate;
    private serialOptions?;
    private terminal?;
    private romBaudrate;
    private debugLogging;
    private syncStubDetected;
    private resetConstructors;
    /**
     * Create a new ESPLoader to perform serial communication
     * such as read/write flash memory and registers using a LoaderOptions object.
     * @param {LoaderOptions} options - LoaderOptions object argument for ESPLoader.
     * ```
     * const myLoader = new ESPLoader({ transport: Transport, baudrate: number, terminal?: IEspLoaderTerminal });
     * ```
     */
    constructor(options: LoaderOptions);
    /**
     * Write to ESP Loader constructor's terminal with or without new line.
     * @param {string} str - String to write.
     * @param {boolean} withNewline - Add new line at the end ?
     */
    write(str: string, withNewline?: boolean): void;
    /**
     * Write error message to ESP Loader constructor's terminal with or without new line.
     * @param {string} str - String to write.
     * @param {boolean} withNewline - Add new line at the end ?
     */
    error(str: string, withNewline?: boolean): void;
    /**
     * Write information message to ESP Loader constructor's terminal with or without new line.
     * @param {string} str - String to write.
     * @param {boolean} withNewline - Add new line at the end ?
     */
    info(str: string, withNewline?: boolean): void;
    /**
     * Write debug message to ESP Loader constructor's terminal with or without new line.
     * @param {string} str - String to write.
     * @param {boolean} withNewline - Add new line at the end ?
     */
    debug(str: string, withNewline?: boolean): void;
    /**
     * Convert short integer to byte array
     * @param {number} i - Number to convert.
     * @returns {Uint8Array} Byte array.
     */
    _shortToBytearray(i: number): Uint8Array;
    /**
     * Convert an integer to byte array
     * @param {number} i - Number to convert.
     * @returns {ROM} The chip ROM class related to given magic hex number.
     */
    _intToByteArray(i: number): Uint8Array;
    /**
     * Convert a byte array to integer.
     * @param {number} i - Number to convert.
     * @param {number} j - Number to convert.
     * @param {number} k - Number to convert.
     * @param {number} l - Number to convert.
     * @returns {number} Return a integer number.
     */
    _byteArrayToInt(i: number, j: number, k: number, l: number): number;
    /**
     * Append a buffer array after another buffer array
     * @param {Uint8Array} arr1 - First array buffer.
     * @param {Uint8Array} arr2 - magic hex number to select ROM.
     * @returns {Uint8Array} Return a 8 bit unsigned array.
     */
    _appendArray(arr1: Uint8Array, arr2: Uint8Array): Uint8Array;
    /**
     * Use the device serial port read function with given timeout to create a valid packet.
     * @param {number} op Operation number
     * @param {number} timeout timeout number in milliseconds
     * @returns {[number, Uint8Array]} valid response packet.
     */
    readPacket(op?: number | null, timeout?: number): Promise<[number, Uint8Array]>;
    /**
     * Write a serial command to the chip
     * @param {number} op - Operation number
     * @param {Uint8Array} data - Unsigned 8 bit array
     * @param {number} chk - channel number
     * @param {boolean} waitResponse - wait for response ?
     * @param {number} timeout - timeout number in milliseconds
     * @returns {Promise<[number, Uint8Array]>} Return a number and a 8 bit unsigned integer array.
     */
    command(op?: number | null, data?: Uint8Array, chk?: number, waitResponse?: boolean, timeout?: number): Promise<[number, Uint8Array]>;
    /**
     * Read a register from chip.
     * @param {number} addr - Register address number
     * @param {number} timeout - Timeout in milliseconds (Default: 3000ms)
     * @returns {number} - Command number value
     */
    readReg(addr: number, timeout?: number): Promise<number>;
    /**
     * Write a number value to register address in chip.
     * @param {number} addr - Register address number
     * @param {number} value - Number value to write in register
     * @param {number} mask - Hex number for mask
     * @param {number} delayUs Delay number
     * @param {number} delayAfterUs Delay after previous delay
     */
    writeReg(addr: number, value: number, mask?: number, delayUs?: number, delayAfterUs?: number): Promise<void>;
    /**
     * Sync chip by sending sync command.
     * @returns {[number, Uint8Array]} Command result
     */
    sync(): Promise<[number, Uint8Array]>;
    /**
     * Attempt to connect to the chip by sending a reset sequence and later a sync command.
     * @param {string} mode - Reset mode to use
     * @param {ResetStrategy} resetStrategy - Reset strategy class to use for connect
     * @returns {string} - Returns 'success' or 'error' message.
     */
    _connectAttempt(mode: string | undefined, resetStrategy: ResetStrategy | null): Promise<string>;
    /**
     * True when Web Serial reports Espressif USB-Serial/JTAG VID/PID.
     * Safe before chip identify/sync (no register reads).
     */
    private isUsbJtagSerialPort;
    /**
     * True if the host sees this port as Espressif USB-OTG (VID/PID match).
     * Falls back to the chip UARTDEV_BUF_NO helper when Web Serial omits IDs
     * or the USB product ID was customized.
     */
    usesUsbOtg(): Promise<boolean>;
    /**
     * Cap FLASH_WRITE_SIZE to USB_RAM_BLOCK when the stub is talking over
     * USB-OTG. Matches esptool.py's stub USB buffer limit; USB-Serial/JTAG
     * and USB-UART bridges keep the 16 KB default.
     */
    private applyUsbFlashWriteSize;
    /**
     * Constructs a sequence of reset strategies based on the OS,
     * used ESP chip, external settings, and environment variables.
     * Returns a tuple of one or more reset strategies to be tried sequentially.
     * @param {string} mode - Reset mode to use
     * @returns {ResetStrategy[]} - Array of reset strategies
     */
    constructResetSequence(mode: Before): ResetStrategy[];
    /**
     * Open the serial port and sync with the ROM bootloader.
     * Does not identify the chip.
     * @param {Before} mode - Reset mode to use. Example: 'default_reset' | 'no_reset'
     * @param {number} attempts - Number of connection attempts
     */
    private openAndSync;
    /**
     * Perform a connection to chip.
     * @param {string} mode - Reset mode to use. Example: 'default_reset' | 'no_reset'
     * @param {number} attempts - Number of connection attempts
     * @param {boolean} detecting - Detect the connected chip
     */
    connect(mode?: Before, attempts?: number, detecting?: boolean): Promise<void>;
    /**
     * Identify the connected chip using GET_SECURITY_INFO chip-id, then magic-register fallback.
     * @returns {Promise<ROM>} Detected chip ROM
     */
    private identifyChip;
    /**
     * Map a GET_SECURITY_INFO chip ID to a registered ROM instance.
     * @param {number} chipId Chip ID from GET_SECURITY_INFO
     * @returns {ROM | null} Matching ROM, or null if the ID is unknown
     */
    private romFromChipId;
    /**
     * Read GET_SECURITY_INFO flags and update secure download mode.
     * @returns {Promise<boolean>} Whether secure download mode is enabled
     */
    private readSecureDownloadMode;
    /**
     * Identify the chip from the magic register, or ESP32-S2 in secure download mode.
     * @returns {Promise<ROM>} Detected chip ROM
     */
    private identifyChipByMagic;
    /**
     * Store the detected ROM on the loader.
     * @param {ROM} chip Detected chip ROM
     */
    private applyDetectedChip;
    private chipFromMagicValue;
    /**
     * Read GET_SECURITY_INFO (0x14): flags, flash crypt count, key purposes, chip id, API version.
     * Tries the 20-byte layout first (ESP32-S3 and later), then 12 bytes (ESP32-S2).
     * @param {boolean} cache Return a previously parsed result when available
     * @returns {Promise<SecurityInfo>} Parsed security information
     */
    getSecurityInfo(cache?: boolean): Promise<SecurityInfo>;
    /**
     * Get the CHIP ID from ESP_GET_SECURITY_INFO.
     * @returns {number} Chip ID number
     */
    getChipId(): Promise<number>;
    /**
     * Connect and detect the existing chip.
     * @param {string} mode Reset mode to use for connection.
     * @param {number} attempts - Number of connection attempts
     */
    detectChip(mode?: Before, attempts?: number): Promise<void>;
    /**
     * Execute the command and check the command response.
     * @param {string} opDescription Command operation description.
     * @param {number} op Command operation number
     * @param {Uint8Array} data Command value
     * @param {number} chk Checksum to use
     * @param {number} responseDataLength Length of the response data to expect
     * @param {number} timeout TImeout number in milliseconds (ms)
     * @returns {number} Command result
     */
    checkCommand(opDescription?: string, op?: number | null, data?: Uint8Array, chk?: number, responseDataLength?: number, timeout?: number): Promise<number | Uint8Array>;
    /**
     * Start downloading an application image to RAM
     * @param {number} size Image size number
     * @param {number} blocks Number of data blocks
     * @param {number} blocksize Size of each data block
     * @param {number} offset Image offset number
     */
    memBegin(size: number, blocks: number, blocksize: number, offset: number): Promise<void>;
    /**
     * Get the checksum for given unsigned 8-bit array
     * @param {Uint8Array} data Unsigned 8-bit integer array
     * @param {number} state Initial checksum
     * @returns {number} - Array checksum
     */
    checksum(data: Uint8Array, state?: number): number;
    /**
     * Send a block of image to RAM
     * @param {Uint8Array} buffer Unsigned 8-bit array
     * @param {number} seq Sequence number
     */
    memBlock(buffer: Uint8Array, seq: number): Promise<void>;
    /**
     * Leave RAM download mode and run application
     * @param {number} entrypoint - Entrypoint number
     */
    memFinish(entrypoint: number): Promise<void>;
    /**
     * Configure SPI flash pins
     * @param {number} hspiArg -  Argument for SPI attachment
     */
    flashSpiAttach(hspiArg: number): Promise<void>;
    /**
     * Scale timeouts which are size-specific.
     * @param {number} secondsPerMb Seconds per megabytes as number
     * @param {number} sizeBytes Size bytes number
     * @returns {number} - Scaled timeout for specified size.
     */
    timeoutPerMb(secondsPerMb: number, sizeBytes: number): number;
    /**
     * Start downloading to Flash (performs an erase)
     * @param {number} size Size to erase
     * @param {number} offset Offset to erase
     * @returns {number} Number of blocks (of size self.FLASH_WRITE_SIZE) to write.
     */
    flashBegin(size: number, offset: number): Promise<number>;
    /**
     * Start downloading compressed data to Flash (performs an erase)
     * @param {number} size Write size
     * @param {number} compsize Compressed size
     * @param {number} offset Offset for write
     * @returns {number} Returns number of blocks (size self.FLASH_WRITE_SIZE) to write.
     */
    flashDeflBegin(size: number, compsize: number, offset: number): Promise<number>;
    /**
     * Write block to flash, retry up to WRITE_BLOCK_ATTEMPTS times on failure.
     * @param {Uint8Array} data Unsigned 8-bit array data.
     * @param {number} seq Sequence number
     * @param {number} timeout Timeout in milliseconds (ms)
     * @returns {Promise<void>} Promise that resolves when the block is written.
     */
    flashBlock(data: Uint8Array, seq: number, timeout: number): Promise<void>;
    /**
     * Write compressed block to flash, retry up to WRITE_BLOCK_ATTEMPTS times on failure.
     * @param {Uint8Array} data Unsigned int 8-bit array data to write
     * @param {number} seq Sequence number
     * @param {number} timeout Timeout in milliseconds (ms)
     */
    flashDeflBlock(data: Uint8Array, seq: number, timeout: number): Promise<void>;
    /**
     * Leave flash mode and run/reboot
     * @param {boolean} reboot Reboot after leaving flash mode ?
     * @param {number} timeout Timeout in milliseconds (ms)
     * @returns {Promise<void>} Promise that resolves when the flash mode is left.
     */
    flashFinish(reboot?: boolean, timeout?: number): Promise<void>;
    /**
     * Leave compressed flash mode and run/reboot
     * @param {boolean} reboot Reboot after leaving flash mode ?
     * @param {number} timeout Timeout in milliseconds (ms)
     * @returns {Promise<void>} Promise that resolves when the compressed flash mode is left.
     */
    flashDeflFinish(reboot?: boolean, timeout?: number): Promise<void>;
    /**
     * Run an arbitrary SPI flash command.
     *
     * This function uses the "USR_COMMAND" functionality in the ESP
     * SPI hardware, rather than the precanned commands supported by
     * hardware. So the value of spiflashCommand is an actual command
     * byte, sent over the wire.
     *
     * After writing command byte, writes 'data' to MOSI and then
     * reads back 'readBits' of reply on MISO. Result is a number.
     * @param {number} spiflashCommand Command to execute in SPI
     * @param {Uint8Array} data Data to send
     * @param {number} readBits Number of bits to read
     * @param {number} addr Address to use
     * @param {number} addrLen Length of address
     * @param {number} dummyLen length of dummy
     * @returns {number} Register SPI_W0_REG value
     */
    runSpiflashCommand(spiflashCommand: number, data: Uint8Array, readBits: number, addr?: number | null, addrLen?: number, dummyLen?: number): Promise<number>;
    /**
     * Read flash id by executing the SPIFLASH_RDID flash command.
     * @returns {Promise<number>} Register SPI_W0_REG value
     */
    readFlashId(): Promise<number>;
    /**
     * Execute the erase flash command
     * @returns {Promise<number | Uint8Array>} Erase flash command result
     */
    eraseFlash(): Promise<number | Uint8Array>;
    /**
     * Convert a number or unsigned 8-bit array to hex string
     * @param {number | Uint8Array } buffer Data to convert to hex string.
     * @returns {string} A hex string
     */
    toHex(buffer: number | Uint8Array): string;
    /**
     * Calculate the MD5 Checksum command
     * @param {number} addr Address number
     * @param {number} size Package size
     * @returns {string} MD5 Checksum string
     */
    flashMd5sum(addr: number, size: number): Promise<string>;
    /**
     * Read data from flash memory of the chip.
     * This function reads a specified amount of data from the flash memory starting at a given address.
     * It sends a read command to the chip and processes the response packets until the requested size is read.
     * @param {number} addr - The starting address in flash memory to read from.
     * @param {number} size - The number of bytes to read from flash memory.
     * @param {FlashReadCallback} onPacketReceived - Optional callback function to handle each received packet.
     * @returns {Promise<Uint8Array>} A promise that resolves to the data read from flash memory as a Uint8Array.
     * @throws {ESPError} If the read operation fails or an unexpected response is received.
     */
    readFlash(addr: number, size: number, onPacketReceived?: FlashReadCallback): Promise<Uint8Array>;
    /**
     * Upload the flasher ROM bootloader (flasher stub) to the chip.
     * @returns {ROM} The Chip ROM
     */
    runStub(): Promise<ROM>;
    /**
     * Probe whether the chip still answers a register read.
     * Used after a baud-rate port reopen; sync() is unsuitable because the stub
     * answers SYNC once, not eight times like the ROM.
     */
    private isResponsive;
    /**
     * Change the chip baudrate.
     */
    changeBaud(): Promise<void>;
    /**
     * Execute the main function of ESPLoader.
     * @param {string} mode Reset mode to use
     * @returns {string} chip ROM
     */
    main(mode?: Before): Promise<string>;
    /**
     * Get flash size bytes from flash size string.
     * @param {string} flashSize Flash Size string
     * @returns {number} Flash size bytes
     */
    flashSizeBytes(flashSize: FlashSizeValues): number;
    /**
     * Parse a given flash size string to a number
     * @param {string} flsz Flash size to request
     * @returns {number} Flash size number
     */
    parseFlashSizeArg(flsz: FlashSizeValues): number;
    /**
     * Update the image flash parameters with given arguments.
     * @param {Uint8Array} image binary image as Uint8Array
     * @param {number} address flash address number
     * @param {FlashModeValues} flashMode Flash mode string
     * @param {FlashFreqValues} flashFreq Flash frequency string
     * @param {FlashSizeValues} flashSize Already-resolved flash size (`"keep"` or a concrete size such as `"8MB"`).
     * `"detect"` is not accepted here; resolve it in `writeFlash` first.
     * @returns {Uint8Array} modified image Uint8Array
     */
    _updateImageFlashParams(image: Uint8Array, address: number, flashMode?: FlashModeValues, flashFreq?: FlashFreqValues, flashSize?: FlashSizeValues): Promise<Uint8Array>;
    /**
     * Write set of file images into given address based on given FlashOptions object.
     * When `options.flashSize` is `"detect"`, the flash ID is read before any file is written
     * (including images that are not at `BOOTLOADER_FLASH_OFFSET`) so the bounds check can
     * use a concrete size. Throws if the flash ID cannot be read or mapped.
     * @param {FlashOptions} options FlashOptions to configure how and what to write into flash.
     */
    writeFlash(options: FlashOptions): Promise<void>;
    /**
     * Read SPI flash manufacturer and device id.
     */
    flashId(): Promise<void>;
    /**
     * Detect attached flash size from the SPI flash ID.
     * @returns {Promise<string | undefined>} Detected size string, or undefined if the flash ID could not be read / mapped.
     */
    detectFlashSize(): Promise<string | undefined>;
    /**
     * Soft reset the device chip. Soft reset with run user code is the closest.
     * @param {boolean} stayInBootloader Flag to indicate if to stay in bootloader
     */
    softReset(stayInBootloader: boolean): Promise<void>;
    /**
     * Execute this function to execute after operation reset functions.
     * @param {After} mode After operation mode. Default is 'hard_reset'.
     * @param { boolean } usingUsbOtg For 'hard_reset'. When omitted, detected via usesUsbOtg()
     * @param {string} sequenceString For 'custom_reset' to specify the custom reset sequence string
     */
    after(mode?: After, usingUsbOtg?: boolean, sequenceString?: string): Promise<void>;
}
