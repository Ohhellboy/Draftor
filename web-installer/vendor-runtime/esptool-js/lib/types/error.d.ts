/**
 * Represents a Espressif chip error.
 */
declare class ESPError extends Error {
    constructor(message?: string);
}
/**
 * Represents a Espressif timeout chip error.
 */
declare class TimeoutError extends ESPError {
}
/**
 * ROM loader returned an invalid command response (e.g. restricted download mode).
 */
declare class UnsupportedCommandError extends ESPError {
    constructor(message?: string);
}
/**
 * GET_SECURITY_INFO returned a chip ID that does not match any known target.
 */
declare class UnexpectedChipIdError extends ESPError {
    constructor(chipId: number);
}
/**
 * Chip-detect magic register value does not match any known target.
 */
declare class UnexpectedChipMagicError extends ESPError {
    constructor(chipMagicValue: number);
}
/**
 * GET_SECURITY_INFO succeeded but the payload has no chip ID (ESP32-S2).
 */
declare class MissingChipIdError extends ESPError {
    constructor(message?: string);
}
export { ESPError, TimeoutError, UnsupportedCommandError, UnexpectedChipIdError, UnexpectedChipMagicError, MissingChipIdError, };
