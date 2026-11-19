import { ROM } from "../targets/rom.js";
import { BaseFirmwareImage } from "./base.js";
/**
 * Function to load a firmware image from a Uint8Array or string
 * @param {ROM} rom - The ROM object representing the target device
 * @param {Uint8Array | string} imageData Image data as a Uint8Array or string
 * @returns {Promise<BaseFirmwareImage>} - A promise that resolves to the loaded firmware image
 */
export declare function loadFirmwareImage(rom: ROM, imageData: Uint8Array | string): Promise<BaseFirmwareImage>;
