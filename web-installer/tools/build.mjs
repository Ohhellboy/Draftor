import { build } from 'esbuild';
import { cp, readFile, writeFile, mkdir, readdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { verifyReleaseInputs } from './verify-release.mjs';
import { PINS } from '../src/pins.js';

await verifyReleaseInputs();
const deviceTest = process.argv.includes('--device-test-update');
const firstTest = process.argv.includes('--device-test-first-install');
const r6Locked = process.argv.includes('--r6-locked');
if ([deviceTest, firstTest, r6Locked].filter(Boolean).length > 1) throw new Error('Mutually exclusive build scopes');
const out = firstTest ? 'dist-first-install-test' : r6Locked ? 'dist-r6-locked' : deviceTest ? 'dist-device-test' : 'dist';
await mkdir(`${out}/assets`, { recursive: true });
await cp('public', out, { recursive: true });
const html = await readFile('index.html', 'utf8');
await writeFile(`${out}/index.html`, html);
await writeFile(`${out}/simulation.html`, html.replace('assets/app.js', 'assets/simulation.js'));
for (const [entry, name] of [['src/main.js', 'app'], ['src/demo.js', 'simulation']]) {
  await build({ entryPoints: [entry], bundle: true, format: 'esm', target: ['chrome120'], outfile: `${out}/assets/${name}.js`,
    minify: false, sourcemap: true, legalComments: 'linked',
    banner: { js: '/*! Draftor Web Installer R1. Combined browser program: GPL-3.0. ROM sequence adapted from esptool (GPL-2.0-or-later). Transport: esptool-js (Apache-2.0); spark-md5 (MIT). Corresponding source: ../sources/Draftor_Web_Installer_R1_source.zip ; licenses: ../docs/SOURCE.html */' },
    define: { DEVICE_TEST_UPDATE: deviceTest ? 'true' : 'false', DEVICE_TEST_FIRST_INSTALL: firstTest ? 'true' : 'false' } });
}
await cp('docs', `${out}/docs`, { recursive: true });
await writeFile(`${out}/BUILD.json`, JSON.stringify({ release: PINS.version, commit: PINS.commit, hardwareUpdateWrites: deviceTest, hardwareFreshWrites: firstTest, experimental: firstTest, deviceQualification: 'DEVICE_PENDING', runtimeNetwork: 'same-origin static files only', dependencies: JSON.parse(await readFile('package.json', 'utf8')).dependencies }, null, 2) + '\n');
const lock = JSON.parse(await readFile('package-lock.json', 'utf8'));
await writeFile(`${out}/DEPENDENCIES.json`, JSON.stringify(Object.fromEntries(Object.entries(lock.packages).filter(([name]) => name).map(([name, p]) => [name, { version: p.version, integrity: p.integrity, resolved: p.resolved, license: p.license, dev: !!p.dev }])), null, 2) + '\n');
const files = [];
async function list(path) {
  for (const e of (await readdir(path, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
    const p = `${path}/${e.name}`;
    if (e.isDirectory()) await list(p);
    else if (e.name !== 'SITE_SHA256SUMS') { const b = await readFile(p); files.push(`${createHash('sha256').update(b).digest('hex')}  ${p.slice(out.length + 1)}`); }
  }
}
await list(out); await writeFile(`${out}/SITE_SHA256SUMS`, files.join('\n') + '\n');
console.log(`${out}: ${files.length} files; hardware update writes ${deviceTest ? 'EXPERIMENT ONLY' : 'LOCKED'}; real fresh ${firstTest ? 'R6 EXPERIMENT ONLY' : 'LOCKED'}.`);
