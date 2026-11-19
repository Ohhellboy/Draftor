// SPDX-License-Identifier: Apache-2.0
import { PINS } from './pins.js';
import { requireSafe, deepFreeze, sha256, md5, bytesEqual, checkPartitions, activeSlot, Stop } from './safety.js';

deepFreeze(PINS);
const verified = new WeakMap();
export const releaseIsVerified = release => verified.has(release);
export function releaseFile(release, name) {
  requireSafe(verified.has(release) && PINS.files[name], 'ASSET');
  return verified.get(release)[name].slice();
}

export async function verifyRelease(inputs) {
  const files = {};
  for (const [name, pin] of Object.entries({ ...PINS.documents, ...PINS.files })) {
    const bytes = inputs[name];
    requireSafe(bytes instanceof Uint8Array && bytes.length === pin.bytes, 'ASSET');
    const copy = bytes.slice();
    requireSafe(await sha256(copy) === pin.sha256, 'ASSET');
    if (PINS.files[name]) {
      requireSafe(md5(copy) === pin.md5, 'ASSET');
      files[name] = copy;
    }
  }
  const manifest = JSON.parse(new TextDecoder().decode(inputs['FIRMWARE_MANIFEST.json']));
  const pair = JSON.parse(new TextDecoder().decode(inputs['RELEASE_PAIR.json']));
  requireSafe(manifest.version === PINS.version && manifest.board === PINS.board && manifest.chip === PINS.chip && manifest.flash_bytes === PINS.flashBytes, 'ASSET');
  requireSafe(pair.app.sha256 === PINS.files['app.bin'].sha256, 'ASSET');
  for (const name of Object.keys(PINS.files)) {
    const pin = PINS.files[name], item = manifest.files[name];
    requireSafe(item.bytes === pin.bytes && item.offset === pin.offset && item.sha256 === pin.sha256, 'ASSET');
  }
  for (const name of ['app.bin', 'bootloader.bin']) {
    const b = files[name];
    requireSafe(b.length >= 24 && b[0] === 0xe9 && (b[12] | b[13] << 8) === 9, 'IMAGE');
  }
  requireSafe(files['app.bin'].length > 0 && files['app.bin'].length <= PINS.slotCapacity && files['bootloader.bin'].length < 0x8000, 'CAPACITY');
  requireSafe(new TextDecoder().decode(files['app.bin']).includes(PINS.label), 'IMAGE');
  checkPartitions(files['partitions.bin']);
  requireSafe(activeSlot(files['boot_app0.bin']) === 0, 'OTA');
  const release = Object.freeze({ version: PINS.version, board: PINS.board, appSha256: PINS.files['app.bin'].sha256 });
  verified.set(release, files);
  return release;
}

export async function loadRelease(base = new URL('../firmware/', import.meta.url), fetcher = fetch) {
  const inputs = {};
  for (const [name, pin] of Object.entries({ ...PINS.documents, ...PINS.files })) {
    const url = new URL(name, base);
    requireSafe(url.origin === base.origin, 'ASSET');
    let response;
    try {
      response = await fetcher(url, { redirect: 'error', credentials: 'omit', cache: 'no-store', signal: AbortSignal.timeout(60000) });
      requireSafe(response.ok && !response.redirected && response.body, 'DOWNLOAD');
      if (response.headers.get('content-length')) requireSafe(Number(response.headers.get('content-length')) === pin.bytes, 'ASSET');
      const reader = response.body.getReader(), out = new Uint8Array(pin.bytes);
      let at = 0;
      try {
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          requireSafe(at + value.length <= out.length, 'ASSET');
          out.set(value, at); at += value.length;
        }
        requireSafe(at === out.length, 'DOWNLOAD');
      } catch (error) { await reader.cancel().catch(() => {}); throw error; }
      finally { reader.releaseLock(); }
      inputs[name] = out;
    } catch (error) { throw error instanceof Stop ? error : new Stop('DOWNLOAD'); }
  }
  return verifyRelease(inputs);
}

export function tableMatchesRelease(release, actual) {
  checkPartitions(actual);
  requireSafe(bytesEqual(actual.slice(0, 3072), releaseFile(release, 'partitions.bin')), 'PARTITIONS');
}
