// SPDX-License-Identifier: GPL-2.0-or-later
// ESP32-S3 ROM sequence adapted from esptool 5.3.1 (GPL-2.0-or-later).
// SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton,
// Espressif Systems (Shanghai) CO LTD, other contributors as noted.
// Copyright 2026 Draftor contributors (JavaScript adaptation)
// Provenance and upstream anchors: docs/CAPABILITY_AUDIT.md.
import { PINS } from './pins.js';
import { Stop, requireSafe, u32, le32, concat, sha256, normalizeMac } from './safety.js';
import { FIRST_FILES, verifyInitialFlash } from './first-install.js';

export const OP = Object.freeze({ FLASH_BEGIN: 0x02, FLASH_DATA: 0x03, SYNC: 0x08, WRITE_REG: 0x09, READ_REG: 0x0a, SPI_PARAMS: 0x0b, SPI_ATTACH: 0x0d, READ_FLASH_SLOW: 0x0e, MD5: 0x13, SECURITY: 0x14 });
export function parseSecurity(data) {
  requireSafe(data.length === 20, 'SECURITY');
  requireSafe(u32(data, 12) === 9, 'CHIP');
  const flags = u32(data), count = data[4];
  requireSafe((flags & ~0x7ff) === 0 && count <= 7, 'SECURITY');
  const encryption = (count.toString(2).split('1').length - 1) % 2 === 1;
  requireSafe((flags & 5) === 0 && !encryption, 'SECURITY');
  return { chip: 'ESP32-S3', securityKnown: true, secureBoot: false, flashEncryption: false };
}

