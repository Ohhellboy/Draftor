import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { verifyReleaseInputs } from '../tools/verify-release.mjs';
import { verifyRelease } from '../src/firmware.js';
import { StrictSerialTransport } from '../src/serial.js';
import { S3Rom } from '../src/rom.js';
import { InstallSession } from '../src/session.js';
import { SimulatedFlash, SIM_MAC } from '../src/simulator.js';
import { PINS } from '../src/pins.js';
import { FACTORY } from '../src/factory-pins.js';
import { RomDevicePort } from './rom-device.mjs';

const { release, inputs } = await verifyReleaseInputs({ verifyDesignBrief: false });
const factory = await readFile(new URL('../release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin', import.meta.url));
const hash = (b, alg = 'sha256') => createHash(alg).update(b).digest('hex');
assert.equal(factory.length, 4548144); assert.equal(hash(factory), FACTORY.sha256);
assert.equal(hash(Buffer.concat([Buffer.from(`blob ${factory.length}\0`),factory]), 'sha1'), FACTORY.git_blob_sha1);
const confirmation = { boardConfirmed: true, expectedMac: SIM_MAC };
const approval = { backupConfirmed: true, tfRemoved: true, planConfirmed: true, eraseConfirmed: true };
const evidence = new URL('../evidence/R6_20261121/software/', import.meta.url);
await mkdir(evidence, { recursive: true });

function setup(kind = 'factory', options = {}, enabled = true) {
  const flash = new Uint8Array(16777216).fill(255);
  if (kind === 'factory') flash.set(factory);
  const port = new RomDevicePort(flash, { firstInstall: true, reservedFF: true, emptySlip: true, ...options });
  const wire = new StrictSerialTransport(port, { writeTimeout: 150, closeTimeout: 150 });
  const bounded = { connect: () => wire.connect(), write: p => wire.write(p), read: t => wire.read(Math.min(t, 500)), getInfo: () => wire.getInfo(), disconnect: () => wire.disconnect(), cancel: () => wire.cancel() };
  const rom = new S3Rom(bounded, { allowFirstInstall: enabled });
  const session = new InstallSession(rom, release, { hardwareFreshWritesEnabled: enabled });
  return { flash, port, rom, session };
}
const writes = port => port.transcript.filter(p => [2,3].includes(p.op));
async function closed(x, fn) { try { await fn(x); } finally { await x.session.close(); } }

for (const kind of ['factory','blank']) test(`real ROM code on ${kind} fixture: full erase, four exact files, full initialized digest, no reset/stub/TF`, async () => closed(setup(kind), async ({flash,port,session}) => {
  const plan = await session.prepare(confirmation);
  assert.equal(plan.initialState, kind === 'factory' ? 'waveshare-factory-20260120' : 'blank');
  assert.equal(plan.mode,'fresh'); assert.equal(writes(port).length,0);
  const receipt = await session.execute(approval);
  assert.equal(receipt.status,'VERIFIED_BOOT_PENDING'); assert.equal(receipt.fullFlashInitialized,true);
  const expected = new Uint8Array(16777216).fill(255);
  for (const name of ['bootloader.bin','partitions.bin','boot_app0.bin','app.bin']) expected.set(inputs[name],PINS.files[name].offset);
  assert.equal(hash(flash),hash(expected)); assert.equal(hash(flash,'md5'),FACTORY.draftorInitializedFullMd5);
  assert.equal(port.counts.get(2),5); assert.equal(port.counts.get(3),3958); assert.equal(port.counts.get(8),1);
  assert.equal(port.signalCount,0); assert(port.transcript.every(p=>[2,3,8,9,10,11,13,14,19,20].includes(p.op)));
  await writeFile(new URL(`${kind}.json`,evidence),JSON.stringify({result:'PASS',physicalDeviceAccess:false,fixture:kind,receipt,opCounts:Object.fromEntries(port.counts),begin:port.transcript.filter(p=>p.op===2),fullInitializedSha256:hash(expected)},null,2)+'\n');
}));

