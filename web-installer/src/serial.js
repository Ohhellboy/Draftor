// SPDX-License-Identifier: Apache-2.0
// Controlled adapter over esptool-js 0.7.1's SLIP encoder. Its high-level loader,
// reset strategies and retrying read loop are deliberately not invoked.
import { Transport } from 'esptool-js/lib/webserial.js';
import { requireSafe, Stop } from './safety.js';

export class StrictSerialTransport {
  #port; #codec; #reader; #writer; #pendingRead; #opened = false; #dead = false; #bytes = []; #inFrame = false; #escape = false; #packets = []; #writeTimeout; #closeTimeout;
  constructor(port, { writeTimeout = 10000, closeTimeout = 2000 } = {}) {
    this.#port = port; this.#codec = new Transport(port, false); this.#writeTimeout = writeTimeout; this.#closeTimeout = closeTimeout;
  }
  async #bounded(promise, ms) {
    let timer;
    try { return await Promise.race([promise, new Promise((_, reject) => { timer = setTimeout(() => reject(new Stop('TIMEOUT')), ms); })]); }
    finally { clearTimeout(timer); }
  }
  async #chunk(ms, quietAllowed = false) {
    if (!this.#pendingRead) this.#pendingRead = this.#reader.read();
    try {
      const chunk = await this.#bounded(this.#pendingRead, ms);
      this.#pendingRead = undefined; return chunk;
    } catch (error) {
      // A quiet initial port leaves one read pending, reused by the first SYNC.
      if (quietAllowed && error instanceof Stop && error.code === 'TIMEOUT') return undefined;
      throw error;
    }
  }
  getInfo() { return this.#port.getInfo(); }
  async connect() {
    requireSafe(!this.#opened && !this.#dead, 'STATE');
    try {
      await this.#port.open({ baudRate: 115200, dataBits: 8, stopBits: 1, parity: 'none', flowControl: 'none', bufferSize: 65536 });
      this.#opened = true;
      requireSafe(this.#port.readable && this.#port.writable, 'IO');
      this.#reader = this.#port.readable.getReader();
      // Drain a bounded ROM boot banner before sending the ONE SYNC. There is
      // no command retry and no relaxation of framing once synchronization starts.
      const deadline = Date.now() + 500; let discarded = 0;
      while (true) {
        requireSafe(Date.now() < deadline, 'PROTOCOL');
        const chunk = await this.#chunk(Math.min(50, deadline - Date.now()), true);
        if (!chunk) break;
        requireSafe(!chunk.done && chunk.value, 'DISCONNECTED');
        discarded += chunk.value.length; requireSafe(discarded <= 4096, 'PROTOCOL');
      }
    } catch { this.#dead = true; throw new Stop('CONNECTION'); }
  }
  async write(packet) {
    requireSafe(this.#opened && !this.#dead && this.#port.writable, 'IO');
    const writer = this.#port.writable.getWriter();
    this.#writer = writer;
    try {
      await this.#bounded(writer.write(this.#codec.slipWriter(packet)), this.#writeTimeout);
      requireSafe(!this.#dead, 'CANCELLED');
    } catch { this.#dead = true; writer.abort().catch(() => {}); throw new Stop('IO'); }
    finally { writer.releaseLock(); this.#writer = undefined; }
  }
  #consume(chunk) {
    requireSafe(chunk instanceof Uint8Array && chunk.length <= 65536, 'PROTOCOL');
    for (const value of chunk) {
      if (value === 0xc0) {
        requireSafe(!this.#escape, 'PROTOCOL');
        if (this.#inFrame && this.#bytes.length) {
          this.#packets.push(Uint8Array.from(this.#bytes));
          requireSafe(this.#packets.length <= 8, 'PROTOCOL');
        }
        this.#inFrame = true; this.#bytes = []; continue;
      }
      requireSafe(this.#inFrame, 'PROTOCOL');
      if (this.#escape) {
        requireSafe(value === 0xdc || value === 0xdd, 'PROTOCOL');
        this.#bytes.push(value === 0xdc ? 0xc0 : 0xdb); this.#escape = false;
      } else if (value === 0xdb) this.#escape = true;
      else this.#bytes.push(value);
      requireSafe(this.#bytes.length <= 512, 'PROTOCOL');
    }
  }
  async read(timeout = 10000) {
    requireSafe(this.#opened && !this.#dead, 'IO');
    const deadline = Date.now() + timeout;
    try {
      while (!this.#packets.length) {
        requireSafe(Date.now() < deadline, 'TIMEOUT');
        const chunk = await this.#chunk(Math.max(1, deadline - Date.now()));
        requireSafe(!chunk.done && chunk.value, 'DISCONNECTED');
        this.#consume(chunk.value);
      }
      return this.#packets.shift();
    } catch (error) {
      this.#dead = true;
      this.#reader?.cancel().catch(() => {});
      throw error instanceof Stop ? error : new Stop('IO');
    }
  }
  cancel() { this.#dead = true; this.#reader?.cancel().catch(() => {}); this.#writer?.abort().catch(() => {}); }
  async disconnect() {
    this.#dead = true;
    if (this.#reader) {
      // An errored stream rejects cancel with its original I/O error. It is
      // already terminal; still release its lock and close the selected port.
      await this.#bounded(this.#reader.cancel().catch(() => {}), this.#closeTimeout);
      this.#reader.releaseLock(); this.#reader = undefined;
    }
    if (this.#opened) { await this.#bounded(this.#port.close(), this.#closeTimeout); this.#opened = false; }
    // Never setSignals, reset, reconnect or send another protocol command.
  }
}
