// SPDX-License-Identifier: Apache-2.0
// Produce a complete Pages tree for review. No ref update or push.
import { execFileSync } from 'node:child_process';
import { cp, mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { build, root, output } from './build.mjs';
import { checkInstallerLinks } from './check-installer-links.mjs';
import { addProjectLinks, projectLinks } from './project-pages.mjs';
import { checkProjectPages } from './check-project-pages.mjs';

const repo = path.resolve(root, '../..');
const git = (...args) => execFileSync('git', args, { cwd: repo, encoding: 'utf8', maxBuffer: 8 * 1024 * 1024 }).trim();
const parent = git('rev-parse', '--verify', `${process.argv[2] || 'origin/gh-pages'}^{commit}`);
const target = path.resolve(root, process.argv[3] || 'dist-pages');
if (!path.basename(target).startsWith('dist-pages') || path.dirname(target) !== root) throw Error('Use a new dist-pages* directory inside docs/bgime');
const entries = git('ls-tree', '-r', parent).split('\n').map(line => {
  const m = line.match(/^(\d+) blob ([a-f0-9]+)\t(.+)$/);
  if (!m || m[1] !== '100644' || m[3].startsWith('/') || m[3].split('/').includes('..')) throw Error('Unsupported Pages tree entry: ' + line);
  return { name: m[3], oid: m[2] };
});
for (const name of ['.nojekyll', 'CNAME', 'index.html', 'first-install.html', 'guide.html', 'recovery.html', 'BUILD.json', 'SITE_SHA256SUMS']) {
  if (!entries.some(e => e.name === name)) throw Error('Installer baseline missing ' + name);
}
if (git('show', `${parent}:CNAME`) !== 'draftor.tcssn.org') throw Error('Unexpected Pages domain');
await build();
execFileSync(process.execPath, [fileURLToPath(new URL('./check.mjs', import.meta.url))], { stdio: 'inherit' });
await mkdir(target); // Refuse to overwrite any earlier candidate or user's files.
// Replace only generated project documents. Preserve all other installer files.
const generated = name => name.startsWith('bgime/') || ['branding/index.html', 'llms.txt'].includes(name);
for (const entry of entries.filter(e => !generated(e.name))) {
  const destination = path.join(target, entry.name);
  await mkdir(path.dirname(destination), { recursive: true });
  const bytes = execFileSync('git', ['cat-file', 'blob', entry.oid], { cwd: repo, maxBuffer: 128 * 1024 * 1024 });
  await writeFile(destination, bytes);
}
await cp(output, path.join(target, 'bgime'), { recursive: true, errorOnExist: true, force: false });
await mkdir(path.join(target, 'branding'), { recursive: true });
for (const name of ['branding/index.html', 'llms.txt']) await cp(path.join(path.dirname(output), name), path.join(target, name), { errorOnExist: true, force: false });
const homePath = path.join(target, 'index.html');
const originalHome = await readFile(homePath, 'utf8');
const home = addProjectLinks(originalHome);
await writeFile(homePath, home);
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const blobId = bytes => createHash('sha1').update(`blob ${bytes.length}\0`).update(bytes).digest('hex');
let preserved = 0;
for (const entry of entries.filter(e => !generated(e.name))) {
  if (['index.html', 'SITE_SHA256SUMS'].includes(entry.name)) continue;
  if (blobId(await readFile(path.join(target, entry.name))) !== entry.oid) throw Error('Installer file changed: ' + entry.name);
  preserved++;
}
const addedLinks = projectLinks.filter(link => !originalHome.includes(link));
if (addedLinks.reduce((text, link) => text.replace(link, ''), home) !== originalHome) throw Error('Unexpected homepage change');
const files = [];
async function inventory(dir = '') {
  for (const entry of await readdir(path.join(target, dir), { withFileTypes: true })) {
    const name = path.posix.join(dir, entry.name);
    if (entry.isDirectory()) await inventory(name);
    else if (entry.isFile()) { if (name !== 'SITE_SHA256SUMS') files.push({ name, sha256: sha256(await readFile(path.join(target, name))) }); }
    else throw Error('Unsupported output ' + name);
  }
}
await inventory(); files.sort((a,b) => a.name < b.name ? -1 : 1);
await writeFile(path.join(target, 'SITE_SHA256SUMS'), files.map(f => `${f.sha256}  ${f.name}\n`).join(''));
const installerLinks = await checkInstallerLinks(target);
const projectPages = await checkProjectPages(target, { installer: true });
const receipt = { parent, installerLinks, projectPages, output: target, unchangedInstallerFiles: preserved, allowedInstallerChanges: ['index.html: docs/policy footer links only', 'branding/index.html: generated policy', 'llms.txt: root discovery index', 'SITE_SHA256SUMS: combined inventory'], docsFiles: files.filter(f => f.name.startsWith('bgime/')).length, totalFiles: files.length + 1, result: 'PASS', deployment: false };
await writeFile(target + '-preservation.json', JSON.stringify(receipt, null, 2) + '\n');
console.log(JSON.stringify(receipt, null, 2));
