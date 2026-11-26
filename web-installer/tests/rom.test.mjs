import test from 'node:test';
import assert from 'node:assert/strict';
import { writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { verifyReleaseInputs } from '../tools/verify-release.mjs';
import { StrictSerialTransport } from '../src/serial.js';
import { S3Rom, parseSecurity } from '../src/rom.js';
import { InstallSession } from '../src/session.js';
import { SimulatedFlash, SIM_MAC } from '../src/simulator.js';
import { RomDevicePort } from './rom-device.mjs';

const { release, inputs } = await verifyReleaseInputs({ verifyDesignBrief: false });
const approve = { backupConfirmed: true, tfRemoved: true, planConfirmed: true };
function setup(scenario = 'app0', options = {}, allowUpdate = true) {
  const flash = new SimulatedFlash(release, scenario).flash;
  const port = new RomDevicePort(flash, options), wire = new StrictSerialTransport(port, { writeTimeout: 100, closeTimeout: 100 });
  const shortWire = { connect: () => wire.connect(), write: b => wire.write(b), read: t => wire.read(Math.min(t, 200)), getInfo: () => wire.getInfo(), disconnect: () => wire.disconnect(), cancel: () => wire.cancel() };
  const rom = new S3Rom(shortWire, { allowUpdate });
  return { flash, port, wire, rom };
}

for (const slot of [0, 1]) test(`raw Web Serial ROM integration: app${slot}, 64-byte reads, exact writes, independent MD5, no stub/reset/erase`, async () => {
  const { rom, port, flash } = setup(`app${slot}`, { bootBanner: true });
  const offset = slot === 0 ? 0x10000 : 0x410000;
  const beforePrefix = createHash('sha256').update(flash.subarray(0, offset)).digest('hex');
  const beforeSuffix = createHash('sha256').update(flash.subarray(offset + 0x400000)).digest('hex');
  const session = new InstallSession(rom, release, { hardwareWritesEnabled: true });
  const plan = await session.prepare({ boardConfirmed: true, expectedMac: SIM_MAC });
  assert.equal(plan.writes[0].offset, offset);
  const receipt = await session.execute(approve);
  assert.equal(receipt.status, 'VERIFIED_BOOT_PENDING');
  assert.equal(port.counts.get(8), 1); // eight replies, exactly ONE request
  assert.equal(port.counts.get(2), 1); assert.equal(port.counts.get(3), Math.ceil(4021152 / 1024));
  assert.deepEqual(flash.subarray(offset, offset + 4021152), inputs['app.bin']);
  assert.equal(createHash('sha256').update(flash.subarray(0, offset)).digest('hex'), beforePrefix);
  assert.equal(createHash('sha256').update(flash.subarray(offset + 0x400000)).digest('hex'), beforeSuffix);
  assert(port.transcript.every(t => [2, 3, 8, 9, 10, 11, 13, 14, 19, 20].includes(t.op)));
  assert.equal(port.transcript.filter(t => t.op === 13)[0].size, 8);
  const begin = port.transcript.find(t => t.op === 2); assert.equal(begin.size, 20);
  assert.equal(new DataView(Uint8Array.from(begin.payload).buffer).getUint32(12, true), offset);
  assert.equal(port.signalCount, 0); assert.equal(port.openCount, 1);
  await session.close(); assert.equal(port.closeCount, 1); assert.equal(port.signalCount, 0);
  const histogram = Object.fromEntries([...port.counts].map(([op, count]) => [`0x${op.toString(16)}`, count]));
  await writeFile(`evidence/rom-app${slot}.json`, JSON.stringify({ result: 'PASS', physicalDeviceAccess: false, fixture: 'synthetic Web Serial streams', receipt, opcodeCounts: histogram, begin, blockSize: 1024, noFinishCommand: true, noSignals: true }, null, 2) + '\n');
});

test('SLIP escaped c0/db and fragmented frames, including a short slow-read tail', async () => {
  const { rom, port, flash } = setup('app0', { fragment: true, bootBanner: true });
  await rom.connect(); assert.equal((await rom.identity()).mac, SIM_MAC);
  const data = await rom.readFlash(0x9000, 67); assert.equal(data.length, 67); assert.deepEqual(data, flash.slice(0x9000, 0x9043));
  assert.equal(port.transcript.filter(t => t.op === 14).length, 2);
  const digest = await rom.flashMd5(0x9000, 67);
  assert.equal(digest, createHash('md5').update(data).digest('hex')); assert.equal(digest.length, 32);
  await rom.disconnect();
});
test('copy-before-hash prevents caller mutation from changing verified write bytes', async () => {
  const { rom, flash } = setup(); await rom.connect();
  const bytes = inputs['app.bin'].slice(), write = rom.writeImage(0x10000, bytes, { mode: 'update', slotOffset: 0x10000 });
  bytes[0] ^= 1; await write;
  assert.deepEqual(flash.subarray(0x10000, 0x10000 + 4021152), inputs['app.bin']); await rom.disconnect();
});
test('direct adapter concurrent writes reserve the single write before async hashing', async () => {
  const { rom, port } = setup(); await rom.connect();
  const first = rom.writeImage(0x10000, inputs['app.bin'], { mode: 'update', slotOffset: 0x10000 });
  await assert.rejects(rom.writeImage(0x10000, inputs['app.bin'], { mode: 'update', slotOffset: 0x10000 }), { code: 'RANGE' });
  await first; assert.equal(port.counts.get(2), 1); await rom.disconnect();
});
test('cancel after first ROM block prevents all later Flash packets', async () => {
  const { rom, port } = setup('app1'); await rom.connect();
  await assert.rejects(rom.writeImage(0x410000, inputs['app.bin'], { mode: 'update', slotOffset: 0x410000, progress: () => rom.cancel() }));
  assert.equal(port.counts.get(2), 1); assert.equal(port.counts.get(3), 1); assert.equal(port.signalCount, 0); await rom.disconnect();
});

for (const [label, options, code] of [
  ['stub', { stub: true }, 'STUB'], ['wrong chip', { chipId: 5 }, 'CHIP'], ['secure boot', { securityFlags: 1 }, 'SECURITY'],
  ['secure download', { securityFlags: 4 }, 'SECURITY'], ['unknown flags', { securityFlags: 0x800 }, 'SECURITY'],
  ['encryption', { cryptCount: 1 }, 'SECURITY'], ['unknown count', { cryptCount: 8 }, 'SECURITY'],
  ['8MiB', { jedec: 0x1740ef }, 'FLASH'], ['unknown JEDEC', { jedec: 0xffffff }, 'FLASH'],
  ['SPI busy', { spiBusy: true }, 'FLASH'], ['ROM read register failure', { statusOn: 10 }, 'ROM_STATUS'],
  ['wrong response opcode', { wrongOpOn: 20 }, 'PROTOCOL'], ['truncated security', { truncatedOn: 20 }, 'PROTOCOL'],
  ['declared length mismatch', { malformedOn: 20 }, 'PROTOCOL'], ['only seven SYNC replies', { syncResponses: 7 }, 'TIMEOUT'],
]) test(`ROM ${label}: terminal refusal, no Flash writes`, async () => {
  const { rom, port } = setup('app0', options);
  await assert.rejects(rom.connect(), { code });
  const count = port.transcript.length;
  await assert.rejects(rom.readFlash(0x8000, 64)); assert.equal(port.transcript.length, count);
  assert(!port.counts.has(2) && !port.counts.has(3)); assert.equal(port.signalCount, 0);
  await rom.disconnect();
});
test('unsupported USB transport is refused before opening any port', async () => {
  const { rom, port } = setup('app0', { usb: { usbVendorId: 1, usbProductId: 9 } });
  await assert.rejects(rom.connect(), { code: 'USB_TRANSPORT' }); assert.equal(port.openCount, 0);
});
for (const count of [0, 3, 5, 6]) test(`security crypt-count ${count} is known disabled (even parity)`, () => {
  const data = new Uint8Array(20); data[4] = count; data[12] = 9; assert.equal(parseSecurity(data).flashEncryption, false);
});
for (const count of [1, 2, 4, 7, 8, 255]) test(`security crypt-count ${count} refuses`, () => {
  const data = new Uint8Array(20); data[4] = count; data[12] = 9; assert.throws(() => parseSecurity(data), { code: 'SECURITY' });
});
test('unknown/truncated security structure is never treated as disabled', () => { assert.throws(() => parseSecurity(new Uint8Array(12)), { code: 'SECURITY' }); });

for (const kind of ['status', 'throw', 'hang', 'corrupt']) test(`ROM write ${kind}: one attempt, STOP, no finish/reset/fallback`, async () => {
  const options = kind === 'status' ? { statusOn: 3 } : kind === 'throw' ? { throwOn: { op: 3 } } : kind === 'hang' ? { hangWriteOn: 3 } : { corruptWrite: true };
  const { rom, port } = setup('app0', options); const s = new InstallSession(rom, release, { hardwareWritesEnabled: true });
  await s.prepare({ boardConfirmed: true, expectedMac: SIM_MAC }); await assert.rejects(s.execute(approve));
  assert.equal(s.state, 'STOP'); assert.equal(s.receipt().independentVerification, false); assert.equal(s.receipt().flashOutcome, 'POSSIBLY_PARTIAL');
  assert.equal(port.counts.get(2), 1); assert.equal(port.counts.get(3), kind === 'corrupt' ? 3927 : 1);
  assert(!port.counts.has(4) && !port.counts.has(0xd0)); assert.equal(port.signalCount, 0);
  await rom.disconnect();
});
test('non-hex ROM MD5 is fatal and cannot be hex-encoded into a false match', async () => {
  const { rom, port } = setup('app0', { badDigest: true }); await rom.connect();
  await assert.rejects(rom.flashMd5(0x8000, 4096), { code: 'PROTOCOL' }); const n = port.transcript.length;
  await assert.rejects(rom.readFlash(0x8000, 64)); assert.equal(port.transcript.length, n); await rom.disconnect();
});
test('actual adapter refuses fresh, wrong offsets, altered App, and default-locked writes', async () => {
  for (const spec of [
    { offset: 0x10000, mode: 'fresh', allowed: true }, { offset: 0x8000, mode: 'update', allowed: true },
    { offset: 0x10000, mode: 'update', allowed: false }, { offset: 0x10000, mode: 'update', allowed: true, tamper: true },
  ]) {
    const { rom, port } = setup('app0', {}, spec.allowed); await rom.connect(); const b = inputs['app.bin'].slice(); if (spec.tamper) b[0] = 0;
    await assert.rejects(rom.writeImage(spec.offset, b, { mode: spec.mode, slotOffset: spec.offset }));
    assert(!port.counts.has(2) && !port.counts.has(3)); await rom.disconnect();
  }
});
test('read timeout, read stream errors and noise become terminal failures', async () => {
  for (const fault of ['timeout', 'error', 'noise']) {
    const p = new RomDevicePort(new Uint8Array(0x1000000)); const t = new StrictSerialTransport(p, { closeTimeout: 30 }); await t.connect();
    if (fault === 'error') p.controller.error(new Error('FramingError'));
    if (fault === 'noise') p.controller.enqueue(Uint8Array.of(1, 2, 3));
    await assert.rejects(t.read(20)); await assert.rejects(t.write(Uint8Array.of(0, 8))); await t.disconnect();
  }
});
test('blocked port close is bounded, with no retry or reset', async () => {
  const p = new RomDevicePort(new Uint8Array(0x1000000)); p.close = () => new Promise(() => {});
  const t = new StrictSerialTransport(p, { closeTimeout: 20 }); await t.connect();
  await assert.rejects(t.disconnect(), { code: 'TIMEOUT' }); assert.equal(p.signalCount, 0);
});
