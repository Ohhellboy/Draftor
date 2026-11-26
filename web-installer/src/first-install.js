// SPDX-License-Identifier: Apache-2.0
import { FACTORY } from './factory-pins.js';
import { PINS } from './pins.js';
import { requireSafe, sha256, activeSlot, Stop } from './safety.js';

export const FIRST_FILES = Object.freeze(['bootloader.bin', 'partitions.bin', 'boot_app0.bin', 'app.bin']);
export const CLEAN_INITIAL_STATES = Object.freeze(['blank', 'waveshare-factory-20260120', 'unrecognized']);
export function cleanStateAllowed(state) { return CLEAN_INITIAL_STATES.includes(state); }
export async function inspectOriginalFlash(driver, table, ota) {
  // Firmware labels are descriptive. I/O and digest failures still propagate.
  if (await driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5)
    return { firmware: 'blank', activeApp: 'none' };
  if (await isFactoryTable(table)) {
    let matches = true;
    for (const row of FACTORY.fingerprint) {
      if (await driver.flashMd5(row.offset, row.bytes) !== row.md5) matches = false;
    }
    if (matches) return { firmware: 'waveshare-factory-20260120', activeApp: 'factory' };
  }
  if (await sha256(table.subarray(0, 3072)) === PINS.files['partitions.bin'].sha256) {
    let slot;
    try { slot = activeSlot(ota); }
    catch (error) {
      if (!(error instanceof Stop) || error.code !== 'OTA') throw error;
    }
    if (slot === 0) {
      let matches = true;
      for (const name of ['bootloader.bin', 'app.bin']) {
        const pin = PINS.files[name];
        if (await driver.flashMd5(pin.offset, pin.bytes) !== pin.md5) matches = false;
      }
      if (matches) return { firmware: 'draftor-alpha3-app0', activeApp: 'app0' };
    }
  }
  return { firmware: 'unrecognized', activeApp: 'unknown' };
}
export async function isFactoryTable(table) {
  return table.length === 4096 && await sha256(table) === FACTORY.tableSha256;
}

// Recognition is a finite fingerprint, never a fallback from a failed update.
// The only variable region is the pinned factory layout's NVS/phy_init.
// The entire remainder, including unused Flash after the merged image, is checked.
export async function verifyInitialFlash(driver, source) {
  requireSafe(['blank', 'waveshare-factory-20260120'].includes(source), 'UNKNOWN');
  if (source === 'blank') {
    requireSafe(await driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, 'UNKNOWN');
  } else {
    for (const row of FACTORY.fingerprint)
      requireSafe(await driver.flashMd5(row.offset, row.bytes) === row.md5, 'FACTORY_FINGERPRINT');
  }
}
