// SPDX-License-Identifier: Apache-2.0
import { PINS } from './pins.js';
import { releaseIsVerified, releaseFile, tableMatchesRelease } from './firmware.js';
import { requireSafe, normalizeMac, activeSlot, sha256, md5, hex, deepFreeze, Stop, bytesEqual } from './safety.js';
import { FIRST_FILES, isFactoryTable, verifyInitialFlash } from './first-install.js';
import { FACTORY } from './factory-pins.js';

const UPDATE_NEVER = ['bootloader', 'partition table', 'otadata', 'NVS / pairing', 'inactive App', 'ime_ro', 'ffat', 'coredump', 'TF card'];

export class InstallSession {
  #driver; #release; #plan; #fingerprint; #mac; #busy = false; #writesEnabled; #state = 'IDLE';
  #receipt; #emit; #cancelled = false; #freshEnabled; #bindSelectedIdentity;
  #candidate; #backupMd5; #capturedSha256;
  #diagnostics;
  constructor(driver, release, { hardwareWritesEnabled = false, hardwareFreshWritesEnabled = false, bindSelectedIdentity = false, candidate = false, diagnostics, onState = () => {} } = {}) {
    this.#diagnostics=diagnostics;
    requireSafe(!(hardwareWritesEnabled && hardwareFreshWritesEnabled), 'BUILD_SCOPE');
    requireSafe(!(candidate && (hardwareWritesEnabled || hardwareFreshWritesEnabled)), 'BUILD_SCOPE');
    this.#candidate = candidate;
    this.#freshEnabled = hardwareFreshWritesEnabled;
    this.#bindSelectedIdentity = bindSelectedIdentity;
    this.#driver = driver; this.#release = release; this.#writesEnabled = hardwareWritesEnabled; this.#emit = onState;
    this.#receipt = { status: 'IDLE', simulation: driver.kind === 'simulation', release: PINS.version, appSha256: PINS.files['app.bin'].sha256,
      eraseStarted: false, writeStarted: false, independentVerification: false, resetIssued: false, deviceQualification: 'DEVICE_PENDING', tfResources: 'NOT_CHECKED' };
  }
  get state() { return this.#state; }
  get plan() { return this.#plan; }
  get selectedIdentity() { return this.#mac; } // Display only, never included in receipts.
  get backupVerified() { return this.#backupMd5 !== undefined; }
  receipt() { return structuredClone({...this.#receipt,...(this.#diagnostics?{diagnostics:this.#diagnostics.snapshot()}:{})}); }
  #set(status) { this.#state = status; this.#receipt.status = status; this.#diagnostics?.stage(status);this.#emit(status, this.receipt()); }
  #alive() { requireSafe(!this.#cancelled, 'CANCELLED'); }
  async #run(fn) {
    // A second click never creates a second writer or changes the active state.
    requireSafe(!this.#busy, 'BUSY'); this.#busy = true;
    try { this.#alive(); return await fn(); }
    catch (error) {
      const code = this.#cancelled ? (this.#receipt.error || 'CANCELLED') : (error instanceof Stop ? error.code : 'IO');
      this.#receipt.error = code;
      this.#diagnostics?.error(new Stop(code));
      if (this.#receipt.writeStarted || this.#receipt.eraseStarted) this.#receipt.flashOutcome = 'POSSIBLY_PARTIAL';
      this.#set('STOP'); throw new Stop(code);
    } finally { this.#busy = false; }
  }
  async #identity() {
    this.#alive();
    const info = await this.#driver.identity();
    requireSafe(info.chip === 'ESP32-S3', 'CHIP');
    requireSafe(info.flashBytes === PINS.flashBytes, 'FLASH');
    requireSafe(info.securityKnown === true && info.secureBoot === false && info.flashEncryption === false, 'SECURITY');
    const actual = normalizeMac(info.mac);
    if (this.#mac === undefined && this.#bindSelectedIdentity) this.#mac = actual;
    requireSafe(actual === this.#mac, 'IDENTITY');
    this.#diagnostics?.check('originalIdentityMatch',true);
    return info;
  }
  async #metadata() {
    this.#alive();
    const table = await this.#driver.readFlash(0x8000, 0x1000);
    this.#diagnostics?.check('partitionRead',true);
    this.#alive();
    const ota = await this.#driver.readFlash(0xe000, 0x2000);
    this.#diagnostics?.check('otaRead',true);
    requireSafe(table.length === 4096 && ota.length === 8192, 'READBACK');
    // Independent ROM digest validates slow-read transport before trusting metadata.
    const tableMatch=await this.#driver.flashMd5(0x8000,0x1000)===md5(table);this.#diagnostics?.check('partitionMd5Match',tableMatch);requireSafe(tableMatch,'READBACK');
    const otaMatch=await this.#driver.flashMd5(0xe000,0x2000)===md5(ota);this.#diagnostics?.check('otaMd5Match',otaMatch);requireSafe(otaMatch,'READBACK');
    this.#alive();
    return { table, ota, fingerprint: `${await sha256(table)}:${await sha256(ota)}` };
  }
  async prepare({ boardConfirmed, expectedMac }) {
    return this.#run(async () => {
      requireSafe(this.#state === 'IDLE', 'STATE');
      requireSafe(releaseIsVerified(this.#release), 'ASSET');
      requireSafe(boardConfirmed === true, 'BOARD');
      this.#mac = this.#bindSelectedIdentity ? undefined : normalizeMac(expectedMac);
      this.#set('CONNECTING');
      await this.#driver.connect();
      this.#set('CHECKING');
      await this.#identity();
      const { table, ota, fingerprint } = await this.#metadata();
      let mode = 'update', slot, initialState = 'compatible-draftor';
      if (table.every(v => v === 0xff) && ota.every(v => v === 0xff)) {
        // Erased metadata is not proof of a blank device: check all 16 MiB.
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, 'UNKNOWN');
        mode = 'fresh'; slot = 0; initialState = 'blank';
      } else if (await isFactoryTable(table)) {
        initialState = 'waveshare-factory-20260120';
        await verifyInitialFlash(this.#driver, initialState);
        mode = 'fresh'; slot = 0;
      } else {
        tableMatchesRelease(this.#release, table);
        slot = activeSlot(ota);
      }
      if (this.#candidate && mode === 'update') {
        requireSafe(slot === 0, 'UNQUALIFIED_SLOT');
        requireSafe(await this.#driver.flashMd5(0x10000, PINS.files['app.bin'].bytes) === PINS.files['app.bin'].md5, 'UNQUALIFIED_FIRMWARE');
      }
      const offset = slot === 0 ? 0x10000 : 0x410000;
      const writes = mode === 'update'
        ? [{ name: 'app.bin', offset, bytes: PINS.files['app.bin'].bytes }]
        : FIRST_FILES.map(name => ({ name, offset: PINS.files[name].offset, bytes: PINS.files[name].bytes }));
      this.#fingerprint = fingerprint;
      this.#plan = deepFreeze({ mode, initialState, appSlot: `app${slot}`, appOffset: hex(offset), appBytes: PINS.files['app.bin'].bytes, appSha256: PINS.files['app.bin'].sha256,
        writes: writes.map(w => ({ ...w, offsetHex: hex(w.offset), endExclusive: hex(w.offset + w.bytes), sectorEndExclusive: hex(w.offset + Math.ceil(w.bytes / 4096) * 4096) })),
        fullErase: mode === 'fresh',
        atomicUpdate: false, interruption: 'Active App may be partial; no automatic retry, alternate slot or reboot. Recover from verified private backups.',
        neverWrite: mode === 'update' ? UPDATE_NEVER : ['TF card'],
        requires: mode === 'update' ? ['physical Waveshare board', this.#bindSelectedIdentity ? 'same selected USB identity bound to this session' : 'matching private MAC', 'verified recoverable Flash and TF backups', 'TF removed', 'explicit plan approval']
          : ['proven blank or exact pinned factory fingerprint', 'verified recoverable full Flash backup', 'separate full internal Flash erase approval', 'TF removed', 'ordinary boot qualification pending'],
        ...(mode === 'fresh' ? { eraseRange: { offset: 0, bytes: PINS.flashBytes }, eraseMethod: 'ROM FLASH_BEGIN then complete all-FF MD5', factoryCommit: initialState === 'blank' ? null : FACTORY.commit,
          clears: ['entire internal Flash', 'old firmware', 'settings', 'NVS and pairing', 'internal filesystem'], preservedOutsideFlash: ['TF card', 'eFuses'], expectedFullMd5: FACTORY.draftorInitializedFullMd5 } : {}),
        hardwareWriteAllowed: this.#candidate ? initialState !== 'blank' : mode === 'update' ? this.#writesEnabled : this.#freshEnabled,
        ...(this.#candidate ? { candidateOnly: true, publicApproval: 'PENDING', backupSavedFileReimportRequired: true, supportedUpdate: 'exact alpha.3 app0 same-version only', blankHardwareQualification: 'UNVERIFIED_LOCKED' } : {}),
        deviceQualification: 'DEVICE_PENDING', tfResources: 'NOT_CHECKED' });
      this.#receipt.plan = this.#plan;
      this.#diagnostics?.check('plan',this.#plan);this.#diagnostics?.check('writeLock',!this.#plan.hardwareWriteAllowed);
      this.#set('PLAN_READY'); return this.#plan;
    });
  }
  async #samePlannedDevice() {
    await this.#identity();
    requireSafe((await this.#metadata()).fingerprint === this.#fingerprint, 'STALE_PLAN');
  }
  async captureBackup() {
    return this.#run(async () => {
      requireSafe(this.#candidate && this.#state === 'PLAN_READY', 'STATE');
      this.#set('BACKUP_READING');
      await this.#samePlannedDevice();
      const before = await this.#driver.flashMd5(0, PINS.flashBytes);
      const bytes = new Uint8Array(PINS.flashBytes);
      for (let at = 0; at < bytes.length; at += 65536) {
        this.#alive(); const chunk = await this.#driver.readFlash(at, 65536);
        requireSafe(chunk.length === 65536, 'READBACK'); bytes.set(chunk, at);
        this.#emit('BACKUP_READING', { ...this.receipt(), backupProgress: (at + chunk.length) / bytes.length });
      }
      await this.#samePlannedDevice();
      requireSafe(md5(bytes) === before && await this.#driver.flashMd5(0, bytes.length) === before, 'READBACK');
      this.#capturedSha256 = await sha256(bytes);
      this.#backupMd5 = undefined;
      this.#receipt.backup = 'SAVED_FILE_REIMPORT_REQUIRED';
      this.#set('PLAN_READY'); return bytes;
    });
  }
  async verifySavedBackup(input) {
    return this.#run(async () => {
      requireSafe(this.#candidate && this.#state === 'PLAN_READY', 'STATE');
      requireSafe(input instanceof Uint8Array && input.length === PINS.flashBytes, 'BACKUP_FILE');
      const bytes = input.slice();
      this.#set('BACKUP_VERIFYING');
      await this.#samePlannedDevice();
      const digest = md5(bytes);
      if (this.#capturedSha256) requireSafe(await sha256(bytes) === this.#capturedSha256, 'BACKUP_FILE');
      requireSafe(await this.#driver.flashMd5(0, bytes.length) === digest, 'BACKUP_FILE');
      this.#backupMd5 = digest; // Private session evidence, never a receipt/upload.
      this.#receipt.backup = 'SAVED_FILE_REIMPORTED_AND_DEVICE_MD5_MATCHED';
      this.#set('PLAN_READY'); return true;
    });
  }
  async execute({ backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed = false, currentApp } = {}) {
    return this.#run(async () => {
      requireSafe(this.#state === 'PLAN_READY', 'STATE');
      requireSafe(backupConfirmed === true && tfRemoved === true && planConfirmed === true, 'BACKUP');
      const fresh = this.#plan.mode === 'fresh';
      requireSafe(fresh || eraseConfirmed === false, 'ERASE_FORBIDDEN');
      requireSafe(!fresh || eraseConfirmed === true, 'ERASE_CONFIRM');
      if (this.#candidate) {
        requireSafe(this.#plan.initialState !== 'blank', 'UNQUALIFIED_BLANK');
        requireSafe(this.backupVerified, 'BACKUP_FILE');
      } else requireSafe(this.#driver.kind === 'simulation' || (fresh ? this.#freshEnabled : this.#writesEnabled), fresh ? 'FRESH_DEVICE_PENDING' : 'DEVICE_WRITES_LOCKED');
      this.#set('RECHECKING');
      await this.#identity();
      const metadata = await this.#metadata();
      requireSafe(metadata.fingerprint === this.#fingerprint, 'STALE_PLAN');
      if (fresh) await verifyInitialFlash(this.#driver, this.#plan.initialState);
      else {
        tableMatchesRelease(this.#release, metadata.table);
        requireSafe(`app${activeSlot(metadata.ota)}` === this.#plan.appSlot, 'STALE_PLAN');
      }
      const offset = Number(this.#plan.appOffset);
      if (currentApp) {
        requireSafe(!fresh && currentApp instanceof Uint8Array && currentApp.length >= 24 && currentApp.length <= PINS.slotCapacity, 'CURRENT_APP');
        const previous = currentApp.slice();
        requireSafe(await this.#driver.flashMd5(offset, previous.length) === md5(previous), 'CURRENT_APP');
      }
      const protectedRanges = fresh ? [] : [[0, offset], [offset + PINS.slotCapacity, PINS.flashBytes - offset - PINS.slotCapacity]];
      const before = [];
      for (const [start, size] of protectedRanges) before.push(await this.#driver.flashMd5(start, size));
      this.#alive();
      if (this.#candidate) {
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === this.#backupMd5, 'BACKUP_STALE');
        await this.#driver.grantExclusiveMode({ mode: this.#plan.mode, initialState: this.#plan.initialState, expectedMac: this.#mac, metadataFingerprint: this.#fingerprint, backupMd5: this.#backupMd5 });
      }
      if (fresh) {
        this.#receipt.eraseStarted = true; this.#set('ERASING');
        await this.#driver.eraseAll({ source: this.#plan.initialState, expectedMac: this.#mac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed });
        this.#alive();
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, 'ERASE_VERIFY');
        this.#receipt.erased = true;
      }
      this.#receipt.writeStarted = true; this.#set('WRITING');
      for (const row of this.#plan.writes) {
        this.#alive();
        const data = releaseFile(this.#release, row.name);
        requireSafe(data.length === row.bytes && await sha256(data) === PINS.files[row.name].sha256, 'ASSET');
        await this.#driver.writeImage(row.offset, data, { mode: this.#plan.mode, slotOffset: offset, progress: value => this.#emit('WRITING', { ...this.receipt(), progress: value }) });
        if (fresh) { this.#alive(); requireSafe(await this.#driver.flashMd5(row.offset, row.bytes) === PINS.files[row.name].md5, 'VERIFY'); }
      }
      this.#set('VERIFYING');
      for (const row of this.#plan.writes) {
        this.#alive();
        requireSafe(await this.#driver.flashMd5(row.offset, row.bytes) === PINS.files[row.name].md5, 'VERIFY');
      }
      if (fresh) {
        const after = await this.#metadata();
        tableMatchesRelease(this.#release, after.table);
        requireSafe(after.table.slice(3072).every(b => b === 255) && bytesEqual(after.ota, releaseFile(this.#release, 'boot_app0.bin')) && activeSlot(after.ota) === 0, 'READBACK');
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === FACTORY.draftorInitializedFullMd5, 'INITIALIZATION_VERIFY');
        await this.#identity();
        this.#receipt.fullFlashInitialized = true; this.#receipt.partitionOtaReadback = true;
      } else {
        const after = await this.#metadata();
        requireSafe(after.fingerprint === this.#fingerprint, 'PROTECTED_CHANGED');
        for (let i = 0; i < protectedRanges.length; i++) {
          const [start, size] = protectedRanges[i];
          requireSafe(await this.#driver.flashMd5(start, size) === before[i], 'PROTECTED_CHANGED');
        }
        this.#receipt.protectedRegionsUnchanged = true;
      }
      this.#alive(); this.#receipt.independentVerification = true;
      this.#receipt.flashOutcome = 'VERIFIED_CONTENT_BOOT_PENDING';
      this.#set('VERIFIED_BOOT_PENDING');
      return this.receipt();
    });
  }
  cancel(code = 'CANCELLED') { this.#cancelled = true;this.#mac=undefined;this.#backupMd5=undefined;this.#capturedSha256=undefined;this.#fingerprint=undefined; this.#driver.cancel?.(); this.#receipt.error = code; if (this.#receipt.writeStarted || this.#receipt.eraseStarted) this.#receipt.flashOutcome = 'POSSIBLY_PARTIAL'; this.#set('STOP'); }
  async close() { this.#mac = undefined; this.#backupMd5 = undefined; this.#capturedSha256 = undefined;this.#fingerprint=undefined;
    try {await this.#driver.disconnect();} finally {this.#diagnostics?.check('cleanup',{sessionPrivateReferencesCleared:true,physicalMemoryErasureGuaranteed:false});} }
}
