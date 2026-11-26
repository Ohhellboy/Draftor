// SPDX-License-Identifier: Apache-2.0
// Rebuild installer assets while preserving the rest of the pinned public site.
import { build } from 'esbuild';
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { verifyReleaseInputs } from './verify-release.mjs';
import { PINS } from '../src/pins.js';

const [out, baseline = 'dist-r13-public'] = process.argv.slice(2);
if (out !== 'dist-r13-download-public') throw Error('Expected dist-r13-download-public');
const hash = b => createHash('sha256').update(b).digest('hex');
const b64 = b => createHash('sha256').update(b).digest('base64');
const baseManifest = await readFile(path.join(baseline, 'SITE_SHA256SUMS'));
if (hash(baseManifest) !== '5f0daa7a3e4610d5d68a0d53eb93e45e7f90017231f2f065f41543b5461eca82') throw Error('Publication baseline mismatch');
const previousPages = 'b673e971da06cc4b8fc9d14a0ea656cac5634657';
let source;
try { source = JSON.parse(await readFile('SOURCE_IDENTITY.json', 'utf8')).sourceCommit; }
catch (e) {
  if (e.code !== 'ENOENT') throw e;
  execFileSync('git', ['diff', '--exit-code', 'HEAD', '--', '.']);
  source = execFileSync('git', ['rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
}
if (!/^[0-9a-f]{40}$/.test(source)) throw Error('Source identity');
await verifyReleaseInputs({ verifyDesignBrief: false });
await mkdir(out);
async function put(name, data) {
  await mkdir(path.dirname(path.join(out, name)), { recursive: true });
  await writeFile(path.join(out, name), data);
}
const readBase = name => readFile(path.join(baseline, name), 'utf8');
for (const row of baseManifest.toString().trim().split('\n')) {
  const name = row.slice(66), data = await readFile(path.join(baseline, name));
  if (hash(data) !== row.slice(0, 64)) throw Error('Baseline file mismatch ' + name);
  // Recreate the current corresponding-source archive; preserve the prior site on disk.
  if (name !== 'sources/Draftor_Web_R13_source.zip') await put(name, data);
}
const version = 'R13.1-public-' + source.slice(0, 12);
const offlineName = 'Draftor_Recovery_R13_Offline.html';
const defines = { __R11_BUILD__: JSON.stringify(version), __R11_SOURCE__: JSON.stringify(source), __INSTALLER_OFFLINE_NAME__: JSON.stringify(offlineName) };
const config = { bundle: true, target: ['chrome120'], legalComments: 'inline' };
const inline = await build({ ...config, entryPoints: ['src/r11-recovery.js'], format: 'iife', write: false, define: defines });
const js = inline.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const css = await readFile('public/style.css', 'utf8') + '\n' + await readFile('public/recovery.css', 'utf8');
let offline = await readFile('recovery.html', 'utf8');
offline = offline.replace(/<link rel="stylesheet"[^>]*>/g, '').replace('<script src="assets/recovery.js"></script>', '<script>' + js + '</script>')
  .replace('</head>', '<style>' + css + '</style></head>')
  .replace("script-src 'self'", "script-src 'sha256-" + b64(js) + "'").replace("style-src 'self'", "style-src 'sha256-" + b64(css) + "'")
  .replace('href="index.html"', 'href="#"').replace('href="docs/SOURCE.html"', 'href="#offline-licenses"');
const escape = s => s.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;');
const licenses = await Promise.all(['GNU_GPL_v3.txt', 'esptool_GPL-2.0.txt', 'esptool-js-LICENSE', 'spark-md5-LICENSE'].map(async name =>
  '<details><summary>' + name + '</summary><pre>' + escape(await readFile('public/LICENSES/' + name, 'utf8')) + '</pre></details>'));
const recoveryGuide = await readFile('docs/r13/RECOVERY_GUIDE.zh-CN.md', 'utf8');
const englishGuide = await readFile('docs/r13/RECOVERY_GUIDE.en.md', 'utf8');
offline = offline.replace('</main>', '<section id="offline-guide"><h2>原设备恢复 / Original-device recovery</h2><pre>' + escape(recoveryGuide + '\n\n' + englishGuide) + '</pre></section><section id="offline-licenses"><h2>Source &amp; licenses</h2><p>Combined browser program GPL-3.0. Source ' + source + '. Corresponding source and build instructions at https://draftor.tcssn.org/sources/Draftor_Web_R13_source.zip</p>' + licenses.join('') + '</section></main>');
const offlineHash = hash(offline);
await put('offline/' + offlineName, offline);
await build({ ...config, entryPoints: ['src/r11-main.js'], format: 'esm', outfile: out + '/assets/app.js', sourcemap: true, define: { ...defines, __R11_OFFLINE_HASH__: JSON.stringify(offlineHash) } });
await build({ ...config, entryPoints: ['src/r11-online-recovery.js'], format: 'iife', outfile: out + '/assets/recovery.js', sourcemap: true, define: defines });
const reinstallScript = 'assets/reinstall-' + source.slice(0, 12) + '.js';
await build({ ...config, entryPoints: ['src/r12-local-app0.js'], format: 'esm', outfile: out + '/' + reinstallScript,
  define: { __R12_PUBLIC__: 'true', __R12_BUILD__: JSON.stringify(version), __R12_SOURCE__: JSON.stringify(source) } });

for (const name of ['index.html', 'first-install.html', 'recovery.html']) {
  let html = await readBase(name);
  html = html.replaceAll(/R(?:12|13(?:\.1)?)-public-[a-f0-9]{12}/g, version)
    .replaceAll('Draftor_Recovery_R11_Offline.html', offlineName)
    .replaceAll('Draftor_Recovery_R9_Offline.html', offlineName);
  if (name === 'index.html') html = html.replace(/assets\/reinstall-[a-f0-9]{12}\.js/, reinstallScript)
    .replaceAll('第一次安装草稿机', '第一次安装／清空安装')
    .replace('手里的微雪板还在运行厂家演示，请选“第一次安装／清空安装”。已经在用 Alpha 3，请选“重新安装 Alpha 3”。', '已在用 Alpha 3，可选择保留数据重装。其他固件或希望清空设备，请选“第一次安装／清空安装”。');
  if (name === 'first-install.html') html = html.replace(/src="assets\/app\.js(?:\?[^"]*)?"/, 'src="assets/app.js?v=' + version + '"')
    .replace('第一次安装草稿机 / First-time installation', '第一次安装／清空安装 / Clean installation')
    .replace('查看首装步骤 / Instructions', '查看清空安装步骤 / Instructions');
  if (name === 'recovery.html') html = html.replace(/src="assets\/recovery\.js(?:\?[^"]*)?"/, 'src="assets/recovery.js?v=' + version + '"');
  await put(name, html);
}
await put('guide.html', await readFile('public/guide.html'));
await put('docs/PUBLIC_STATUS.html', (await readFile('public/docs/R12_PUBLIC_STATUS.html', 'utf8')).replace('{{VERSION}}', version));
for (const lang of ['zh-CN', 'en']) {
  await put('docs/RECOVERY_GUIDE.' + lang + '.md', await readFile('docs/r13/RECOVERY_GUIDE.' + lang + '.md'));
  await put('docs/WINDOWS_FIRST_USER_GUIDE.' + lang + '.md', await readFile('docs/r13/WINDOWS_FIRST_USER_GUIDE.' + lang + '.md'));
}
let sourcePage = await readBase('docs/SOURCE.html');
sourcePage = sourcePage.replace('<h1>', '<p><a href="../sources/Draftor_Web_R13_source.zip">R13 当前安装器与恢复工具源码 / Current installer and recovery source</a></p><p>' + version + ' · ' + source + '</p><h1>');
await put('docs/SOURCE.html', sourcePage);
execFileSync('python3', ['tools/package-r12-source.py', out + '/sources/Draftor_Web_R13_source.zip', source], { stdio: 'inherit' });
const metadata = {
  schema: 'draftor-public-release/1', profile: 'R13_SUPPORTED_HARDWARE_CLEAN_INSTALL',
  version, sourceCommit: source, previousPages, reviewOnly: false, software: false, realUsbAvailable: true, frozen: true,
  download: { idleTimeoutMs: 60000, totalFileDeadlineMs: null, progress: 'actual-decoded-bytes', retry: 'user-only-network-preparation', cache: 'session-length-sha256-md5-verified-files' },
  release: PINS.version, appSha256: PINS.files['app.bin'].sha256, windowsDevice: 'PENDING',
  installation: { firstPage: 'first-install.html', reinstallPage: 'index.html', cleanInstall: 'confirmed-board-chip-capacity-security-target-and-current-full-backup', blank: 'ROM-full-blank-proof-no-backup', preservedReinstall: 'exact-alpha3-app0' },
  recovery: { packageSchemas: ['draftor-private-recovery/1', 'draftor-private-recovery/2'], online: 'backup-and-read-only', offlineHtml: 'offline/' + offlineName, offlineSha256: offlineHash, scope: 'validated-full-original-image-same-board-current-image-may-be-damaged' },
  qualification: { genericCleanInstall: 'SOFTWARE_TESTED_DEVICE_PENDING', genericRestore: 'SOFTWARE_TESTED_DEVICE_PENDING', priorDeviceResults: 'R12_SAME_VERSION_MACOS_AND_R6_EXACT_FACTORY_INHERITED' },
  appBundleSha256: hash(await readFile(out + '/assets/app.js')),
  reinstall: { script: reinstallScript, sha256: hash(await readFile(out + '/' + reinstallScript)) },
  recoveryBundleSha256: hash(await readFile(out + '/assets/recovery.js')),
};
await put('BUILD.json', JSON.stringify(metadata, null, 2) + '\n');
const rows = [];
async function walk(prefix = '') {
  for (const entry of await readdir(path.join(out, prefix), { withFileTypes: true })) {
    const name = path.posix.join(prefix, entry.name);
    if (entry.isDirectory()) await walk(name);
    else if (entry.isFile() && name !== 'SITE_SHA256SUMS') rows.push(hash(await readFile(path.join(out, name))) + '  ' + name);
    else if (!entry.isFile()) throw Error('Unexpected output ' + name);
  }
}
await walk();
rows.sort((a, b) => a.slice(66).localeCompare(b.slice(66), 'en'));
await put('SITE_SHA256SUMS', rows.join('\n') + '\n');
console.log(JSON.stringify({ version, sourceCommit: source, previousPages, files: rows.length + 1, offlineHash }));
