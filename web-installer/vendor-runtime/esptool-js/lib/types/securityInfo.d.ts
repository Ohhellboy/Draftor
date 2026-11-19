/** ROM security-info flag bits (same mapping across targets). */
export declare const SECURITY_INFO_FLAG_MAP: {
    readonly SECURE_BOOT_EN: number;
    readonly SECURE_BOOT_AGGRESSIVE_REVOKE: number;
    readonly SECURE_DOWNLOAD_ENABLE: number;
    readonly SECURE_BOOT_KEY_REVOKE0: number;
    readonly SECURE_BOOT_KEY_REVOKE1: number;
    readonly SECURE_BOOT_KEY_REVOKE2: number;
    readonly SOFT_DIS_JTAG: number;
    readonly HARD_DIS_JTAG: number;
    readonly DIS_USB: number;
    readonly DIS_DOWNLOAD_DCACHE: number;
    readonly DIS_DOWNLOAD_ICACHE: number;
};
/**
 * Name of a flag in {@link SECURITY_INFO_FLAG_MAP}.
 */
export type SecurityInfoFlagName = keyof typeof SECURITY_INFO_FLAG_MAP;
export type ParsedSecurityFlags = Record<SecurityInfoFlagName, boolean>;
/**
 * Parsed GET_SECURITY_INFO (0x14) response.
 * ESP32-S2 omits chipId and apiVersion (12-byte payload).
 */
export interface SecurityInfo {
    flags: number;
    flashCryptCnt: number;
    keyPurposes: number[];
    chipId: number | null;
    apiVersion: number | null;
    parsedFlags: ParsedSecurityFlags;
}
/**
 * Parse GET_SECURITY_INFO flag bits into named booleans.
 * @param {number} flagsValue Raw flags word from the ROM response
 * @returns {ParsedSecurityFlags} Individual flag status
 */
export declare function parseSecurityFlags(flagsValue: number): ParsedSecurityFlags;
