// SPDX-License-Identifier: Apache-2.0
// Publish the proven reinstall entry beside the existing Pages first-install and recovery assets.
import { build } from 'esbuild';
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { FACTORY } from '../src/factory-pins.js';
import { verifyReleaseInputs } from './verify-release.mjs';

const output = process.argv[2];
if (output !== 'dist-r12-public') throw Error('Expected dist-r12-public');
const baseline = process.argv[3] || 'dist-r11-public';
const previousPages = '8ca75a4b71d104407098d54001bcbae8dd38e898';
const previousManifest = '21a93d4129dda377dd71bcd4fc8eba3b3271ad166d59f7da8de85c888d117902';
const testedSource = '197e5613df6a5a58d052c9fcce17beb168f8dd38';
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const git = (...args) => execFileSync('git', args, { encoding: 'utf8' }).trim();
let source;
try {
  source = JSON.parse(await readFile('SOURCE_IDENTITY.json', 'utf8')).sourceCommit;
} catch (error) {
  if (error.code !== 'ENOENT') throw error;
  if (git('status', '--porcelain', '--untracked-files=no')) throw Error('Commit the release source before building');
  source = git('rev-parse', 'HEAD');
}
if (!/^[a-f0-9]{40}$/.test(source)) throw Error('Invalid source commit');
const version = 'R12-public-' + source.slice(0, 12);
const script = 'assets/reinstall-' + source.slice(0, 12) + '.js';
const manifest = await readFile(path.join(baseline, 'SITE_SHA256SUMS'));
if (sha(manifest) !== previousManifest) throw Error('R11 Pages baseline manifest mismatch');
await verifyReleaseInputs({ verifyDesignBrief: false });
await mkdir(output);
async function put(name, bytes) {
  const target = path.join(output, name);
  await mkdir(path.dirname(target), { recursive: true });
  await writeFile(target, bytes);
}
const fromBaseline = name => readFile(path.join(baseline, name), 'utf8');
for (const line of manifest.toString().trim().split('\n')) {
  const name = line.slice(66);
  const bytes = await readFile(path.join(baseline, name));
  if (sha(bytes) !== line.slice(0, 64)) throw Error('R11 asset mismatch: ' + name);
  await put(name, bytes);
}
const legacy = JSON.parse(await fromBaseline('BUILD.json'));
const factory = await readFile('release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin');
if (sha(factory) !== FACTORY.sha256) throw Error('Factory reference mismatch');
await put('factory/FactoryOnly.bin', factory);
await put('LICENSES/Waveshare-RLCD42-LICENSE.txt', await readFile('public/LICENSES/Waveshare-RLCD42-LICENSE.txt'));
await put('Notices/R12_FACTORY_SOURCE.txt', await readFile('public/Notices/R12_FACTORY_SOURCE.txt'));
await put('guide.html', await readFile('public/guide.html'));
await put('install-navigation.css', await readFile('public/install-navigation.css'));

const navigationCss = '<link rel="stylesheet" href="install-navigation.css">';
const versionLine = '<p class="build-note">' + version + ' · v0.1.0-alpha.3</p>';
function replaceOnce(html, needle, replacement) {
  if (html.split(needle).length !== 2) throw Error('Missing or duplicate HTML marker: ' + needle);
  return html.replace(needle, replacement);
}

let home = await readFile('index.html', 'utf8');
home = replaceOnce(home, '</head>', navigationCss + '</head>');
home = replaceOnce(home, '通过 USB 连接设备，即可在浏览器中安装固件。无需额外下载软件。', '手里的微雪板还在运行厂家演示，请选“第一次安装草稿机”。已经在用 Alpha 3，请选“重新安装 Alpha 3”。');
home = replaceOnce(home, '>连接设备并安装</span>', '>重新安装 Alpha 3</span>');
home = replaceOnce(home, '网页预览版 · 尚未接入正式烧录程序', '微雪 ST7305 · 初次安装或同版本重装');
home = replaceOnce(home, '<p class="install-hint" id="install-hint"', '<nav class="install-links" aria-label="安装与说明"><a href="first-install.html" data-i18n="firstInstallLink">第一次安装草稿机</a><a href="guide.html" data-i18n="testerGuide">安装操作说明</a></nav><p class="install-hint" id="install-hint"');
home = replaceOnce(home, '<details class="tf-downloads"', versionLine + '<p class="install-hint"><a href="recovery.html">完整备份与原系统恢复 / Backup &amp; recovery</a> · <a href="offline/Draftor_Recovery_R11_Offline.html" download>离线恢复工具 / Offline tool</a> · <a href="guide.html#failure">安装中断处理 / Help</a></p><details class="tf-downloads"');
home = replaceOnce(home, '<nav aria-label="帮助与信息">', '<nav aria-label="帮助与信息"><a href="docs/PUBLIC_STATUS.html">支持范围 / Scope</a><a href="docs/SOURCE.html">源码 / Source</a>');
home = replaceOnce(home, 'src="assets/app.js"', 'src="' + script + '"');
await put('index.html', home);

