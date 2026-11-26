// SPDX-License-Identifier: GPL-2.0-or-later
// ESP32-S3 ROM sequence adapted from esptool 5.3.1 (GPL-2.0-or-later).
// SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton,
// Espressif Systems (Shanghai) CO LTD, other contributors as noted.
// Copyright 2026 Draftor contributors (JavaScript adaptation)
// Provenance and upstream anchors: docs/CAPABILITY_AUDIT.md.
import { PINS } from './pins.js';
import { Stop, requireSafe, u32, le32, concat, sha256, normalizeMac, md5, activeSlot, checkPartitions } from './safety.js';
import { FIRST_FILES, verifyInitialFlash, cleanStateAllowed } from './first-install.js';
import { recoverySnapshot, recoverySummary, sameOriginalIdentity } from './recovery-package.js';
import { savedProtection } from './install-protection.js';

export const OP = Object.freeze({ FLASH_BEGIN: 0x02, FLASH_DATA: 0x03, SYNC: 0x08, WRITE_REG: 0x09, READ_REG: 0x0a, SPI_PARAMS: 0x0b, SPI_ATTACH: 0x0d, READ_FLASH_SLOW: 0x0e, MD5: 0x13, SECURITY: 0x14 });
export function parseSecurity(data) {
  requireSafe(data.length === 20, 'SECURITY');
  requireSafe(u32(data, 12) === 9, 'CHIP');
  const flags = u32(data), count = data[4];
  requireSafe((flags & ~0x7ff) === 0 && count <= 7, 'SECURITY');
  const encryption = (count.toString(2).split('1').length - 1) % 2 === 1;
  requireSafe((flags & 5) === 0 && !encryption, 'SECURITY');
  return { chip: 'ESP32-S3', securityKnown: true, secureBoot: false, flashEncryption: false,
    securityRule: 'ESP32S3_ROM20_ESPt531_V1', securityFlags: flags, cryptCount: count, romChipId: u32(data,12) };
}

