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

// A deadline on *inactivity*, never on the total transfer duration.
export const DOWNLOAD_IDLE_MS = 60000;
export class DownloadFailure extends Stop {
  constructor(category, detail = {}) {
    super(['LENGTH', 'SHA256', 'MD5', 'RELEASE'].includes(category) ? 'ASSET' : category === 'CANCELLED' ? 'CANCELLED' : 'DOWNLOAD');
    this.name = 'DownloadFailure';
    this.detail = Object.freeze({ category, ...detail });
  }
}
function abortable(work, signal) {
  if (signal.aborted) return Promise.reject(new Error('download aborted'));
  return new Promise((resolve, reject) => {
    const abort = () => { signal.removeEventListener('abort', abort); reject(new Error('download aborted')); };
    signal.addEventListener('abort', abort, { once: true });
    Promise.resolve(work).then(value => { signal.removeEventListener('abort', abort); resolve(value); }, error => { signal.removeEventListener('abort', abort); reject(error); });
  });
}

// Shared bounded, progress-aware transfer for firmware and the offline HTML.
// The caller owns the returned bytes and must clear them when no longer needed.
async function readPinnedAsset({ entry, controller, fetcher, idleTimeoutMs, onProgress }) {
  const { name, pin, url } = entry, signal = controller.signal, limit = pin.bytes ?? pin.maxBytes;
  let reader, response, buffer, received = 0, expected = pin.bytes, httpStatus = null, stage = 'CONNECTING', timer, complete = false;
  const detail = () => ({ file: name, stage, httpStatus, receivedBytes: received, expectedBytes: expected });
  const failure = category => new DownloadFailure(category, detail());
  const emit = () => onProgress(detail());
  const arm = () => { clearTimeout(timer); timer = setTimeout(() => controller.abort('IDLE_TIMEOUT'), idleTimeoutMs); };
  const alive = () => { if (signal.aborted) throw failure(signal.reason === 'IDLE_TIMEOUT' ? 'IDLE_TIMEOUT' : 'CANCELLED'); };
  emit(); arm();
  try {
    alive();
    response = await abortable(fetcher.call(globalThis, url, { redirect: 'error', credentials: 'omit', cache: 'no-store', signal }), signal);
    alive(); httpStatus = response.status;
    if (!response.ok || response.redirected) throw failure('HTTP_STATUS');
    if (!response.body) throw failure('INCOMPLETE');
    // Fetch delivers decoded bytes, not compressed Content-Length bytes.
    const encoding = response.headers.get('content-encoding'), length = response.headers.get('content-length');
    if ((!encoding || encoding === 'identity') && length !== null) {
      const n = Number(length);
      if (!Number.isSafeInteger(n) || n < 0 || n > limit || (pin.bytes !== undefined && n !== pin.bytes)) throw failure('LENGTH');
      expected = n;
    }
    reader = response.body.getReader(); buffer = new Uint8Array(limit); stage = 'RECEIVING'; emit();
    while (true) {
      alive(); const { value, done } = await abortable(reader.read(), signal); alive();
      if (done) break;
      if (!(value instanceof Uint8Array)) throw failure('INCOMPLETE');
      if (!value.byteLength) continue;
      if (received + value.byteLength > limit || (expected !== undefined && received + value.byteLength > expected)) throw failure('LENGTH');
      buffer.set(value, received); received += value.byteLength; arm(); emit();
    }
    clearTimeout(timer);
    if (!received || (expected !== undefined && received !== expected)) throw failure('INCOMPLETE');
    stage = 'VERIFYING'; emit();
    const bytes = buffer.subarray(0, received);
    if (await sha256(bytes) !== pin.sha256) throw failure('SHA256');
    alive(); if (pin.md5 && md5(bytes) !== pin.md5) throw failure('MD5');
    alive();
    const result = received === buffer.length ? buffer : bytes.slice();
    if (result !== buffer) buffer.fill(0);
    complete = true; return result;
  } catch (error) {
    throw signal.aborted ? failure(signal.reason === 'IDLE_TIMEOUT' ? 'IDLE_TIMEOUT' : 'CANCELLED')
      : error instanceof DownloadFailure ? error : failure('NETWORK');
  } finally {
    clearTimeout(timer);
    if (!complete) {
      buffer?.fill(0); controller.abort(signal.reason || 'FAILED');
      // A broken body may never acknowledge cancel. Abort and release ownership
      // without waiting for that acknowledgement (or keeping a serial port open).
      try { Promise.resolve(reader ? reader.cancel() : response?.body?.cancel()).catch(() => {}); } catch {}
    }
    try { reader?.releaseLock(); } catch {}
  }
}