test('known factory NVS/phy changes accepted only within the explicit mutable 0x9000..0x10000 region, no implicit erase', async () => closed(setup(),async ({flash,port,session})=>{
  flash.fill(0x5a,0x9000,0x10000); const p=await session.prepare(confirmation);
  assert.equal(p.initialState,'waveshare-factory-20260120'); assert.equal(writes(port).length,0);
}));
for (const at of [0,0x10000,4548143,4548144,16777215]) test(`factory fingerprint corruption at ${at}: STOP before any erase`,async()=>closed(setup(),async({flash,port,session})=>{
  flash[at]^=1; await assert.rejects(session.prepare(confirmation),{code:'FACTORY_FINGERPRINT'}); assert.equal(writes(port).length,0);
}));
test('unknown nonempty Flash with blank metadata is never classified as blank',async()=>closed(setup('blank'),async({flash,port,session})=>{
  flash[0x700000]=0; await assert.rejects(session.prepare(confirmation),{code:'UNKNOWN'}); assert.equal(writes(port).length,0);
}));
test('unknown partition table is not an erase permission',async()=>closed(setup(),async({flash,port,session})=>{
  flash[0x8000]^=1; await assert.rejects(session.prepare(confirmation),{code:'PARTITIONS'}); assert.equal(writes(port).length,0);
}));
test('board confirmation missing: no port open',async()=>closed(setup(),async({port,session})=>{
  await assert.rejects(session.prepare({...confirmation,boardConfirmed:false}),{code:'BOARD'}); assert.equal(port.openCount,0);
}));
for (const [options,code] of [[{chipId:0},'CHIP'],[{jedec:0x1740ef},'FLASH'],[{jedec:0},'FLASH'],[{securityFlags:1},'SECURITY'],[{securityFlags:4},'SECURITY'],[{securityFlags:0x800},'SECURITY'],[{cryptCount:1},'SECURITY'],[{macLo:2},'IDENTITY']]) test(`identity/security refusal ${JSON.stringify(options)}`,async()=>closed(setup('factory',options),async({port,session})=>{
  await assert.rejects(session.prepare(confirmation),{code});assert.equal(writes(port).length,0);
}));
for(const key of ['backupConfirmed','tfRemoved','planConfirmed','eraseConfirmed'])test(`factory recognition alone cannot bypass ${key}`,async()=>closed(setup(),async({port,session})=>{
  await session.prepare(confirmation);await assert.rejects(session.execute({...approval,[key]:false}),{code:key==='eraseConfirmed'?'ERASE_CONFIRM':'BACKUP'});assert.equal(writes(port).length,0);
}));
test('default locked driver/session reject actual factory write even with confirmations',async()=>closed(setup('factory',{},false),async({port,rom,session})=>{
  const p=await session.prepare(confirmation);assert.equal(p.hardwareWriteAllowed,false);
  await assert.rejects(session.execute(approval),{code:'FRESH_DEVICE_PENDING'});
  await assert.rejects(rom.eraseAll({...approval,source:'waveshare-factory-20260120',expectedMac:SIM_MAC}),{code:'FRESH_DEVICE_PENDING'});assert.equal(writes(port).length,0);
}));
test('low-level fresh flag alone cannot write without verified erase and four-step allowlist',async()=>closed(setup(),async({port,rom})=>{
  await rom.connect();await assert.rejects(rom.writeImage(0,inputs['bootloader.bin'],{mode:'fresh'}),{code:'STATE'});
  await assert.rejects(rom.eraseAll(),{code:'BACKUP'});assert.equal(writes(port).length,0);
}));
for(const name of Object.keys({...PINS.documents,...PINS.files}))test(`corrupt ${name} rejected before device access`,async()=>{
  const b=inputs[name].slice();b[0]^=1;await assert.rejects(verifyRelease({...inputs,[name]:b}),{code:'ASSET'});
});
test('changed identity after plan stops before erase',async()=>closed(setup(),async({port,session})=>{
  await session.prepare(confirmation);port.options.macLo=2;await assert.rejects(session.execute(approval),{code:'IDENTITY'});assert.equal(writes(port).length,0);
}));
test('changed factory app after plan stops before erase',async()=>closed(setup(),async({port,session,flash})=>{
  await session.prepare(confirmation);flash[0x20000]^=1;await assert.rejects(session.execute(approval),{code:'FACTORY_FINGERPRINT'});assert.equal(writes(port).length,0);
}));
for(const [label,op,nth]of [['erase interruption',2,1],['bootloader write interruption',3,1],['partition write interruption',3,21],['OTA write interruption',3,24],['App write interruption',3,32]])test(`${label}: partial STOP, no retry, following file, reset or recovery`,async()=>closed(setup('factory',{throwOn:{op,nth}}),async({port,session})=>{
  await session.prepare(confirmation);await assert.rejects(session.execute(approval));assert.equal(session.state,'STOP');assert.equal(session.receipt().flashOutcome,'POSSIBLY_PARTIAL');
  const count=port.transcript.length;await assert.rejects(session.execute(approval));assert.equal(port.transcript.length,count);assert.equal(port.signalCount,0);
}));
test('postwrite mismatch blocks the next file',async()=>closed(setup('factory',{corruptWrite:true}),async({port,session})=>{
  await session.prepare(confirmation);await assert.rejects(session.execute(approval),{code:'VERIFY'});assert.equal(port.counts.get(2),2);assert.equal(port.counts.get(3),20);
}));
for(const kind of ['disconnect','power-loss','page-cancel'])test(`${kind}: terminal cancellation prevents any subsequent device command`,async()=>closed(setup(),async({port,session,rom})=>{
  await session.prepare(confirmation);const original=rom.eraseAll.bind(rom);
  rom.eraseAll=async a=>{await original(a);session.cancel(kind==='page-cancel'?'CANCELLED':'DISCONNECTED');throw new Error(kind);};
  await assert.rejects(session.execute(approval));const count=port.transcript.length;await assert.rejects(session.execute(approval));assert.equal(port.transcript.length,count);assert.equal(port.counts.get(3),undefined);assert.equal(session.receipt().flashOutcome,'POSSIBLY_PARTIAL');
}));
test('incomplete erase cannot start a firmware write',async()=>closed(setup(),async({port,session,rom,flash})=>{
  await session.prepare(confirmation);const original=rom.eraseAll.bind(rom);rom.eraseAll=async a=>{await original(a);flash[0x9000]=0;};
  await assert.rejects(session.execute(approval),{code:'ERASE_VERIFY'});assert.equal(port.counts.get(3),undefined);
}));
test('R4 active-App update retains exact byte ranges and preserves NVS/OTA/inactive App',async()=>{
  const flash=new SimulatedFlash(release,'app0').flash, before=flash.slice();const port=new RomDevicePort(flash,{reservedFF:true,emptySlip:true});
  const rom=new S3Rom(new StrictSerialTransport(port),{allowUpdate:true});const session=new InstallSession(rom,release,{hardwareWritesEnabled:true});
  try {const p=await session.prepare(confirmation);assert.equal(p.mode,'update');await session.execute({...approval,eraseConfirmed:false});assert.equal(port.counts.get(2),1);assert.deepEqual(flash.slice(0,0x10000),before.slice(0,0x10000));assert.deepEqual(flash.slice(0x410000),before.slice(0x410000));assert.deepEqual(flash.slice(0x10000,0x10000+4021152),inputs['app.bin']);}
  finally{await session.close();}
});
test('first-install build cannot execute a compatible update',async()=>{
 const d=new SimulatedFlash(release,'app0');d.kind='hardware';const s=new InstallSession(d,release,{hardwareFreshWritesEnabled:true});await s.prepare(confirmation);await assert.rejects(s.execute({...approval,eraseConfirmed:false}),{code:'DEVICE_WRITES_LOCKED'});assert.equal(d.trace.filter(r=>r.op==='write'||r.op==='eraseAll').length,0);await s.close();
});
