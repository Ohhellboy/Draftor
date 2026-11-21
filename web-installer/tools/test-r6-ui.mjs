// Isolated headless browser tests. The entry installs synthetic serial before
// importing the unchanged production main/UI. No OS serial requests are possible.
import { build } from 'esbuild';
import { chromium } from '@playwright/test';
import { cp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import assert from 'node:assert/strict';
import { resolve } from 'node:path';

const out='.test-cache/r6-ui', evidence='evidence/R6_20261121/software';
await mkdir(`${out}/assets`,{recursive:true});await mkdir(evidence,{recursive:true});
for(const path of ['firmware','style.css'])await cp(`public/${path}`,`${out}/${path}`,{recursive:true});
await cp('release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin',`${out}/factory-fixture.bin`);
const html=await readFile('index.html','utf8');
const source=`import {RomDevicePort} from './tests/rom-device.mjs';
import {SimulatedFlash} from './src/simulator.js';
import {loadRelease} from './src/firmware.js';
const scene=new URLSearchParams(location.search).get('scenario')||'factory';
let flash=new Uint8Array(16777216).fill(255);
if(scene==='update')flash=new SimulatedFlash(await loadRelease(new URL('/firmware/',location.href)),'app0').flash;
else if(scene!=='blank'){flash.set(new Uint8Array(await(await fetch('/factory-fixture.bin')).arrayBuffer()));if(scene==='unknown')flash[0x10000]^=1;}
const port=new RomDevicePort(flash,{firstInstall:true,reservedFF:true,emptySlip:true});
Object.defineProperty(navigator,'serial',{value:{requestPort:async()=>port},configurable:false,writable:false});
await import('./src/main.js');`;
const browserAssert=`const assert=ok=>{if(!ok)throw new Error('Synthetic ROM assertion');};assert.equal=(a,b)=>assert(a===b);assert.deepEqual=(a,b)=>assert(JSON.stringify(a)===JSON.stringify(b));`;
for(const scope of ['locked','first']){
  await writeFile(`${out}/${scope}.html`,html.replace('assets/app.js',`assets/${scope}.js`).replace('<body>','<body><p role="note">OFFLINE MOCK — no physical device</p>'));
  await build({stdin:{contents:source,resolveDir:process.cwd(),sourcefile:'r6-ui-fixture-entry.js'},outfile:`${out}/assets/${scope}.js`,bundle:true,format:'esm',target:'chrome120',define:{DEVICE_TEST_UPDATE:'false',DEVICE_TEST_FIRST_INSTALL:scope==='first'?'true':'false'},plugins:[{name:'synthetic-rom-browser-dependencies',setup(b){b.onLoad({filter:/tests\/rom-device\.mjs$/},async(args)=>({contents:(await readFile(args.path,'utf8')).replace("import assert from 'node:assert/strict';",browserAssert).replace("import { createHash } from 'node:crypto';","import SparkMD5 from 'spark-md5'; const createHash=alg=>{assert(alg==='md5');return{update:b=>({digest:()=>SparkMD5.ArrayBuffer.hash(b.slice().buffer)})}};"),loader:'js'}));}}]});
}
const port=4186,server=spawn(process.execPath,['tools/serve.mjs',out],{env:{...process.env,DRAFTOR_PORT:String(port)},stdio:['ignore','pipe','pipe']});
let browser,lastPage;
try{
 await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(new Error(`Server exit ${code}`)));});
 browser=await chromium.launch({channel:'chrome',headless:true});
 const results=[];
 for(const row of [{scope:'locked',scenario:'factory'},{scope:'first',scenario:'unknown'},{scope:'first',scenario:'update'},{scope:'first',scenario:'factory'},{scope:'first',scenario:'blank'}]){
  const ctx=await browser.newContext({viewport:{width:390,height:844},acceptDownloads:true});
  await ctx.route('**/*',route=>{const u=new URL(route.request().url());if(!['localhost','127.0.0.1'].includes(u.hostname)&&u.protocol!=='blob:')throw new Error('No external requests in offline UI tests');return route.continue();});
  const page=await ctx.newPage(),errors=[];lastPage=page;page.on('pageerror',e=>{errors.push(e.message);console.log('pageerror',e.message);});
  console.log('UI scenario',row.scope,row.scenario);
  await page.goto(`http://localhost:${port}/${row.scope}.html?scenario=${row.scenario}`);
  await page.locator('#install-dialog').waitFor({state:'attached'});
  await page.locator('#install-btn').click();await page.locator('#connect-read:enabled').waitFor();
  await page.locator('#physical-board').check();await page.locator('#expected-mac').fill('02:00:00:00:00:01');await page.locator('#connect-read').click();
  if(row.scenario==='unknown'){
    await page.locator('#workflow-error').filter({hasText:'FACTORY_FINGERPRINT'}).waitFor();
    assert(await page.locator('#execute-plan').isDisabled());
  }else{
    await page.locator('#workflow-status').filter({hasText:'只读计划已就绪'}).waitFor();
    for(const id of ['backup-confirmed','tf-removed','plan-confirmed'])await page.locator(`#${id}`).check();
    assert(await page.locator('#execute-plan').isDisabled());
    if(row.scenario!=='update')await page.locator('#erase-confirmed').check();
    if(row.scope==='locked'||row.scenario==='update')assert(await page.locator('#execute-plan').isDisabled());
    else{
      assert(await page.locator('#execute-plan').isEnabled());
      if(row.scenario==='factory')await page.screenshot({path:`${evidence}/offline-factory-confirmation.png`,fullPage:true});
      await page.locator('#execute-plan').click();
      await page.locator('#workflow-status').filter({hasText:'VERIFIED_BOOT_PENDING'}).waitFor({timeout:30000});
      const downloading=page.waitForEvent('download');await page.locator('#save-receipt').click();const downloaded=await downloading;
      const receipt=JSON.parse(await readFile(await downloaded.path(),'utf8'));
      assert.equal(receipt.fullFlashInitialized,true);assert.equal(receipt.resetIssued,false);
      assert(!JSON.stringify(receipt).includes('02:00:00:00:00:01'));
      await writeFile(`${evidence}/offline-ui-${row.scenario}-receipt.json`,JSON.stringify({physicalDeviceAccess:false,syntheticSerial:true,receipt},null,2)+'\n');
      await page.screenshot({path:`${evidence}/offline-ui-${row.scenario}-verified.png`,fullPage:true});
    }
  }
  assert.deepEqual(errors,[]);results.push({...row,status:'PASS',physicalDeviceAccess:false});await ctx.close();
 }
 await writeFile(`${evidence}/browser-ui-tests.json`,JSON.stringify({status:'PASS',syntheticSerial:true,physicalDeviceAccess:false,tests:results},null,2)+'\n');
 console.log(`PASS: ${results.length} normal-UI offline scenarios; no physical serial or external requests.`);
}catch(error){if(lastPage&&!lastPage.isClosed()){console.log((await lastPage.locator('body').innerText()).slice(-5000));await lastPage.screenshot({path:`${evidence}/offline-ui-failure.png`,fullPage:true});}throw error;}
finally{await browser?.close();server.kill('SIGTERM');}
