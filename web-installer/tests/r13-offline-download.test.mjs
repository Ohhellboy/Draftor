// SPDX-License-Identifier: Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import {PinnedAssetDownloader,isDownloadNetworkFailure} from '../src/firmware.js';
import {sha256} from '../src/safety.js';
const bytes=new TextEncoder().encode('<!doctype html><p>Public offline recovery fixture — no device</p>');
const options={url:'https://example.invalid/offline/tool.html',name:'tool.html',maxBytes:1000,sha256:await sha256(bytes)};
const category=wanted=>e=>{assert.equal(e.detail.category,wanted);return true;};
for(const headers of [{},{'content-length':String(bytes.length)},{'content-encoding':'br','content-length':'12'}])test('offline exact SHA with known, unknown or compressed length',async()=>{
 const states=[],loader=new PinnedAssetDownloader({...options,onProgress:s=>states.push(s),fetcher:async()=>new Response(bytes,{headers})});
 const result=await loader.load();assert.deepEqual(result,bytes);assert.equal(loader.snapshot().stage,'READY');assert.equal(loader.snapshot().verifiedCount,1);assert(states.some(s=>s.stage==='VERIFYING'&&s.receivedBytes===bytes.length));result.fill(0);loader.dispose();
});
test('offline body hang cancel settles even if stream cancel itself never acknowledges',async()=>{
 let aborted=false,cancelled=false,signal;
 const loader=new PinnedAssetDownloader({...options,fetcher:async(_url,opts)=>{signal=opts.signal;signal.addEventListener('abort',()=>aborted=true);return new Response(new ReadableStream({start(c){c.enqueue(bytes.slice(0,8));},cancel(){cancelled=true;return new Promise(()=>{});}}));}});
 const task=loader.load();await new Promise(r=>setTimeout(r,20));const t=performance.now();loader.cancel();
 await assert.rejects(task,category('CANCELLED'));assert(performance.now()-t<500);assert(aborted&&cancelled&&signal.aborted);assert.equal(loader.snapshot().verifiedCount,undefined);loader.dispose();
});
test('offline idle timeout is retryable, does not keep a total-transfer deadline',async()=>{
 let attempt=0,interval;const loader=new PinnedAssetDownloader({...options,idleTimeoutMs:100,fetcher:async()=>{
  if(attempt++===0)return new Response(new ReadableStream({start(c){c.enqueue(bytes.slice(0,3));}}));
  let offset=0;return new Response(new ReadableStream({start(c){interval=setInterval(()=>{const end=Math.min(bytes.length,offset+10);c.enqueue(bytes.slice(offset,end));offset=end;if(offset===bytes.length){clearInterval(interval);c.close();}},30);},cancel(){clearInterval(interval);}}));
 }});
 await assert.rejects(loader.load(),e=>{assert(isDownloadNetworkFailure(e));return category('IDLE_TIMEOUT')(e);});
 const t=performance.now(),result=await loader.load();assert(performance.now()-t>100);assert.deepEqual(result,bytes);assert.equal(loader.snapshot().attempt,2);assert.equal(attempt,2);result.fill(0);loader.dispose();
});
for(const [kind,reply,retryable] of [
 ['HTTP_STATUS',()=>new Response('',{status:503}),true],
 ['NETWORK',()=>Promise.reject(new TypeError('network interrupted')),true],
 ['INCOMPLETE',()=>new Response(bytes.slice(0,5),{headers:{'content-length':String(bytes.length)}}),true],
 ['LENGTH',()=>new Response(new Uint8Array(1001)),false],
 ['SHA256',()=>{const b=bytes.slice();b[2]^=1;return new Response(b);},false]
])test('offline '+kind+' has explicit classification and no unverified result',async()=>{
 const loader=new PinnedAssetDownloader({...options,fetcher:reply});await assert.rejects(loader.load(),e=>{assert.equal(isDownloadNetworkFailure(e),retryable);return category(kind)(e);});assert.equal(loader.snapshot().stage,'FAILED');assert.equal(loader.snapshot().verifiedCount,undefined);loader.dispose();
});
