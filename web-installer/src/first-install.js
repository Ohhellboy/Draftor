// SPDX-License-Identifier: Apache-2.0
import { FACTORY } from './factory-pins.js';
import { PINS } from './pins.js';
import { requireSafe, sha256 } from './safety.js';

export const FIRST_FILES = Object.freeze(['bootloader.bin', 'partitions.bin', 'boot_app0.bin', 'app.bin']);
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
