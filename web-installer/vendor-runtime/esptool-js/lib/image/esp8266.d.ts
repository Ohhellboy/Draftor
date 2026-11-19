import { ESP8266ROM } from "../targets/esp8266.js";
import { BaseFirmwareImage } from "./base.js";
export declare class ESP8266ROMFirmwareImage extends BaseFirmwareImage {
    version: number;
    ROM_LOADER: ESP8266ROM;
    constructor(rom: ESP8266ROM, loadFile?: Uint8Array | string | null);
    loadFromFile(file: Uint8Array | string): void;
    defaultOutputName(inputFile: string): string;
}
export declare class ESP8266V2FirmwareImage extends BaseFirmwareImage {
    static readonly IMAGE_V2_MAGIC = 234;
    static readonly IMAGE_V2_SEGMENT = 4;
    version: number;
    ROM_LOADER: ESP8266ROM;
    constructor(rom: ESP8266ROM, loadFile?: Uint8Array | string | null);
    loadFromFile(fileStr: Uint8Array | string): Promise<void>;
    defaultOutputName(inputFile: string): string;
}
