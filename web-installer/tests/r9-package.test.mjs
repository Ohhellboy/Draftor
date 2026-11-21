import test from 'node:test';import assert from 'node:assert/strict';
import { mkdtemp,writeFile,readFile,mkdir } from 'node:fs/promises';import { join } from 'node:path';
import { packageFor,draftorFlash,factoryFlash,identity,source } from './r9-fixture.mjs';
import { validateRecoveryPackage,recoverySnapshot,destroyRecoveryPackage,createRecoveryPackage } from '../src/recovery-package.js';
import { storePackage,readPackageArchive } from '../src/recovery-zip.js';
import { md5 } from '../src/safety.js';
const good=await packageFor(draftorFlash());
const changeManifest=fn=>{const e=readPackageArchive(good),m=JSON.parse(new TextDecoder().decode(e['manifest.json']));fn(m);e['manifest.json']=new TextEncoder().encode(JSON.stringify(m,null,2)+'\n');return storePackage(e);};
test('saved file reopens after original handle destruction; immutable snapshot',async()=>{
 await mkdir('.test-cache',{recursive:true});const dir=await mkdtemp('.test-cache/r9-package-');{
  const h=await validateRecoveryPackage(good);const snap=recoverySnapshot(h);snap.image[0]^=1;snap.manifest.device.mac='020000000002';
  await writeFile(join(dir,'private-synthetic.zip'),good);destroyRecoveryPackage(h);assert.throws(()=>recoverySnapshot(h));
  const reloaded=await validateRecoveryPackage(new Uint8Array(await readFile(join(dir,'private-synthetic.zip'))));
  assert.deepEqual(recoverySnapshot(reloaded).image,draftorFlash());destroyRecoveryPackage(reloaded);
 }
});
test('factory package supported without inventing Draftor OTA',async()=>{const h=await validateRecoveryPackage(await packageFor(await factoryFlash()));const s=recoverySnapshot(h);assert.equal(s.manifest.original.ota.applicable,false);assert.equal(s.manifest.original.activeApp,'factory');destroyRecoveryPackage(h);});
for(const [label,fn] of [
 ['schema',m=>m.schema='2'],['image offset',m=>m.image.offset=65536],['image size',m=>m.image.bytes--],['digest',m=>m.image.sha256='0'.repeat(64)],
 ['extra keys',m=>m.force=true],['unknown original',m=>m.original.firmware='other'],['false metadata',m=>m.original.ota.entries[0].sequence++],
 ['missing ROM proof',m=>delete m.creationProof],['assumed security',m=>delete m.device.security.flags],['encryption',m=>m.device.security.cryptCount=1],
 ['unknown flags',m=>m.device.security.flags=0x800],['wrong chip',m=>m.device.chip='ESP32'],['wrong capacity',m=>m.device.flashBytes=8388608],
 ['unknown board',m=>m.board.controller='ST7306'],['missing identity',m=>m.device.mac='000000000000'],['unknown rule',m=>m.device.security.rule='trust-me']
])test(`manifest rejects ${label}`,async()=>assert.rejects(validateRecoveryPackage(changeManifest(fn))));
test('duplicate JSON key rejected even with valid ZIP CRC',async()=>{const e=readPackageArchive(good),txt=new TextDecoder().decode(e['manifest.json']);e['manifest.json']=new TextEncoder().encode(txt.replace('{\n','{\n  "schema": "duplicate",\n'));await assert.rejects(validateRecoveryPackage(storePackage(e)),{code:'PACKAGE_JSON'});});
for(const [label,mutate]of [
 ['truncated',b=>b.slice(0,-1)],['trailing',b=>{const o=new Uint8Array(b.length+1);o.set(b);return o;}],
 ['CRC corruption',b=>{b[50000]^=1;return b;}],['local extra',b=>{b[28]=1;return b;}],['compressed',b=>{b[8]=8;return b;}],['encrypted',b=>{b[6]=1;return b;}],
 ['path traversal',b=>{b[30]=46;return b;}],['entry count',b=>{b[b.length-14]=255;return b;}],['directory overflow',b=>{b.fill(255,b.length-6,b.length-2);return b;}],
 ['ZIP64',b=>{b[b.length-14]=255;b[b.length-13]=255;return b;}],['descriptor',b=>{b[6]=8;return b;}],['split disk',b=>{b[b.length-18]=1;return b;}],
 ['duplicate central name',b=>{const v=new DataView(b.buffer),d=v.getUint32(b.length-6,true),next=d+46+13;b.set(new TextEncoder().encode('manifest.'),next+46);return b;}]
])test(`bounded ZIP rejects ${label}`,async()=>assert.rejects(validateRecoveryPackage(mutate(good.slice()))));
test('oversize archive rejected before parsing or allocation',async()=>assert.rejects(validateRecoveryPackage(new Uint8Array(16777216+32769)),{code:'PACKAGE_SIZE'}));
for(const [label,change]of [['App',b=>b[0x10000]^=1],['bootloader',b=>b[1]^=1],['table',b=>b[0x8000]^=1],['OTA',b=>b[0xe000]^=1]])test(`unknown original ${label} rejected even when image hashes are recomputed`,async()=>{const b=draftorFlash();change(b);await assert.rejects(packageFor(b));});
test('fabricated or missing ROM/security observations cannot be defaulted',async()=>{const b=draftorFlash();await assert.rejects(createRecoveryPackage(b,{...identity,securityRule:undefined},{boardConfirmed:true,romMd5Before:md5(b),romMd5After:md5(b),method:'ROM_MD5_MATCH_EXISTING_V1',build:'test',sourceCommit:source}),{code:'SECURITY'});});
