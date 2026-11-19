// SPDX-License-Identifier: Apache-2.0
import SparkMD5 from 'spark-md5';

export class Stop extends Error {
  constructor(code) { super(code); this.name = 'Stop'; this.code = code; }
}
export function requireSafe(ok, code) { if (!ok) throw new Stop(code); }
export const hex = n => `0x${n.toString(16)}`;
export const bytesEqual = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
export const u32 = (a, offset = 0) => new DataView(a.buffer, a.byteOffset, a.byteLength).getUint32(offset, true);
export const le32 = (...values) => {
  const out = new Uint8Array(values.length * 4), view = new DataView(out.buffer);
  values.forEach((value, i) => view.setUint32(i * 4, value, true));
  return out;
};
export function concat(...arrays) {
  const out = new Uint8Array(arrays.reduce((n, a) => n + a.length, 0));
  let at = 0;
  for (const a of arrays) { out.set(a, at); at += a.length; }
  return out;
}
export async function sha256(bytes) {
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(digest), n => n.toString(16).padStart(2, '0')).join('');
}
export const md5 = bytes => SparkMD5.ArrayBuffer.hash(bytes.slice().buffer);
export function deepFreeze(object) {
  for (const value of Object.values(object)) if (value && typeof value === 'object') deepFreeze(value);
  return Object.freeze(object);
}
export function normalizeMac(value) {
  const out = String(value ?? '').toLowerCase().replace(/[:-]/g, '');
  requireSafe(/^[0-9a-f]{12}$/.test(out) && out !== '000000000000' && out !== 'ffffffffffff', 'IDENTITY');
  return out;
}

// zlib.crc32(data, 0xffffffff), as in the release's check_install.py.
export function otaCrc(data) {
  let crc = 0;
  for (const value of data) {
    crc ^= value;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

export const PARTITIONS = deepFreeze([
  ['nvs', 1, 2, 0x9000, 0x5000], ['otadata', 1, 0, 0xe000, 0x2000],
  ['app0', 0, 0x10, 0x10000, 0x400000], ['app1', 0, 0x11, 0x410000, 0x400000],
  ['ime_ro', 0x40, 0, 0x810000, 0x200000], ['ffat', 1, 0x81, 0xa10000, 0x5e0000],
  ['coredump', 1, 3, 0xff0000, 0x10000],
]);

export function checkPartitions(data) {
  requireSafe([3072, 4096].includes(data.length), 'PARTITIONS');
  const rows = [];
  for (let at = 0; at < 3072; at += 32) {
    const magic = data[at] | (data[at + 1] << 8);
    if (magic === 0xffff || magic === 0xebeb) break;
    requireSafe(magic === 0x50aa && u32(data, at + 28) === 0, 'PARTITIONS');
    const name = new TextDecoder('ascii').decode(data.slice(at + 12, at + 28)).split('\0')[0];
    rows.push([name, data[at + 2], data[at + 3], u32(data, at + 4), u32(data, at + 8)]);
  }
  requireSafe(JSON.stringify(rows) === JSON.stringify(PARTITIONS), 'PARTITIONS');
  return rows;
}

export function activeSlot(data) {
  requireSafe(data.length === 8192, 'OTA');
  const entries = [];
  for (const at of [0, 4096]) {
    const entry = data.slice(at, at + 32);
    if (entry.every(x => x === 0xff) || (entry.slice(0, 4).every(x => x === 0) && entry.slice(4).every(x => x === 0xff))) continue;
    const seq = u32(entry), state = u32(entry, 24);
    requireSafe(seq > 0 && seq < 0xffffffff && [2, 0xffffffff].includes(state) && u32(entry, 28) === otaCrc(entry.slice(0, 4)), 'OTA');
    entries.push(seq);
  }
  requireSafe(entries.length === 1, 'OTA');
  return (entries[0] - 1) % 2;
}
