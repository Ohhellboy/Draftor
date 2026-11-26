import test from 'node:test';
import assert from 'node:assert/strict';
import { verifyReleaseInputs } from '../tools/verify-release.mjs';
import { loadRelease } from '../src/firmware.js';
const { inputs } = await verifyReleaseInputs({verifyDesignBrief:false});
for (const encoding of ['gzip', 'br', 'identity']) test(`decoded fetch body remains pinned with ${encoding}`, async () => {
  const r = await loadRelease(new URL('https://example.test/firmware/'), async url => {
    const b = inputs[url.pathname.split('/').at(-1)];
    return new Response(b, { headers: { 'content-encoding': encoding, 'content-length': String(encoding === 'identity' ? b.length : 740) } });
  });
  assert.equal(r.version, 'v0.1.0-alpha.3');
});
for (const fault of ['short', 'long', 'corrupt', '404']) test(`compressed ${fault} still stops without retry`, async () => {
  let calls = 0;
  await assert.rejects(loadRelease(new URL('https://example.test/firmware/'), async url => {
    calls++; const original = inputs[url.pathname.split('/').at(-1)];
    const b = fault === 'short' ? original.slice(0, -1) : fault === 'long' ? new Uint8Array(original.length + 1) : original.slice();
    if (fault === 'corrupt') b[0] ^= 1;
    return new Response(b, { status: fault === '404' ? 404 : 200, headers: { 'content-encoding': 'gzip', 'content-length': '740' } });
  }));
  assert.equal(calls, 1); // Reject before caching/downloading further files.
});
