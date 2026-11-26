// SPDX-License-Identifier: Apache-2.0
// Formal artifact bytes + an independent synthetic ROM. No physical USB.
import {build} from 'esbuild';
import {chromium} from '@playwright/test';
import {readFile,writeFile,realpath} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import assert from 'node:assert/strict';
import {networkServer} from '../tests/r13-network-server.mjs';
const [root,out,engine='chrome',mode='fixed']=process.argv.slice(2);
assert(root&&out);const hash=b=>createHash('sha256').update(b).digest('hex');
const meta=JSON.parse(await readFile(root+'/BUILD.json','utf8'));
const identity={classification:'FORMAL_RELEASE_ARTIFACT_WITH_SYNTHETIC_SERIAL',root:await realpath(root),version:meta.version,sourceCommit:meta.sourceCommit,
 manifestSha256:hash(await readFile(root+'/SITE_SHA256SUMS')),appBundleSha256:hash(await readFile(root+'/assets/app.js')),offlineSha256:hash(await readFile(root+'/'+meta.recovery.offlineHtml))};
assert.equal(identity.appBundleSha256,meta.appBundleSha256);assert.equal(identity.offlineSha256,meta.recovery.offlineSha256);
const fixture=await build({entryPoints:['tests/r13-offline-browser-port.mjs'],bundle:true,format:'iife',write:false,target:['chrome120'],loader:{'.bin':'binary'},alias:{'node:assert/strict':path.resolve('tests/browser-assert.mjs'),'node:crypto':path.resolve('tests/browser-crypto.mjs')}});
const fixtureCode=fixture.outputFiles[0].text;
const net=await networkServer(root,{artifactOnly:true}),edge=engine!=='chrome';
const browser=await chromium.launch(edge?{executablePath:engine,headless:false}:{channel:'chrome',headless:true});
const results=[];let error;
async function run(scenario,lang='zh'){
 const ctx=await browser.newContext({viewport:{width:1280,height:1000},reducedMotion:'reduce'});await ctx.addInitScript({content:fixtureCode});
 const page=await ctx.newPage(),pageErrors=[],scripts=[];page.on('pageerror',e=>pageErrors.push(e.message));
 page.on('response',r=>{if(new URL(r.url()).pathname.endsWith('/assets/app.js'))scripts.push(r.body().then(hash));});
 await page.goto(net.base+'/'+scenario+'/first-install.html?scenario=factory');await page.bringToFront();
 await page.locator('#test-wire').waitFor();assert.equal(JSON.parse(await page.locator('#test-wire').textContent()).realUsbAvailable,false);
 if(lang==='en')await page.locator('#lang-toggle').click();await page.locator('#install-btn').click();
 await page.locator('#connect-section').waitFor({state:'visible'});await page.locator('#physical-board').check();await page.locator('#prepared-tf').check();await page.locator('#connect-read').click();
 await page.waitForFunction(()=>!document.querySelector('#download-tool').disabled,null,{timeout:45000});
 const counters=async()=>JSON.parse(await page.locator('#test-wire').textContent());
 const before=await counters();assert.equal(before.opens,1);assert.equal(before.flashBegin,0);assert.equal(before.flashData,0);
 let receipt,closeMs,baselineHeldOpen=false;const initial=Date.now();
 await page.locator('#download-tool').click();
 if(['tool-cancel','tool-close-x','tool-close-escape'].includes(scenario)){
  await page.waitForFunction(()=>__r13OfflineEvents.some(e=>e.event==='HTTP_START'));
  if(mode==='fixed')await page.waitForFunction(()=>JSON.parse(document.querySelector('#tool-download-details pre').textContent).receivedBytes>0);
  const start=Date.now();
  if(scenario==='tool-close-escape')await page.keyboard.press('Escape');else await page.locator(scenario==='tool-close-x'?'#install-close':'#stop-close').click();
  if(mode==='baseline'){
   await new Promise(r=>setTimeout(r,2200));baselineHeldOpen=await page.locator('#install-dialog').isVisible();assert(baselineHeldOpen);assert.equal((await counters()).closes,0);
   assert(!await page.evaluate(()=>__r13OfflineEvents.some(e=>e.event==='HTTP_ABORT')));
  }else{
   await page.locator('#install-dialog').waitFor({state:'hidden',timeout:4000});closeMs=Date.now()-start;assert(closeMs<4000);
   await page.waitForFunction(()=>JSON.parse(document.querySelector('#test-wire').textContent).closes===1);
   const event=page.waitForEvent('download');await page.locator('#closed-receipt').click();receipt=JSON.parse(await readFile(await (await event).path(),'utf8'));
   assert.equal(receipt.offlineToolDownload.error.category,'CANCELLED');
   const events=await page.evaluate(()=>__r13OfflineEvents);assert(events.findIndex(e=>e.event==='HTTP_ABORT')>=0);assert(events.findIndex(e=>e.event==='HTTP_ABORT')<events.findIndex(e=>e.event==='SERIAL_CLOSE'));
  }
 }else if(scenario==='tool-corrupt'){
  await page.waitForFunction(()=>document.querySelector('#workflow-error').textContent.includes('RECOVERY_TOOL'));await page.waitForFunction(()=>JSON.parse(document.querySelector('#test-wire').textContent).closes===1);
  assert.equal(await page.locator('#retry-tool-download').isVisible(),false);assert.equal(await page.locator('#execute-plan').isEnabled(),false);
 }else{
  if(['tool-http-once','tool-interrupt-once','tool-stall'].includes(scenario)){
   await page.locator('#retry-tool-download').waitFor({state:'visible',timeout:75000});
   const state=JSON.parse(await page.locator('#tool-download-details pre').textContent());
   assert.equal(state.error.category,{'tool-http-once':'HTTP_STATUS','tool-interrupt-once':'NETWORK','tool-stall':'IDLE_TIMEOUT'}[scenario]);
   assert.equal((await counters()).closes,0);assert.equal(await page.locator('#execute-plan').isEnabled(),false);
   await page.locator('#retry-tool-download').click();
  }
  await page.locator('#tool-ready').waitFor({state:'visible',timeout:80000});assert.equal(await page.locator('#tool-saved').isChecked(),false);assert.equal(await page.locator('#execute-plan').isEnabled(),false);
  if(scenario==='tool-slow')assert(Date.now()-initial>60000);
  assert.equal((await counters()).opens,1);assert.equal((await counters()).requests,1);
  await page.locator('#stop-close').click();await page.locator('#install-dialog').waitFor({state:'hidden'});
 }
 if(mode==='fixed'&&!receipt){const e=page.waitForEvent('download');await page.locator(await page.locator('#install-dialog').isVisible()?'#save-receipt':'#closed-receipt').click();receipt=JSON.parse(await readFile(await (await e).path(),'utf8'));}
 const after=await counters();assert.equal(after.flashBegin,0);assert.equal(after.flashData,0);assert.equal(after.resets,0);assert.equal(after.opens,1);assert.equal(after.requests,1);
 assert.deepEqual(await Promise.all(scripts),[identity.appBundleSha256]);assert.deepEqual(pageErrors,[]);
 const events=await page.evaluate(()=>__r13OfflineEvents);
 results.push({scenario,lang,elapsedMs:Date.now()-initial,closeMs,baselineHeldOpen,counters:after,events,receipt,pageErrors,loadedAppSha256:identity.appBundleSha256});
 await ctx.close();console.log(scenario,'PASS');
}
try{
 if(mode==='baseline')await run('tool-cancel');
 else for(const scenario of ['tool-cancel','tool-close-x','tool-close-escape','tool-http-once','tool-interrupt-once','tool-stall','tool-corrupt','tool-slow'])await run(scenario,scenario==='tool-http-once'?'en':'zh');
}catch(e){error=String(e.stack||e);}
finally{await browser.close();await net.close();await writeFile(out,JSON.stringify({result:error?'FAIL':mode==='baseline'?'BASELINE_BUG_REPRODUCED':'PASS',artifactIdentity:identity,browser:edge?'macOS native Microsoft Edge':'macOS native Google Chrome',browserVersion:browser.version(),syntheticSerial:true,realDeviceAccess:false,windowsDevice:'PENDING',results,http:net.records,error},null,2)+'\n');}
if(error)throw Error(error);
