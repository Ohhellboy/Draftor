// SPDX-License-Identifier: Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {ReleaseDownloader,releaseIsVerified} from '../src/firmware.js';
import {PINS} from '../src/pins.js';
const inputs=Object.fromEntries(await Promise.all(Object.keys({...PINS.documents,...PINS.files}).map(async n=>[n,new Uint8Array(await readFile(new URL('../public/firmware/'+n,import.meta.url)))])));
const name=u=>new URL(u).pathname.split('/').at(-1),base=new URL('https://test.invalid/firmware/');
const good=async u=>new Response(inputs[name(u)]);
function failure(category){return e=>{assert.equal(e.detail.category,category);assert.equal(typeof e.detail.stage,'string');return true;};}
for(const [category,respond] of [
 ['NETWORK',()=>{throw new TypeError('private browser detail must not leak');}],
 ['HTTP_STATUS',()=>new Response('',{status:503})],
 ['INCOMPLETE',b=>new Response(b.slice(0,-1))],
 ['LENGTH',b=>new Response(new Uint8Array(b.length+1))],
 ['LENGTH',b=>new Response(b,{headers:{'Content-Length':String(b.length-1)}})],
 ['SHA256',b=>{const c=b.slice();c[0]^=1;return new Response(c);}],
 ['NETWORK',b=>new Response(new ReadableStream({start(c){c.enqueue(b.slice(0,10));},pull(c){c.error(new TypeError('connection interrupted'));}}))]
])test(`typed download failure ${category}; unverified content never cached`,async()=>{
 let count=0;const loader=new ReleaseDownloader({base,fetcher:u=>{count++;return respond(inputs[name(u)]);}});
 await assert.rejects(loader.load(),failure(category));assert.equal(count,1);assert.deepEqual(loader.snapshot().verifiedFiles,[]);
 assert(!JSON.stringify(loader.snapshot()).includes('private browser'));loader.dispose();
});
test('manual retry reuses exactly the verified files; remaining four assets checked, no automatic retry',async()=>{
 const calls={},events=[];let retry=false;
 const loader=new ReleaseDownloader({base,onProgress:s=>events.push(s),fetcher:u=>{const n=name(u);calls[n]=(calls[n]||0)+1;if(n==='app.bin'&&!retry)return new Response('',{status:502});return good(u);}});
 await assert.rejects(loader.load(),failure('HTTP_STATUS'));
 assert.equal(calls['app.bin'],1);assert.deepEqual(loader.snapshot().verifiedFiles,Object.keys(PINS.documents));
 assert.equal(loader.snapshot().error.httpStatus,502);retry=true;
 assert(releaseIsVerified(await loader.load()));assert.equal(calls['app.bin'],2);
 for(const n of Object.keys(PINS.documents))assert.equal(calls[n],1);
 assert.equal(loader.snapshot().verifiedFiles.length,8);
 for(const n of Object.keys(inputs))assert(events.some(s=>s.file===n&&s.stage==='VERIFYING'&&s.receivedBytes===inputs[n].length));
 assert.equal(loader.snapshot().history.filter(e=>e.result==='VERIFIED_CACHE').length,4);
 loader.dispose();assert.deepEqual(loader.snapshot().verifiedFiles,[]);
});
test('corrupted download can be manually fetched again, never reused',async()=>{
 let corrupt=true,count=0;const loader=new ReleaseDownloader({base,fetcher:u=>{count++;if(corrupt){const b=inputs[name(u)].slice();b[0]^=1;return new Response(b);}return good(u);}});
 await assert.rejects(loader.load(),failure('SHA256'));corrupt=false;assert(releaseIsVerified(await loader.load()));assert.equal(count,9);loader.dispose();
});
test('inactivity aborts headers and an unresponsive body; cancellation releases stream',async()=>{
 for(const where of ['headers','body','empty-chunks']){
  let aborted=false,cancelled=false,interval;
  const loader=new ReleaseDownloader({base,idleTimeoutMs:150,fetcher:(_u,{signal})=>{
   signal.addEventListener('abort',()=>{aborted=true;});
   if(where==='headers')return new Promise(()=>{});
   return new Response(new ReadableStream({start(c){if(where==='empty-chunks')interval=setInterval(()=>c.enqueue(new Uint8Array()),10);},cancel(){cancelled=true;clearInterval(interval);}}));
  }});
  await assert.rejects(loader.load(),failure('IDLE_TIMEOUT'));assert(aborted);assert.deepEqual(loader.snapshot().verifiedFiles,[]);
  if(where!=='headers')assert(cancelled);loader.dispose();
 }
});
test('explicit cancel while waiting for data settles promptly and allows no cached partial file',async()=>{
 let abort=false,cancel=false;const loader=new ReleaseDownloader({base,fetcher:(_u,{signal})=>{signal.addEventListener('abort',()=>abort=true);return new Response(new ReadableStream({start(c){c.enqueue(new Uint8Array([1]));},cancel(){cancel=true;}}));}});
 const task=loader.load();setTimeout(()=>loader.cancel(),20);await assert.rejects(task,failure('CANCELLED'));assert(abort);assert(cancel);assert.equal(loader.snapshot().verifiedFiles.length,0);loader.dispose();
});
test('progress deadline resets on real bytes; total duration can exceed the idle deadline',async()=>{
 const loader=new ReleaseDownloader({base,idleTimeoutMs:300,fetcher:u=>{
  const b=inputs[name(u)];if(name(u)!=='app.bin')return good(u);let n=0,interval;
  return new Response(new ReadableStream({start(c){interval=setInterval(()=>{const end=Math.min(b.length,n+Math.ceil(b.length/8));c.enqueue(b.slice(n,end));n=end;if(n===b.length){clearInterval(interval);c.close();}},80);},cancel(){clearInterval(interval);}}));
 }});const start=performance.now();assert(releaseIsVerified(await loader.load()));assert(performance.now()-start>600);loader.dispose();
});
test('disposed preparation cannot be restarted; externally aborted request cannot connect',async()=>{
 const control=new AbortController();control.abort();let calls=0;const loader=new ReleaseDownloader({base,fetcher:u=>{calls++;return good(u);}});
 await assert.rejects(loader.load({signal:control.signal}),failure('CANCELLED'));assert.equal(calls,0);loader.dispose();await assert.rejects(loader.load(),{code:'DOWNLOAD_STATE'});
});

test('browser fetch is invoked with its Window/global receiver',async()=>{
 const loader=new ReleaseDownloader({base,fetcher:function(u){assert.equal(this,globalThis);return good(u);}});
 assert(releaseIsVerified(await loader.load()));loader.dispose();
});