export const isDownloadNetworkFailure = error => error instanceof DownloadFailure &&
  ['NETWORK', 'IDLE_TIMEOUT', 'HTTP_STATUS', 'INCOMPLETE'].includes(error.detail.category);

// One public, SHA-pinned file; no cache, device methods or write authority.
export class PinnedAssetDownloader {
  #entry; #fetch; #idle; #notify; #active; #disposed = false;
  #attempt = 0; #history = []; #state = { stage: 'IDLE' };
  constructor({ url, name, sha256: hash, maxBytes, fetcher = fetch, idleTimeoutMs = DOWNLOAD_IDLE_MS, onProgress = () => {} }) {
    requireSafe(/^[A-Za-z0-9_.-]+$/.test(name) && /^[a-f0-9]{64}$/.test(hash), 'DOWNLOAD_CONFIG');
    requireSafe(Number.isSafeInteger(maxBytes) && maxBytes > 0 && maxBytes < 4000000, 'DOWNLOAD_CONFIG');
    requireSafe(Number.isFinite(idleTimeoutMs) && idleTimeoutMs > 0, 'DOWNLOAD_CONFIG');
    this.#entry = { name, url: new URL(url), pin: { sha256: hash, maxBytes } };
    this.#fetch = fetcher; this.#idle = idleTimeoutMs; this.#notify = onProgress;
  }
  snapshot() { return structuredClone({ schema: 'draftor-download/1', attempt: this.#attempt, idleTimeoutMs: this.#idle, totalDeadlineMs: null,
    fileCount: 1, fileIndex: 1, ...this.#state, history: this.#history }); }
  #emit(value) { this.#state = value; this.#notify(this.snapshot()); }
  cancel() { this.#active?.abort('CANCELLED'); }
  dispose() { this.#disposed = true; this.cancel(); }
  async load() {
    requireSafe(!this.#active && !this.#disposed, 'DOWNLOAD_STATE');
    const controller = new AbortController(), started = performance.now(); this.#active = controller; this.#attempt++;
    try {
      const bytes = await readPinnedAsset({ entry: this.#entry, controller, fetcher: this.#fetch, idleTimeoutMs: this.#idle,
        onProgress: value => this.#emit(value) });
      this.#history.push({ attempt: this.#attempt, ...this.#state, elapsedMs: Math.round(performance.now() - started), result: 'VERIFIED' });
      if (this.#history.length > 64) this.#history.shift();
      this.#emit({ ...this.#state, stage: 'READY', verifiedCount: 1 }); return bytes;
    } catch (e) {
      this.#history.push({ attempt: this.#attempt, ...e.detail, elapsedMs: Math.round(performance.now() - started), result: 'FAILED' });
      if (this.#history.length > 64) this.#history.shift();
      this.#emit({ stage: e.detail?.category === 'CANCELLED' ? 'CANCELLED' : 'FAILED', error: e.detail }); throw e;
    } finally { this.#history = this.#history.slice(-64); this.#active = undefined; }
  }
}

// Cache ownership stays inside one preparation session. Nothing enters this cache
// until the actual bytes pass the pinned length, SHA-256 and (where pinned) MD5.
export class ReleaseDownloader {
  #base; #fetch; #idle; #notify; #entries; #cache = new Map(); #active;
  #attempt = 0; #history = []; #current = { stage: 'IDLE' }; #disposed = false;
  constructor({ base = new URL('../firmware/', import.meta.url), fetcher = fetch, idleTimeoutMs = DOWNLOAD_IDLE_MS, onProgress = () => {}, extras = [] } = {}) {
    requireSafe(Number.isFinite(idleTimeoutMs) && idleTimeoutMs > 0, 'DOWNLOAD_CONFIG');
    this.#base = new URL(base); this.#fetch = fetcher; this.#idle = idleTimeoutMs; this.#notify = onProgress;
    this.#entries = Object.entries({ ...PINS.documents, ...PINS.files }).map(([name, pin]) => ({ name, pin, url: new URL(name, this.#base) }));
    for (const extra of extras) {
      requireSafe(/^[A-Za-z0-9_.-]+$/.test(extra.name) && !this.#entries.some(e => e.name === extra.name), 'DOWNLOAD_CONFIG');
      requireSafe(Number.isSafeInteger(extra.pin.bytes) && extra.pin.bytes > 0 && extra.pin.bytes <= PINS.flashBytes && /^[a-f0-9]{64}$/.test(extra.pin.sha256), 'DOWNLOAD_CONFIG');
      this.#entries.push({ name: extra.name, pin: Object.freeze({ ...extra.pin }), url: new URL(extra.url, this.#base) });
    }
    for (const entry of this.#entries) requireSafe(entry.url.origin === this.#base.origin, 'ASSET');
  }
  snapshot() {
    return structuredClone({ schema: 'draftor-download/1', attempt: this.#attempt, idleTimeoutMs: this.#idle, totalDeadlineMs: null,
      ...this.#current, verifiedFiles: [...this.#cache.keys()], history: this.#history });
  }
  #emit(value) { this.#current = { ...value, attempt: this.#attempt, verifiedCount: this.#cache.size, fileCount: this.#entries.length }; this.#notify(this.snapshot()); }
  #remember(value) { this.#history.push(value); if (this.#history.length > 64) this.#history.shift(); }
  cancel() { this.#active?.abort('CANCELLED'); }
  dispose() { this.#disposed = true; this.cancel(); for (const b of this.#cache.values()) b.fill(0); this.#cache.clear(); }
  extraFile(name) { requireSafe(this.#cache.has(name) && !PINS.files[name] && !PINS.documents[name], 'ASSET'); return this.#cache.get(name).slice(); }
  async #read(entry, controller, index) {
    const started = performance.now();
    try {
      const bytes = await readPinnedAsset({ entry, controller, fetcher: this.#fetch, idleTimeoutMs: this.#idle,
        onProgress: value => this.#emit({ ...value, fileIndex: index + 1 }) });
      this.#cache.set(entry.name, bytes);
      const detail = { file: entry.name, stage: 'VERIFIED', receivedBytes: bytes.length, expectedBytes: entry.pin.bytes };
      this.#emit({ ...detail, fileIndex: index + 1 });
      this.#remember({ attempt: this.#attempt, ...detail, elapsedMs: Math.round(performance.now() - started), result: 'VERIFIED' });
    } catch (e) {
      this.#remember({ attempt: this.#attempt, ...e.detail, elapsedMs: Math.round(performance.now() - started), result: 'FAILED' });
      throw e;
    }
  }
  async load({ signal } = {}) {
    requireSafe(!this.#active && !this.#disposed, 'DOWNLOAD_STATE');
    const controller = new AbortController(); this.#active = controller; this.#attempt++;
    const abort = () => controller.abort('CANCELLED');
    signal?.addEventListener('abort', abort, { once: true }); if (signal?.aborted) abort();
    try {
      for (let index = 0; index < this.#entries.length; index++) {
        if (controller.signal.aborted) throw new DownloadFailure('CANCELLED', { file: this.#entries[index].name, stage: 'CONNECTING', httpStatus: null });
        const entry = this.#entries[index];
        if (this.#cache.has(entry.name)) {
          this.#emit({ file: entry.name, stage: 'REUSED', fileIndex: index + 1, receivedBytes: entry.pin.bytes, expectedBytes: entry.pin.bytes });
          this.#remember({ attempt: this.#attempt, file: entry.name, stage: 'REUSED', result: 'VERIFIED_CACHE' });
        } else await this.#read(entry, controller, index);
      }
      this.#emit({ stage: 'VERIFYING_RELEASE' });
      let release;
      try { release = await verifyRelease(Object.fromEntries(this.#cache)); }
      catch { if (controller.signal.aborted) throw new DownloadFailure('CANCELLED', { file: null, stage: 'VERIFYING_RELEASE', httpStatus: null }); for (const b of this.#cache.values()) b.fill(0); this.#cache.clear(); throw new DownloadFailure('RELEASE', { file: null, stage: 'VERIFYING_RELEASE', httpStatus: null }); }
      if (controller.signal.aborted) throw new DownloadFailure('CANCELLED', { file: null, stage: 'VERIFYING_RELEASE', httpStatus: null });
      this.#emit({ stage: 'READY' }); return release;
    } catch (error) {
      const e = error instanceof DownloadFailure ? error : new DownloadFailure('NETWORK', { file: null, stage: this.#current.stage, httpStatus: null });
      this.#emit({ stage: e.detail.category === 'CANCELLED' ? 'CANCELLED' : 'FAILED', error: e.detail }); throw e;
    } finally { signal?.removeEventListener('abort', abort); this.#active = undefined; }
  }
}

export async function loadRelease(base = new URL('../firmware/', import.meta.url), fetcher = fetch, options = {}) {
  const loader = new ReleaseDownloader({ ...options, base, fetcher });
  try { return await loader.load({ signal: options.signal }); } finally { loader.dispose(); }
}

export function tableMatchesRelease(release, actual) {
  checkPartitions(actual);
  requireSafe(bytesEqual(actual.slice(0, 3072), releaseFile(release, 'partitions.bin')), 'PARTITIONS');
}
