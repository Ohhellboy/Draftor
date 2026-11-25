// SPDX-License-Identifier: Apache-2.0
import { readFile, writeFile, mkdir, copyFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import MarkdownIt from 'markdown-it';
import hljs from 'highlight.js/lib/core';
import cpp from 'highlight.js/lib/languages/cpp';
import bash from 'highlight.js/lib/languages/bash';
import cmake from 'highlight.js/lib/languages/cmake';
import { baseline, repository, pages, pageUrl } from '../site.config.mjs';
import { codeLabel } from '../assets/reader-utils.mjs';

export const root = path.resolve(fileURLToPath(new URL('..', import.meta.url)));
export const output = path.join(root, 'dist/bgime');
for (const [name, language] of Object.entries({ cpp, bash, cmake })) hljs.registerLanguage(name, language);
const escape = s => String(s).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const md = new MarkdownIt({ html: true, linkify: false, typographer: false,
  highlight: (str, language) => hljs.getLanguage(language) ? hljs.highlight(str, { language }).value : escape(str),
});
const plain = token => (token.children || []).map(t => t.type === 'html_inline' ? '' : t.content).join('');
const renderFence = md.renderer.rules.fence;
md.renderer.rules.fence = (tokens, index, ...rest) => {
  const language = tokens[index].info.trim().split(/\s+/u)[0];
  return renderFence(tokens, index, ...rest).replace('<pre>', `<pre><span class="code-label">${escape(codeLabel(language))}</span>`);
};
md.renderer.rules.table_open = () => '<div class="table-scroll" tabindex="0" role="region" aria-label="可横向滚动的表格"><table>\n';
md.renderer.rules.table_close = () => '</table></div>\n';
const slugify = s => s.normalize('NFKC').toLowerCase().replace(/[^\p{L}\p{N}_]+/gu, '-').replace(/^-|-$/g, '');

function navigation(current) {
  let group = '';
  return pages.map(p => {
    const heading = p.group !== group ? `<p class="nav-group">${p.group}</p>` : '';
    group = p.group;
    return `${heading}<a class="nav-link${p.slug === current.slug ? ' active' : ''}" href="${pageUrl(p.slug)}"${p.slug === current.slug ? ' aria-current="page"' : ''}>${p.title}</a>`;
  }).join('\n');
}
const mark = '<svg viewBox="0 0 32 32" aria-hidden="true"><path d="M8 25c-4-5-1-11 5-11h5V7c0-5 8-5 8 0v3h-5v9c0 6-8 10-13 6Z" fill="currentColor"/><path d="m26 7 5 2-5 1Z" fill="#a84732"/><circle cx="23" cy="6" r="1" fill="#fffaf4"/></svg>';
function template(p, article, headings, index) {
  const previous = pages[index - 1], next = pages[index + 1];
  const tocItems = headings.filter(h => h.level === 2).map(h => `<a href="#${encodeURIComponent(h.id)}">${escape(h.text)}</a>`).join('');
  const mobileToc = tocItems ? `<details class="mobile-toc"><summary>本页目录</summary><nav aria-label="本页目录（折叠）">${tocItems}</nav></details>` : '';
  article = article.replace('</h1>', `</h1>${mobileToc}`);
  return `<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>${p.title} · bgIME 开发者文档</title><meta name="description" content="${escape(p.description)}">
<meta name="theme-color" content="#f7f5ef"><link rel="canonical" href="https://draftor.tcssn.org${pageUrl(p.slug)}">
<link rel="icon" href="/bgime/assets/favicon.svg" type="image/svg+xml"><link rel="stylesheet" href="/bgime/assets/site.css">
<script type="module" src="/bgime/assets/site.js"></script></head>
<body><a class="skip-link" href="#main">跳到正文</a>
<header class="topbar"><a class="brand" href="/bgime/">${mark}<span>bgIME<small>开发者文档</small></span></a>
<div class="header-actions"><button class="search-trigger" type="button" aria-haspopup="dialog" aria-keyshortcuts="Control+k Meta+k"><span>搜索文档</span><kbd>Ctrl / ⌘ K</kbd></button>
<a class="header-site" href="/">草稿机安装</a><a class="header-github" href="${repository}">GitHub <span aria-hidden="true">↗</span></a>
<button class="menu-toggle" type="button" aria-expanded="false" aria-controls="sidebar">目录</button></div></header>
<div class="layout"><aside class="sidebar" id="sidebar"><nav aria-label="文档导航">${navigation(p)}</nav>
<div class="version-note"><span class="status-dot"></span> 适用于 Draftor Alpha 3<br><a href="/bgime/about/">版本与源码依据 <span aria-hidden="true">↗</span></a></div></aside>
<main id="main" tabindex="-1"><div class="breadcrumb">bgIME / ${p.group}</div><article class="prose${p.slug === '' ? ' introduction' : ''}">${article}</article>
<nav class="page-turn" aria-label="章节翻页">${previous ? `<a href="${pageUrl(previous.slug)}"><small>← 上一篇</small>${previous.title}</a>` : '<span></span>'}${next ? `<a class="next" href="${pageUrl(next.slug)}"><small>下一篇 →</small>${next.title}</a>` : '<span></span>'}</nav>
<footer class="article-footer">本文档适用于 Draftor Alpha 3 中使用的 bgIME。<br><a href="${repository}/tree/${baseline}">查看对应源码</a><span>·</span><a href="${repository}/issues">反馈文档问题</a><span>·</span><a href="/guide.html">草稿机操作指南</a></footer></main>
<aside class="toc"><nav aria-label="本页目录"><p>本页内容</p>${tocItems}</nav></aside></div>
<dialog class="search-dialog" aria-labelledby="search-title"><div class="search-top"><label id="search-title" for="search-input">搜索 bgIME 文档</label><button type="button" class="search-close" aria-label="关闭搜索">关闭 <kbd>Esc</kbd></button></div>
<input id="search-input" type="search" placeholder="输入关键词或接口名，例如：学习、confirm" autocomplete="off" spellcheck="false" aria-controls="search-results">
<p id="search-status" role="status" aria-live="polite">支持中文与接口名。多个关键词用空格分隔。</p><ol id="search-results"></ol></dialog>
</body></html>\n`;
}

export async function build() {
  await mkdir(path.join(output, 'assets'), { recursive: true });
  const search = [];
  const links = [];
  for (const [index, p] of pages.entries()) {
    let source = await readFile(path.join(root, 'content', p.file), 'utf8');
    source = source.replaceAll('@source/', `${repository}/blob/${baseline}/`)
      .replace(/@doc\/([a-z/-]*)/g, (_, slug) => {
        if (!pages.some(p => p.slug === slug)) throw Error('Unknown documentation link: ' + slug);
        return pageUrl(slug);
      });
    const tokens = md.parse(source, {}), headings = [], used = new Set();
    let section = { title: p.title, heading: p.title, url: pageUrl(p.slug), text: '' };
    for (let i = 0; i < tokens.length; i++) {
      const t = tokens[i];
      if (t.type === 'heading_open') {
        const text = plain(tokens[i + 1]), level = Number(t.tag.slice(1));
        let id = slugify(text), serial = 1;
        while (used.has(id)) id = `${slugify(text)}-${++serial}`;
        used.add(id); t.attrSet('id', id); headings.push({ text, id, level });
        if (level >= 2) {
          if (section.text.trim()) search.push(section);
          section = { title: p.title, heading: text, url: `${pageUrl(p.slug)}#${encodeURIComponent(id)}`, text: '' };
        }
      }
      if (t.type === 'inline') {
        section.text += plain(t) + ' ';
        for (const c of t.children || []) if (c.type === 'link_open') links.push({ from: p.file, url: c.attrGet('href') });
      }
      if (t.type === 'fence') section.text += t.content + ' ';
    }
    if (section.text.trim()) search.push(section);
    if (headings.filter(h => h.level === 1).length !== 1) throw Error(`${p.file}: expected one H1`);
    const target = path.join(output, p.slug);
    await mkdir(target, { recursive: true });
    await writeFile(path.join(target, 'index.html'), template(p, md.renderer.render(tokens, md.options, {}), headings, index));
  }
  await writeFile(path.join(output, 'search-index.json'), JSON.stringify(search));
  await writeFile(path.join(output, 'links.json'), JSON.stringify(links, null, 2) + '\n');
  await writeFile(path.join(output, 'version.json'), JSON.stringify({ product: 'Draftor Alpha 3', tag: 'v0.1.0-alpha.3', source: baseline }, null, 2) + '\n');
  for (const file of ['site.css', 'site.js', 'reader-utils.mjs', 'favicon.svg']) await copyFile(path.join(root, 'assets', file), path.join(output, 'assets', file));
  console.log(`Built ${pages.length} pages and ${search.length} search sections in ${output}`);
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await build();
