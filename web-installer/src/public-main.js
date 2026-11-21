// SPDX-License-Identifier: Apache-2.0
import { startUI } from './ui.js';
import { StrictSerialTransport } from './serial.js';
import { S3Rom } from './rom.js';
import { Stop } from './safety.js';

// No compile-time test switch, URL, DOM attribute or BUILD.json grants a writer.
// The only object reachable by the public session exposes read operations.
export class ReadOnlyDriver {
  #rom;
  kind = 'hardware';
  constructor(transport) { this.#rom = new S3Rom(transport); }
  connect() { return this.#rom.connect(); }
  identity() { return this.#rom.identity(); }
  readFlash(offset, length) { return this.#rom.readFlash(offset, length); }
  flashMd5(offset, length) { return this.#rom.flashMd5(offset, length); }
  writeImage() { throw new Stop('DEVICE_WRITES_LOCKED'); }
  eraseAll() { throw new Stop('DEVICE_WRITES_LOCKED'); }
  cancel() { this.#rom.cancel(); }
  disconnect() { return this.#rom.disconnect(); }
}
startUI({ publicRelease: true, bindSelectedIdentity: true,
  requestDriver: async () => new ReadOnlyDriver(new StrictSerialTransport(await navigator.serial.requestPort())),
});
