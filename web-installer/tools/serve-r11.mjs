// Static-only local review server. No upload, serial proxy, directory listing or private files.
import http from 'node:http';import {readFile,stat,realpath} from 'node:fs/promises';import path from 'node:path';
const root=await realpath(process.argv[2]),port=Number(process.argv[3]||4199);
if(!/\/(dist-r11-(public|test)(-dev[0-9]+)?|dist-r13-(public|software))$/.test(root)||!Number.isInteger(port)||port<1024||port>65535)throw Error('Explicit installer static build/port required');
const types={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.json':'application/json','.md':'text/plain; charset=utf-8','.zip':'application/zip','.bin':'application/octet-stream'};
http.createServer(async(req,res)=>{try{
 if(req.method!=='GET'&&req.method!=='HEAD'){res.writeHead(405);return res.end();}
 if(!new RegExp(`^(localhost|127\\.0\\.0\\.1):${port}$`).test(req.headers.host||'')){res.writeHead(403);return res.end();}
 const name=decodeURIComponent(new URL(req.url,'http://localhost').pathname);if(name.includes('\0')||name.includes('\\')||name.split('/').includes('..'))throw Error();
 const file=await realpath(path.join(root,name==='/'?'index.html':name));if(!file.startsWith(root+path.sep)||!(await stat(file)).isFile())throw Error();
 const bytes=await readFile(file);res.writeHead(200,{'Content-Type':types[path.extname(file)]||'application/octet-stream','Content-Length':bytes.length,'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Referrer-Policy':'no-referrer'});res.end(req.method==='HEAD'?undefined:bytes);
 }catch{res.writeHead(404);res.end('Not found');}}).listen(port,'127.0.0.1',()=>console.log(`R10 static-only review: http://localhost:${port}/`));
