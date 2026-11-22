// SPDX-License-Identifier: Apache-2.0
// R12 protection is NOT a full backup. Importing a file never grants installation.
import {PINS} from './pins.js';
import {FACTORY} from './factory-pins.js';
import {releaseFile} from './firmware.js';
import {requireSafe,sha256,md5} from './safety.js';
import {checkedIdentity,sameOriginalIdentity,classifyRecoveryImage,createRecoveryPackage} from './recovery-package.js';
import {inspectSupportedOriginal} from './recovery-session.js';

export const POINT_SCHEMA='draftor-install-protection/1';
export const APP_END=0x10000+Math.ceil(PINS.files['app.bin'].bytes/4096)*4096;
export const PRIVATE_RANGES=[[0,0x10000],[APP_END,PINS.flashBytes-APP_END]];
const te=new TextEncoder(),td=new TextDecoder('utf-8',{fatal:true}),live=new WeakMap();
const canonical=v=>JSON.stringify(v)+'\n';
const hex=b=>Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');
const unhex=s=>Uint8Array.from(s.match(/../g)||[],x=>parseInt(x,16));
const hasKeys=(v,s)=>requireSafe(v&&Object.keys(v).sort().join(',')===s.split(',').sort().join(','),'PROTECTION_FORMAT');
const digest=(v,n)=>typeof v==='string'&&new RegExp(`^[0-9a-f]{${n}}$`).test(v);
export async function verifiedFactory(input){requireSafe(input instanceof Uint8Array&&input.length===FACTORY.bytes&&await sha256(input)===FACTORY.sha256,'FACTORY_ASSET');return input.slice();}
async function factoryImage(point,factory){const b=new Uint8Array(PINS.flashBytes).fill(255);b.set(await verifiedFactory(factory));b.set(unhex(point.saved.hex),0x9000);requireSafe(md5(b)===point.originalMd5,'PROTECTION_REBUILD');await classifyRecoveryImage(b);return b;}
async function encode(point){return te.encode(canonical({point,sha256:await sha256(te.encode(canonical(point)))}));}
export async function decodeProtection(input){
 requireSafe(input instanceof Uint8Array&&input.length>100&&input.length<70000,'PROTECTION_SIZE');
 let raw,m;try{raw=td.decode(input);m=JSON.parse(raw);}catch{requireSafe(false,'PROTECTION_FORMAT');}
 requireSafe(canonical(m)===raw,'PROTECTION_FORMAT');hasKeys(m,'point,sha256');
 const p=m.point;hasKeys(p,'schema,id,createdAt,release,appSha256,mode,device,originalMd5,saved,protectedRanges');
 requireSafe(p.schema===POINT_SCHEMA&&p.release===PINS.version&&p.appSha256===PINS.files['app.bin'].sha256&&['fresh','update'].includes(p.mode),'PROTECTION_SCOPE');
 requireSafe(/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(p.id)&&/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(p.createdAt)&&Number.isFinite(Date.parse(p.createdAt)),'PROTECTION_FORMAT');
 const s=p.device?.security;requireSafe(s,'PROTECTION_FORMAT');
 const device=checkedIdentity({...p.device,securityKnown:true,secureBoot:s.secureBoot,flashEncryption:s.flashEncryption,securityRule:s.rule,securityFlags:s.flags,cryptCount:s.cryptCount,romChipId:s.romChipId});
 requireSafe(JSON.stringify(device)===JSON.stringify(p.device)&&digest(p.originalMd5,32),'PROTECTION_FORMAT');
 hasKeys(p.saved,'offset,bytes,hex,md5');
 const offset=p.mode==='fresh'?0x9000:0x10000+PINS.files['app.bin'].bytes,size=p.mode==='fresh'?0x7000:APP_END-offset;
 requireSafe(p.saved.offset===offset&&p.saved.bytes===size&&digest(p.saved.hex,size*2)&&md5(unhex(p.saved.hex))===p.saved.md5,'PROTECTION_FORMAT');
 const ranges=p.mode==='fresh'?[]:PRIVATE_RANGES;requireSafe(Array.isArray(p.protectedRanges)&&p.protectedRanges.length===ranges.length,'PROTECTION_FORMAT');
 for(let i=0;i<ranges.length;i++){const r=p.protectedRanges[i];hasKeys(r,'offset,bytes,md5');requireSafe(r.offset===ranges[i][0]&&r.bytes===ranges[i][1]&&digest(r.md5,32),'PROTECTION_FORMAT');}
 requireSafe(m.sha256===await sha256(te.encode(canonical(p))),'PROTECTION_HASH');return p;
}
export async function captureProtection(driver,plan,{factory,alive=()=>{}}={}){
 const info=await driver.identity(),device=checkedIdentity(info),originalMd5=await driver.flashMd5(0,PINS.flashBytes);
 const original=await inspectSupportedOriginal(driver);alive();
 requireSafe(original.firmware===(plan.mode==='fresh'?'waveshare-factory-20260120':'draftor-alpha3-app0'),'STALE_PLAN');
 const offset=plan.mode==='fresh'?0x9000:0x10000+PINS.files['app.bin'].bytes,size=plan.mode==='fresh'?0x7000:APP_END-offset;
 const b=await driver.readFlash(offset,size);alive();requireSafe(b.length===size&&await driver.flashMd5(offset,size)===md5(b),'READBACK');
 const ranges=[];for(const [offset,bytes] of plan.mode==='fresh'?[]:PRIVATE_RANGES){alive();ranges.push({offset,bytes,md5:await driver.flashMd5(offset,bytes)});}
 const p={schema:POINT_SCHEMA,id:crypto.randomUUID(),createdAt:new Date().toISOString(),release:PINS.version,appSha256:PINS.files['app.bin'].sha256,mode:plan.mode,device,originalMd5,saved:{offset,bytes:size,hex:hex(b),md5:md5(b)},protectedRanges:ranges};b.fill(0);
 if(plan.mode==='fresh'){const rebuilt=await factoryImage(p,factory);rebuilt.fill(0);}
 sameOriginalIdentity(await driver.identity(),device);requireSafe(await driver.flashMd5(0,PINS.flashBytes)===originalMd5,'PROTECTION_STALE');alive();
 const bytes=await encode(p),handle=Object.freeze({});live.set(handle,{point:p,bytes,saved:false});return handle;
}
export function protectionBytes(handle){const x=live.get(handle);requireSafe(x,'PROTECTION_REQUIRED');return x.bytes.slice();}
export async function confirmProtectionSaved(handle,input){const x=live.get(handle);requireSafe(x,'PROTECTION_REQUIRED');x.saved=false;await decodeProtection(input);requireSafe(await sha256(input)===await sha256(x.bytes),'PROTECTION_FILE');requireSafe(live.get(handle)===x,'CANCELLED');x.saved=true;}
export function savedProtection(handle,{mode,mac}={}){const x=live.get(handle);requireSafe(x?.saved,'PROTECTION_SAVE');requireSafe(x.point.mode===mode&&x.point.device.mac===mac,'PROTECTION_SCOPE');return structuredClone(x.point);}
export function destroyProtection(handle){const x=live.get(handle);if(x){x.bytes.fill(0);live.delete(handle);}}

