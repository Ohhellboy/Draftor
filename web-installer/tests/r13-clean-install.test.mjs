import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, writeFile, readFile, mkdir } from 'node:fs/promises';
import { PublicInstallSession } from '../src/public-install-session.js';
import { RecoverySession } from '../src/recovery-session.js';
import { validateRecoveryPackage, recoverySnapshot, destroyRecoveryPackage } from '../src/recovery-package.js';
import { release, inputs, source, draftorFlash, factoryFlash, realRomFixture, packageFor } from './r9-fixture.mjs';
import { sha256, md5 } from '../src/safety.js';
import { PINS } from '../src/pins.js';
import { SimulatedFlash } from '../src/simulator.js';

const tool = new TextEncoder().encode('R13 synthetic recovery tool hash');
const options = { platformQualified: true, build: 'R13_SOFTWARE_TEST', sourceCommit: source, offlineToolSha256: await sha256(tool) };
const prepare = { boardConfirmed: true, tfRemoved: true };
const consent = { backupConfirmed: true, tfRemoved: true, planConfirmed: true, eraseConfirmed: true, offlineToolSaved: true };
await mkdir('.test-cache', { recursive: true });
const disk = await mkdtemp('.test-cache/r13-generic-');
function fixture(flash, faults = {}) {
  const f = realRomFixture(flash, { fullRecovery: false, firstInstall: true, ...faults }, { candidate: true });
  f.session = new PublicInstallSession(f.rom, release, { ...options, diagnostics: f.diagnostics });
  return f;
}
const writes = f => (f.port.counts.get(2) || 0) + (f.port.counts.get(3) || 0);
async function saved(f, image) {
  const zip = await f.session.importExistingRaw(image);
  await f.session.verifySavedBackup(zip);
  await f.session.verifyOfflineTool(tool);
  return zip;
}
const samples = {
  'different factory executable': async () => { const b = await factoryFlash(); b[65580] ^= 1; return b; },
  'non-FF factory tail': async () => { const b = await factoryFlash(); b[b.length - 1] = 7; return b; },
  'unknown partition and firmware': async () => new Uint8Array(PINS.flashBytes).fill(0x5a),
  'erased metadata with residual private data': async () => { const b = new Uint8Array(PINS.flashBytes).fill(255); b[0x900000] = 42; return b; },
  'invalid OTA on former Draftor': async () => { const b = draftorFlash(); b[0xe000] ^= 1; b[0xf000] ^= 1; return b; },
  'previous app1 layout': async () => new SimulatedFlash(release, 'app1').flash,
  'fully blank': async () => new Uint8Array(PINS.flashBytes).fill(255),
};
for (const [name, create] of Object.entries(samples)) {
  test(name + ' passes prepare, grant, erase, write, target verification and original recovery', async () => {
    const image = await create(), original = image.slice(), f = fixture(image);
    const plan = await f.session.prepare(prepare);
    const blank = name === 'fully blank';
    assert.equal(plan.mode, 'fresh');
    assert.equal(plan.initialState, blank ? 'blank' : 'unrecognized');
    assert.equal(writes(f), 0);
    let zip;
    if (!blank) {
      zip = await saved(f, original);
      const path = disk + '/' + name.replaceAll(' ', '-') + '.zip';
      await writeFile(path, zip);
      zip = new Uint8Array(await readFile(path));
      const h = await validateRecoveryPackage(zip), snap = recoverySnapshot(h);
      assert.equal(snap.manifest.schema, 'draftor-private-recovery/2');
      assert.equal(snap.manifest.original.firmware, 'unrecognized');
      assert.deepEqual(snap.image, original); snap.image.fill(0); destroyRecoveryPackage(h);
    } else assert.equal(f.session.backupVerified, true);
    await f.session.execute(consent);
    assert.equal(f.session.state, 'VERIFIED_BOOT_PENDING');
    assert.equal(f.port.counts.get(2), 5);
    assert.equal(f.port.signalCount, 0);
    const expected = new Uint8Array(PINS.flashBytes).fill(255);
    for (const n of ['bootloader.bin', 'partitions.bin', 'boot_app0.bin', 'app.bin']) expected.set(inputs[n], PINS.files[n].offset);
    assert.deepEqual(image, expected);
    assert.equal(plan.expectedFullMd5, md5(expected));
    await assert.rejects(f.session.execute(consent));
    await f.session.close();
    if (zip) {
      const r = realRomFixture(new Uint8Array(PINS.flashBytes).fill(0xcc));
      const recovery = new RecoverySession(r.rom, { ...options, mode: 'restore', diagnostics: r.diagnostics });
      await recovery.selectPackage(zip);
      await recovery.prepare(prepare);
      await recovery.restore({ restoreConfirmed: true });
      assert.deepEqual(r.flash, original);
      assert.equal(r.port.counts.get(2), 1); assert.equal(r.port.counts.get(3), 16384);
      assert.equal(r.port.signalCount, 0);
      await recovery.close();
    }
  });
}
test('unrecognized original full ROM slow capture creates a saved, reimportable package', async () => {
  const f = fixture(new Uint8Array(PINS.flashBytes).fill(0x3c));
  await f.session.prepare(prepare);
  const zip = await f.session.captureBackup();
  assert(f.port.counts.get(14) >= PINS.flashBytes / 64);
  await writeFile(disk + '/captured.zip', zip);
  await f.session.verifySavedBackup(new Uint8Array(await readFile(disk + '/captured.zip')));
  assert.equal(f.session.backupVerified, true);
  assert.equal(writes(f), 0);
  await f.session.close();
});
for (const [name, faults, code] of [
  ['chip', { chipId: 2 }, 'CHIP'], ['capacity', { jedec: 0x1740ef }, 'FLASH'],
  ['secure boot', { securityFlags: 1 }, 'SECURITY'], ['encryption', { cryptCount: 1 }, 'SECURITY'],
  ['unknown security flags', { securityFlags: 0x800 }, 'SECURITY'],
  ['protocol error', { malformedOn: 14 }, 'PROTOCOL'],
]) test('generic installation still rejects ' + name + ' without writing', async () => {
  const f = fixture(new Uint8Array(PINS.flashBytes).fill(0x5a), faults);
  await assert.rejects(f.session.prepare(prepare), { code });
  assert.equal(writes(f), 0); assert.equal(f.session.receipt().mutationStarted, false);
  await f.session.close();
});
test('changed blank contents require a new backup instead of using blank exemption', async () => {
  const f = fixture(new Uint8Array(PINS.flashBytes).fill(255));
  await f.session.prepare(prepare); f.flash[0x900000] = 1;
  await assert.rejects(f.session.execute(consent), { code: 'BACKUP_STALE' });
  assert.equal(writes(f), 0); assert.equal(f.session.receipt().mutationStarted, false);
  await f.session.close();
});
test('generic saved content changes and missing consent remain zero-write failures', async () => {
  for (const fault of ['changed', 'wrong board', 'no erase consent']) {
    const f = fixture(new Uint8Array(PINS.flashBytes).fill(0x3c));
    await f.session.prepare(prepare); await saved(f, f.flash);
    if (fault === 'changed') f.flash[0x900000] ^= 1;
    if (fault === 'wrong board') f.port.options.macLo = 2;
    await assert.rejects(f.session.execute({ ...consent, eraseConfirmed: fault !== 'no erase consent' }));
    assert.equal(writes(f), 0); assert.equal(f.session.receipt().mutationStarted, false);
    await f.session.close();
  }
});
test('pre-erase recheck failure and actual write failure have distinct mutation evidence', async () => {
  for (const fault of ['before erase', 'during write']) {
    const f = fixture(new Uint8Array(PINS.flashBytes).fill(255));
    await f.session.prepare(prepare);
    if (fault === 'before erase') {
      const erase = f.rom.eraseAll.bind(f.rom);
      f.rom.eraseAll = async args => { f.flash[0x900000] = 1; return erase(args); };
    } else f.port.options.throwOn = { op: 3, nth: 1 };
    await assert.rejects(f.session.execute(consent));
    assert.equal(f.session.receipt().mutationStarted, fault === 'during write');
    assert.equal(f.session.receipt().flashOutcome === 'POSSIBLY_PARTIAL', fault === 'during write');
    await f.session.close();
  }
});
test('a corrupt erase is detected before any firmware data is sent', async () => {
  const f = fixture(new Uint8Array(PINS.flashBytes).fill(255));
  await f.session.prepare(prepare);
  const digest = f.rom.flashMd5.bind(f.rom);
  f.rom.flashMd5 = async (...args) => { if (f.port.counts.get(2)) f.flash[0x500000] = 1; return digest(...args); };
  await assert.rejects(f.session.execute(consent), { code: 'ERASE_VERIFY' });
  assert.equal(f.port.counts.get(3) || 0, 0);
  assert.equal(f.session.receipt().mutationStarted, true);
  await f.session.close();
});
