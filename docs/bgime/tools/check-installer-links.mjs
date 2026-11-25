// SPDX-License-Identifier: Apache-2.0
import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { pages, pageUrl } from '../site.config.mjs';

const attributes = (html, name) => [...html.matchAll(new RegExp(`\\b${name}="([^"]+)"`, 'g'))]
  .map(match => match[1].replaceAll('&amp;', '&'));

// Run only after overlaying the documentation on the complete installer tree.
export async function checkInstallerLinks(tree) {
  const checked = new Set();
  for (const page of pages) {
    const html = await readFile(path.join(tree, 'bgime', page.slug, 'index.html'), 'utf8');
    for (const href of [...attributes(html, 'href'), ...attributes(html, 'src')]) {
      const target = new URL(href, 'https://draftor.tcssn.org' + pageUrl(page.slug));
      if (target.origin !== 'https://draftor.tcssn.org' || target.pathname.startsWith('/bgime/')) continue;
      if (checked.has(target.href)) continue;
      const pathname = decodeURIComponent(target.pathname);
      let file = path.resolve(tree, '.' + pathname);
      if (!file.startsWith(path.resolve(tree) + path.sep) && file !== path.resolve(tree)) throw Error('Unsafe installer URL: ' + href);
      if ((await stat(file)).isDirectory()) file = path.join(file, 'index.html');
      if (!(await stat(file)).isFile()) throw Error('Missing installer target: ' + href);
      if (target.hash) {
        const body = await readFile(file, 'utf8');
        const ids = [...attributes(body, 'id'), ...attributes(body, 'name')];
        if (!ids.includes(decodeURIComponent(target.hash.slice(1)))) throw Error(`Broken installer anchor ${pageUrl(page.slug)} -> ${href}`);
      }
      checked.add(target.href);
    }
  }
  return [...checked].sort();
}
