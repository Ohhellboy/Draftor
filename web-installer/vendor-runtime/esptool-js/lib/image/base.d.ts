import { ROM } from "../targets/rom.js";
export declare const ESP_IMAGE_MAGIC = 233;
/**
 * Return position aligned to size
 * @param {number} position Position to align
 * @param {number} size Alignment size
 * @returns {number} Aligned position
 */
export declare function alignFilePosition(position: number, size: number): number;
export declare class ImageSegment {
    addr: number;
    data: Uint8Array;
    fileOffs: number | null;
    flags: number;
    includeInChecksum: boolean;
    constructor(addr: number, data: Uint8Array, fileOffs?: number | null, flags?: number);
    copyWithNewAddr(newAddr: number): ImageSegment;
    splitImage(splitLen: number): ImageSegment;
    toString(): string;
    getMemoryType(image: BaseFirmwareImage): string[];
    padToAlignment(alignment: number): void;
}
export declare class ELFSection extends ImageSegment {
    name: string;
    constructor(name: string, addr: number, data: Uint8Array, flags: number);
    toString(): string;
}
export declare class BaseFirmwareImage {
    SEG_HEADER_LEN: number;
    SHA256_DIGEST_LEN: number;
    ELF_FLAG_WRITE: number;
    ELF_FLAG_READ: number;
    ELF_FLAG_EXEC: number;
    segments: ImageSegment[];
    entrypoint: number;
    elfSha256: Uint8Array | null;
    elfSha256Offset: number;
    padToSize: number;
    flashMode: number;
    flashSizeFreq: number;
    checksum: number;
    ROM_LOADER: ROM;
    datalength: number;
    IROM_ALIGN: number;
    MMU_PAGE_SIZE_CONF: number[];
    constructor(rom: ROM);
    loadCommonHeader(data: Uint8Array, offset: number, expectedMagic: number): number;
    verify(): void;
    loadSegment(data: Uint8Array, offset: number, isIromSegment?: boolean): ImageSegment;
    warnIfUnusualSegment(offset: number, size: number, isIromSegment: boolean): void;
    maybePatchSegmentData(data: Uint8Array, filePos: number): Uint8Array;
    saveSegment(output: Uint8Array, offset: number, segment: ImageSegment, checksumValue?: number | null): number;
    saveFlashSegment(output: Uint8Array, offset: number, segment: ImageSegment, checksumValue?: number | null): number;
    /**
     * Return ESPLoader checksum from end of just-read image
     * @param {Uint8Array} data image to read checksum from
     * @param {number} offset Current offset in image
     * @returns {number} checksum value
     */
    readChecksum(data: Uint8Array, offset: number): number;
    /**
     * Calculate checksum of loaded image, based on segments in segment array.
     * @returns {number} checksum value
     */
    calculateChecksum(): number;
    appendChecksum(output: Uint8Array, offset: number, checksumValue: number): void;
    writeCommonHeader(output: Uint8Array, offset: number, segments: number): void;
    isIromAddr(addr: number): boolean;
    getIromSegment(): ImageSegment | null;
    getNonIromSegments(): ImageSegment[];
    sortSegments(): void;
    mergeAdjacentSegments(): void;
    setMmuPageSize(size: number): void;
}
