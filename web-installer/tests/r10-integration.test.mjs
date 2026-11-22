import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,writeFile,readFile} from 'node:fs/promises';
import {PublicInstallSession} from '../src/public-install-session.js';
import {RecoverySession} from '../src/recovery-session.js';
import {qualifiedDesktopChrome} from '../src/release-platform.js';
import {validateRecoveryPackage,recoverySummary,destroyRecoveryPackage} from '../src/recovery-package.js';
import {release,inputs,source,draftorFlash,factoryFlash,packageFor,realRomFixture} from './r9-fixture.mjs';
import {sha256,md5} from '../src/safety.js';
import {SimulatedFlash} from '../src/simulator.js';
const tool=new TextEncoder().encode('synthetic offline tool test pin'),toolHash=await sha256(tool);
const confirm={backupConfirmed:true,tfRemoved:true,planConfirmed:true,offlineToolSaved:true};
const options={platformQualified:true,build:'R10-SOFTWARE-TEST',sourceCommit:source,offlineToolSha256:toolHash};
const make=(flash=draftorFlash(),portOpts={})=>{const f=realRomFixture(flash,{fullRecovery:false,...portOpts},{candidate:true});return {...f,session:new PublicInstallSession(f.rom,release,{...options,diagnostics:f.diagnostics})};};
const prepare=f=>f.session.prepare({boardConfirmed:true,tfRemoved:true});
const writes=f=>(f.port.counts.get(2)||0)+(f.port.counts.get(3)||0);
const old=async f=>{await prepare(f);await f.session.verifySavedBackup(await packageFor(f.flash));await f.session.verifyOfflineTool(tool);};
await mkdir('.test-cache',{recursive:true});const disk=await mkdtemp('.test-cache/r10-integration-');
for(const factory of [true,false])test(`no old files: ${factory?'exact factory fresh':'alpha3 app0 update'} actual ROM full capture, disk ZIP reimport, exclusive install, independent offline restore`,async()=>{
 const original=factory?await factoryFlash():draftorFlash(),before=original.slice(),f=make(original,{firstInstall:factory,yieldEvery:4096});
 const started=performance.now();let progress=0,fullProgress=false;
 // This wraps only the integration observer, not the protocol parser.
 const p=await prepare(f);assert.equal(p.mode,factory?'fresh':'update');assert.equal(writes(f),0);
 const zip=await f.session.captureBackup();assert(f.port.counts.get(14)>=262144);
 const filename=`${disk}/${factory?'factory':'draftor'}-SYNTHETIC.zip`;await writeFile(filename,zip);zip.fill(0);
 const saved=new Uint8Array(await readFile(filename));const h=await validateRecoveryPackage(saved);assert.equal(recoverySummary(h).firmware,factory?'waveshare-factory-20260120':'draftor-alpha3-app0');destroyRecoveryPackage(h);
 assert.equal(f.session.backupVerified,false);await assert.rejects(f.session.execute(confirm),{code:'BACKUP_FILE'});
 await f.session.verifySavedBackup(saved);await assert.rejects(f.session.execute(confirm),{code:'RECOVERY_TOOL'});
 await f.session.verifyOfflineTool(tool);saved.fill(0);
 await f.session.execute({...confirm,eraseConfirmed:factory});assert.equal(f.session.state,'VERIFIED_BOOT_PENDING');
 assert.equal(f.port.counts.get(2),factory?5:1);assert.equal(f.port.counts.get(3),factory?3958:3927);assert.equal(f.port.signalCount,0);
 for(const [name,offset] of [['app.bin',65536],...(factory?[['bootloader.bin',0],['partitions.bin',32768],['boot_app0.bin',57344]]:[])])assert.deepEqual(f.flash.slice(offset,offset+inputs[name].length),inputs[name]);
 if(!factory){assert.deepEqual(f.flash.slice(0,65536),before.slice(0,65536));assert.deepEqual(f.flash.slice(0x410000),before.slice(0x410000));}
 await assert.rejects(f.session.execute(confirm));await f.session.close();assert.equal(f.port.closeCount,1);
 // New driver/session, only the ZIP on disk; current Flash may be damaged.
 const other=realRomFixture(new Uint8Array(16777216).fill(0x5a));const recovery=new RecoverySession(other.rom,{mode:'restore',build:'R10-SOFTWARE-TEST',sourceCommit:source,diagnostics:other.diagnostics});
 await recovery.selectPackage(new Uint8Array(await readFile(filename)));await recovery.prepare({boardConfirmed:true,tfRemoved:true});assert.equal(writes(other),0);
 await recovery.restore({restoreConfirmed:true});assert.deepEqual(other.flash,before);assert.equal(other.port.counts.get(2),1);assert.equal(other.port.counts.get(3),16384);assert.equal(other.port.signalCount,0);await recovery.close();assert.equal(other.port.closeCount,1);
 console.log(JSON.stringify({scenario:factory?'factory':'app0',synthetic:true,captureReadCommands:f.port.counts.get(14),installBegin:f.port.counts.get(2),installData:f.port.counts.get(3),restoreData:other.port.counts.get(3),wallSeconds:Math.round((performance.now()-started)/1000),diskRoundtrip:true}));
});
test('platform policy is finite; reading/downloads independent of this device gate',()=>{
 const env={userAgent:'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 Chrome/157.0.0.0 Safari/537.36',vendor:'Google Inc.',platform:'MacIntel',secureContext:true,serialAvailable:true,brands:[{brand:'Google Chrome'}]};
 assert(qualifiedDesktopChrome(env));for(const patch of [{platform:'Windows',userAgent:'Windows Chrome/157.0.0.0'},{userAgent:env.userAgent+' Edg/157'},{brave:true},{secureContext:false},{serialAvailable:false},{brands:[{brand:'Chromium'}]},{userAgent:env.userAgent+' Mobile'},{vendor:'Apple Computer, Inc.'}])assert.equal(qualifiedDesktopChrome({...env,...patch}),false);
 assert.throws(()=>new PublicInstallSession({},release,{...options,platformQualified:false}),{code:'PLATFORM'});
});
test('unsupported states and physical preconditions never grant writes',async()=>{
 const cases=[['blank',new Uint8Array(16777216).fill(255),{},'UNQUALIFIED_BLANK'],['app1',new SimulatedFlash(release,'app1').flash,{},'UNQUALIFIED_SLOT'],['unknown',new Uint8Array(16777216).fill(4),{},'PARTITIONS'],['security',draftorFlash(),{securityFlags:1},'SECURITY'],['capacity',draftorFlash(),{jedec:0x1740ef},'FLASH'],['chip',draftorFlash(),{chipId:2},'CHIP']];
 const unknownFactory=await factoryFlash();unknownFactory[16777215]=0;cases.push(['factory-tail',unknownFactory,{},'FACTORY_FINGERPRINT']);
 const unknownApp=draftorFlash();unknownApp[65580]^=1;cases.push(['unknown-app',unknownApp,{},'UNQUALIFIED_FIRMWARE']);
 for(const [name,flash,opts,code] of cases){const f=make(flash,opts);await assert.rejects(prepare(f),{code},name);assert.equal(writes(f),0);await f.session.close();}
 for(const c of [{boardConfirmed:false,tfRemoved:true},{boardConfirmed:true,tfRemoved:false}]){const f=make();await assert.rejects(f.session.prepare(c));assert.equal(writes(f),0);await f.session.close();}
});
test('wrong/corrupt ZIP cannot replace actual bytes and invalidates earlier proof',async()=>{
 for(const kind of ['truncated','modified','plain-success']){const f=make();await old(f);let b=await packageFor(f.flash);if(kind==='truncated')b=b.slice(0,-5);if(kind==='modified')b[200]^=1;if(kind==='plain-success')b=new TextEncoder().encode('{"success":true,"savedPackageReimported":true}');await assert.rejects(f.session.verifySavedBackup(b));assert.equal(f.session.backupVerified,false);await assert.rejects(f.session.execute(confirm));assert.equal(writes(f),0);await f.session.close();}
});
test('saved package identity/current content must match, no restore exemption',async()=>{
 for(const kind of ['wrong-board','current-changed','after-verified']){const f=make();await prepare(f);const b=await packageFor(f.flash);if(kind==='after-verified'){await f.session.verifySavedBackup(b);await f.session.verifyOfflineTool(tool);f.flash[0x9000]^=1;await assert.rejects(f.session.execute(confirm),{code:'BACKUP_STALE'});}else{if(kind==='wrong-board')f.port.options.macLo=2;else f.flash[0x9000]^=1;await assert.rejects(f.session.verifySavedBackup(b));}assert.equal(writes(f),0);await f.session.close();}
});
test('change file/tool/device, duplicate calls and consumed permission cannot write again',async()=>{
 const f=make();await old(f);f.session.invalidateSavedPackage();await assert.rejects(f.session.execute(confirm));await f.session.verifySavedBackup(await packageFor(f.flash));f.session.invalidateOfflineTool();await assert.rejects(f.session.execute(confirm));await assert.rejects(f.session.verifyOfflineTool(new Uint8Array([1])),{code:'RECOVERY_TOOL'});assert.equal(writes(f),0);await f.session.close();
 const g=make();await old(g);g.port.options.macLo=2;await assert.rejects(g.session.execute(confirm),{code:'IDENTITY'});assert.equal(writes(g),0);await g.session.close();
 const x=make();await old(x);const first=x.session.execute(confirm);await assert.rejects(x.session.execute(confirm),{code:'BUSY'});await first;await assert.rejects(x.session.execute(confirm));assert.equal(x.port.counts.get(2),1);await x.session.close();
});
test('full capture with delayed scheduling sees change and aborts, no stale package',async()=>{
 const f=make(undefined,{yieldEvery:128});await prepare(f);const pending=f.session.captureBackup();await new Promise(resolve=>setTimeout(resolve,15));f.flash[0x9000]^=1;await assert.rejects(pending,{code:'BACKUP_STALE'});assert.equal(writes(f),0);await f.session.close();
});
test('cancel and write timeout during capture stop; no background retry and port closes',async()=>{
 for(const fault of ['cancel','timeout']){const f=make(undefined,{yieldEvery:128});await prepare(f);if(fault==='timeout')f.port.options.hangWriteOn=14;const pending=f.session.captureBackup();if(fault==='cancel'){await new Promise(r=>setTimeout(r,15));f.session.cancel();}await assert.rejects(pending);const commands=f.port.transcript.length;await f.session.close();await new Promise(r=>setTimeout(r,5));assert.equal(f.port.transcript.length,commands);assert.equal(writes(f),0);assert.equal(f.port.closeCount,1);}
});
test('write failure or corruption stops without retry; diagnostics do not expose package identity',async()=>{
 for(const failure of [{throwOn:{op:3,nth:2}},{corruptWrite:true}]){const f=make(undefined,failure);await old(f);await assert.rejects(f.session.execute(confirm));assert.equal(f.session.state,'STOP');assert.equal(f.port.counts.get(2),1);await f.session.close();assert.equal(f.port.closeCount,1);const r=JSON.stringify(f.session.receipt());assert(!r.includes('02:00:00:00:00:01'));assert(!r.includes(md5(f.flash)));}
});