// Recovery preparation is read-only. An exact ORIGINAL full image must pass the
// existing package validator/writer. Never install onto an unrecognized App.
export async function reconstructOriginal(driver,input,release,{factory,boardConfirmed,tfRemoved,build,sourceCommit,alive=()=>{},progress=()=>{}}={}){
 requireSafe(boardConfirmed===true&&tfRemoved===true,'BOARD_CONFIRM');const point=await decodeProtection(input);
 const info=await driver.identity();sameOriginalIdentity(info,point.device);alive();let image;
 try{
  if(point.mode==='fresh')image=await factoryImage(point,factory);
  else{
   for(const r of point.protectedRanges){requireSafe(await driver.flashMd5(r.offset,r.bytes)===r.md5,'PRIVATE_CHANGED');alive();}
   const before=await driver.flashMd5(0,PINS.flashBytes);image=new Uint8Array(PINS.flashBytes);
   for(let offset=0;offset<image.length;offset+=65536){alive();const part=await driver.readFlash(offset,65536);requireSafe(part.length===65536,'READBACK');image.set(part,offset);part.fill(0);progress((offset+65536)/image.length);}
   requireSafe(md5(image)===before&&await driver.flashMd5(0,PINS.flashBytes)===before,'READBACK');
   for(const r of point.protectedRanges)requireSafe(md5(image.subarray(r.offset,r.offset+r.bytes))===r.md5,'PRIVATE_CHANGED');
   image.set(releaseFile(release,'app.bin'),0x10000);image.set(unhex(point.saved.hex),point.saved.offset);
  }
  sameOriginalIdentity(await driver.identity(),point.device);alive();requireSafe(md5(image)===point.originalMd5,'PROTECTION_REBUILD');
  await classifyRecoveryImage(image);
  return await createRecoveryPackage(image,info,{boardConfirmed:true,romMd5Before:point.originalMd5,romMd5After:point.originalMd5,method:'ROM_ORIGINAL_RECONSTRUCTED_V1',build,sourceCommit,
   reconstruction:{pointSha256:await sha256(input),originalCheckedAt:point.createdAt,mode:point.mode,currentPrivateRangesUnchanged:point.mode==='update',currentFlashIsOriginal:false}});
 }finally{image?.fill(0);}
}