export class S3Rom {
  kind = 'hardware';
  #wire; #fatal = false; #connected = false; #inFlight = false; #writesAllowed; #writeUsed = false;
  #firstAllowed; #firstStage = 'IDLE'; #firstNext = 0;
  constructor(wire, { allowUpdate = false, allowFirstInstall = false } = {}) {
    requireSafe(!(allowUpdate && allowFirstInstall), 'BUILD_SCOPE');
    this.#wire = wire; this.#writesAllowed = allowUpdate; this.#firstAllowed = allowFirstInstall;
  }
  #alive() { requireSafe(!this.#fatal, 'STATE'); }
  async #reply(op, dataBytes, timeout) {
    const packet = await this.#wire.read(timeout);
    requireSafe(packet.length === 8 + dataBytes + 4 && packet[0] === 1 && packet[1] === op && (packet[2] | packet[3] << 8) === dataBytes + 4, 'PROTOCOL');
    // esptool 5.3.1 check_command(): status, error reason, then two reserved
    // bytes. Only a nonzero status byte signals failure; the reason is only
    // meaningful on failure. Keep the exact S3 frame/length checks above.
    requireSafe(packet[8 + dataBytes] === 0, 'ROM_STATUS');
    return { value: u32(packet, 4), data: packet.slice(8, 8 + dataBytes) };
  }
  async #command(op, payload = new Uint8Array(), { dataBytes = 0, checksum = 0, timeout = 10000, responses = 1 } = {}) {
    this.#alive(); requireSafe(!this.#inFlight, 'BUSY'); this.#inFlight = true;
    try {
      requireSafe(Object.values(OP).includes(op) && payload.length <= 1040, 'PROTOCOL');
      const header = new Uint8Array(8); header[1] = op;
      new DataView(header.buffer).setUint16(2, payload.length, true);
      new DataView(header.buffer).setUint32(4, checksum, true);
      await this.#wire.write(concat(header, payload));
      let reply;
      for (let i = 0; i < responses; i++) {
        reply = await this.#reply(op, dataBytes, timeout);
        if (op === OP.SYNC) requireSafe(reply.value !== 0, 'STUB');
      }
      return reply;
    } catch (error) { this.#fatal = true; throw error instanceof Stop ? error : new Stop('IO'); }
    finally { this.#inFlight = false; }
  }
  async #readReg(address) { return (await this.#command(OP.READ_REG, le32(address))).value; }
  async #writeReg(address, value) {
    // Only these volatile S3 watchdog/SPI controller registers can be written.
    requireSafe([0x600080b0, 0x60008098, 0x600080b8, 0x600080b4, 0x60002000, 0x60002018, 0x60002020, 0x60002028, 0x60002058].includes(address), 'REGISTER');
    await this.#command(OP.WRITE_REG, le32(address, value, 0xffffffff, 0));
  }
  async #security() { return parseSecurity((await this.#command(OP.SECURITY, new Uint8Array(), { dataBytes: 20 })).data); }
  async #flashId() {
    const usr = await this.#readReg(0x60002018), usr2 = await this.#readReg(0x60002020);
    await this.#writeReg(0x60002028, 23);
    await this.#writeReg(0x60002018, 0x90000000);
    await this.#writeReg(0x60002020, 0x7000009f); // JEDEC RDID only; never WREN or erase.
    await this.#writeReg(0x60002058, 0);
    await this.#writeReg(0x60002000, 1 << 18);
    let complete = false;
    for (let poll = 0; poll < 10; poll++) {
      if ((await this.#readReg(0x60002000) & (1 << 18)) === 0) { complete = true; break; }
    }
    requireSafe(complete, 'FLASH');
    const id = await this.#readReg(0x60002058);
    await this.#writeReg(0x60002018, usr);
    await this.#writeReg(0x60002020, usr2);
    // 0x18 and 0x38 are the two 16 MiB JEDEC encodings in esptool's table.
    requireSafe((id & 0xff) !== 0 && (id & 0xff) !== 0xff && [0x18, 0x38].includes((id >>> 16) & 0xff), 'FLASH');
    return PINS.flashBytes;
  }
  async connect() {
    this.#alive(); requireSafe(!this.#connected, 'STATE');
    try {
      const usb = this.#wire.getInfo();
      requireSafe(usb.usbVendorId === 0x303a && usb.usbProductId === 0x1001, 'USB_TRANSPORT');
      await this.#wire.connect();
      await this.#command(OP.SYNC, concat(Uint8Array.of(7, 7, 0x12, 0x20), new Uint8Array(32).fill(0x55)), { responses: 8 });
      await this.#security();
      // As in esptool 5.3.1: inhibit JTAG watchdog resets, RAM registers only.
      await this.#writeReg(0x600080b0, 0x50d83aa1);
      await this.#writeReg(0x60008098, 0);
      await this.#writeReg(0x600080b0, 0);
      await this.#writeReg(0x600080b8, 0x8f1d312a);
      await this.#writeReg(0x600080b4, (await this.#readReg(0x600080b4) | 0x80000000) >>> 0);
      await this.#writeReg(0x600080b8, 0);
      await this.#command(OP.SPI_ATTACH, le32(0, 0));
      await this.#flashId();
      await this.#command(OP.SPI_PARAMS, le32(0, PINS.flashBytes, 0x10000, 0x1000, 256, 0xffff));
      this.#connected = true;
    } catch (error) { this.#fatal = true; throw error; }
  }
  async identity() {
    this.#alive(); requireSafe(this.#connected, 'STATE');
    try {
      const security = await this.#security();
      const flashBytes = await this.#flashId();
      const lo = await this.#readReg(0x60007044), hi = await this.#readReg(0x60007048);
      const mac = [hi >>> 8 & 255, hi & 255, lo >>> 24, lo >>> 16 & 255, lo >>> 8 & 255, lo & 255].map(v => v.toString(16).padStart(2, '0')).join(':');
      return { ...security, flashBytes, mac };
    } catch (error) { this.#fatal = true; throw error; }
  }
  #range(offset, size) {
    this.#alive(); requireSafe(this.#connected && Number.isSafeInteger(offset) && Number.isSafeInteger(size) && offset >= 0 && size > 0 && offset + size <= PINS.flashBytes, 'RANGE');
  }
  async readFlash(offset, size) {
    this.#range(offset, size);
    const out = new Uint8Array(size);
    for (let at = 0; at < size; at += 64) {
      const n = Math.min(64, size - at);
      const { data } = await this.#command(OP.READ_FLASH_SLOW, le32(offset + at, n), { dataBytes: 64 });
      out.set(data.slice(0, n), at);
    }
    return out;
  }
  async flashMd5(offset, size) {
    this.#range(offset, size);
    const { data } = await this.#command(OP.MD5, le32(offset, size, 0, 0), { dataBytes: 32, timeout: 120000 });
    const digest = new TextDecoder('ascii').decode(data).toLowerCase();
    try { requireSafe(/^[a-f0-9]{32}$/.test(digest), 'PROTOCOL'); }
    catch (error) { this.#fatal = true; throw error; }
    return digest;
  }
  async writeImage(offset, image, { mode, slotOffset, progress = () => {} } = {}) {
    if (mode === 'fresh') return this.#writeFirstImage(offset, image, progress);
    const data = image.slice(); // Snapshot before any asynchronous validation.
    this.#range(offset, data.length);
    requireSafe(this.#writesAllowed, 'DEVICE_WRITES_LOCKED');
    requireSafe(!this.#writeUsed && mode === 'update' && [0x10000, 0x410000].includes(offset) && slotOffset === offset, 'RANGE');
    this.#writeUsed = true; // Reserve once before yielding to asynchronous hashing.
    requireSafe(data.length === PINS.files['app.bin'].bytes && await sha256(data) === PINS.files['app.bin'].sha256, 'ASSET');
    requireSafe(Math.ceil(data.length / 4096) * 4096 <= PINS.slotCapacity, 'CAPACITY');
    const blocks = Math.ceil(data.length / 1024);
    await this.#command(OP.FLASH_BEGIN, le32(data.length, blocks, 1024, offset, 0), { timeout: 120000 });
    for (let sequence = 0; sequence < blocks; sequence++) {
      const block = new Uint8Array(1024).fill(0xff);
      block.set(data.subarray(sequence * 1024, Math.min(data.length, (sequence + 1) * 1024)));
      const checksum = block.reduce((sum, byte) => sum ^ byte, 0xef);
      await this.#command(OP.FLASH_DATA, concat(le32(1024, sequence, 0, 0), block), { checksum });
      if (sequence % 32 === 0 || sequence === blocks - 1) progress(Math.min(1, (sequence + 1) / blocks));
    }
    // No FLASH_END: in ROM it can run user code, violating --after no-reset.
  }
  async eraseAll({ source, expectedMac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed } = {}) {
    this.#range(0, PINS.flashBytes);
    requireSafe(this.#firstAllowed && !this.#writesAllowed, 'FRESH_DEVICE_PENDING');
    requireSafe(backupConfirmed === true && tfRemoved === true && planConfirmed === true, 'BACKUP');
    requireSafe(eraseConfirmed === true, 'ERASE_CONFIRM');
    requireSafe(this.#firstStage === 'IDLE' && !this.#writeUsed, 'STATE');
    this.#firstStage = 'RESERVED'; this.#writeUsed = true;
    try {
      const info = await this.identity();
      requireSafe(normalizeMac(info.mac) === normalizeMac(expectedMac), 'IDENTITY');
      await verifyInitialFlash(this, source);
      // esptool 5.3.1 cmds.erase_region() in ROM uses flash_begin(size, address).
      // SPI_PARAMS already sets the actual 16 MiB capacity. No stub-only 0xd0/d1.
      await this.#command(OP.FLASH_BEGIN, le32(PINS.flashBytes, PINS.flashBytes / 1024, 1024, 0, 0), { timeout: 480000 });
      requireSafe(await this.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, 'ERASE_VERIFY');
      this.#firstStage = 'ERASED_VERIFIED';
    } catch (error) { this.#fatal = true; throw error; }
  }
  async #writeFirstImage(offset, image, progress) {
    this.#range(offset, image.length);
    requireSafe(this.#firstAllowed && !this.#writesAllowed, 'FRESH_DEVICE_PENDING');
    requireSafe(this.#firstStage === 'ERASED_VERIFIED' && this.#firstNext < FIRST_FILES.length, 'STATE');
    // Reserve the step before awaiting a hash; double clicks cannot create writers.
    this.#firstStage = 'WRITING';
    try {
      const name = FIRST_FILES[this.#firstNext], pin = PINS.files[name], data = image.slice();
      requireSafe(offset === pin.offset && data.length === pin.bytes && await sha256(data) === pin.sha256, 'ASSET');
      const blocks = Math.ceil(data.length / 1024), end = offset + Math.ceil(data.length / 4096) * 4096;
      requireSafe(end <= (this.#firstNext + 1 < FIRST_FILES.length ? PINS.files[FIRST_FILES[this.#firstNext + 1]].offset : 0x410000), 'RANGE');
      await this.#command(OP.FLASH_BEGIN, le32(data.length, blocks, 1024, offset, 0), { timeout: 120000 });
      for (let sequence = 0; sequence < blocks; sequence++) {
        const block = new Uint8Array(1024).fill(255);
        block.set(data.subarray(sequence * 1024, (sequence + 1) * 1024));
        await this.#command(OP.FLASH_DATA, concat(le32(1024, sequence, 0, 0), block), { checksum: block.reduce((sum, b) => sum ^ b, 0xef) });
        if (sequence % 32 === 0 || sequence === blocks - 1) progress((this.#firstNext + (sequence + 1) / blocks) / FIRST_FILES.length);
      }
      requireSafe(await this.flashMd5(offset, data.length) === pin.md5, 'VERIFY');
      this.#firstNext++;
      this.#firstStage = this.#firstNext === FIRST_FILES.length ? 'COMPLETE' : 'ERASED_VERIFIED';
    } catch (error) { this.#fatal = true; throw error; }
  }
  cancel() { this.#fatal = true; this.#wire.cancel(); }
  async disconnect() { this.#fatal = true; await this.#wire.disconnect(); }
}
