// SPDX-License-Identifier: Apache-2.0
import { createServer } from 'node:http';
import { readFile, realpath, stat } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = path.resolve(fileURLToPath(new URL('..', import.meta.url)));
const directory = await realpath(path.resolve(root, process.argv[2] || 'dist'));
const port = Number(process.env.BGIME_DOCS_PORT || 4178);
const types = { '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8', '.json': 'application/json; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png', '.jpg': 'image/jpeg', '.md': 'text/plain; charset=utf-8', '.txt': 'text/plain; charset=utf-8', '.zip': 'application/zip', '.bin': 'application/octet-stream' };
const server = createServer(async (request, response) => {
  if (!['GET', 'HEAD'].includes(request.method)) { response.writeHead(405).end(); return; }
  try {
    const url = new URL(request.url, 'http://localhost');
    let file = path.resolve(directory, '.' + decodeURIComponent(url.pathname));
    if (file !== directory && !file.startsWith(directory + path.sep)) throw Error('Forbidden');
    if ((await stat(file)).isDirectory()) {
      if (!url.pathname.endsWith('/')) { response.writeHead(301, { Location: url.pathname + '/' + url.search }).end(); return; }
      file = path.join(file, 'index.html');
    }
    file = await realpath(file);
    if (!file.startsWith(directory + path.sep)) throw Error('Forbidden');
    const data = await readFile(file);
    response.writeHead(200, { 'Content-Type': types[path.extname(file)] || 'application/octet-stream', 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' });
    response.end(request.method === 'HEAD' ? undefined : data);
  } catch { response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' }).end('页面不存在。请访问 /bgime/ 或检查预览目录。'); }
});
server.on('error', error => { console.error(error.message); process.exitCode = 1; });
server.listen(port, '127.0.0.1', () => console.log(`Local: http://127.0.0.1:${port}/bgime/\nServing: ${directory}`));
for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, () => server.close());
