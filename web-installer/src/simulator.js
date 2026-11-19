// SPDX-License-Identifier: Apache-2.0
// Synthetic Flash only. This module never imports or calls Web Serial.
import { PINS } from './pins.js';
import { releaseFile } from './firmware.js';
import { le32, otaCrc, md5, requireSafe, Stop } from './safety.js';

export const SIM_MAC = '02:00:00:00:00:01';
export function otaFixture(slot = 0, { seq = slot + 1, state = 2, second = false, sentinel = false } = {}) {
  const data = new Uint8Array(8192).fill(0xff);
  data.set(le32(seq)); data.set(le32(state, otaCrc(data.slice(0, 4))), 24);
  if (second) data.set(data.slice(0, 32), 4096);
  if (sentinel) data.set(le32(0), 4096);
  return data;
}
export class SimulatedFlash {
  kind = 'simulation'; connected = false; trace = []; info = { chip: 'ESP32-S3', flashBytes: PINS.flashBytes, mac: SIM_MAC, securityKnown: true, secureBoot: false, flashEncryption: false };
  flash = new Uint8Array(PINS.flashBytes).fill(0xff);
  constructor(release, scenario = 'app0') {
    this.scenario = scenario;
    if (scenario !== 'fresh' && scenario !== 'unknown') {
      this.flash.set(releaseFile(release, 'partitions.bin'), 0x8000);
      this.flash.set(otaFixture(scenario === 'app1' ? 1 : 0, { state: scenario === 'pending' ? 1 : 2 }), 0xe000);
      this.flash.set(Uint8Array.of(0x4e, 0x56, 0x53, 0xc0, 0xdb), 0x9000); // synthetic private-data sentinel
      this.flash.fill(0x42, scenario === 'app1' ? 0x410000 : 0x10000, scenario === 'app1' ? 0x411000 : 0x11000);
    }
    if (scenario === 'partition') this.flash[0x8001] ^= 1;
    if (scenario === 'unknown') this.flash[0x9000] = 0;
    if (scenario === 'flash8') this.info.flashBytes = 8 * 1024 * 1024;
    if (scenario === 'security') this.info.securityKnown = false;
  }
  async connect() { requireSafe(!this.connected, 'STATE'); this.connected = true; this.trace.push({ op: 'connect' }); }
  #check() { requireSafe(this.connected, 'DISCONNECTED'); }
  async identity() { this.#check(); this.trace.push({ op: 'identity' }); return { ...this.info }; }
  async readFlash(offset, size) { this.#check(); this.trace.push({ op: 'read', offset, size }); return this.flash.slice(offset, offset + size); }
  async flashMd5(offset, size) { this.#check(); this.trace.push({ op: 'md5', offset, size }); return md5(this.flash.subarray(offset, offset + size)); }
  async eraseAll() { this.#check(); this.trace.push({ op: 'eraseAll' }); this.flash.fill(0xff); }
  async writeImage(offset, bytes, { mode, slotOffset, progress = () => {} } = {}) {
    this.#check();
    if (mode === 'update') requireSafe(offset === slotOffset && [0x10000, 0x410000].includes(offset) && Math.ceil(bytes.length / 4096) * 4096 <= 0x400000, 'RANGE');
    this.trace.push({ op: 'write', offset, size: bytes.length, mode });
    const end = offset + Math.ceil(bytes.length / 4096) * 4096;
    this.flash.fill(0xff, offset, end);
    this.flash.set(bytes.subarray(0, 1024), offset);
    if (this.scenario === 'disconnect') { this.connected = false; throw new Stop('DISCONNECTED'); }
    this.flash.set(bytes, offset);
    if (this.scenario === 'verify') this.flash[offset + 55] ^= 1;
    progress(1);
  }
  cancel() { this.connected = false; }
  async disconnect() { this.connected = false; this.trace.push({ op: 'disconnect' }); }
}
