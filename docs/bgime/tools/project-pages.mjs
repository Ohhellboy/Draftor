// SPDX-License-Identifier: Apache-2.0
// One policy source for repository readers, the website and installer builds.
import { mkdir, readFile, writeFile, copyFile, cp } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import MarkdownIt from 'markdown-it';

const root = fileURLToPath(new URL('..', import.meta.url));
const repo = path.resolve(root, '../..');
export const projectLinks = [
  '<a href="bgime/">bgIME 开发者文档</a>',
  '<a href="/branding/">名称与标识使用政策</a>',
];

export function addProjectLinks(home) {
  const marker = '<nav aria-label="帮助与信息">';
  if (home.split(marker).length !== 2) throw Error('Homepage navigation marker missing or ambiguous');
  for (const link of projectLinks) {
    if (home.split(link).length > 2) throw Error('Duplicate project footer link');
    if (!home.includes(link)) home = home.replace(marker, marker + link);
  }
  return home;
}

export function renderPolicy(source) {
  const md = new MarkdownIt({ html: false, linkify: true, typographer: false });
  const tokens = md.parse(source, {});
  let language = 'zh';
  for (let i = 0; i < tokens.length; i++) {
    const token = tokens[i];
    if (token.type !== 'heading_open') continue;
    const title = tokens[i + 1].content;
    if (token.tag === 'h2') {
      language = title === 'English' ? 'en' : 'zh';
      token.attrSet('id', language);
      if (language === 'en') token.attrSet('lang', 'en');
    }
    if (token.tag === 'h3') token.attrSet('id', `${language}-${title.match(/^\d+/)?.[0]}`);
  }
  const html = md.renderer.render(tokens, md.options, {});
  return html.replace('<h2 id="en"', '<div lang="en"><h2 id="en"') + '</div>\n';
}

export async function buildProjectPages(destination) {
  const source = await readFile(path.join(repo, 'TRADEMARKS.md'), 'utf8');
  const html = `<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>名称与标识使用政策 · Draftor / bgIME</title>
<meta name="description" content="Draftor 与 bgIME 的中英双语名称与标识使用政策。Name and Branding Policy for Draftor and bgIME.">
<link rel="canonical" href="https://draftor.tcssn.org/branding/">
<link rel="stylesheet" href="/bgime/assets/site.css">
<style>.policy-header{position:static;height:auto;min-height:78px;padding-block:16px;flex-wrap:wrap;gap:16px}.policy-header nav{display:flex;flex-wrap:wrap;gap:12px 22px;font-size:14px}.policy-main{max-width:900px;margin:auto}.policy-main a{overflow-wrap:anywhere}.policy-languages{display:flex;gap:24px;margin-bottom:28px}</style>
</head><body><a class="skip-link" href="#main">跳到正文</a>
<header class="topbar policy-header"><a class="brand" href="/">Draftor / bgIME</a><nav aria-label="项目入口"><a href="/">Draftor 网站</a><a href="/bgime/">bgIME 文档</a><a href="https://github.com/Ohhellboy/Draftor">GitHub</a></nav></header>
<main class="policy-main" id="main" tabindex="-1"><nav class="policy-languages" aria-label="语言 / Language"><a href="#zh" lang="zh-CN">中文</a><a href="#en" lang="en">English</a></nav>
<article class="prose">
${renderPolicy(source)}</article>
<footer class="article-footer"><a href="https://github.com/Ohhellboy/Draftor/blob/main/TRADEMARKS.md">政策源文件 / Policy source</a></footer></main></body></html>
`;
  await mkdir(path.join(destination, 'branding'), { recursive: true });
  await mkdir(path.join(destination, 'bgime'), { recursive: true });
  await writeFile(path.join(destination, 'branding/index.html'), html);
  await copyFile(path.join(repo, 'llms.txt'), path.join(destination, 'llms.txt'));
  await copyFile(path.join(root, 'llms.txt'), path.join(destination, 'bgime/llms.txt'));
}

// Called before the full installer's final manifest is calculated. No device code runs.
export async function overlayProjectDocs(target) {
  execFileSync(process.execPath, [path.join(root, 'tools/build.mjs')], { stdio: 'inherit' });
  execFileSync(process.execPath, [path.join(root, 'tools/check.mjs')], { stdio: 'inherit' });
  for (const name of ['bgime', 'branding', 'llms.txt']) {
    await cp(path.join(root, 'dist', name), path.join(target, name), { recursive: true });
  }
  const home = path.join(target, 'index.html');
  await writeFile(home, addProjectLinks(await readFile(home, 'utf8')));
}
