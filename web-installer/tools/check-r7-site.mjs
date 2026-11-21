// Automated tests for this repository's own site. Never requests an OS serial port.
import { chromium } from '@playwright/test';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { spawn } from 'node:child_process';
import assert from 'node:assert/strict';
const base=process.argv[2]||'http://localhost:4187/';
const remote=base.startsWith('https:');
const evidence=`evidence/R7_20261121/${remote?'remote':'local'}`;
await mkdir(evidence,{recursive:true});
let server,browser;
try{
 if(!remote){server=spawn(process.execPath,['tools/serve.mjs','dist-r7-locked'],{env:{...process.env,DRAFTOR_PORT:'4187'},stdio:['ignore','pipe','pipe']});await new Promise((yes,no)=>{server.stdout.once('data',yes);server.once('error',no);server.once('exit',code=>no(Error(`Server ${code}`)));});}
 const sumResponse=await fetch(new URL('SITE_SHA256SUMS',base));assert.equal(sumResponse.status,200);const sums=await sumResponse.text();
 assert.equal(sums,await readFile('dist-r7-locked/SITE_SHA256SUMS','utf8'));
 const assets=[];
 for(const line of sums.trim().split('\n')){const[h,name]=line.split('  ');const r=await fetch(new URL(name,base),{redirect:'error',cache:'no-store'});assert.equal(r.status,200,name);const b=new Uint8Array(await r.arrayBuffer());assert.equal(createHash('sha256').update(b).digest('hex'),h,name);assets.push({name,status:r.status,bytes:b.length,sha256:h});}
 browser=await chromium.launch({channel:'chrome',headless:true});
 const results=[];
 for(const [label,width,height,noSerial]of[['desktop',1280,900,false],['mobile',390,844,true]]){
  const context=await browser.newContext({viewport:{width,height}});const page=await context.newPage();const errors=[],requests=[];
  page.on('pageerror',e=>errors.push(e.message));page.on('request',r=>requests.push(r.url()));
  if(noSerial)await context.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:undefined}));
  const response=await page.goto(base+'?hardwareUpdateWrites=true&device-test-update=1',{waitUntil:'networkidle'});assert.equal(response.status(),200);
  await page.locator('#install-dialog').waitFor({state:'attached'});
  assert((await page.locator('[data-i18n="previewHint"]').innerText()).includes('暂未公开开放'));
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  await page.locator('#tf-downloads summary').first().click();
  const baseUrl=await page.locator('#tf-base-download').getAttribute('href'),indexUrl=await page.locator('#tf-index-download').getAttribute('href');
  assert(baseUrl.includes('/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_Resources.zip'));assert(indexUrl.includes('/v0.1.0-alpha.3/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip'));
  await page.screenshot({path:`${evidence}/${label}-zh.png`,fullPage:true});
  await page.locator('#lang-toggle').click();assert.equal(await page.locator('html').getAttribute('lang'),'en');assert((await page.locator('[data-i18n="previewHint"]').innerText()).includes('not yet public'));
  await page.screenshot({path:`${evidence}/${label}-en.png`,fullPage:true});
  await page.locator('#install-btn').click();
  if(noSerial){await page.locator('#workflow-status').filter({hasText:'unavailable'}).waitFor();assert(await page.locator('#connect-section').isHidden());}
  else{await page.locator('#connect-read:enabled').waitFor();assert(await page.locator('#expected-mac').isHidden());assert(await page.locator('#execute-plan').isDisabled());}
  await page.locator('#install-close').click();
  assert.deepEqual(errors,[]);assert(requests.every(u=>u.startsWith(base)||u.startsWith('data:')),'runtime makes same-origin requests only');
  results.push({label,status:'PASS',languageSwitch:true,overflow:false,noSerialNotice:noSerial,physicalSerialRequested:false,requests:requests.length,tfLinks:[baseUrl,indexUrl]});await context.close();
 }
 const result={status:'PASS',base,remote,assets,ui:results,realDeviceAccess:false};await writeFile(`${evidence}/result.json`,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({status:result.status,base,assetCount:assets.length,ui:results.length,realDeviceAccess:false}));
}finally{await browser?.close();server?.kill('SIGTERM');}
