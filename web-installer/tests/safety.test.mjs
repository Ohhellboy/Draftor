import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, writeFile, readFile } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { verifyReleaseInputs } from '../tools/verify-release.mjs';
import { verifyRelease, releaseFile, loadRelease } from '../src/firmware.js';
import { activeSlot, checkPartitions, le32, otaCrc } from '../src/safety.js';
import { InstallSession } from '../src/session.js';
import { SimulatedFlash, otaFixture, SIM_MAC } from '../src/simulator.js';
import { PINS } from '../src/pins.js';

const { release, inputs } = await verifyReleaseInputs();
const confirmation = { boardConfirmed: true, expectedMac: SIM_MAC };
const approve = { backupConfirmed: true, tfRemoved: true, planConfirmed: true };
const forbiddenWrites = d => d.trace.filter(t => ['write', 'eraseAll'].includes(t.op));
function setup(scenario = 'app0', opts = {}) { const d = new SimulatedFlash(release, scenario); return [d, new InstallSession(d, release, opts)]; }

test('exact release four files and immutable verified storage', async () => {
  assert.equal(release.appSha256, '165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd');
  const copy = releaseFile(release, 'app.bin'); copy[0] = 0;
  assert.equal(releaseFile(release, 'app.bin')[0], 0xe9);
  checkPartitions(inputs['partitions.bin']); assert.equal(activeSlot(inputs['boot_app0.bin']), 0);
});
for (const name of Object.keys({ ...PINS.files, ...PINS.documents })) test(`reject tampered ${name}`, async () => {
  const bad = { ...inputs, [name]: inputs[name].slice() }; bad[name][0] ^= 1;
  await assert.rejects(verifyRelease(bad), { code: 'ASSET' });
});
test('reject truncated / oversized App and forged release handle before opening a port', async () => {
  for (const bytes of [inputs['app.bin'].slice(0, -1), new Uint8Array(0x400001)]) await assert.rejects(verifyRelease({ ...inputs, 'app.bin': bytes }), { code: 'ASSET' });
  const d = new SimulatedFlash(release), s = new InstallSession(d, { version: PINS.version });
  await assert.rejects(s.prepare(confirmation), { code: 'ASSET' }); assert.equal(d.trace.length, 0);
});
for (const fault of ['reject', 'http', 'redirect', 'truncated', 'oversized', 'sha', 'stream-error']) test(`firmware fetch ${fault} stops without retry`, async () => {
  let calls = 0;
  await assert.rejects(loadRelease(new URL('http://localhost/firmware/'), async (url, options) => {
    calls++; assert.equal(options.redirect, 'error'); assert.equal(options.credentials, 'omit');
    if (fault === 'reject') throw new Error('synthetic connection loss');
    if (fault === 'http') return new Response('', { status: 500 });
    const source = inputs[url.pathname.split('/').at(-1)].slice();
    if (fault === 'redirect') return { ok: true, redirected: true, body: {} };
    if (fault === 'truncated') return new Response(source.slice(0, -1));
    if (fault === 'oversized') return new Response(new Uint8Array(source.length + 1));
    if (fault === 'sha') source[0] ^= 1;
    if (fault === 'stream-error') return new Response(new ReadableStream({ start(c) { c.enqueue(source.slice(0, 10)); c.error(new Error('interrupt')); } }));
    return new Response(source);
  }));
  assert.equal(calls, 1); // Per-file verification fails immediately.
});

