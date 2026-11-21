// Independent synthetic Web Serial/ROM fixture. No OS port enumeration or USB.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';

const word = (b, at = 0) => new DataView(b.buffer, b.byteOffset, b.byteLength).getUint32(at, true);
function words(...values) { const b = new Uint8Array(values.length * 4); values.forEach((v, i) => new DataView(b.buffer).setUint32(i * 4, v, true)); return b; }
function encode(b) { const out = [192]; for (const n of b) out.push(...(n === 192 ? [219, 220] : n === 219 ? [219, 221] : [n])); out.push(192); return Uint8Array.from(out); }
function decode(b) {
  assert.equal(b[0], 192); assert.equal(b.at(-1), 192);
  const out = [];
  for (let i = 1; i < b.length - 1; i++) {
    if (b[i] === 219) { i++; assert([220, 221].includes(b[i])); out.push(b[i] === 220 ? 192 : 219); }
    else out.push(b[i]);
  }
  return Uint8Array.from(out);
}
export class RomDevicePort {
  transcript = []; flash; registers = new Map(); openCount = 0; closeCount = 0; signalCount = 0; counts = new Map();
  constructor(flash, options = {}) { this.flash = flash; this.options = options; }
  getInfo() { return this.options.usb || { usbVendorId: 0x303a, usbProductId: 0x1001 }; }
  async open(options) {
    this.openCount++; this.serialOptions = options;
    if(this.options.openError)throw Error('Synthetic open failure');
    this.readable = new ReadableStream({ start: c => {
      this.controller = c;
      if (this.options.bootBanner) c.enqueue(new TextEncoder().encode('ESP-ROM:esp32s3-20210327\r\nwaiting for download\r\n'));
    }, cancel: () => { this.cancelled = true; } });
    this.writable = new WritableStream({ write: b => this.#receive(b), abort: () => { this.aborted = true; } });
  }
  async close() { this.closeCount++; if(this.options.closeError)throw Error('Synthetic close failure'); this.readable = null; this.writable = null; }
  async setSignals() { this.signalCount++; throw new Error('No signal changes allowed'); }
  #receive(slip) {
    const b = decode(slip), op = b[1], p = b.slice(8);
    assert.equal(b[0], 0); assert.equal(b[2] | b[3] << 8, p.length);
    const nth = (this.counts.get(op) || 0) + 1; this.counts.set(op, nth);
    this.transcript.push({ op, size: p.length, payload: Array.from(p.slice(0, 24)), checksum: word(b, 4) });
    if (this.options.throwOn?.op === op && (this.options.throwOn.nth || 1) === nth) throw new Error('Synthetic I/O error');
    if (this.options.hangWriteOn === op) return new Promise(() => {});
    let data = new Uint8Array(), value = 0, responses = 1;
    if (op === 8) {
      assert.deepEqual(p.slice(0, 4), Uint8Array.of(7, 7, 18, 32)); assert.equal(p.length, 36); assert(p.slice(4).every(n => n === 85));
      responses = this.options.syncResponses ?? 8; value = this.options.stub ? 0 : 0x20120707;
    } else if (op === 20) {
      assert.equal(p.length, 0); data = new Uint8Array(20); data.set(words(this.options.securityFlags || 0)); data[4] = this.options.cryptCount || 0;
      data.set(words(this.options.chipId ?? 9, 0), 12);
    } else if (op === 9) {
      assert.equal(p.length, 16); assert.equal(word(p, 8), 0xffffffff); assert.equal(word(p, 12), 0);
      this.registers.set(word(p), word(p, 4));
      if (word(p) === 0x60002000) {
        assert.equal(this.registers.get(0x60002020), 0x7000009f);
        this.registers.set(0x60002058, this.options.jedec ?? 0x1840ef);
        this.registers.set(0x60002000, this.options.spiBusy ? 1 << 18 : 0);
      }
    } else if (op === 10) {
      assert.equal(p.length, 4);
      value = word(p) === 0x60007044 ? (this.options.macLo ?? 1) : word(p) === 0x60007048 ? 0x0200 : this.registers.get(word(p)) || 0;
    } else if (op === 13) {
      assert.deepEqual(p, words(0, 0));
    } else if (op === 11) {
      assert.deepEqual(p, words(0, 0x1000000, 0x10000, 0x1000, 256, 0xffff));
    } else if (op === 14) {
      assert.equal(p.length, 8); const start = word(p), size = word(p, 4);
      assert(size >= 1 && size <= 64); assert(start + size <= this.flash.length);
      data = new Uint8Array(64); data.set(this.flash.subarray(start, start + size));
    } else if (op === 19) {
      assert.equal(p.length, 16); assert.equal(word(p, 8), 0); assert.equal(word(p, 12), 0);
      const digest = createHash('md5').update(this.flash.subarray(word(p), word(p) + word(p, 4))).digest('hex');
      data = new TextEncoder().encode(this.options.badDigest ? 'z'.repeat(32) : digest);
    } else if (op === 2) {
      assert.equal(p.length, 20); const size = word(p), blocks = word(p, 4), blockSize = word(p, 8), offset = word(p, 12);
      assert.equal(blockSize, 1024); assert.equal(word(p, 16), 0); assert.equal(blocks, Math.ceil(size / 1024));
      if(this.options.fullRecovery) {assert.equal(nth,1);assert.equal(offset,0);assert.equal(size,16777216);assert.equal(blocks,16384);}
      else if (this.options.firstInstall) {
        // Independently specified expected protocol: erase, then four release files.
        const expected = [[0,16777216],[0,19968],[32768,3072],[57344,8192],[65536,4021152]][nth - 1];
        assert.deepEqual([offset,size], expected);
      } else { assert([0x10000, 0x410000].includes(offset)); assert(size <= 0x400000); }
      this.pending = { offset, blocks, next: 0, size }; this.flash.fill(255, offset, offset + Math.ceil(size / 4096) * 4096);
    } else if (op === 3) {
      assert(this.pending); assert.equal(p.length, 1040); assert.equal(word(p), 1024); assert.equal(word(p, 4), this.pending.next);
      assert.equal(word(p, 8), 0); assert.equal(word(p, 12), 0);
      let checksum = 239; for (const v of p.slice(16)) checksum ^= v;
      assert.equal(word(b, 4), checksum);
      this.flash.set(p.slice(16), this.pending.offset + this.pending.next * 1024); this.pending.next++;
      if (this.options.corruptWrite && this.pending.next === this.pending.blocks) this.flash[this.pending.offset] ^= 1;
    } else throw new Error(`Forbidden or unsupported ROM op: ${op}`);
    for (let i = 0; i < responses; i++) {
      const reply = new Uint8Array(12 + data.length); reply[0] = 1; reply[1] = op;
      new DataView(reply.buffer).setUint16(2, data.length + 4, true); reply.set(words(value), 4); reply.set(data, 8);
      if (this.options.statusOn === op) reply[reply.length - 4] = 1;
      if (this.options.malformedOn === op) reply[2]++;
      if (this.options.wrongOpOn === op) reply[1]++;
      if (this.options.reservedFF) reply.set([255,255], reply.length - 2);
      const output = encode(this.options.truncatedOn === op ? reply.slice(0, -1) : reply);
      if (this.options.emptySlip) this.controller.enqueue(Uint8Array.of(192,192));
      if (this.options.fragment) { for (let j = 0; j < output.length; j += 3) this.controller.enqueue(output.slice(j, j + 3)); }
      else this.controller.enqueue(output);
    }
    if(this.options.yieldEvery&&nth%this.options.yieldEvery===0)return new Promise(resolve=>setTimeout(resolve,0));
  }
}
