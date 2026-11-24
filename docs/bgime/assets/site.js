// SPDX-License-Identifier: Apache-2.0
const $ = selector => document.querySelector(selector);
const toggle = $('.menu-toggle'), sidebar = $('#sidebar');
function closeMenu() { sidebar.classList.remove('open'); toggle.setAttribute('aria-expanded', 'false'); }
toggle.addEventListener('click', () => {
  const open = sidebar.classList.toggle('open'); toggle.setAttribute('aria-expanded', String(open));
});
document.addEventListener('click', e => { if (!sidebar.contains(e.target) && !toggle.contains(e.target)) closeMenu(); });
document.addEventListener('keydown', e => { if (e.key === 'Escape' && sidebar.classList.contains('open')) { closeMenu(); toggle.focus(); } });
for (const pre of document.querySelectorAll('.prose pre')) {
  const code = pre.querySelector('code'), button = document.createElement('button'), label = document.createElement('span');
  label.className = 'code-label'; label.textContent = code.className.replace('language-', '') || 'code';
  button.type = 'button'; button.className = 'copy-code'; button.textContent = '复制'; button.setAttribute('aria-label', '复制代码');
  button.addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(code.textContent); button.textContent = '已复制'; }
    catch { button.textContent = '请选中代码复制'; }
    setTimeout(() => { button.textContent = '复制'; }, 2500);
  });
  pre.prepend(label, button);
}
const dialog = $('.search-dialog'), input = $('#search-input'), results = $('#search-results'), status = $('#search-status');
let index, loading, sequence = 0;
const normalize = s => s.normalize('NFKC').toLocaleLowerCase();
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
      const text = r.text.replace(/\s+/g, ' '), at = Math.max(0, normalize(text).indexOf(terms[0]) - 28);
      snippet.textContent = (at ? '…' : '') + text.slice(at, at + 130) + (text.length > at + 130 ? '…' : '');
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
const tocLinks = [...document.querySelectorAll('.toc a')];
const observer = new IntersectionObserver(entries => {
  const entry = entries.find(e => e.isIntersecting); if (!entry) return;
  for (const a of tocLinks) a.classList.toggle('current', decodeURIComponent(a.hash.slice(1)) === entry.target.id);
}, { rootMargin: '-90px 0px -60% 0px', threshold: 0 });
document.querySelectorAll('.prose h2').forEach(h => observer.observe(h));
