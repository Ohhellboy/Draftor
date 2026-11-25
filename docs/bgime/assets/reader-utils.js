// SPDX-License-Identifier: Apache-2.0
export const normalize = text => text.normalize('NFKC').toLowerCase();

export function codeLanguage(classes) {
  return classes.split(/\s+/u).find(name => /^language-[\w+-]+$/u.test(name))?.slice(9) || '';
}

export function codeLabel(language) {
  return ['text', 'plaintext', 'output', 'console'].includes(language) ? '输出' : language || '代码';
}

export function searchExcerpt(body, terms, length = 130) {
  const text = body.replace(/\s+/gu, ' ').normalize('NFKC');
  // A term may match only the page title. Locate the excerpt using body hits.
  const hits = terms.map(term => normalize(text).indexOf(term)).filter(index => index >= 0);
  const start = hits.length ? Math.max(0, Math.min(...hits) - 28) : 0;
  return (start ? '…' : '') + text.slice(start, start + length) + (text.length > start + length ? '…' : '');
}

export function activeHeadingIndex(tops, anchorLine, atBottom) {
  if (!tops.length) return -1;
  if (atBottom) return tops.length - 1;
  let current = 0;
  for (let i = 0; i < tops.length; i++) {
    if (tops[i] <= anchorLine) current = i;
    else break;
  }
  return current;
}

export function shortcutLabel(platform) {
  return /Mac|iPhone|iPad|iPod/i.test(platform) ? '⌘ K' : 'Ctrl K';
}