let first = await fromBaseline('index.html');
first = replaceOnce(first, '</head>', navigationCss + '</head>');
first = replaceOnce(first, '<button class="install-primary" id="install-btn"', '<p class="factory-nav"><strong>第一次安装草稿机 / First-time installation</strong><br><a href="guide.html#first">查看首装步骤 / Instructions</a> · <a href="./#install-title">已经在用 Alpha 3？返回重装 / Reinstall</a></p><button class="install-primary" id="install-btn"');
first = replaceOnce(first, '<details class="tf-downloads"', versionLine + '<details class="tf-downloads"');
await put('first-install.html', first);
const tfGuide = await fromBaseline('docs/TF_GUIDE.html');
await put('docs/TF_GUIDE.html', replaceOnce(tfGuide, 'href="TF_RESOURCES_GUIDE.md"', 'href="../guide.html#tf"'));
const recoveryPage = await fromBaseline('recovery.html');
await put('recovery.html', replaceOnce(recoveryPage, 'href="offline/Draftor_Recovery_R9_Offline.html"', 'href="' + legacy.offlineHtml + '"'));

await put('docs/R11_PUBLIC_STATUS.html', await fromBaseline('docs/PUBLIC_STATUS.html'));
await put('docs/PUBLIC_STATUS.html', (await readFile('public/docs/R12_PUBLIC_STATUS.html', 'utf8')).replace('{{VERSION}}', version));
let license = await fromBaseline('docs/SOURCE.html');
license = replaceOnce(license, '<title>R11 source and licenses</title>', '<title>Draftor source and licenses</title>');
license = replaceOnce(license, '<h1>Draftor R11</h1>', '<h1>Draftor ' + version + '</h1><p><a href="../sources/Draftor_Web_R12_source.zip">R12 重装页面源码与构建说明 / Reinstall source and build instructions</a> · <a href="../Notices/R12_FACTORY_SOURCE.txt">Factory reference notice</a> · <a href="../LICENSES/Waveshare-RLCD42-LICENSE.txt">Waveshare license</a></p><p>Source commit ' + source + '</p><h2>R11 first installation and recovery</h2>');
await put('docs/SOURCE.html', license);
for (const language of ['zh-CN', 'en']) {
  const name = 'docs/WINDOWS_FIRST_USER_GUIDE.' + language + '.md';
  const absoluteFirst = 'https://draftor.tcssn.org/first-install.html';
  const guide = (await fromBaseline(name)).replaceAll('https://draftor.tcssn.org/', absoluteFirst);
  const note = language === 'zh-CN' ? '当前安装步骤与故障处理' : 'Current installation steps and troubleshooting';
  await put(name, '[' + note + '](https://draftor.tcssn.org/guide.html)\n\n' + guide);
}

await build({
  entryPoints: ['src/r12-local-app0.js'], outfile: path.join(output, script),
  bundle: true, format: 'esm', target: ['chrome120'], legalComments: 'inline',
  define: { __R12_PUBLIC__: 'true', __R12_BUILD__: JSON.stringify(version), __R12_SOURCE__: JSON.stringify(source) },
});
execFileSync('python3', ['tools/package-r12-source.py', path.join(output, 'sources/Draftor_Web_R12_source.zip'), source], { stdio: 'inherit' });
const metadata = {
  schema: 'draftor-public-release/1', profile: 'R12_PUBLIC_APP0_WITH_R11_FIRST_INSTALL',
  version, sourceCommit: source, testedSource, previousPages, frozen: true, productionDeployment: true,
  reviewOnly: false, software: false, realUsbAvailable: true, windowsDevice: 'PENDING',
  release: legacy.release, appSha256: legacy.appSha256, releaseCapabilities: legacy.releaseCapabilities,
  reinstall: { page: 'index.html', script, sha256: sha(await readFile(path.join(output, script))), origin: 'https://draftor.tcssn.org', writeScope: 'EXACT_ALPHA3_APP0_REINSTALL', protection: 'saved-and-read-back-small-original-board-file' },
  firstInstall: { page: 'first-install.html', script: 'assets/app.js', sha256: legacy.appBundleSha256, sourceCommit: legacy.sourceCommit, fingerprint: 'FactoryOnly_20260120', protection: 'full-current-device-zip-reimport-and-saved-offline-tool' },
  recovery: { page: 'recovery.html', online: 'read-only', scriptSha256: legacy.recoveryBundleSha256, offlineHtml: legacy.offlineHtml, offlineSha256: legacy.offlineHtmlSha256, restore: 'same-original-device-full-ZIP' },
  guide: 'guide.html', tf: 'docs/TF_GUIDE.html',
};
await put('BUILD.json', JSON.stringify(metadata, null, 2) + '\n');
const rows = [];
async function inventory(prefix) {
  for (const entry of await readdir(path.join(output, prefix), { withFileTypes: true })) {
    const name = path.posix.join(prefix, entry.name);
    if (entry.isDirectory()) await inventory(name);
    else if (entry.isFile()) rows.push({ name, digest: sha(await readFile(path.join(output, name))) });
    else throw Error('Unexpected output: ' + name);
  }
}
await inventory('');
rows.sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
await put('SITE_SHA256SUMS', rows.map(row => row.digest + '  ' + row.name + '\n').join(''));
console.log(version, '·', rows.length + 1, 'public files ·', output);