export class S3Rom {
  kind = 'hardware';
  #wire; #fatal = false; #connected = false; #inFlight = false; #writesAllowed; #writeUsed = false;
  #firstAllowed; #firstStage = 'IDLE'; #firstNext = 0;
  #candidate; #candidateMode; #cleanInstall = false; #grantedBackupMd5; #mutationStarted = false;
  get flashMutationStarted() { return this.#mutationStarted; }
  #recoveryAllowed; #recoveryStage='IDLE'; #recoveryHandle; #recoveryImage; #recoveryManifest;
  #diagnostics;
  constructor(wire, { allowUpdate = false, allowFirstInstall = false, candidate = false, allowRecovery = false, diagnostics } = {}) {
    this.#diagnostics=diagnostics;
    requireSafe(!(allowUpdate && allowFirstInstall), 'BUILD_SCOPE');
    requireSafe(!(candidate && (allowUpdate || allowFirstInstall)), 'BUILD_SCOPE');
    requireSafe(!(allowRecovery && (candidate || allowUpdate || allowFirstInstall)), 'BUILD_SCOPE');
    this.#recoveryAllowed=allowRecovery;
    this.#candidate = candidate;
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
    this.#diagnostics?.command(op,'acceptedReplies');
    return { value: u32(packet, 4), data: packet.slice(8, 8 + dataBytes) };
  }
  async #command(op, payload = new Uint8Array(), { dataBytes = 0, checksum = 0, timeout = 10000, responses = 1 } = {}) {
    this.#alive(); requireSafe(!this.#inFlight, 'BUSY'); this.#inFlight = true;
    this.#diagnostics?.pending(1);
    try {
      requireSafe(Object.values(OP).includes(op) && payload.length <= 1040, 'PROTOCOL');
      const header = new Uint8Array(8); header[1] = op;
      new DataView(header.buffer).setUint16(2, payload.length, true);
      new DataView(header.buffer).setUint32(4, checksum, true);
      this.#diagnostics?.command(op,'attempted');
      if (op === OP.FLASH_BEGIN || op === OP.FLASH_DATA) { this.#mutationStarted = true;  }
      await this.#wire.write(concat(header, payload));
      this.#diagnostics?.command(op,'sent');
      let reply;
      for (let i = 0; i < responses; i++) {
        reply = await this.#reply(op, dataBytes, timeout);
        if (op === OP.SYNC) requireSafe(reply.value !== 0, 'STUB');
      }
      return reply;
    } catch (error) { this.#fatal = true; this.#diagnostics?.error(error); throw error instanceof Stop ? error : new Stop('IO'); }
    finally { this.#inFlight = false; this.#diagnostics?.pending(-1); }
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
      this.#diagnostics?.check('chip',security.chip);
      this.#diagnostics?.check('flashBytes',flashBytes);
      this.#diagnostics?.check('security',{known:security.securityKnown,secureBoot:security.secureBoot,flashEncryption:security.flashEncryption,rule:security.securityRule,flags:security.securityFlags,cryptCount:security.cryptCount,romChipId:security.romChipId});
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
  async grantExclusiveMode({ mode, initialState, boardConfirmed, cleanInstall = false, expectedMac, metadataFingerprint, backupMd5, protection }) {
    this.#alive();
    requireSafe(this.#candidate && this.#connected && this.#candidateMode === undefined && !this.#writesAllowed && !this.#firstAllowed && !this.#writeUsed, 'BUILD_SCOPE');
    this.#candidateMode = 'RESERVING'; // One attempt; no mode switching after failure.
    try {
      requireSafe(['update', 'fresh'].includes(mode), 'BUILD_SCOPE');
      const identity=await this.identity();
      requireSafe(normalizeMac(identity.mac) === normalizeMac(expectedMac), 'IDENTITY');
      if(protection!==undefined){const p=savedProtection(protection,{mode,mac:normalizeMac(expectedMac)});sameOriginalIdentity(identity,p.device);backupMd5=p.originalMd5;}
      const table = await this.readFlash(0x8000, 4096), ota = await this.readFlash(0xe000, 8192);
      requireSafe(await this.flashMd5(0x8000, 4096) === md5(table) && await this.flashMd5(0xe000, 8192) === md5(ota), 'READBACK');
      requireSafe(`${await sha256(table)}:${await sha256(ota)}` === metadataFingerprint, 'STALE_PLAN');
      requireSafe(/^[a-f0-9]{32}$/.test(backupMd5) && await this.flashMd5(0, PINS.flashBytes) === backupMd5, 'BACKUP_STALE');
      if (mode === 'update') {
        requireSafe(initialState === 'compatible-draftor', 'UNKNOWN');
        checkPartitions(table);
        requireSafe(await sha256(table.slice(0, 3072)) === PINS.files['partitions.bin'].sha256, 'PARTITIONS');
        requireSafe(activeSlot(ota) === 0, 'UNQUALIFIED_SLOT');
        requireSafe(await this.flashMd5(0x10000, PINS.files['app.bin'].bytes) === PINS.files['app.bin'].md5, 'UNQUALIFIED_FIRMWARE');
        this.#writesAllowed = true;
      } else {
        if (cleanInstall) {
          requireSafe(boardConfirmed === true && cleanStateAllowed(initialState) && protection === undefined, 'BOARD_CONFIRM');
          requireSafe(initialState !== 'blank' || backupMd5 === PINS.blankFlashMd5, 'BACKUP_STALE');
          this.#cleanInstall = true;
          this.#grantedBackupMd5 = backupMd5;
        } else {
          requireSafe(initialState === 'waveshare-factory-20260120', 'UNQUALIFIED_BLANK');
          await verifyInitialFlash(this, initialState);
        }
        this.#firstAllowed = true;
      }
      requireSafe(!(this.#writesAllowed && this.#firstAllowed), 'BUILD_SCOPE');
      this.#candidateMode = mode;
    } catch (error) { this.#fatal = true; throw error; }
  }
  async grantRecovery(handle,{boardConfirmed,tfRemoved,restoreConfirmed}={}) {
    this.#alive();
    requireSafe(this.#recoveryAllowed&&!this.#candidate&&!this.#writesAllowed&&!this.#firstAllowed&&this.#connected&&!this.#writeUsed&&this.#recoveryStage==='IDLE','DEVICE_WRITES_LOCKED');
    this.#recoveryStage='RESERVING';
    try {
      requireSafe(boardConfirmed===true&&tfRemoved===true&&restoreConfirmed===true,'RESTORE_CONFIRM');
      const {manifest,image}=recoverySnapshot(handle);this.#recoveryImage=image;this.#recoveryManifest=manifest;this.#recoveryHandle=handle;
      requireSafe(image.length===PINS.flashBytes&&await sha256(image)===manifest.image.sha256,'PACKAGE_HASH');
      sameOriginalIdentity(await this.identity(),manifest.device);
      this.#diagnostics?.check('originalIdentityMatch',true);
      this.#recoveryStage='READY_FOR_CONFIRMATION';
    } catch(error) { this.#fatal=true;this.#clearRecovery();throw error; }
  }
  async restoreRecovery({progress=()=>{}}={}) {
    this.#alive();
    requireSafe(this.#recoveryAllowed&&this.#recoveryStage==='READY_FOR_CONFIRMATION'&&!this.#writeUsed,'STATE');
    this.#writeUsed=true;this.#recoveryStage='RESTORING';
    try {
      recoverySummary(this.#recoveryHandle); // Re-selection/destruction invalidates the grant.
      const data=this.#recoveryImage,manifest=this.#recoveryManifest;
      requireSafe(data?.length===PINS.flashBytes&&await sha256(data)===manifest.image.sha256,'PACKAGE_HASH');
      // Read real security/capacity/MAC again immediately before the first
      // destructive command. Current partitions, OTA and App may be damaged.
      sameOriginalIdentity(await this.identity(),manifest.device);
      this.#alive();this.#diagnostics?.stage('RESTORING');
      await this.#command(OP.FLASH_BEGIN,le32(PINS.flashBytes,PINS.flashBytes/1024,1024,0,0),{timeout:480000});
      requireSafe(await this.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, 'ERASE_VERIFY');
      for(let sequence=0;sequence<PINS.flashBytes/1024;sequence++) {
        const block=data.subarray(sequence*1024,(sequence+1)*1024);
        await this.#command(OP.FLASH_DATA,concat(le32(1024,sequence,0,0),block),{checksum:block.reduce((n,b)=>n^b,0xef)});
        if(sequence%32===0||sequence===16383)progress((sequence+1)/16384);
      }
      this.#recoveryStage='VERIFYING';this.#diagnostics?.stage('VERIFYING');
      const verification=await this.verifyRecoveryImage(this.#recoveryHandle);
      this.#recoveryStage='VERIFIED_BOOT_PENDING';this.#diagnostics?.stage('VERIFIED_BOOT_PENDING');
      return verification;
    } catch(error) {this.#fatal=true;this.#recoveryStage='STOP';this.#diagnostics?.error(error);throw error;}
    finally {this.#clearRecovery();}
  }
  async verifyRecoveryImage(handle) {
    this.#alive();const {manifest,image}=recoverySnapshot(handle);
    try {
      sameOriginalIdentity(await this.identity(),manifest.device);this.#diagnostics?.check('originalIdentityMatch',true);
      const full=await this.flashMd5(0,PINS.flashBytes)===manifest.image.md5;
      this.#diagnostics?.check('fullFlashMd5Match',full);requireSafe(full,'RESTORE_VERIFY');
      for(const [kind,range] of [['partition',manifest.original.partition],['ota',manifest.original.ota]]) {
        if(range.applicable===false) {this.#diagnostics?.check('otaRead','NOT_APPLICABLE_FACTORY');this.#diagnostics?.check('otaMd5Match','NOT_APPLICABLE_FACTORY');continue;}
        const read=await this.readFlash(range.offset,range.bytes);this.#diagnostics?.check(`${kind}Read`,true);
        const match=md5(read)===await this.flashMd5(range.offset,range.bytes)&&await sha256(read)===range.sha256;
        this.#diagnostics?.check(`${kind}Md5Match`,match);read.fill(0);requireSafe(match,'RESTORE_METADATA_VERIFY');
      }
      return {status:'VERIFIED_BOOT_PENDING',fullFlashMd5Match:true,metadataMatch:true,resetIssued:false};
    } finally {image.fill(0);}
  }
  #clearRecovery() {this.#recoveryImage?.fill(0);this.#recoveryImage=undefined;this.#recoveryManifest=undefined;this.#recoveryHandle=undefined;}
  async writeImage(offset, image, { mode, slotOffset, progress = () => {} } = {}) {
    if (mode === 'fresh') return this.#writeFirstImage(offset, image, progress);
    const data = image.slice(); // Snapshot before any asynchronous validation.
    this.#range(offset, data.length);
    requireSafe(this.#writesAllowed, 'DEVICE_WRITES_LOCKED');
    requireSafe(!this.#candidate || (this.#candidateMode === 'update' && offset === 0x10000 && slotOffset === 0x10000), 'UNQUALIFIED_SLOT');
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
  async eraseAll({ source, cleanInstall = false, expectedOriginalMd5, expectedMac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed, protection } = {}) {
    this.#range(0, PINS.flashBytes);
    requireSafe(this.#firstAllowed && !this.#writesAllowed, 'FRESH_DEVICE_PENDING');
    requireSafe(!this.#candidate || (this.#candidateMode === 'fresh' && (this.#cleanInstall ? cleanInstall && cleanStateAllowed(source) : source === 'waveshare-factory-20260120')), 'UNKNOWN');
    requireSafe(tfRemoved === true && planConfirmed === true, 'BACKUP');
    if(protection!==undefined){requireSafe(this.#candidate,'BUILD_SCOPE');savedProtection(protection,{mode:'fresh',mac:normalizeMac(expectedMac)});}
    else requireSafe(backupConfirmed === true, 'BACKUP');
    requireSafe(eraseConfirmed === true, 'ERASE_CONFIRM');
    requireSafe(this.#firstStage === 'IDLE' && !this.#writeUsed, 'STATE');
    this.#firstStage = 'RESERVED'; this.#writeUsed = true;
    try {
      const info = await this.identity();
      requireSafe(normalizeMac(info.mac) === normalizeMac(expectedMac), 'IDENTITY');
      if (this.#cleanInstall) {
        requireSafe(expectedOriginalMd5 === this.#grantedBackupMd5 && await this.flashMd5(0, PINS.flashBytes) === this.#grantedBackupMd5, 'BACKUP_STALE');
      } else await verifyInitialFlash(this, source);
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
  cancel() { this.#fatal = true; this.#clearRecovery(); this.#wire.cancel(); }
  async disconnect() { this.#fatal = true; this.#clearRecovery(); await this.#wire.disconnect(); }
}
