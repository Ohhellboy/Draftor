// SPDX-License-Identifier: Apache-2.0
import { readFile, stat } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { root, output } from './build.mjs';
import { baseline, repository, pages, pageUrl } from '../site.config.mjs';

const repo = path.resolve(root, '../..');
const sourcePaths = new Set(execFileSync('git', ['ls-tree', '-r', '--name-only', baseline], { cwd: repo, encoding: 'utf8' }).trim().split('\n'));
const documents = new Map(), allLinks = [];
const attr = (html, name) => [...html.matchAll(new RegExp(`\\b${name}="([^"]+)"`, 'g'))].map(m => m[1].replaceAll('&amp;', '&'));
function assert(ok, message) { if (!ok) throw Error(message); }
for (const p of pages) {
  const html = await readFile(path.join(output, p.slug, 'index.html'), 'utf8');
  const ids = attr(html, 'id');
  assert(ids.length === new Set(ids).size, `${p.file}: duplicate anchors`);
  assert(html.includes('lang="zh-CN"') && html.includes('name="viewport"'), `${p.file}: missing language/viewport`);
  assert(html.includes(`href="${pageUrl(p.slug)}" aria-current="page"`), `${p.file}: active navigation missing`);
  for (const other of pages) assert(html.includes(`href="${pageUrl(other.slug)}"`), `${p.file}: page absent from navigation`);
  assert(!/@(?:source|doc)\//.test(html), `${p.file}: unresolved link shorthand`);
  documents.set(pageUrl(p.slug), { html, ids: new Set(ids) });
  for (const url of [...attr(html, 'href'), ...attr(html, 'src')]) allLinks.push({ from: pageUrl(p.slug), url });
}
const installerLinks = new Set(['/', '/guide.html']);
let checkedLocal = 0, checkedSource = 0;
for (const { from, url } of allLinks) {
  const target = new URL(url, 'http://preview' + from);
  if (target.origin === 'http://preview') {
    if (installerLinks.has(target.pathname)) continue;
    if (documents.has(target.pathname)) {
      if (target.hash) assert(documents.get(target.pathname).ids.has(decodeURIComponent(target.hash.slice(1))), `Broken anchor ${from} -> ${url}`);
    } else {
      assert(target.pathname.startsWith('/bgime/'), `Unexpected local link ${from} -> ${url}`);
      assert((await stat(path.join(root, 'dist', decodeURIComponent(target.pathname)))).isFile(), `Missing asset ${url}`);
    }
    checkedLocal++;
  } else if (url.startsWith(repository + '/blob/')) {
    const prefix = `${repository}/blob/${baseline}/`;
    assert(url.startsWith(prefix), `Unpinned source link ${url}`);
    const file = decodeURIComponent(url.slice(prefix.length).split('#')[0]);
    assert(sourcePaths.has(file), `Missing Alpha 3 source ${file}`); checkedSource++;
  }
}
const search = JSON.parse(await readFile(path.join(output, 'search-index.json'), 'utf8'));
for (const section of search) {
  const target = new URL(section.url, 'http://preview');
  assert(documents.has(target.pathname), `Missing search target ${section.url}`);
  if (target.hash) assert(documents.get(target.pathname).ids.has(decodeURIComponent(target.hash.slice(1))), `Missing search fragment ${section.url}`);
}
for (const term of ['共享选词', 'StateFilePort', 'UnknownOutcome', 'select_explicit', 'begin_input']) {
  assert(search.some(row => (row.title + row.heading + row.text).includes(term)), `Missing searchable topic ${term}`);
}
let snippets = 0;
for (const p of pages) {
  const markdown = await readFile(path.join(root, 'content', p.file), 'utf8');
  for (const block of markdown.matchAll(/```cpp\n([\s\S]*?)```/g)) {
    execFileSync('c++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsyntax-only', '-x', 'c++', '-I', path.join(repo, 'integration/product/dependencies/core/include'), '-'], { input: block[1], stdio: ['pipe', 'pipe', 'pipe'] });
    snippets++;
  }
}
console.log(JSON.stringify({ pages: pages.length, localLinks: checkedLocal, pinnedSourceLinks: checkedSource, searchSections: search.length, compiledCppSnippets: snippets, result: 'PASS' }, null, 2));
