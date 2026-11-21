import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { verifyReleaseInputs } from '../tools/verify-release.mjs';
import { InstallSession } from '../src/session.js';
import { S3Rom } from '../src/rom.js';
import { StrictSerialTransport } from '../src/serial.js';
import { SimulatedFlash } from '../src/simulator.js';
import { RomDevicePort } from './rom-device.mjs';
import { PINS } from '../src/pins.js';
import { md5, sha256 } from '../src/safety.js';
const { release, inputs } = await verifyReleaseInputs({verifyDesignBrief:false});
const factory = new Uint8Array(await readFile('release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin'));
const approve={backupConfirmed:true,tfRemoved:true,planConfirmed:true};
function setup(kind='app0',options={}){
 let flash=new SimulatedFlash(release,kind==='factory'?'fresh':kind).flash;
 if(kind==='factory')flash.set(factory);
 if(['app0','app1'].includes(kind))flash.set(inputs['app.bin'],kind==='app0'?0x10000:0x410000);
 const port=new RomDevicePort(flash,{firstInstall:kind==='factory',emptySlip:true,reservedFF:true,...options});
 const wire=new StrictSerialTransport(port,{writeTimeout:300,closeTimeout:300});
 const rom=new S3Rom(wire,{candidate:true});
 const s=new InstallSession(rom,release,{candidate:true,bindSelectedIdentity:true});
 return {flash,port,rom,s};
}
const writes=x=>x.port.transcript.filter(t=>[2,3].includes(t.op));
async function run(kind,fn,options={}){const x=setup(kind,options);try{await fn(x);}finally{await x.s.close();}}
for(const kind of ['app0','factory'])test(`${kind}: unique mode, saved backup gate, exact writes, independent verification`,()=>run(kind,async x=>{
 const before=x.flash.slice();const p=await x.s.prepare({boardConfirmed:true});assert.equal(writes(x).length,0);assert.equal(p.hardwareWriteAllowed,true);
 assert.equal(x.s.backupVerified,false);await x.s.verifySavedBackup(before);assert.equal(x.s.backupVerified,true);
 const r=await x.s.execute({...approve,eraseConfirmed:kind==='factory'});assert.equal(r.status,'VERIFIED_BOOT_PENDING');assert.equal(r.resetIssued,false);assert.equal(x.port.openCount,1);assert.equal(x.port.signalCount,0);
 assert.equal(x.port.counts.get(2),kind==='factory'?5:1);assert.equal(x.port.counts.get(3),kind==='factory'?3958:3927);
 if(kind==='app0'){assert.deepEqual(x.flash.slice(0,0x10000),before.slice(0,0x10000));assert.deepEqual(x.flash.slice(0x410000),before.slice(0x410000));assert(r.protectedRegionsUnchanged);}
 assert(!JSON.stringify(r).includes(md5(before)));assert(!JSON.stringify(r).includes(x.s.selectedIdentity));
 const count=x.port.transcript.length;await assert.rejects(x.s.execute(approve));assert.equal(x.port.transcript.length,count);
}));
for(const [kind,code]of [['app1','UNQUALIFIED_SLOT'],['partition','PARTITIONS'],['pending','OTA'],['unknown','UNKNOWN']])test(`${kind} refuses with no writes or fallback`,()=>run(kind,async x=>{await assert.rejects(x.s.prepare({boardConfirmed:true}),{code});assert.equal(writes(x).length,0);}));
test('unknown app0 firmware is refused despite matching B layout',()=>run('app0',async x=>{x.flash[0x10000+123]^=1;await assert.rejects(x.s.prepare({boardConfirmed:true}),{code:'UNQUALIFIED_FIRMWARE'});assert.equal(writes(x).length,0);}));
test('factory unknown tail is refused',()=>run('factory',async x=>{x.flash[0xffffff]=0;await assert.rejects(x.s.prepare({boardConfirmed:true}),{code:'FACTORY_FINGERPRINT'});assert.equal(writes(x).length,0);}));
test('proven blank is recognized but hardware write remains locked',()=>run('fresh',async x=>{const p=await x.s.prepare({boardConfirmed:true});assert.equal(p.mode,'fresh');assert.equal(p.hardwareWriteAllowed,false);await x.s.verifySavedBackup(x.flash);await assert.rejects(x.s.execute({...approve,eraseConfirmed:true}),{code:'UNQUALIFIED_BLANK'});assert.equal(writes(x).length,0);}));
for(const [options,code]of [[{chipId:0},'CHIP'],[{jedec:0x1740ef},'FLASH'],[{securityFlags:1},'SECURITY'],[{cryptCount:1},'SECURITY'],[{usb:{usbVendorId:1,usbProductId:1}},'USB_TRANSPORT']])test(`hardware/security refusal ${code}`,()=>run('app0',async x=>{await assert.rejects(x.s.prepare({boardConfirmed:true}),{code});assert.equal(writes(x).length,0);},options));
test('checkbox alone does not establish a saved backup',()=>run('app0',async x=>{await x.s.prepare({boardConfirmed:true});await assert.rejects(x.s.execute(approve),{code:'BACKUP_FILE'});assert.equal(writes(x).length,0);}));
for(const fault of ['short','wrong'])test(`saved backup ${fault} is refused`,()=>run('app0',async x=>{await x.s.prepare({boardConfirmed:true});const b=x.flash.slice(0,fault==='short'?-1:undefined);if(fault==='wrong')b[0x9000]^=1;await assert.rejects(x.s.verifySavedBackup(b),{code:'BACKUP_FILE'});assert.equal(writes(x).length,0);}));
for(const [label,change,code]of [
 ['identity',x=>{x.port.options.macLo=2;},'IDENTITY'],
 ['partition',x=>{x.flash[0x8000]^=1;},'STALE_PLAN'],
 ['ota',x=>{x.flash[0xe000]^=1;},'STALE_PLAN'],
 ['private settings',x=>{x.flash[0x9000]^=1;},'BACKUP_STALE']
])test(`change after backup: ${label} stops before grant`,()=>run('app0',async x=>{await x.s.prepare({boardConfirmed:true});await x.s.verifySavedBackup(x.flash);change(x);await assert.rejects(x.s.execute(approve),{code});assert.equal(writes(x).length,0);}));
for(const key of ['backupConfirmed','tfRemoved','planConfirmed','eraseConfirmed'])test(`factory independent ${key} confirmation retained`,()=>run('factory',async x=>{await x.s.prepare({boardConfirmed:true});await x.s.verifySavedBackup(x.flash);await assert.rejects(x.s.execute({...approve,eraseConfirmed:true,[key]:false}),{code:key==='eraseConfirmed'?'ERASE_CONFIRM':'BACKUP'});assert.equal(writes(x).length,0);}));
for(const [kind,options,code]of [['app0',{corruptWrite:true},'VERIFY'],['app0',{throwOn:{op:3,nth:1}},null],['factory',{throwOn:{op:2,nth:1}},null]])test(`${kind} fault ${JSON.stringify(options)} is terminal/partial`,()=>run(kind,async x=>{await x.s.prepare({boardConfirmed:true});await x.s.verifySavedBackup(x.flash);await assert.rejects(x.s.execute({...approve,eraseConfirmed:kind==='factory'}),code?{code}:undefined);assert.equal(x.s.receipt().flashOutcome,'POSSIBLY_PARTIAL');const n=x.port.transcript.length;await assert.rejects(x.s.execute(approve));assert.equal(x.port.transcript.length,n);assert.equal(x.port.signalCount,0);},options));
test('ROM candidate cannot bypass delayed grant or original two-flag mutex',async()=>{
 assert.throws(()=>new S3Rom({}, {allowUpdate:true,allowFirstInstall:true}),{code:'BUILD_SCOPE'});
 assert.throws(()=>new S3Rom({}, {candidate:true,allowUpdate:true}),{code:'BUILD_SCOPE'});
 await run('app0',async x=>{await x.s.prepare({boardConfirmed:true});await assert.rejects(x.rom.writeImage(0x10000,inputs['app.bin'],{mode:'update',slotOffset:0x10000}),{code:'DEVICE_WRITES_LOCKED'});assert.equal(writes(x).length,0);});
});
test('granted update capability cannot select app1 or switch to fresh',()=>run('app0',async x=>{
 await x.s.prepare({boardConfirmed:true});const table=x.flash.slice(0x8000,0x9000),ota=x.flash.slice(0xe000,0x10000);
 await x.rom.grantExclusiveMode({mode:'update',initialState:'compatible-draftor',expectedMac:x.s.selectedIdentity,metadataFingerprint:`${await sha256(table)}:${await sha256(ota)}`,backupMd5:md5(x.flash)});
 await assert.rejects(x.rom.writeImage(0x410000,inputs['app.bin'],{mode:'update',slotOffset:0x410000}),{code:'UNQUALIFIED_SLOT'});
 await assert.rejects(x.rom.grantExclusiveMode({mode:'fresh'}),{code:'BUILD_SCOPE'});await assert.rejects(x.rom.eraseAll({...approve,eraseConfirmed:true}),{code:'FRESH_DEVICE_PENDING'});assert.equal(writes(x).length,0);
}));
test('capture is read-only and download completion is not backup proof; saved-file reimport is required',async()=>{
 const d=new SimulatedFlash(release);d.flash.set(inputs['app.bin'],0x10000);const s=new InstallSession(d,release,{candidate:true,bindSelectedIdentity:true});
 await s.prepare({boardConfirmed:true});const b=await s.captureBackup();assert.deepEqual(b,d.flash);assert.equal(s.backupVerified,false);assert.equal(s.receipt().backup,'SAVED_FILE_REIMPORT_REQUIRED');
 await s.verifySavedBackup(b);assert(s.backupVerified);assert(!d.trace.some(t=>['write','eraseAll'].includes(t.op)));await s.close();assert.equal(s.backupVerified,false);
});
