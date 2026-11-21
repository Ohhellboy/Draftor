// Offline integration of the normal candidate UI and real ROM implementation.
// The only substitution is an independent synthetic USB device; no real port exists.
import { build } from 'esbuild';
import { chromium } from '@playwright/test';
import { cp,mkdir,readFile,writeFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { resolve } from 'node:path';
import assert from 'node:assert/strict';
import { verifyReleaseInputs } from './verify-release.mjs';
import { SimulatedFlash } from '../src/simulator.js';
const {release,inputs}=await verifyReleaseInputs({verifyDesignBrief:false});
const out='.test-cache/r7-ui',evidence='evidence/R7_20261121/candidate-ui';
await mkdir(`${out}/assets`,{recursive:true});await mkdir(evidence,{recursive:true});
for(const n of ['firmware','style.css'])await cp(`public/${n}`,`${out}/${n}`,{recursive:true});
for(const kind of ['app0','app1','factory','blank']){
 const b=new Uint8Array(16777216).fill(255);
 if(kind==='factory')b.set(await readFile('release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin'));
 else if(kind!=='blank'){b.set(new SimulatedFlash(release,kind).flash);b.set(inputs['app.bin'],kind==='app0'?0x10000:0x410000);}
 await writeFile(`${out}/${kind}-synthetic.bin`,b);
}
await writeFile(`${out}/index.html`,(await readFile('index.html','utf8')).replace('<body>','<body><p>OFFLINE SYNTHETIC USB — NO PHYSICAL DEVICE</p>'));
const source=`import {RomDevicePort} from './tests/rom-device.mjs';
const scenario=new URLSearchParams(location.search).get('scenario')||'app0';
const flash=new Uint8Array(await(await fetch('/'+scenario+'-synthetic.bin')).arrayBuffer());
const port=new RomDevicePort(flash,{firstInstall:scenario==='factory',reservedFF:true,emptySlip:true});
Object.defineProperty(navigator,'serial',{value:{requestPort:async()=>port},writable:false});
await import('./src/candidate-main.js');`;
const browserAssert=`const assert=ok=>{if(!ok)throw Error('Independent synthetic ROM assertion');};assert.equal=(a,b)=>assert(a===b);assert.deepEqual=(a,b)=>assert(JSON.stringify(a)===JSON.stringify(b));`;
await build({stdin:{contents:source,resolveDir:process.cwd(),sourcefile:'OFFLINE_TEST_ENTRY.js'},outfile:`${out}/assets/app.js`,bundle:true,format:'esm',target:'chrome120',plugins:[{name:'synthetic-rom-only',setup(b){b.onLoad({filter:/tests\/rom-device\.mjs$/},async args=>({contents:(await readFile(args.path,'utf8')).replace("import assert from 'node:assert/strict';",browserAssert).replace("import { createHash } from 'node:crypto';","import SparkMD5 from 'spark-md5';const createHash=alg=>{assert(alg==='md5');return{update:b=>({digest:()=>SparkMD5.ArrayBuffer.hash(b.slice().buffer)})}};"),loader:'js'}));}}]});
const server=spawn(process.execPath,['tools/serve.mjs',out],{env:{...process.env,DRAFTOR_PORT:'4188'},stdio:['ignore','pipe','pipe']});let browser;
try{
 await new Promise((yes,no)=>{server.stdout.once('data',yes);server.once('error',no);server.once('exit',c=>no(Error(`Server ${c}`)));});
 browser=await chromium.launch({channel:'chrome',headless:true});const results=[];
 for(const row of [{kind:'app0',capture:true},{kind:'factory'},{kind:'app1',stop:'UNQUALIFIED_SLOT'},{kind:'blank',locked:true},{kind:'app0',asset404:true}]){
  const ctx=await browser.newContext({viewport:{width:1280,height:900},acceptDownloads:true});
  await ctx.route('**/*',route=>{const u=new URL(route.request().url());assert(u.hostname==='localhost'||u.protocol==='blob:');if(row.asset404&&u.pathname==='/firmware/app.bin')return route.fulfill({status:404,body:'synthetic missing release'});return route.continue();});
  const page=await ctx.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto(`http://localhost:4188/?scenario=${row.kind}`);await page.locator('#install-dialog').waitFor({state:'attached'});await page.locator('#install-btn').click();
  if(row.asset404){await page.locator('#workflow-error').filter({hasText:'DOWNLOAD'}).waitFor();assert(await page.locator('#connect-read').isDisabled());}
  else{
   await page.locator('#connect-read:enabled').waitFor();assert(await page.locator('#expected-mac').isHidden());await page.locator('#physical-board').check();await page.locator('#connect-read').click();
   if(row.stop){await page.locator('#workflow-error').filter({hasText:row.stop}).waitFor();assert(await page.locator('#execute-plan').isDisabled());}
   else{
    await page.locator('#workflow-status').filter({hasText:'只读计划已就绪'}).waitFor();
    for(const id of ['backup-confirmed','tf-removed','plan-confirmed'])await page.locator('#'+id).check();
    assert(await page.locator('#execute-plan').isDisabled());
    if(row.locked){await page.locator('#erase-confirmed').check();assert(await page.locator('#execute-plan').isDisabled());}
    else{
     let saved=resolve(`${out}/${row.kind}-synthetic.bin`);
     if(row.capture){
      const downloading=page.waitForEvent('download',{timeout:180000});await page.locator('#capture-backup').click();const download=await downloading;
      saved=await download.path();const b=await readFile(saved);assert.equal(b.length,16777216);assert.deepEqual(b,await readFile(`${out}/app0-synthetic.bin`));
      assert(await page.locator('#execute-plan').isDisabled());
     }
     await page.locator('#saved-backup').setInputFiles(saved);await page.locator('#backup-ready').waitFor();
     if(row.kind==='factory'){assert(await page.locator('#execute-plan').isDisabled());await page.locator('#erase-confirmed').check();}
     assert(await page.locator('#execute-plan').isEnabled());await page.screenshot({path:`${evidence}/${row.kind}-confirmed.png`,fullPage:true});
     await page.locator('#execute-plan').click();await page.locator('#workflow-status').filter({hasText:'VERIFIED_BOOT_PENDING'}).waitFor({timeout:60000});
     const downloading=page.waitForEvent('download');await page.locator('#save-receipt').click();const file=await downloading;const receipt=JSON.parse(await readFile(await file.path(),'utf8'));
     assert.equal(receipt.independentVerification,true);assert.equal(receipt.backup,'SAVED_FILE_REIMPORTED_AND_DEVICE_MD5_MATCHED');assert.equal(receipt.resetIssued,false);assert(!JSON.stringify(receipt).includes('020000000001'));
     await writeFile(`${evidence}/${row.kind}-synthetic-receipt.json`,JSON.stringify({synthetic:true,physicalDeviceAccess:false,receipt},null,2)+'\n');
     await page.screenshot({path:`${evidence}/${row.kind}-verified.png`,fullPage:true});
    }
   }
  }
  assert.deepEqual(errors,[]);results.push({...row,status:'PASS',physicalDeviceAccess:false});console.log('PASS',JSON.stringify(row));await ctx.close();
 }
 await writeFile(`${evidence}/result.json`,JSON.stringify({status:'PASS',normalCandidateUi:true,syntheticUsb:true,physicalDeviceAccess:false,tests:results},null,2)+'\n');
}finally{await browser?.close();server.kill('SIGTERM');}
