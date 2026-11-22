import test from 'node:test';import assert from 'node:assert/strict';import {readFile,mkdir,writeFile,mkdtemp} from 'node:fs/promises';
import {ProductInstallSession} from '../src/product-install-session.js';
import {decodeProtection,APP_END,reconstructOriginal,savedProtection} from '../src/install-protection.js';
import {saveAndReadBack,chooseProtectionFile} from '../src/save-protection.js';
import {RecoverySession} from '../src/recovery-session.js';
import {validateRecoveryPackage,recoverySnapshot,destroyRecoveryPackage} from '../src/recovery-package.js';
import {release,source,draftorFlash,factoryFlash,realRomFixture,inputs} from './r9-fixture.mjs';
import {SimulatedFlash} from '../src/simulator.js';import {md5} from '../src/safety.js';
const factory=new Uint8Array(await readFile('release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin'));
const writes=f=>(f.port.counts.get(2)||0)+(f.port.counts.get(3)||0);
const make=(flash=draftorFlash(),opts={})=>{const f=realRomFixture(flash,{fullRecovery:false,...opts},{candidate:true});return {...f,s:new ProductInstallSession(f.rom,release,{platformQualified:true,diagnostics:f.diagnostics})};};
const prep=f=>f.s.prepare({boardConfirmed:true,tfRemoved:true,factory});
const confirm=f=>({tfRemoved:true,planConfirmed:true,eraseConfirmed:f.s.plan.mode==='fresh'});
async function saved(f){await prep(f);const b=f.s.protectionFile();await f.s.verifySavedProtection(b);return b;}
await mkdir('.test-cache',{recursive:true});const disk=await mkdtemp('.test-cache/r12-points-');
for(const fresh of [true,false])test(`R12 ${fresh?'factory':'app0'} small actual disk file, no full capture, exact single install`,async()=>{
 const original=fresh?await factoryFlash():draftorFlash();original.fill(0x37,0x9000,fresh?0x10000:0xe000);
 if(!fresh){original.fill(0x69,0xa10000,0xa11000);original.fill(0x43,0x10000+inputs['app.bin'].length,APP_END);}
 const before=original.slice(),f=make(original,{firstInstall:fresh});await prep(f);assert.equal(writes(f),0);
 const b=f.s.protectionFile(),p=await decodeProtection(b);assert.equal(p.saved.bytes,fresh?28672:1120);assert(b.length<60000);
 assert(f.port.counts.get(14)<1500);assert.equal(f.s.plan.fullBackupRequired,false);assert.equal(f.s.plan.backupSavedFileReimportRequired,false);
 const name=`${disk}/${fresh?'factory':'update'}.json`;await writeFile(name,b);b.fill(0);await f.s.verifySavedProtection(new Uint8Array(await readFile(name)));
 await f.s.execute(confirm(f));assert.equal(f.s.state,'VERIFIED_BOOT_PENDING');assert.equal(f.port.counts.get(2),fresh?5:1);assert.equal(f.port.counts.get(3),fresh?3958:3927);
 if(!fresh){assert.deepEqual(f.flash.slice(0,65536),before.slice(0,65536));assert.deepEqual(f.flash.slice(APP_END),before.slice(APP_END));}
 assert.deepEqual(f.flash.slice(65536,65536+inputs['app.bin'].length),inputs['app.bin']);assert.equal(f.port.signalCount,0);
 await assert.rejects(f.s.execute(confirm(f)));assert.equal(f.port.counts.get(2),fresh?5:1);await f.s.close();assert.equal(f.port.closeCount,1);
 const receipt=JSON.stringify(f.s.receipt());assert(!receipt.includes('02:00:00:00:00:01'));assert(!receipt.includes(p.originalMd5));
 console.log(JSON.stringify({synthetic:true,path:fresh?'factory':'app0',pointBytes:(await readFile(name)).length,slowReadCommands:f.port.counts.get(14),flashBegin:f.port.counts.get(2),flashData:f.port.counts.get(3),resets:f.port.signalCount,closed:f.port.closeCount}));
});
test('no save proof, fake handle, wrong point, corruption, truncation fail before writes',async()=>{
 assert.throws(()=>savedProtection({},{mode:'fresh',mac:'02:00:00:00:00:01'}));
 for(const type of ['none','wrong','corrupt','truncated']){const f=make();await prep(f);const b=f.s.protectionFile();
  if(type==='none')await assert.rejects(f.s.execute(confirm(f)),{code:'PROTECTION_SAVE'});
  else{let changed=b;if(type==='wrong'){const other=make();await prep(other);changed=other.s.protectionFile();await other.s.close();}else if(type==='corrupt')changed[300]^=1;else changed=changed.slice(0,-2);await assert.rejects(f.s.verifySavedProtection(changed));}
  assert.equal(writes(f),0);await f.s.close();
 }
});
test('known hardware/current original gates unchanged, including bootloader/app1/factory tail/blank/security',async()=>{
 const badBoot=draftorFlash();badBoot[24]^=1;const badApp=draftorFlash();badApp[65580]^=1;const badFactory=await factoryFlash();badFactory[16777215]=0;
 const cases=[[badBoot,{}],[badApp,{}],[badFactory,{}],[new SimulatedFlash(release,'app1').flash,{}],[new Uint8Array(16777216).fill(255),{}],[draftorFlash(),{securityFlags:1}],[draftorFlash(),{jedec:0x1740ef}],[draftorFlash(),{chipId:2}]];
 for(const [b,opts]of cases){const f=make(b,opts);await assert.rejects(prep(f));assert.equal(writes(f),0);await f.s.close();}
});
test('state changes after protection save never grant a writer',async()=>{
 for(const kind of ['identity','private','app','ota']){const f=make();await saved(f);if(kind==='identity')f.port.options.macLo=2;else f.flash[{private:0x9000,app:65570,ota:0xe001}[kind]]^=1;
  await assert.rejects(f.s.execute(confirm(f)));assert.equal(writes(f),0);await f.s.close();}
});
test('separate erase approval, TF and plan consent remain required',async()=>{
 for(const field of ['tfRemoved','planConfirmed','eraseConfirmed']){const f=make(await factoryFlash(),{firstInstall:true});await saved(f);await assert.rejects(f.s.execute({...confirm(f),[field]:false}));assert.equal(writes(f),0);await f.s.close();}
});
test('concurrent execute has one writer; cancellation consumes session and releases once',async()=>{
 const f=make();await saved(f);const first=f.s.execute(confirm(f));await assert.rejects(f.s.execute(confirm(f)),{code:'BUSY'});await first;assert.equal(f.port.counts.get(2),1);await f.s.close();await f.s.close();assert.equal(f.port.closeCount,1);
 const g=make();await saved(g);g.s.cancel();await assert.rejects(g.s.execute(confirm(g)));assert.equal(writes(g),0);await g.s.close();
});
for(const fresh of [true,false])test(`partial ${fresh?'factory':'app0'} refuses reinstall; point reconstructs exact original for existing full restore`,async()=>{
 const initial=fresh?await factoryFlash():draftorFlash();initial.fill(0x67,0x9000,fresh?0x10000:0xe000);if(!fresh)initial.fill(0x35,0x10000+inputs['app.bin'].length,APP_END);
 const before=initial.slice(),f=make(initial,{firstInstall:fresh,throwOn:{op:3,nth:5}}),point=await saved(f);
 await assert.rejects(f.s.execute(confirm(f)));assert.equal(f.s.receipt().flashOutcome,'POSSIBLY_PARTIAL');await f.s.close();const count=writes(f);await assert.rejects(f.s.execute(confirm(f)));assert.equal(writes(f),count);
 const rejected=make(f.flash.slice());await assert.rejects(prep(rejected));assert.equal(writes(rejected),0);await rejected.s.close();
 const reader=realRomFixture(f.flash.slice());await reader.rom.connect();
 const zip=await reconstructOriginal(reader.rom,point,release,{factory,boardConfirmed:true,tfRemoved:true,build:'R12-SOFTWARE',sourceCommit:source});assert.equal(writes(reader),0);await reader.rom.disconnect();
 const h=await validateRecoveryPackage(zip),snap=recoverySnapshot(h);assert.deepEqual(snap.image,before);assert.equal(snap.manifest.creationProof.method,'ROM_ORIGINAL_RECONSTRUCTED_V1');snap.image.fill(0);destroyRecoveryPackage(h);
 const restore=realRomFixture(f.flash.slice()),s=new RecoverySession(restore.rom,{mode:'restore',build:'R12-SOFTWARE',sourceCommit:source});await s.selectPackage(zip);await s.prepare({boardConfirmed:true,tfRemoved:true});await s.restore({restoreConfirmed:true});assert.deepEqual(restore.flash,before);assert.equal(restore.port.counts.get(2),1);assert.equal(restore.port.counts.get(3),16384);assert.equal(restore.port.signalCount,0);await s.close();
});
test('reconstruction never overwrites changed private data or different board',async()=>{
 const f=make();const point=await saved(f);await f.s.close();
 for(const kind of ['identity','private','unused-tail']){const g=realRomFixture(f.flash.slice());if(kind==='identity')g.port.options.macLo=2;else g.flash[kind==='private'?0xa10000:APP_END+33]^=1;await g.rom.connect();await assert.rejects(reconstructOriginal(g.rom,point,release,{factory,boardConfirmed:true,tfRemoved:true,build:'test',sourceCommit:source}),{code:kind==='identity'?'IDENTITY':'PRIVATE_CHANGED'});assert.equal(writes(g),0);await g.rom.disconnect();}
});
test('factory official reconstruction bytes must be pinned before any erase',async()=>{const f=make(await factoryFlash(),{firstInstall:true}),wrong=factory.slice();wrong[0]^=1;await assert.rejects(f.s.prepare({boardConfirmed:true,tfRemoved:true,factory:wrong}),{code:'FACTORY_ASSET'});assert.equal(writes(f),0);await f.s.close();});
test('disk commit/readback and all save failures; no download-success shortcut',async()=>{
 const data=new Uint8Array([1,2,3]);let order=[];
 const handle={createWritable:async()=>({write:async b=>{order.push('write');assert.deepEqual(b,data);},close:async()=>order.push('close'),abort:async()=>order.push('abort')}),getFile:async()=>{order.push('read');return new Blob([data]);}};
 assert.deepEqual(await saveAndReadBack(handle,data),data);assert.deepEqual(order,['write','close','read']);
 for(const phase of ['create','write','close','read','mismatch']){const h={createWritable:async()=>{if(phase==='create')throw Error('denied');return{write:async()=>{if(phase==='write')throw Error('disk-full');},close:async()=>{if(phase==='close')throw Error('disk-full');},abort:async()=>{}};},getFile:async()=>{if(phase==='read')throw Error('permission');return new Blob([new Uint8Array([1,2,4])]);}};await assert.rejects(saveAndReadBack(h,data));}
 let called=false;const pending=chooseProtectionFile(()=>{called=true;return Promise.reject(new DOMException('cancelled','AbortError'));});assert(called);await assert.rejects(pending,{name:'AbortError'});
});
test('wire corruption remains a stopped partial outcome with no automatic retry',async()=>{const f=make(undefined,{corruptWrite:true});await saved(f);await assert.rejects(f.s.execute(confirm(f)));assert.equal(f.s.receipt().flashOutcome,'POSSIBLY_PARTIAL');assert.equal(f.port.counts.get(2),1);await f.s.close();});
test('cancel during protection read and changed security before grant stop without writes',async()=>{
 const f=make(),read=f.rom.readFlash.bind(f.rom);f.rom.readFlash=async(offset,size)=>{const b=await read(offset,size);if(size===1120)f.s.cancel();return b;};await assert.rejects(prep(f));assert.equal(writes(f),0);await f.s.close();
 const g=make();await saved(g);g.port.options.securityFlags=2;await assert.rejects(g.s.execute(confirm(g)),{code:'SECURITY_CHANGED'});assert.equal(writes(g),0);await g.s.close();
});
test('lost erase acknowledgement is possibly partial and never retried',async()=>{
 const f=make(await factoryFlash(),{firstInstall:true});await saved(f);f.port.options.malformedOn=2;
 await assert.rejects(f.s.execute(confirm(f)));assert.equal(f.s.receipt().flashOutcome,'POSSIBLY_PARTIAL');assert.equal(f.port.counts.get(2),1);assert.equal(f.port.counts.get(3)||0,0);assert(f.flash.every(b=>b===255));await f.s.close();
});
