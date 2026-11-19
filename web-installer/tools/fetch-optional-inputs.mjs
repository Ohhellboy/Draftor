// Explicit maintainer action only; never imported by the browser/build/runtime.
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
const rows = [
  ['v0.1.0-alpha.2.1', 'Draftor_v0.1.0-alpha.2.1_Resources.zip', 60546074, '460096846036bd01c941bc07ab98e6db4e130a247563e44fa3cb01d12a9c6636'],
  ['v0.1.0-alpha.3', 'Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip', 8151209, '7cca7a13b685ab74f22150f0ed6521081bb9fcceec078da49ff457c85f67f69e'],
];
await mkdir('release-inputs', { recursive: true });
for (const [tag, name, size, hash] of rows) {
  const target = `release-inputs/${name}`;
  let data;
  try { data = await readFile(target); }
  catch (error) {
    if (error.code !== 'ENOENT') throw error;
    const response = await fetch(`https://github.com/Ohhellboy/Draftor/releases/download/${tag}/${name}`, { signal: AbortSignal.timeout(120000) });
    if (!response.ok) throw new Error(`Download failed: ${name}`);
    data = new Uint8Array(await response.arrayBuffer());
  }
  if (data.length !== size || createHash('sha256').update(data).digest('hex') !== hash) throw new Error(`Integrity mismatch: ${name}; preserve and inspect`);
  try { await writeFile(target, data, { flag: 'wx' }); } catch (error) { if (error.code !== 'EEXIST') throw error; }
  console.log(`Verified ${name}`);
}