for (const slot of [0, 1]) test(`app${slot}: exact active-slot update, independent verify and all other Flash preserved`, async () => {
  const [d, s] = setup(`app${slot}`); const before = d.flash.slice();
  const plan = await s.prepare(confirmation); const offset = slot ? 0x410000 : 0x10000;
  assert.equal(plan.writes.length, 1); assert.equal(plan.writes[0].offset, offset); assert.equal(plan.fullErase, false);
  assert.deepEqual(forbiddenWrites(d), []);
  const receipt = await s.execute(approve);
  assert.equal(receipt.status, 'VERIFIED_BOOT_PENDING'); assert.equal(receipt.deviceQualification, 'DEVICE_PENDING');
  assert.equal(receipt.tfResources, 'NOT_CHECKED'); assert.equal(receipt.protectedRegionsUnchanged, true);
  assert.deepEqual(d.flash.slice(0, offset), before.slice(0, offset));
  assert.deepEqual(d.flash.slice(offset + 0x400000), before.slice(offset + 0x400000));
  assert.deepEqual(d.flash.slice(offset, offset + 4021152), inputs['app.bin']);
  assert.equal(forbiddenWrites(d).length, 1); assert.equal(forbiddenWrites(d)[0].op, 'write');
  assert.equal(JSON.stringify(receipt).includes(SIM_MAC), false); assert.equal(JSON.stringify(receipt).includes(SIM_MAC.replaceAll(':', '')), false);
  await mkdir('evidence/plans', { recursive: true }); await writeFile(`evidence/plans/app${slot}.json`, JSON.stringify({ plan, receipt, syntheticCommandTrace: d.trace }, null, 2) + '\n');
});

