/**
 * One contiguous region from an Intel HEX file.
 * Addresses are absolute flash offsets taken from the HEX records.
 * @interface IntelHexSegment
 */
export interface IntelHexSegment {
    /**
     * Absolute flash offset of the first byte.
     * @type {number}
     */
    address: number;
    /**
     * Bytes that were present in the HEX file for this region.
     * Gaps between regions are omitted.
     * @type {Uint8Array}
     */
    data: Uint8Array;
}
/**
 * Parse an Intel HEX file into contiguous flash regions.
 *
 * Gaps in the address map become separate regions, so a later `writeFlash` of
 * those regions leaves partitions that have no records (for example NVS and
 * factory data) unerased. Addresses come from the HEX file: extended linear
 * address (type 04) and extended segment address (type 02) set the upper bits.
 * Do not add a caller-supplied base offset on top of the returned addresses.
 *
 * The HEX must be sparse. A file converted from a padded merged binary still
 * contains the gap bytes and will erase those partitions.
 * @param {string | Uint8Array} input Intel HEX text, or the same text as bytes.
 * @returns {IntelHexSegment[]} Contiguous regions in ascending address order.
 */
export declare function parseIntelHex(input: string | Uint8Array): IntelHexSegment[];
