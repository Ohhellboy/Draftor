// SPDX-License-Identifier: Apache-2.0
// Test-only HTTP fault injection. Public bundles stay unchanged; no USB is used.
import http from 'node:http';
import {readFile} from 'node:fs/promises';
import path from 'node:path';
export async function networkServer(root,{artifactOnly=false}={}){
 const records=[];const count=new Map();
 const server=http.createServer(async(req,res)=>{
  const parts=new URL(req.url,'http://localhost').pathname.split('/').filter(Boolean),scenario=parts.shift(),name=parts.join('/');
  if(!scenario||!name||parts.includes('..')){res.writeHead(404).end();return;}
  const key=scenario+'/'+name,n=(count.get(key)||0)+1;count.set(key,n);
  const record={scenario,file:name,request:n,bytes:0,startedAt:Date.now(),finished:false,aborted:false};records.push(record);
  let timer;
  res.on('close',()=>{clearInterval(timer);record.finished=res.writableFinished;record.aborted=!res.writableFinished;record.elapsedMs=Date.now()-record.startedAt;});
  try{
   const location=artifactOnly?path.join(root,name):name.startsWith('firmware/')?path.join('public',name):name==='factory/FactoryOnly.bin'?'release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin':path.join(root,name);
   const bytes=await readFile(location),app=name==='firmware/app.bin',offline=name.startsWith('offline/')&&name.endsWith('.html');
   const type=name.endsWith('.js')?'text/javascript':name.endsWith('.html')?'text/html':name.endsWith('.css')?'text/css':'application/octet-stream';
   res.setHeader('Content-Type',type);res.setHeader('Cache-Control','no-store');
   const write=b=>{record.bytes+=b.length;res.write(b);};
   if(offline&&scenario==='tool-http-once'&&n===1){record.status=503;res.writeHead(503);res.end('temporary');return;}
   if(offline){
    record.status=200;res.setHeader('Content-Length',bytes.length);
    if(scenario==='tool-corrupt'){const b=Buffer.from(bytes);b[100]^=1;write(b);res.end();return;}
    if(scenario==='tool-interrupt-once'&&n===1){write(bytes.subarray(0,4096));timer=setTimeout(()=>res.destroy(),100);return;}
    if(scenario==='tool-cancel'||scenario==='tool-close-x'||scenario==='tool-close-escape'||(scenario==='tool-stall'&&n===1)){write(bytes.subarray(0,4096));return;}
    if(scenario==='tool-slow'){
     const size=Math.ceil(bytes.length/40);let offset=0;
     const chunk=()=>{const end=Math.min(bytes.length,offset+size);write(bytes.subarray(offset,end));offset=end;if(offset===bytes.length){clearInterval(timer);res.end();}};
     chunk();timer=setInterval(chunk,1650);return;
    }
   }
   if(app&&scenario==='http-once'&&n===1){record.status=503;res.writeHead(503);res.end('temporary');return;}
   record.status=200;
   if(app&&scenario==='truncate-once'&&n===1){write(bytes.subarray(0,50000));res.end();return;}
   if(app&&scenario==='corrupt-once'&&n===1){const b=Buffer.from(bytes);b[100]^=1;write(b);res.end();return;}
   res.setHeader('Content-Length',bytes.length);
   if(app&&scenario==='interrupt-once'&&n===1){write(bytes.subarray(0,50000));timer=setTimeout(()=>res.destroy(),100);return;}
   if(app&&['slow','stall','cancel','pagehide'].includes(scenario)){
    const size=Math.ceil(bytes.length/40);let offset=0;
    const chunk=()=>{const end=Math.min(bytes.length,offset+size);write(bytes.subarray(offset,end));offset=end;if(offset===bytes.length){clearInterval(timer);res.end();}};
    chunk();if(scenario==='slow')timer=setInterval(chunk,1650);return;
   }
   write(bytes);res.end();
  }catch{res.writeHead(404).end();}
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 return {base:`http://127.0.0.1:${server.address().port}`,records,close:()=>new Promise(resolve=>{server.closeAllConnections();server.close(resolve);})};
}