for (const [scenario, code] of [['partition', 'PARTITIONS'], ['pending', 'OTA'], ['unknown', 'UNKNOWN'], ['flash8', 'FLASH'], ['security', 'SECURITY']]) test(`${scenario}: STOP with no write or fallback`, async () => {
  const [d, s] = setup(scenario); await assert.rejects(s.prepare(confirmation), { code });
  assert.equal(s.state, 'STOP'); assert.deepEqual(forbiddenWrites(d), []);
  await assert.rejects(s.execute(approve)); assert.deepEqual(forbiddenWrites(d), []);
});
for (const [field, value, code] of [['chip', 'ESP32', 'CHIP'], ['mac', '02:00:00:00:00:02', 'IDENTITY'], ['secureBoot', true, 'SECURITY'], ['flashEncryption', true, 'SECURITY']]) test(`device ${field} mismatch refuses`, async () => {
  const [d, s] = setup(); d.info[field] = value; await assert.rejects(s.prepare(confirmation), { code }); assert.deepEqual(forbiddenWrites(d), []);
});
test('same chip family without physical board confirmation never opens port', async () => {
  const [d, s] = setup(); await assert.rejects(s.prepare({ ...confirmation, boardConfirmed: false }), { code: 'BOARD' }); assert.deepEqual(d.trace, []);
});
for (const key of ['backupConfirmed', 'tfRemoved', 'planConfirmed']) test(`missing ${key} refuses all writes`, async () => {
  const [d, s] = setup(); await s.prepare(confirmation); await assert.rejects(s.execute({ ...approve, [key]: false }), { code: 'BACKUP' }); assert.deepEqual(forbiddenWrites(d), []);
});
test('update rejects erase permission', async () => {
  const [d, s] = setup(); await s.prepare(confirmation); await assert.rejects(s.execute({ ...approve, eraseConfirmed: true }), { code: 'ERASE_FORBIDDEN' }); assert.deepEqual(forbiddenWrites(d), []);
});
test('default build refuses physical updates after a valid plan', async () => {
  const [d, s] = setup(); d.kind = 'hardware'; await s.prepare(confirmation);
  await assert.rejects(s.execute(approve), { code: 'DEVICE_WRITES_LOCKED' }); assert.deepEqual(forbiddenWrites(d), []);
});
test('real fresh remains blocked even with update-test permission and every confirmation', async () => {
  const [d, s] = setup('fresh', { hardwareWritesEnabled: true }); d.kind = 'hardware'; await s.prepare(confirmation);
  await assert.rejects(s.execute({ ...approve, eraseConfirmed: true }), { code: 'FRESH_DEVICE_PENDING' }); assert.deepEqual(forbiddenWrites(d), []);
});
test('fresh requires separate erase approval and never treats erased metadata alone as blank proof', async () => {
  const [d, s] = setup('fresh'); assert.equal((await s.prepare(confirmation)).mode, 'fresh');
  await assert.rejects(s.execute(approve), { code: 'ERASE_CONFIRM' }); assert.deepEqual(forbiddenWrites(d), []);
});
test('fresh without verified recoverable backup cannot erase even with erase approval', async () => {
  const [d, s] = setup('fresh'); await s.prepare(confirmation);
  await assert.rejects(s.execute({ ...approve, backupConfirmed: false, eraseConfirmed: true }), { code: 'BACKUP' });
  assert.deepEqual(forbiddenWrites(d), []);
});
test('fresh interruption during erase retains partial STOP receipt and never starts any file', async () => {
  const [d, s] = setup('fresh');
  d.eraseAll = async () => { d.trace.push({ op: 'eraseAll' }); d.flash.fill(255, 0, 0x10000); throw new Error('Synthetic erase interruption'); };
  await s.prepare(confirmation); await assert.rejects(s.execute({ ...approve, eraseConfirmed: true }), { code: 'IO' });
  assert.equal(s.receipt().eraseStarted, true); assert.equal(s.receipt().flashOutcome, 'POSSIBLY_PARTIAL');
  assert.deepEqual(forbiddenWrites(d).map(x => x.op), ['eraseAll']);
});
test('fresh simulator: proof of all-FF, recoverable backup, separate erase, four exact writes and pending boot receipt', async () => {
  const [d, s] = setup('fresh'); const plan = await s.prepare(confirmation);
  const receipt = await s.execute({ ...approve, eraseConfirmed: true });
  assert.deepEqual(forbiddenWrites(d).map(t => t.op === 'write' ? t.offset : t.op), ['eraseAll', 0, 0x8000, 0xe000, 0x10000]);
  assert.equal(receipt.status, 'VERIFIED_BOOT_PENDING'); assert.equal(receipt.simulation, true);
  await writeFile('evidence/plans/fresh.json', JSON.stringify({ plan, receipt, syntheticCommandTrace: d.trace }, null, 2) + '\n');
});
test('changed OTA after approval invalidates plan before write', async () => {
  const [d, s] = setup(); await s.prepare(confirmation); d.flash.set(otaFixture(1), 0xe000);
  await assert.rejects(s.execute(approve), { code: 'STALE_PLAN' }); assert.deepEqual(forbiddenWrites(d), []);
});
test('metadata transfer inconsistency refuses a plan', async () => {
  const [d, s] = setup(); const read = d.readFlash.bind(d); d.readFlash = async (...args) => { const b = await read(...args); b[99] ^= 1; return b; };
  await assert.rejects(s.prepare(confirmation), { code: 'READBACK' }); assert.deepEqual(forbiddenWrites(d), []);
});
for (const [scenario, code] of [['disconnect', 'DISCONNECTED'], ['verify', 'VERIFY']]) test(`${scenario} during installation: STOP, partial receipt, no erase/retry/reset`, async () => {
  const [d, s] = setup(scenario); await s.prepare(confirmation); await assert.rejects(s.execute(approve), { code });
  const receipt = s.receipt(); assert.equal(receipt.status, 'STOP'); assert.equal(receipt.flashOutcome, 'POSSIBLY_PARTIAL'); assert.equal(receipt.independentVerification, false);
  assert.equal(forbiddenWrites(d).length, 1); assert.equal(forbiddenWrites(d)[0].op, 'write');
  await writeFile(`evidence/plans/stop-${scenario}.json`, JSON.stringify({ receipt, syntheticCommandTrace: d.trace }, null, 2) + '\n');
});
test('protected-data corruption cannot return success even if App matches', async () => {
  const [d, s] = setup(); const write = d.writeImage.bind(d); d.writeImage = async (...a) => { await write(...a); d.flash[0x9000] ^= 1; };
  await s.prepare(confirmation); await assert.rejects(s.execute(approve), { code: 'PROTECTED_CHANGED' }); assert.equal(s.receipt().independentVerification, false);
});
test('optional exact previous App mismatch stops before erasure', async () => {
  const [d, s] = setup(); await s.prepare(confirmation); await assert.rejects(s.execute({ ...approve, currentApp: inputs['app.bin'] }), { code: 'CURRENT_APP' }); assert.deepEqual(forbiddenWrites(d), []);
});
test('concurrent execution never creates two writers', async () => {
  const [d, s] = setup(); await s.prepare(confirmation); const first = s.execute(approve);
  await assert.rejects(s.execute(approve), { code: 'BUSY' }); await first; assert.equal(forbiddenWrites(d).length, 1);
});
test('cancel is terminal and never becomes success', async () => {
  const [d, s] = setup(); await s.prepare(confirmation); s.cancel(); await assert.rejects(s.execute(approve), { code: 'CANCELLED' }); assert.equal(s.state, 'STOP'); assert.deepEqual(forbiddenWrites(d), []);
});
test('cancel during write keeps cancellation reason and partial outcome, with no following verify command', async () => {
  const [d, s] = setup();
  d.writeImage = async (offset, data) => { d.trace.push({ op: 'write', offset, size: 1024 }); d.flash.set(data.slice(0, 1024), offset); s.cancel(); throw new Error('stream cancelled'); };
  await s.prepare(confirmation); await assert.rejects(s.execute(approve), { code: 'CANCELLED' });
  assert.equal(s.receipt().error, 'CANCELLED'); assert.equal(s.receipt().flashOutcome, 'POSSIBLY_PARTIAL');
  assert.equal(d.trace.at(-1).op, 'write'); assert.equal(forbiddenWrites(d).length, 1);
});
test('OTA selection matches the actual release Python oracle across 53 boundary/adversarial fixtures', async () => {
  const fixtures = [];
  for (const seq of [1, 2, 3, 4, 0xfffffffe, 0xffffffff, 0]) for (const state of [0, 1, 2, 3, 4, 0xffffffff]) fixtures.push(otaFixture(0, { seq, state }));
  fixtures.push(new Uint8Array(8192).fill(255), otaFixture(1, { second: true }), otaFixture(1, { sentinel: true }), inputs['boot_app0.bin']);
  for (const at of [0, 1, 24, 28, 29, 30, 31]) { const f = otaFixture(1); f[at] ^= 0x80; fixtures.push(f); }
  await mkdir('.test-cache/ota-oracle', { recursive: true });
  for (let i = 0; i < fixtures.length; i++) await writeFile(`.test-cache/ota-oracle/${i}.bin`, fixtures[i]);
  const macPython = '/Library/Frameworks/Python.framework/Versions/3.14/bin/python3';
  const python = process.env.DRAFTOR_PYTHON || (existsSync(macPython) ? macPython : 'python3');
  const oracle = spawnSync(python, ['-B', '-c', `import sys,json\nfrom pathlib import Path\nsys.path.insert(0,'release-inputs/Firmware')\nfrom check_install import active_slot\nfrom install_firmware import package_plan\npackage_plan(Path('release-inputs/Firmware'),'WAVESHARE_RLCD42')\nr=[]\nfor i in range(${fixtures.length}):\n try:r.append(active_slot(Path('.test-cache/ota-oracle')/(str(i)+'.bin')))\n except ValueError:r.append('STOP')\nprint(json.dumps(r))`], { encoding: 'utf8' });
  assert.equal(oracle.status, 0, oracle.stderr);
  const actual = fixtures.map(f => { try { return activeSlot(f); } catch { return 'STOP'; } });
  assert.deepEqual(actual, JSON.parse(oracle.stdout)); assert.equal(fixtures.length, 53);
  await writeFile('evidence/python-oracle.json', JSON.stringify({ result: 'PASS', fixtures: 53, actual, oracle: JSON.parse(oracle.stdout), packagePlan: 'PASS', hardwareAccess: false }, null, 2) + '\n');
});
