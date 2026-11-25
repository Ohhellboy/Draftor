// SPDX-License-Identifier: Apache-2.0
import { strict as assert } from 'node:assert';
import { readFile, stat } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import MarkdownIt from 'markdown-it';
import { renderPolicy, projectLinks } from './project-pages.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
const repo = path.resolve(root, '../..');
const attributes = (html, name) => [...html.matchAll(new RegExp(`\\b${name}="([^"]+)"`, 'g'))].map(m => m[1].replaceAll('&amp;', '&'));
const md = new MarkdownIt({ linkify: true });
const links = source => md.parse(source, {}).flatMap(t => t.children || []).filter(t => t.type === 'link_open').map(t => t.attrGet('href'));

export async function checkProjectPages(tree, { installer = false } = {}) {
  const source = await readFile(path.join(repo, 'TRADEMARKS.md'), 'utf8');
  const policy = await readFile(path.join(tree, 'branding/index.html'), 'utf8');
  assert.equal(policy.match(/<article class="prose">\n([\s\S]*?)<\/article>/)?.[1], renderPolicy(source), 'Policy must be rendered from the authoritative file');
  assert(policy.includes('rel="canonical" href="https://draftor.tcssn.org/branding/"'));
  assert(!/<script\b/i.test(policy), 'Policy text must work without JavaScript');
  for (const id of ['zh', 'en', ...['zh', 'en'].flatMap(lang => Array.from({ length: 9 }, (_, i) => `${lang}-${i + 1}`))]) assert(attributes(policy, 'id').includes(id), `Missing policy section ${id}`);
  const urls = new Set([...attributes(policy, 'href')]);
  for (const [input, output] of [['llms.txt', 'llms.txt'], ['docs/bgime/llms.txt', 'bgime/llms.txt']]) {
    const text = await readFile(path.join(repo, input), 'utf8');
    assert.equal(await readFile(path.join(tree, output), 'utf8'), text, `${output}: source copy differs`);
    assert(/^# [^\n]+\n\n> /u.test(text) && /^## /mu.test(text), `${input}: expected Markdown discovery index`);
    for (const href of links(text)) urls.add(href);
  }
  const checked = [];
  for (const href of urls) {
    const url = new URL(href, 'https://draftor.tcssn.org/branding/');
    if (url.origin === 'https://draftor.tcssn.org') {
      if (!installer && ['/', '/guide.html'].includes(url.pathname)) continue;
      const relative = decodeURIComponent(url.pathname).replace(/^\//, '');
      let file = path.resolve(tree, relative);
      assert(file === path.resolve(tree) || file.startsWith(path.resolve(tree) + path.sep), 'Unsafe project link');
      if ((await stat(file)).isDirectory()) file = path.join(file, 'index.html');
      assert((await stat(file)).isFile(), `Missing project page: ${href}`);
      if (url.hash) assert(attributes(await readFile(file, 'utf8'), 'id').includes(decodeURIComponent(url.hash.slice(1))), `Missing policy anchor: ${href}`);
    } else if (url.origin === 'https://github.com') {
      assert(url.pathname.startsWith('/Ohhellboy/Draftor'), `Unexpected project source: ${href}`);
      const match = url.pathname.match(/^\/Ohhellboy\/Draftor\/(?:blob|tree)\/([^/]+)(?:\/(.*))?$/);
      if (match && match[2]) {
        if (match[1] === 'main') assert((await stat(path.join(repo, match[2]))).isFile(), `Missing default-branch source: ${href}`);
        else execFileSync('git', ['cat-file', '-e', `${match[1]}:${match[2]}`], { cwd: repo });
      } else if (match) execFileSync('git', ['cat-file', '-e', `${match[1]}^{commit}`], { cwd: repo });
    } else throw Error('Unexpected project link: ' + href);
    checked.push(url.href);
  }
  const about = await readFile(path.join(tree, 'bgime/about/index.html'), 'utf8');
  assert(about.includes('<h2 id="名称与标识使用">') && about.includes('href="/branding/"'), 'Missing documentation policy entry');
  if (installer) {
    const home = await readFile(path.join(tree, 'index.html'), 'utf8');
    for (const link of projectLinks) assert(home.split(link).length === 2, 'Missing or duplicate installer project link');
  }
  return { policy: 'TRADEMARKS.md', policySections: 18, discoveryIndexes: 2, checkedLinks: [...new Set(checked)].sort() };
}
