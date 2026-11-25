// SPDX-License-Identifier: Apache-2.0
// Keep the historical installer/source-package builders unchanged; compose the
// complete current website before publishing its final file manifest.
import { execFileSync } from 'node:child_process';
import { readFile, writeFile, readdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { overlayProjectDocs } from '../../docs/bgime/tools/project-pages.mjs';
import { checkProjectPages } from '../../docs/bgime/tools/check-project-pages.mjs';
import { checkInstallerLinks } from '../../docs/bgime/tools/check-installer-links.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
const [kind, ...args] = process.argv.slice(2);
if (!['default', 'r12-public'].includes(kind)) throw Error('Expected default or r12-public build');
if (kind === 'default' && args.length) throw Error('Use the historical device-test scripts for device test builds');
if (kind === 'r12-public' && args[0] !== 'dist-r12-public') throw Error('Expected dist-r12-public');
execFileSync(process.execPath, [path.join(root, 'tools', kind === 'default' ? 'build.mjs' : 'build-r12-public.mjs'), ...args], { cwd: root, stdio: 'inherit' });
const output = path.join(root, kind === 'default' ? 'dist' : 'dist-r12-public');
await overlayProjectDocs(output);
await checkProjectPages(output, { installer: true });
await checkInstallerLinks(output);
const rows = [];
async function inventory(dir = '') {
  for (const entry of await readdir(path.join(output, dir), { withFileTypes: true })) {
    const name = path.posix.join(dir, entry.name);
    if (entry.isDirectory()) await inventory(name);
    else if (entry.isFile() && name !== 'SITE_SHA256SUMS') rows.push(`${createHash('sha256').update(await readFile(path.join(output, name))).digest('hex')}  ${name}`);
    else if (!entry.isFile()) throw Error('Unsupported output ' + name);
  }
}
await inventory();
rows.sort((a, b) => a.slice(66).localeCompare(b.slice(66), 'en'));
await writeFile(path.join(output, 'SITE_SHA256SUMS'), rows.join('\n') + '\n');
console.log('Complete installer, bgIME, policy and discovery files:', output);
