// SPDX-License-Identifier: Apache-2.0
import { readFile, stat, readdir } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import MarkdownIt from 'markdown-it';
import { strict as nodeAssert } from 'node:assert';
import { normalize, codeLanguage, codeLabel, searchExcerpt, activeHeadingIndex, shortcutLabel } from '../assets/reader-utils.js';
import { root, output } from './build.mjs';
import { baseline, repository, pages, pageUrl, headingAnchors } from '../site.config.mjs';
import { checkProjectPages } from './check-project-pages.mjs';

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
  const mobile = html.match(/<details class="mobile-toc">([\s\S]*?)<\/details>/)?.[1];
  const desktop = html.match(/<aside class="toc">([\s\S]*?)<\/aside>/)?.[1];
  assert(mobile && desktop, `${p.file}: missing responsive table of contents`);
  nodeAssert.deepEqual(attr(mobile, 'href'), attr(desktop, 'href'), `${p.file}: TOCs differ`);
  const article = html.match(/<article class="[^"]*">([\s\S]*?)<\/article>/)?.[1];
  assert(article, `${p.file}: article missing`);
  const sectionIds = [...article.matchAll(/<h2 id="([^"]+)"/g)].map(h => h[1]);
  nodeAssert.deepEqual(attr(desktop, 'href'), sectionIds.map(id => `#${encodeURIComponent(id)}`), `${p.file}: TOCs must use canonical section IDs`);
  const tocStart = article.indexOf('<details class="mobile-toc">');
  if (p.slug === '') {
    assert(/<h1[^>]*>(?:<span[^>]*><\/span>)*bgIME 嵌入式中文输入引擎<\/h1>/.test(article), 'Homepage H1');
    assert(!article.includes('eyebrow') && !article.includes('route-number'), 'Decorative homepage labels remain');
    assert(article.indexOf('class="architecture"') < tocStart && tocStart < article.indexOf('<h2'), 'Homepage TOC must follow its introduction');
    assert(/<\/p>\s*<details class="mobile-toc">/.test(article), 'Homepage introduction must remain continuous');
    assert(/<\/details><h2/.test(article), 'Homepage TOC must precede first H2');
  } else assert(/<\/h1><details class="mobile-toc">/.test(article), `${p.file}: article TOC must follow H1`);

  for (const [title, anchors] of Object.entries(headingAnchors[p.file] || {})) {
    const heading = [...article.matchAll(/<h([1-6]) id="([^"]+)"[^>]*>([\s\S]*?)<\/h\1>/g)].find(h => h[2] === anchors[0]);
    assert(heading && heading[3].replace(/<[^>]*>/g, '') === title, `${p.file}: stable heading target ${anchors[0]}`);
    for (const alias of anchors.slice(1)) assert(attr(heading[3], 'id').includes(alias), `${p.file}: missing in-heading alias ${alias}`);
  }

  assert(!/@(?:source|doc)\//.test(html), `${p.file}: unresolved link shorthand`);
  const canonical = new Set(Object.values(headingAnchors[p.file]).map(ids => ids[0]));
  const aliases = new Set(Object.values(headingAnchors[p.file]).flatMap(ids => ids.slice(1)));
  documents.set(pageUrl(p.slug), { html, ids: new Set(ids), canonical, aliases });
  for (const url of [...attr(html, 'href'), ...attr(html, 'src')]) allLinks.push({ from: pageUrl(p.slug), url });
}
const installerLinks = new Set(['/', '/guide.html', '/first-install.html', '/recovery.html', '/branding/']);
await checkProjectPages(path.dirname(output));
let checkedLocal = 0, checkedSource = 0;
for (const { from, url } of allLinks) {
  const target = new URL(url, 'http://preview' + from);
  if (target.origin === 'http://preview') {
    if (installerLinks.has(target.pathname)) continue;
    if (documents.has(target.pathname)) {
      if (target.hash) {
        const id = decodeURIComponent(target.hash.slice(1)), document = documents.get(target.pathname);
        assert(document.ids.has(id), `Broken anchor ${from} -> ${url}`);
        assert(!document.aliases.has(id), `Use the canonical section ID: ${from} -> ${url}`);
      }
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
  if (target.hash) assert(documents.get(target.pathname).canonical.has(decodeURIComponent(target.hash.slice(1))), `Search must use a canonical heading: ${section.url}`);
}
nodeAssert.equal(new Set(search.map(row => row.url)).size, search.length, 'Duplicate search sections');
for (const p of pages) {
  const canonical = new Set(Object.values(headingAnchors[p.file] || {}).map(ids => ids[0]));
  for (const alias of Object.values(headingAnchors[p.file] || {}).flatMap(ids => ids.slice(1))) {
    assert(!search.some(row => row.url === `${pageUrl(p.slug)}#${encodeURIComponent(alias)}`), `${p.file}: compatibility alias indexed`);
    assert(!canonical.has(alias), `${p.file}: alias duplicates a canonical anchor`);
  }
}
for (const term of ['共享选词', 'StateFilePort', 'UnknownOutcome', 'select_explicit', 'begin_input']) {
  assert(search.some(row => (row.title + row.heading + row.text).includes(term)), `Missing searchable topic ${term}`);
}
const assets = ['favicon.png', 'goose.png', 'reader-utils.js', 'site.css', 'site.js'];
nodeAssert.deepEqual((await readdir(path.join(output, 'assets'))).sort(), assets, 'Use a clean build directory: browser asset inventory differs');
for (const file of assets) nodeAssert.deepEqual(await readFile(path.join(output, 'assets', file)), await readFile(path.join(root, 'assets', file)), `Asset copy mismatch: ${file}`);
const client = await readFile(path.join(output, 'assets/site.js'), 'utf8');
assert(client.includes("from './reader-utils.js'") && !client.includes('reader-utils.mjs'), 'Browser helper import must use .js');
const parser = new MarkdownIt();
let snippets = 0, outputs = 0;
for (const p of pages) {
  const markdown = await readFile(path.join(root, 'content', p.file), 'utf8');
  for (const block of parser.parse(markdown, {}).filter(token => token.type === 'fence')) {
    const language = block.info.trim().split(/\s+/u)[0];
    if (codeLabel(language) === '输出') outputs++;
    if (!['cpp', 'c++'].includes(language)) continue;
    try {
      execFileSync('c++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsyntax-only', '-x', 'c++',
        ...['core', 'extension', 'wubi'].flatMap(module => ['-I', path.join(repo, 'integration/product/dependencies', module, 'include')]), '-'],
        { input: block.content, stdio: ['pipe', 'pipe', 'pipe'] });
    } catch (error) { throw Error(`${p.file}:${block.map[0] + 1}: C++ syntax check failed\n${error.stderr}`); }
    snippets++;
  }
  const html = documents.get(pageUrl(p.slug)).html;
  const labels = (html.match(/class="code-label">输出<\/span>/g) || []).length;
  assert(labels === parser.parse(markdown, {}).filter(t => t.type === 'fence' && codeLabel(t.info.trim().split(/\s+/u)[0]) === '输出').length, `${p.file}: output labels missing`);
}
// Regressions for mixed title/body search hits, TOC direction/boundaries and code metadata.
nodeAssert.equal(codeLanguage('hljs language-cpp extra'), 'cpp');
nodeAssert.equal(codeLanguage('language-c++ hljs'), 'c++');
nodeAssert.equal(codeLanguage('hljs'), '');
nodeAssert.equal(codeLabel('text'), '输出');
nodeAssert.equal(shortcutLabel('MacIntel'), '⌘ K');
nodeAssert.equal(shortcutLabel('Win32'), 'Ctrl K');
nodeAssert.equal(shortcutLabel('Linux x86_64'), 'Ctrl K');
const terms = ['五笔', 'activate'];
const activation = search.find(row => row.heading === '绑定初始化与激活');
assert(activation && terms.every(t => normalize(activation.title + activation.text).includes(t)), 'Missing 五笔 activate search result');
assert(searchExcerpt(activation.text, terms).includes('activate'), 'Search excerpt lost body hit');
assert(searchExcerpt('前言'.repeat(100) + ' activate 后文', terms).includes('activate'), 'Title-only first term must not mask a body hit');
nodeAssert.equal(activeHeadingIndex([300, 900, 1500], 101, false), 0); // top
nodeAssert.equal(activeHeadingIndex([-800, 90, 700], 101, false), 1); // down
nodeAssert.equal(activeHeadingIndex([-700, 110, 800], 101, false), 0); // back up
nodeAssert.equal(activeHeadingIndex([-700, 100, 800], 101, false), 1); // anchor
nodeAssert.equal(activeHeadingIndex([-700, -100, 500], 101, true), 2); // short final section
nodeAssert.equal(activeHeadingIndex([-700, 90, 800], 85, false), 0); // narrower header
nodeAssert.equal(activeHeadingIndex([], 101, false), -1);
nodeAssert.deepEqual(pages.slice(0, 5).map(p => p.slug), ['', 'quickstart', 'schemes', 'integration', 'api']);
for (const [i, p] of pages.entries()) {
  const turns = documents.get(pageUrl(p.slug)).html.match(/<nav class="page-turn"[\s\S]*?<\/nav>/)?.[0];
  nodeAssert.deepEqual(attr(turns, 'href'), [pages[i - 1], pages[i + 1]].filter(Boolean).map(p => pageUrl(p.slug)), `${p.file}: previous/next order`);
}
console.log(JSON.stringify({ pages: pages.length, localLinks: checkedLocal, pinnedSourceLinks: checkedSource, searchSections: search.length,
  cppSyntaxChecks: snippets, outputBlocks: outputs, browserAssets: assets, readerRegressions: 'PASS', installerAnchors: 'checked by prepare:pages against merged tree', result: 'PASS' }, null, 2));
