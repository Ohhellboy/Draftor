import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { StrictSerialTransport } from '../src/serial.js';
import { S3Rom } from '../src/rom.js';
import { RomDevicePort } from './rom-device.mjs';

// Real private response stays outside the repository and test output.
const captured = process.env.DRAFTOR_CAPTURED_ROM_MD5
  ? new Uint8Array(await readFile(process.env.DRAFTOR_CAPTURED_ROM_MD5)) : undefined;
const flash = new Uint8Array(0x10000).fill(0x5a);
const digest = createHash('md5').update(flash.subarray(0x8000, 0x9000)).digest('hex');
const synthetic = new Uint8Array(44);
synthetic.set([1, 0x13, 36, 0]);
synthetic.set(new TextEncoder().encode(digest), 8);
synthetic.set([0, 0, 255, 255], 40);
if (captured) {
  assert.equal(captured.length, 44);
  assert.deepEqual(captured.slice(-4), Uint8Array.of(0, 0, 255, 255));
}
const baseline = captured || synthetic;

async function setup(transform = p => p, allReserved = false) {
  const port = new RomDevicePort(flash);
  const wire = new StrictSerialTransport(port);
  const read = wire.read.bind(wire);
  wire.read = async timeout => {
    const packet = await read(timeout);
    if (packet[1] === 0x13) return transform(baseline.slice());
    if (allReserved) packet.set([0x7e, 0xff], packet.length - 2);
    return packet;
  };
  const rom = new S3Rom(wire); // Default write lock stays in force.
  await rom.connect();
  return { rom, port };
}

test('MD5 accepts 00 00 FF FF, ignores reserved bytes for all ROM operations, stays write-locked', async () => {
  const { rom, port } = await setup(p => p, true);
  try {
    const got = await rom.flashMd5(0x8000, 4096);
    assert.equal(got, new TextDecoder().decode(baseline.slice(8, 40)).toLowerCase());
    assert.equal(got.length, 32);
    assert.deepEqual(await rom.readFlash(0x8000, 64), flash.slice(0x8000, 0x8040));
    await assert.rejects(rom.writeImage(0x10000, new Uint8Array(24), {mode:'update',slotOffset:0x10000}), {code:'DEVICE_WRITES_LOCKED'});
    assert(!port.counts.has(2) && !port.counts.has(3) && !port.counts.has(4));
    assert.equal(port.signalCount, 0);
  } finally { await rom.disconnect(); }
});

test('actual saved ROM MD5 response replays through the unchanged driver API', {skip: !captured}, async () => {
  const { rom, port } = await setup();
  try {
    assert.equal(await rom.flashMd5(0x8000, 4096), new TextDecoder().decode(captured.slice(8, 40)));
    assert.equal(port.counts.get(0x13), 1);
  } finally { await rom.disconnect(); }
});

test('official semantics: reason byte is ignored when status is success', async () => {
  const { rom } = await setup(p => { p[41] = 0x08; return p; });
  try { assert.match(await rom.flashMd5(0x8000, 4096), /^[0-9a-f]{32}$/); }
  finally { await rom.disconnect(); }
});

const faults = [
  ...[1, 2, 0x7f, 0xff].map(status => [`error status ${status}`, p => {p[40]=status;p[41]=5;return p;}, 'ROM_STATUS']),
  ['wrong direction', p => {p[0]=0;return p;}, 'PROTOCOL'],
  ['wrong opcode', p => {p[1]=0x14;return p;}, 'PROTOCOL'],
  ['wrong declared length', p => {p[2]=34;return p;}, 'PROTOCOL'],
  ['oversized frame', p => Uint8Array.from([...p,0]), 'PROTOCOL'],
  ['non-ASCII-hex digest', p => {p[8]=0xff;return p;}, 'PROTOCOL'],
  ...Array.from({length:44}, (_, n) => [`truncated frame ${n}/44`, p => p.slice(0,n), 'PROTOCOL']),
];
for (const [name, change, code] of faults) test(`${name}: terminal rejection, no retry or write`, async () => {
  const { rom, port } = await setup(change);
  try {
    await assert.rejects(rom.flashMd5(0x8000,4096),{code});
    const sent=port.transcript.length;
    await assert.rejects(rom.readFlash(0x8000,64),{code:'STATE'});
    assert.equal(port.transcript.length,sent);
    assert(!port.counts.has(2) && !port.counts.has(3) && !port.counts.has(4));
    assert.equal(port.signalCount,0);
  } finally { await rom.disconnect(); }
});
