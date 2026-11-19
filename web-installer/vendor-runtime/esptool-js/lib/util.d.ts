/**
 * Pad to the next alignment boundary
 * @param {Uint8Array} data Uint8Array data to pad
 * @param {number} alignment Alignment boundary to fulfill
 * @param {number} padCharacter Character to fill with
 * @returns {Uint8Array} Padded UInt8Array image
 */
export declare function padTo(data: Uint8Array, alignment: number, padCharacter?: number): Uint8Array;
export declare const ESP_CHECKSUM_MAGIC = 239;
/**
 * Get the checksum for given unsigned 8-bit array
 * @param {Uint8Array} data Unsigned 8-bit integer array
 * @param {number} state Initial checksum
 * @returns {number} - Array checksum
 */
export declare function checksum(data: Uint8Array, state?: number): number;
/**
 * Convert a byte string to unsigned 8 bit integer array.
 * @param {string} bStr - binary string input
 * @returns {Uint8Array} Return a 8 bit unsigned integer array.
 */
export declare function bstrToUi8(bStr: string): Uint8Array;
/**
 * Sleep for a given number of milliseconds
 * @param {number} ms - Number of milliseconds to sleep
 * @returns {Promise<void>} Promise that resolves after the sleep
 */
export declare function sleep(ms: number): Promise<void>;
