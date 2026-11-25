// SPDX-License-Identifier: Apache-2.0
import { normalize, codeLanguage, codeLabel, searchExcerpt, activeHeadingIndex, shortcutLabel } from './reader-utils.mjs';
const $ = selector => document.querySelector(selector);
const toggle = $('.menu-toggle'), sidebar = $('#sidebar');
function closeMenu() { sidebar.classList.remove('open'); toggle.setAttribute('aria-expanded', 'false'); }
toggle.addEventListener('click', () => {
  const open = sidebar.classList.toggle('open'); toggle.setAttribute('aria-expanded', String(open));
});
document.addEventListener('click', e => { if (!sidebar.contains(e.target) && !toggle.contains(e.target)) closeMenu(); });
document.addEventListener('keydown', e => { if (e.key === 'Escape' && sidebar.classList.contains('open')) { closeMenu(); toggle.focus(); } });
for (const pre of document.querySelectorAll('.prose pre')) {
  const code = pre.querySelector('code'); if (!code) continue;
  const button = document.createElement('button');
  const label = pre.querySelector('.code-label');
  const name = label?.textContent || codeLabel(codeLanguage(code.className));
  button.type = 'button'; button.className = 'copy-code'; button.textContent = '复制'; button.setAttribute('aria-label', name === '输出' ? '复制输出' : '复制代码');
  button.addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(code.textContent); button.textContent = '已复制'; }
    catch { button.textContent = '请选中代码复制'; }
    setTimeout(() => { button.textContent = '复制'; }, 2500);
  });
  pre.prepend(button);
}
$('.search-trigger kbd').textContent = shortcutLabel(navigator.userAgentData?.platform || navigator.platform || '');
const dialog = $('.search-dialog'), input = $('#search-input'), results = $('#search-results'), status = $('#search-status');
let index, loading, sequence = 0;
async function getIndex() {
  if (index) return index;
  if (!loading) loading = fetch('/bgime/search-index.json').then(r => {
    if (!r.ok) throw Error('Search unavailable'); return r.json();
  }).then(rows => index = rows.map(r => ({ ...r, searchable: normalize(r.title + ' ' + r.heading + ' ' + r.text) }))).catch(e => { loading = undefined; throw e; });
  return loading;
}
async function search() {
  const turn = ++sequence, query = input.value.trim(); results.replaceChildren();
  if (!query) { status.textContent = '支持中文与接口名。多个关键词用空格分隔。'; return; }
  status.textContent = '正在查找…';
  try {
    const data = await getIndex(); if (turn !== sequence) return;
    const terms = normalize(query).split(/\s+/u);
    const matches = data.filter(r => terms.every(t => r.searchable.includes(t))).map(r => ({ ...r,
      score: terms.reduce((s, t) => s + (normalize(r.heading).includes(t) ? 3 : 0) + (normalize(r.title).includes(t) ? 2 : 0), 0),
    })).sort((a, b) => b.score - a.score);
    status.textContent = matches.length ? `找到 ${matches.length} 处内容${matches.length > 24 ? '，显示前 24 条' : ''}` : '没有找到相关内容。请尝试更短的关键词，或直接输入接口名。';
    for (const r of matches.slice(0, 24)) {
      const li = document.createElement('li'), a = document.createElement('a'), title = document.createElement('strong'), snippet = document.createElement('p');
      a.href = r.url; title.textContent = r.title === r.heading ? r.title : r.title + ' · ' + r.heading;
      snippet.textContent = searchExcerpt(r.text, terms);
      a.addEventListener('click', () => dialog.close());
      a.append(title, snippet); li.append(a); results.append(li);
    }
  } catch { if (turn === sequence) status.textContent = '搜索索引暂时无法读取。请检查网络或通过左侧目录查阅；再次输入可重试。'; }
}
function openSearch() { closeMenu(); dialog.showModal(); input.focus(); search(); }
$('.search-trigger').addEventListener('click', openSearch);
$('.search-close').addEventListener('click', () => dialog.close());
dialog.addEventListener('click', e => { if (e.target === dialog) { const r = dialog.getBoundingClientRect(); if (e.clientX < r.left || e.clientX > r.right || e.clientY < r.top || e.clientY > r.bottom) dialog.close(); } });
input.addEventListener('input', search);
dialog.addEventListener('keydown', e => {
  if (e.key === 'Escape') { e.preventDefault(); dialog.close(); return; }
  const links = [...results.querySelectorAll('a')];
  if (e.key === 'ArrowDown' && links.length) { e.preventDefault(); links[Math.min(links.indexOf(document.activeElement) + 1, links.length - 1)].focus(); }
  if (e.key === 'ArrowUp') { e.preventDefault(); const i = links.indexOf(document.activeElement); if (i > 0) links[i - 1].focus(); else input.focus(); }
  if (e.key === 'Enter' && document.activeElement === input && links.length) { e.preventDefault(); links[0].click(); }
});
document.addEventListener('keydown', e => {
  if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'k') { e.preventDefault(); if (dialog.open) dialog.close(); else openSearch(); }
});
const tocLinks = [...document.querySelectorAll('.toc a, .mobile-toc a')];
const headings = [...document.querySelectorAll('.prose h2')];
let tocFrame;
function updateToc() {
  tocFrame = undefined;
  const line = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--anchor-offset')) + 1;
  const bottom = window.scrollY > 0 && window.scrollY + window.innerHeight >= document.documentElement.scrollHeight - 2;
  const index = activeHeadingIndex(headings.map(h => h.getBoundingClientRect().top), line, bottom);
  for (const link of tocLinks) {
    const current = index >= 0 && decodeURIComponent(link.hash.slice(1)) === headings[index].id;
    link.classList.toggle('current', current);
    if (current) link.setAttribute('aria-current', 'location');
    else link.removeAttribute('aria-current');
  }
}
function scheduleToc() { if (tocFrame === undefined) tocFrame = requestAnimationFrame(updateToc); }
window.addEventListener('scroll', scheduleToc, { passive: true });
window.addEventListener('resize', scheduleToc);
window.addEventListener('hashchange', scheduleToc);
window.addEventListener('load', scheduleToc);
new ResizeObserver(scheduleToc).observe($('.prose'));
document.fonts?.ready.then(scheduleToc);
scheduleToc();
