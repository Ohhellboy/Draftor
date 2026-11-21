import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { PINS } from '../src/pins.js';
import { verifyRelease } from '../src/firmware.js';
import assert from 'node:assert/strict';

const root = new URL('../', import.meta.url);
export async function verifyReleaseInputs({ evidence = false, verifyDesignBrief = true } = {}) {
  const lock = JSON.parse(await readFile(new URL('release-lock.json', root), 'utf8'));
  assert.deepEqual(lock, PINS, 'checked-in pins must equal the reviewed release lock');
  const inputs = {}, rows = [];
  for (const [name, pin] of Object.entries({ ...PINS.documents, ...PINS.files })) {
    const data = new Uint8Array(await readFile(new URL(`public/firmware/${name}`, root)));
    inputs[name] = data;
    rows.push({ file: `firmware/${name}`, bytes: data.length, sha256: createHash('sha256').update(data).digest('hex'), expected: pin.sha256 });
  }
  const release = await verifyRelease(inputs);
  for (const [name, pin] of Object.entries(PINS.archives)) {
    const data = await readFile(new URL(`release-inputs/${name}`, root));
    assert.equal(data.length, pin.bytes, name);
    const hash = createHash('sha256').update(data).digest('hex'); assert.equal(hash, pin.sha256, name);
    rows.push({ file: `release-inputs/${name}`, bytes: data.length, sha256: hash, expected: pin.sha256 });
  }
  const publicManifest = JSON.parse(await readFile(new URL('release-inputs/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_MANIFEST.json', root), 'utf8'));
  assert.equal(publicManifest.release_commit, PINS.commit);
  assert.equal(publicManifest.app.sha256, PINS.files['app.bin'].sha256);
  for (const asset of publicManifest.assets) if (PINS.archives[asset.name]) {
    assert.equal(asset.sha256, PINS.archives[asset.name].sha256); assert.equal(asset.bytes, PINS.archives[asset.name].bytes);
  }
  const contents = JSON.parse(await readFile(new URL('release-inputs/CONTENTS.json', root), 'utf8'));
  assert.equal(contents.commit, PINS.commit);
  for (const item of contents.files) {
    // The original design brief is not firmware or corresponding runtime source.
    // Public source archives intentionally omit user task documents.
    if (!verifyDesignBrief && item.path === 'BRIEF.md') continue;
    assert(!item.path.startsWith('/') && !item.path.split('/').includes('..'));
    const b = await readFile(new URL(`release-inputs/${item.path}`, root));
    assert.equal(b.length, item.bytes, item.path);
    const hash = createHash('sha256').update(b).digest('hex'); assert.equal(hash, item.sha256, item.path);
    rows.push({ file: `release-inputs/${item.path}`, bytes: b.length, sha256: hash, expected: item.sha256 });
  }
  const source = publicManifest.assets.find(x => x.name.endsWith('_Source.zip'));
  const sourceBytes = await readFile(new URL(`public/sources/${source.name}`, root));
  assert.equal(sourceBytes.length, source.bytes);
  const sourceHash = createHash('sha256').update(sourceBytes).digest('hex'); assert.equal(sourceHash, source.sha256);
  rows.push({ file: `sources/${source.name}`, bytes: sourceBytes.length, sha256: sourceHash, expected: source.sha256 });
  const sums = await readFile(new URL('release-inputs/SHA256SUMS', root), 'utf8');
  for (const name of ['Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Firmware.zip', 'Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip', 'Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_MANIFEST.json'])
    assert(sums.includes(`${PINS.archives[name].sha256}  ${name}`));
  const receipt = { result: 'PASS', release, commit: PINS.commit, files: rows, deviceAccess: false };
  if (evidence) { await mkdir(new URL('evidence/', root), { recursive: true }); await writeFile(new URL('evidence/release-verification.json', root), JSON.stringify(receipt, null, 2) + '\n'); }
  return { release, inputs, receipt };
}
if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const { receipt } = await verifyReleaseInputs({ evidence: true });
  console.log(`PASS: ${receipt.files.length} pinned inputs; exact alpha.3 App and all manifests verified; no device access.`);
}
