// SPDX-License-Identifier: Apache-2.0
// Tests real browser networking and production UI; never selects a real port.
import {chromium} from '@playwright/test';
import {build} from 'esbuild';
import {mkdir,readFile,writeFile,realpath} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
import {networkServer} from '../tests/r13-network-server.mjs';
const finalRoot=process.argv[2],root=finalRoot||'.test-cache/r13-download';
const digest=b=>createHash('sha256').update(b).digest('hex');
let artifactIdentity={classification:'DEVELOPMENT_BUILD',root};
if(finalRoot){
 const b=JSON.parse(await readFile(root+'/BUILD.json','utf8'));
 const appSha=digest(await readFile(root+'/assets/app.js'));assert.equal(appSha,b.appBundleSha256);
 const reinstallSha=digest(await readFile(root+'/'+b.reinstall.script));assert.equal(reinstallSha,b.reinstall.sha256);
 artifactIdentity={classification:'FORMAL_RELEASE_ARTIFACT',root:await realpath(root),version:b.version,sourceCommit:b.sourceCommit,manifestSha256:digest(await readFile(root+'/SITE_SHA256SUMS')),appBundleSha256:appSha,reinstallSha256:reinstallSha,offlineSha256:digest(await readFile(root+'/'+b.recovery.offlineHtml))};
 assert.equal(artifactIdentity.offlineSha256,b.recovery.offlineSha256);
}
const evidence=process.argv[3]||'delivery/R13_Download_Fix_20261126/browser-development.json';
if(!finalRoot){
 await mkdir(root+'/assets',{recursive:true});
 for(const name of ['index.html','first-install.html']){
  const html=(await readFile('dist-r13-public/'+name,'utf8')).replace(/assets\/reinstall-[a-f0-9]+\.js/g,'assets/reinstall.js');await writeFile(root+'/'+name,html);
 }
 const config={bundle:true,target:['chrome120'],legalComments:'inline',format:'esm'};
 await build({...config,entryPoints:['src/r11-main.js'],outfile:root+'/assets/app.js',define:{__R11_BUILD__:'"R13.1-DOWNLOAD-TEST"',__R11_SOURCE__:'"a34806354cf5864a37e923e4a2cca386bda87124"',__R11_OFFLINE_HASH__:JSON.stringify(JSON.parse(await readFile('dist-r13-download-public/BUILD.json','utf8')).recovery.offlineSha256)}});
 await build({...config,entryPoints:['src/r12-local-app0.js'],outfile:root+'/assets/reinstall.js',define:{__R12_PUBLIC__:'true',__R12_BUILD__:'"R13.1-DOWNLOAD-TEST"',__R12_SOURCE__:'"a34806354cf5864a37e923e4a2cca386bda87124"'}});
 for(const name of ['style.css','recovery.css','install-navigation.css'])await writeFile(root+'/'+name,await readFile('dist-r13-public/'+name));
}
const edgeExecutable=process.argv[4];
const net=await networkServer(root,{artifactOnly:!!finalRoot}),browser=await chromium.launch(edgeExecutable?{executablePath:edgeExecutable,headless:false}:{channel:'chrome',headless:true});
const browserKind=edgeExecutable?'macOS Microsoft Edge':'macOS Google Chrome';
const results=[];
async function run(scenario,pageName='first-install.html',language='zh',edge=false){
 const ctx=await browser.newContext(edge?{reducedMotion:'reduce',viewport:{width:1280,height:900},userAgent:'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/142.0.0.0 Safari/537.36 Edg/142.0.0.0'}:{reducedMotion:'reduce',viewport:{width:1280,height:900}});
 await ctx.addInitScript(()=>{globalThis.__portRequests=0;Object.defineProperty(navigator,'serial',{value:{requestPort(){globalThis.__portRequests++;throw Error('Test forbids physical USB');}}});});
 const page=await ctx.newPage(),errors=[],scriptReads=[];page.on('response',r=>{if(new URL(r.url()).pathname.includes('/assets/')&&new URL(r.url()).pathname.endsWith('.js'))scriptReads.push(r.body().then(b=>({file:new URL(r.url()).pathname.split('/assets/')[1],sha256:digest(b)})));});page.on('pageerror',e=>errors.push(e.message));
 const route=(edge?'edge-':'')+scenario;
 await page.goto(`${net.base}/${route}/${pageName}`);await page.bringToFront();
 if(language==='en')await page.locator('#lang-toggle').click();
 let progressSample;
 const started=Date.now();console.log('START',scenario,pageName);await page.locator('#install-btn').click();
 const failed=()=>page.locator('#retry-download').waitFor({state:'visible',timeout:80000});
 const state=async()=>JSON.parse(await page.locator('#download-details pre').textContent());
 // Wait directly for the observable preparation state, never click a port.
 const waitReady=async()=>{await page.locator('#connect-section').waitFor({state:'visible',timeout:85000});};
 if(scenario==='cancel'){
  await page.waitForFunction(()=>JSON.parse(document.querySelector('#download-details pre').textContent).file==='app.bin');
  await page.locator('#stop-close').click();await page.locator('#install-dialog').waitFor({state:'hidden'});
  assert.equal((await state()).stage,'CANCELLED');
 }else if(['http-once','interrupt-once','truncate-once','corrupt-once','stall'].includes(scenario)){
  await failed();const before=await state();
  const expected={ 'http-once':'HTTP_STATUS','interrupt-once':'NETWORK','truncate-once':'INCOMPLETE','corrupt-once':'SHA256',stall:'IDLE_TIMEOUT'}[scenario];assert.equal(before.error.category,expected);
  assert(!before.verifiedFiles.includes('app.bin'));assert.equal(await page.evaluate(()=>__portRequests),0);
  const downloadEvent=page.waitForEvent('download');await page.locator('#save-receipt').click();
  const downloaded=await downloadEvent,receipt=JSON.parse(await readFile(await downloaded.path(),'utf8'));
  assert.equal(receipt.downloadPreparation.error.category,expected);assert.equal(receipt.flashOutcome,'NO_FLASH_WRITE_ATTEMPT');
  const headline=await page.locator('#workflow-status').textContent();assert(headline.includes(language==='en'?'Download failed':'下载失败'));
  if(scenario!=='stall'){
   await page.locator('#retry-download').click();await waitReady();const after=await state();assert.equal(after.attempt,2);assert.equal(after.stage,'READY');assert(after.history.filter(x=>x.result==='VERIFIED_CACHE').length>=4);
  }
 }else{
  if(scenario==='slow'){
   await page.waitForFunction(()=>{const s=JSON.parse(document.querySelector('#download-details pre').textContent);return s.file==='app.bin'&&s.stage==='RECEIVING'&&s.receivedBytes>1000000;},null,{timeout:45000});
   progressSample=await state();assert(progressSample.receivedBytes<progressSample.expectedBytes);
   progressSample.visibleText=await page.locator('#download-progress-text').textContent();assert(progressSample.visibleText.includes('MB'));
   await page.screenshot({path:evidence.replace('.json','-progress.png')});
  }
  await waitReady();
 }
 const final=await state(),elapsedMs=Date.now()-started;console.log('FINISH',scenario,final.stage,elapsedMs);
 assert.equal(await page.evaluate(()=>__portRequests),0);assert.deepEqual(errors,[]);
 if(scenario==='slow'){assert(elapsedMs>60000);assert.equal(final.stage,'READY');}
 if(final.stage==='READY'){
  if(pageName==='first-install.html'){await page.locator('#physical-board').check();await page.locator('#prepared-tf').check();}
  assert.equal(await page.locator('#connect-read').isEnabled(),true);
 }
 // Receipt must also be available when no device session has ever existed.
 assert.equal(await page.locator('#save-receipt').isEnabled(),true);
 const loadedScripts=await Promise.all(scriptReads);
 if(finalRoot){for(const r of loadedScripts)assert.equal(r.sha256,digest(await readFile(root+'/assets/'+r.file)));}
 results.push({artifactIdentity,loadedScripts,scenario,page:pageName,language,environment:edge?browserKind+' with Windows Edge UA (compatibility simulation only)':browserKind,browserVersion:browser.version(),elapsedMs,progressSample,final,portRequests:0,deviceWrites:0,pageErrors:errors});
 if(await page.locator('#install-dialog').isVisible())await page.locator('#stop-close').click();await ctx.close();
}
async function pageExit(){
 const ctx=await browser.newContext();await ctx.addInitScript(()=>{globalThis.__portRequests=0;Object.defineProperty(navigator,'serial',{value:{requestPort(){globalThis.__portRequests++;throw Error('No physical USB');}}});});
 const p=await ctx.newPage();p.on('dialog',d=>d.accept());await p.goto(net.base+'/pagehide/first-install.html');await p.locator('#install-btn').click();
 await p.waitForFunction(()=>JSON.parse(document.querySelector('#download-details pre').textContent).file==='app.bin');
 assert.equal(await p.evaluate(()=>__portRequests),0);await p.goto('about:blank');
 await new Promise(r=>setTimeout(r,200));assert(net.records.some(r=>r.scenario==='pagehide'&&r.file==='firmware/app.bin'&&r.aborted));
 results.push({scenario:'pagehide',environment:browserKind,networkRequestAborted:true,portRequests:0,deviceWrites:0});await ctx.close();
}
let error;
try{
 // Two actual 60+ second network waits run independently; no fake clock.
 if(!process.env.R13_FAST_BROWSER){if(edgeExecutable){await run('slow');await run('stall');}else await Promise.all([run('slow'),run('stall')]);}
 for(const scenario of ['http-once','interrupt-once','truncate-once','corrupt-once','cancel'])await run(scenario);
 await pageExit();
 await run('normal','index.html','en');await run('normal','first-install.html','en',true);
 const interrupted=net.records.filter(r=>r.file==='firmware/app.bin'&&r.scenario==='cancel');assert(interrupted.some(r=>r.aborted));
 for(const scenario of ['http-once','interrupt-once','truncate-once','corrupt-once']){
  for(const row of net.records.filter(r=>r.scenario===scenario&&r.file.endsWith('.json')&&r.file.startsWith('firmware/')))assert.equal(row.request,1);
 }
}catch(e){error=String(e.stack||e);}
finally{await browser.close();await net.close();await writeFile(evidence,JSON.stringify({result:error?'FAIL':'PASS',sourceBuildRoot:root,artifactIdentity,realTimeSlowTest:!process.env.R13_FAST_BROWSER,windowsDevice:'NOT_RUN',edgeNative:edgeExecutable?'MACOS_TESTED_WINDOWS_NOT_RUN':'NOT_RUN_IN_THIS_INVOCATION',results,http:net.records,error},null,2)+'\n');}
if(error)throw Error(error);console.log(JSON.stringify({result:'PASS',cases:results.length,evidence}));
