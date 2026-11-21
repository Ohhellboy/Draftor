import test from 'node:test';import assert from 'node:assert/strict';
import { packageFor,draftorFlash,factoryFlash,identity,source,realRomFixture,release } from './r9-fixture.mjs';
import { validateRecoveryPackage,destroyRecoveryPackage } from '../src/recovery-package.js';
import { RecoverySession } from '../src/recovery-session.js';
import { ReadOnlyDriver } from '../src/read-only-driver.js';
import { S3Rom } from '../src/rom.js';
import { SimulatedFlash } from '../src/simulator.js';
import { Diagnostics,requestSerialPort } from '../src/diagnostics.js';
import { md5 } from '../src/safety.js';
const good=await packageFor(draftorFlash()),consent={boardConfirmed:true,tfRemoved:true,restoreConfirmed:true};
const written=x=>(x.port.counts.get(2)||0)+(x.port.counts.get(3)||0);
async function sessionFor(bytes=good,options={},romOptions) {
 const x=realRomFixture(draftorFlash(),options,romOptions);x.s=new RecoverySession(x.rom,{diagnostics:x.diagnostics,mode:'restore',build:'R9-test',sourceCommit:source});
 await x.s.selectPackage(bytes);return x;
}
test('real ROM writer restores corrupted current App/table/OTA exactly, one begin, 16384 data; no reset or stub',async()=>{
 const x=await sessionFor();x.flash.fill(0,0,0x420000);try{
  await x.s.prepare(consent);assert.equal(written(x),0);
  const running=x.s.restore(consent);await assert.rejects(x.s.restore(consent));await running;
  assert.deepEqual(x.flash,draftorFlash());assert.equal(x.port.counts.get(2),1);assert.equal(x.port.counts.get(3),16384);assert.equal(x.port.signalCount,0);
  const n=x.port.transcript.length;await assert.rejects(x.s.restore(consent));assert.equal(x.port.transcript.length,n);
  assert.equal(x.s.state,'VERIFIED_BOOT_PENDING');
 }finally{await x.s.close();}
 const d=x.diagnostics.snapshot();assert.equal(d.closeResult,'CLOSED');assert.equal(d.pendingCommands,0);assert.equal(d.commands.FLASH_BEGIN.attempted,1);assert.equal(d.commands.FLASH_DATA.acceptedReplies,16384);
 assert.equal(d.commands.SYNC.attempted,1);assert.equal(d.commands.SYNC.acceptedReplies,8);assert.equal(d.events.resetRequests,0);assert.equal(d.events.stubRequests,0);
 assert(!JSON.stringify(d).includes(identity.mac.replaceAll(':','')));assert(!JSON.stringify(d).includes(md5(draftorFlash())));
});
test('factory recovery performs exact full restore without Draftor OTA assumption',async()=>{
 const image=await factoryFlash(),bytes=await packageFor(image),x=await sessionFor(bytes);x.flash.fill(0);try{
  await x.s.prepare(consent);await x.s.restore(consent);assert.deepEqual(x.flash,image);assert.equal(x.diagnostics.snapshot().checks.otaRead,'NOT_APPLICABLE_FACTORY');
 }finally{await x.s.close();}
});
for(const [label,options,code]of [
 ['same model other board',{macLo:2},'IDENTITY'],['different chip',{chipId:1},'CHIP'],['8MiB',{jedec:0x1740ef},'FLASH'],['secure boot',{securityFlags:1},'SECURITY'],['encryption',{cryptCount:1},'SECURITY'],['unknown flags',{securityFlags:0x800},'SECURITY'],['non ROM stub',{stub:true},'STUB']
])test(`original device verification rejects ${label}`,async()=>{const x=await sessionFor(good,options);try{await assert.rejects(x.s.prepare(consent),{code});assert.equal(written(x),0);}finally{await x.s.close();}});
for(const [label,mutation]of [['identity',x=>x.port.options.macLo=2],['capacity',x=>x.port.options.jedec=0x1740ef],['safe but changed security',x=>x.port.options.securityFlags=8]])test(`immediate prewrite recheck rejects ${label}`,async()=>{const x=await sessionFor();try{await x.s.prepare(consent);mutation(x);await assert.rejects(x.s.restore(consent));assert.equal(written(x),0);}finally{await x.s.close();}});
test('package change after preparation cannot retain the capability',async()=>{const x=await sessionFor();try{await x.s.prepare(consent);await assert.rejects(x.s.selectPackage(good),{code:'STATE'});await assert.rejects(x.s.restore(consent));assert.equal(written(x),0);}finally{await x.s.close();}});
test('changing the real identity after ROM grant revokes permission before FLASH_BEGIN',async()=>{const x=realRomFixture(),h=await validateRecoveryPackage(good);try{await x.rom.connect();await x.rom.grantRecovery(h,consent);x.port.options.macLo=2;await assert.rejects(x.rom.restoreRecovery(),{code:'IDENTITY'});assert.equal(written(x),0);}finally{destroyRecoveryPackage(h);await x.rom.disconnect();}});
test('destroyed package revokes ROM grant',async()=>{const x=realRomFixture(),h=await validateRecoveryPackage(good);try{await x.rom.connect();await x.rom.grantRecovery(h,consent);destroyRecoveryPackage(h);await assert.rejects(x.rom.restoreRecovery(),{code:'PACKAGE_REQUIRED'});assert.equal(written(x),0);}finally{await x.rom.disconnect();}});
test('mutual exclusion and no caller-supplied image/address',async()=>{
 for(const opts of [{candidate:true},{allowUpdate:true},{allowFirstInstall:true}])assert.throws(()=>new S3Rom({}, {...opts,allowRecovery:true}),{code:'BUILD_SCOPE'});
 const x=await sessionFor();try{await x.s.prepare(consent);await assert.rejects(x.rom.grantExclusiveMode({mode:'update'}));await assert.rejects(x.rom.writeImage(0,new Uint8Array(16)));assert.equal(written(x),0);}finally{await x.s.close();}
});
for(const op of [2,3])test(`attempted command ${op} failing is not misreported as no write; no retry`,async()=>{
 const x=await sessionFor(good,{throwOn:{op,nth:1}});try{await x.s.prepare(consent);await assert.rejects(x.s.restore(consent));assert.equal(x.s.state,'STOP');const n=x.port.transcript.length;await assert.rejects(x.s.restore(consent));assert.equal(x.port.transcript.length,n);
 const row=x.diagnostics.snapshot().commands[op===2?'FLASH_BEGIN':'FLASH_DATA'];assert.equal(row.attempted,1);assert.equal(row.sent,0);assert.equal(row.acceptedReplies,0);
 }finally{await x.s.close();}
});
test('corruption in actual destination detected by ROM MD5 (not source hash)',async()=>{const x=await sessionFor(good,{corruptWrite:true});try{await x.s.prepare(consent);await assert.rejects(x.s.restore(consent),{code:'RESTORE_VERIFY'});assert.equal(x.s.state,'STOP');assert.equal(x.diagnostics.snapshot().checks.fullFlashMd5Match,false);}finally{await x.s.close();}});
test('write timeout records uncertainty and terminal first-error stage',async()=>{const x=await sessionFor(good,{hangWriteOn:3});try{await x.s.prepare(consent);await assert.rejects(x.s.restore(consent));const d=x.diagnostics.snapshot();assert.equal(d.commands.FLASH_DATA.attempted,1);assert.equal(d.commands.FLASH_DATA.sent,0);assert.equal(d.commands.FLASH_DATA.acceptedReplies,0);assert.equal(d.firstError.stage,'RESTORING');const n=x.port.transcript.length;await assert.rejects(x.s.restore(consent));assert.equal(x.port.transcript.length,n);}finally{await x.s.close();}});
test('cancel during write stops further commands without reset',async()=>{const x=await sessionFor();try{await x.s.prepare(consent);await assert.rejects(x.s.restore({...consent,progress:()=>x.s.cancel()}));assert.equal(x.port.counts.get(2),1);assert.equal(x.port.counts.get(3),1);assert.equal(x.port.signalCount,0);}finally{await x.s.close();}});
test('close failure remains visible; private references still discarded',async()=>{const x=await sessionFor(good,{closeError:true});await x.s.prepare(consent);await assert.rejects(x.s.close());assert.equal(x.diagnostics.snapshot().closeResult,'FAILED_POSSIBLY_OPEN');assert.equal(x.diagnostics.snapshot().events.closeErrors,1);assert.equal(x.diagnostics.snapshot().checks.cleanup.sessionPrivateReferencesCleared,true);await assert.rejects(x.s.restore(consent));});
test('new read-only verification observes changed device, never a cached success',async()=>{
 const x=realRomFixture(draftorFlash(),{},{}),d=new ReadOnlyDriver(x.wire,{diagnostics:x.diagnostics});const s=new RecoverySession(d,{mode:'verify',diagnostics:x.diagnostics});
 try{await s.selectPackage(good);await s.prepare(consent);x.flash[0x9000]^=1;await assert.rejects(s.verifyAgain(),{code:'RESTORE_VERIFY'});assert.equal(written(x),0);}finally{await s.close();}
});
test('strict locked driver rejects every writer/grant even with valid imported package',async()=>{
 const x=realRomFixture(),d=new ReadOnlyDriver(x.wire,{diagnostics:x.diagnostics}),h=await validateRecoveryPackage(good);
 try{await d.connect();for(const call of [()=>d.writeImage(),()=>d.eraseAll(),()=>d.grantRecovery(h,consent),()=>d.restoreRecovery()])assert.throws(call,{code:'DEVICE_WRITES_LOCKED'});assert.equal(written(x),0);}finally{destroyRecoveryPackage(h);await d.disconnect();}
});
test('actual requestPort cancellation, open rejection and frame rejection have distinct counters',async()=>{
 const diag=new Diagnostics();await assert.rejects(requestSerialPort({requestPort:async()=>{throw Object.assign(Error(),{name:'NotFoundError'});}},diag));assert.equal(diag.snapshot().events.requestPortCancelled,1);
 for(const options of [{openError:true},{malformedOn:8}]){const x=realRomFixture(undefined,options);try{await assert.rejects(x.rom.connect());}finally{await x.rom.disconnect();}const d=x.diagnostics.snapshot();assert.equal(d.events.openAttempts,1);assert.equal(d.events.openSuccess,options.openError?0:1);assert.equal(d.events.openErrors,options.openError?1:0);assert.equal(d.commands.SYNC.acceptedReplies,0);}
});
test('backup capture is read-only and saved download must be re-imported against current full Flash',async()=>{
 const d=new SimulatedFlash(release);d.flash=draftorFlash();d.info={...identity};const s=new RecoverySession(d,{mode:'backup',build:'R9-test',sourceCommit:source});
 try{await s.prepare(consent);const b=await s.capture();assert.equal(s.state,'SAVED_FILE_REIMPORT_REQUIRED');await s.verifySavedPackage(b);assert.equal(s.state,'BACKUP_SAVED_VERIFIED');assert(!d.trace.some(t=>['write','eraseAll'].includes(t.op)));}finally{await s.close();}
});
test('existing current backup reused, stale backup rejected without a second full read',async()=>{
 const d=new SimulatedFlash(release);d.flash=draftorFlash();d.info={...identity};const s=new RecoverySession(d,{mode:'backup',build:'R9-test',sourceCommit:source});
 try{await s.prepare(consent);const stale=d.flash.slice();stale[0x9000]^=1;await assert.rejects(s.useExistingRaw(stale),{code:'BACKUP_STALE'});const b=await s.useExistingRaw(d.flash);d.flash[0x9000]^=1;await assert.rejects(s.verifySavedPackage(b),{code:'BACKUP_STALE'});assert.equal(s.state,'STOP');assert(!d.trace.some(t=>t.op==='read'&&t.size>8192));}finally{await s.close();}
});
