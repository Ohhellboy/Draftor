// SPDX-License-Identifier: Apache-2.0
import { PINS } from './pins.js';
import { FACTORY } from './factory-pins.js';
import { requireSafe, sha256, md5, normalizeMac, checkPartitions, activeSlot, u32, deepFreeze } from './safety.js';
import { storePackage, readPackageArchive } from './recovery-zip.js';
export const RECOVERY_SCHEMA='draftor-private-recovery/1';
const te=new TextEncoder(),td=new TextDecoder('utf-8',{fatal:true});
const validated=new WeakMap();
const json=v=>JSON.stringify(v,null,2)+'\n';
const keys=(v,expected)=>requireSafe(v&&typeof v==='object'&&!Array.isArray(v)&&Object.keys(v).sort().join(',')===expected.split(',').sort().join(','),'PACKAGE_SCHEMA');
const text=(v,n=128)=>requireSafe(typeof v==='string'&&v.length>0&&v.length<=n&&!/[\u0000-\u001f]/.test(v),'PACKAGE_SCHEMA');
export function checkedIdentity(info) {
  requireSafe(info?.chip==='ESP32-S3'&&info.flashBytes===PINS.flashBytes,'CHIP');
  requireSafe(info.securityKnown===true&&info.secureBoot===false&&info.flashEncryption===false&&info.securityRule==='ESP32S3_ROM20_ESPt531_V1'&&info.romChipId===9,'SECURITY');
  const flags=info.securityFlags,count=info.cryptCount;
  requireSafe(Number.isInteger(flags)&&flags>=0&&(flags&~0x7ff)===0&&(flags&5)===0&&Number.isInteger(count)&&count>=0&&count<=7&&count.toString(2).split('1').length%2===1,'SECURITY');
  return {chip:info.chip,flashBytes:info.flashBytes,mac:normalizeMac(info.mac),security:{rule:info.securityRule,flags,cryptCount:count,romChipId:9,secureBoot:false,flashEncryption:false}};
}
export function sameOriginalIdentity(info,original) {
  const actual=checkedIdentity(info);
  requireSafe(actual.mac===original.mac,'IDENTITY');
  requireSafe(JSON.stringify(actual)===JSON.stringify(original),'SECURITY_CHANGED');return actual;
}
export async function classifyRecoveryImage(bytes) {
  requireSafe(bytes instanceof Uint8Array&&bytes.length===PINS.flashBytes,'PACKAGE_SIZE');
  const table=bytes.subarray(0x8000,0x9000),tableSha=await sha256(table);
  if(tableSha===FACTORY.tableSha256) {
    for(const pin of FACTORY.fingerprint)requireSafe(md5(bytes.subarray(pin.offset,pin.offset+pin.bytes))===pin.md5,'FACTORY_FINGERPRINT');
    return {firmware:'waveshare-factory-20260120',rule:'FACTORY_FIXED_THREE_RANGES_V1',partition:{offset:0x8000,bytes:4096,sha256:tableSha,md5:md5(table),rows:FACTORY.partitions},ota:{applicable:false},activeApp:'factory'};
  }
  const rows=checkPartitions(table);
  requireSafe(await sha256(table.subarray(0,3072))===PINS.files['partitions.bin'].sha256,'PARTITIONS');
  const ota=bytes.subarray(0xe000,0x10000);requireSafe(activeSlot(ota)===0,'UNQUALIFIED_SLOT');
  for(const name of ['bootloader.bin','app.bin']) { const p=PINS.files[name];requireSafe(await sha256(bytes.subarray(p.offset,p.offset+p.bytes))===p.sha256,'UNQUALIFIED_FIRMWARE'); }
  return {firmware:'draftor-alpha3-app0',rule:'ALPHA3_BOOT_APP0_B_TABLE_V1',partition:{offset:0x8000,bytes:4096,sha256:tableSha,md5:md5(table),rows},
    ota:{applicable:true,offset:0xe000,bytes:8192,sha256:await sha256(ota),md5:md5(ota),entries:[0,4096].map(at=>({sequence:u32(ota,at),state:u32(ota,at+24),crc:u32(ota,at+28)}))},activeApp:'app0'};
}
const readmes={
 'README.zh-CN.txt':'私人完整 Flash 恢复包。不要上传、公开或发送给他人。仅适用于创建时的原微雪 ESP32-S3-RLCD-4.2 / ST7305。恢复覆盖全部 16 MiB 内部 Flash，设置、配对及内部文稿回到备份时；TF 文件不在本包中，必须独立保留。恢复厂家备份会回到厂家演示，不会安装 Draftor。使用随附离线工具，选择本 ZIP 并连接原设备；校验通过后才确认恢复。完成后必须断电、插回 TF、普通启动验收。摘要和 MAC 只防误选及意外损坏，不是数字签名或身份认证。\n',
 'README.en.txt':'PRIVATE full Flash recovery package. Do not upload, publish or share. Only for the original Waveshare ESP32-S3-RLCD-4.2 / ST7305. Recovery overwrites all 16 MiB of internal Flash and rolls settings, pairing and internal documents back to backup time. TF files are NOT included; preserve TF separately. Restoring a factory backup returns the factory demo, not Draftor. Open the offline recovery tool, select this ZIP, connect the original board and confirm only after checks pass. After verification, power off, insert TF and validate normal boot. Hashes and MAC are accidental-corruption / wrong-device safeguards, not signatures or authentication.\n'
};
const proofMethods=['ROM_READ_SLOW_MD5_V1','ROM_MD5_MATCH_EXISTING_V1','ROM_ORIGINAL_RECONSTRUCTED_V1'];
function checkReconstruction(p){
  keys(p,'pointSha256,originalCheckedAt,mode,currentPrivateRangesUnchanged,currentFlashIsOriginal');
  requireSafe(/^[0-9a-f]{64}$/.test(p.pointSha256)&&/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(p.originalCheckedAt)&&Number.isFinite(Date.parse(p.originalCheckedAt)),'PACKAGE_PROOF');
  requireSafe(['fresh','update'].includes(p.mode)&&p.currentPrivateRangesUnchanged===(p.mode==='update')&&p.currentFlashIsOriginal===false,'PACKAGE_PROOF');
}
export async function createRecoveryPackage(bytes,info,{boardConfirmed,romMd5Before,romMd5After,method,build,sourceCommit,reconstruction,createdAt=new Date().toISOString(),id=crypto.randomUUID()}={}) {
  requireSafe(boardConfirmed===true,'BOARD_CONFIRM');const image=bytes.slice(),device=checkedIdentity(info),digest=md5(image);
  requireSafe(romMd5Before===digest&&romMd5After===digest,'BACKUP_STALE');
  requireSafe(proofMethods.includes(method),'PACKAGE_PROOF');
  if(method==='ROM_ORIGINAL_RECONSTRUCTED_V1')checkReconstruction(reconstruction);else requireSafe(reconstruction===undefined,'PACKAGE_PROOF');
  const original=await classifyRecoveryImage(image);
  const manifest={schema:RECOVERY_SCHEMA,id,createdAt,tool:{name:'Draftor Recovery',build,sourceCommit},board:{model:'ESP32-S3-RLCD-4.2',controller:'ST7305',confirmation:'USER_PHYSICAL_CONFIRMATION'},device,
    image:{file:'flash.bin',offset:0,bytes:PINS.flashBytes,sha256:await sha256(image),md5:digest},creationProof:{method,romMd5Before,romMd5After,romMd5Match:true,identitySource:'ROM_READ_REG_EFUSE',securitySource:'ROM_GET_SECURITY_INFO',capacitySource:'JEDEC_RDID',...(reconstruction?{reconstruction}:{})},original};
  const entries={'manifest.json':te.encode(json(manifest)),'flash.bin':image,...Object.fromEntries(Object.entries(readmes).map(([k,v])=>[k,te.encode(v)]))};
  const archive=storePackage(entries);const checked=await validateRecoveryPackage(archive);destroyRecoveryPackage(checked);image.fill(0);return archive;
}
export async function validateRecoveryPackage(input) {
  const entries=readPackageArchive(input),raw=td.decode(entries['manifest.json']);let m;
  try { m=JSON.parse(raw); } catch { requireSafe(false,'PACKAGE_JSON'); }
  requireSafe(json(m)===raw,'PACKAGE_JSON'); // Canonical JSON also rejects duplicate keys.
  keys(m,'schema,id,createdAt,tool,board,device,image,creationProof,original');requireSafe(m.schema===RECOVERY_SCHEMA,'PACKAGE_SCHEMA');
  requireSafe(/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(m.id),'PACKAGE_SCHEMA');
  requireSafe(typeof m.createdAt==='string'&&/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(m.createdAt)&&Number.isFinite(Date.parse(m.createdAt)),'PACKAGE_SCHEMA');
  keys(m.tool,'name,build,sourceCommit');requireSafe(m.tool.name==='Draftor Recovery','PACKAGE_SCHEMA');text(m.tool.build);requireSafe(/^[0-9a-f]{40}$/.test(m.tool.sourceCommit),'PACKAGE_SCHEMA');
  requireSafe(JSON.stringify(m.board)===JSON.stringify({model:'ESP32-S3-RLCD-4.2',controller:'ST7305',confirmation:'USER_PHYSICAL_CONFIRMATION'}),'BOARD_CONFIRM');
  keys(m.device,'chip,flashBytes,mac,security');keys(m.device.security,'rule,flags,cryptCount,romChipId,secureBoot,flashEncryption');
  const s=m.device.security,normalized=checkedIdentity({...m.device,securityKnown:true,secureBoot:s.secureBoot,flashEncryption:s.flashEncryption,securityRule:s.rule,securityFlags:s.flags,cryptCount:s.cryptCount,romChipId:s.romChipId});
  requireSafe(JSON.stringify(normalized)===JSON.stringify(m.device),'PACKAGE_SCHEMA');
  keys(m.image,'file,offset,bytes,sha256,md5');const image=entries['flash.bin'];
  requireSafe(m.image.file==='flash.bin'&&m.image.offset===0&&m.image.bytes===PINS.flashBytes&&m.image.sha256===await sha256(image)&&m.image.md5===md5(image),'PACKAGE_HASH');
  const rebuilt=m.creationProof?.method==='ROM_ORIGINAL_RECONSTRUCTED_V1';
  keys(m.creationProof,'method,romMd5Before,romMd5After,romMd5Match,identitySource,securitySource,capacitySource'+(rebuilt?',reconstruction':''));
  if(rebuilt)checkReconstruction(m.creationProof.reconstruction);
  requireSafe(m.creationProof.identitySource==='ROM_READ_REG_EFUSE'&&m.creationProof.securitySource==='ROM_GET_SECURITY_INFO'&&m.creationProof.capacitySource==='JEDEC_RDID','PACKAGE_PROOF');
  requireSafe(proofMethods.includes(m.creationProof.method)&&m.creationProof.romMd5Match===true&&m.creationProof.romMd5Before===m.image.md5&&m.creationProof.romMd5After===m.image.md5,'PACKAGE_PROOF');
  if(rebuilt)requireSafe(m.original.firmware===(m.creationProof.reconstruction.mode==='fresh'?'waveshare-factory-20260120':'draftor-alpha3-app0'),'PACKAGE_PROOF');
  requireSafe(JSON.stringify(await classifyRecoveryImage(image))===JSON.stringify(m.original),'PACKAGE_METADATA');
  for(const [name,content]of Object.entries(readmes))requireSafe(td.decode(entries[name])===content,'PACKAGE_FORMAT');
  const handle=Object.freeze({});validated.set(handle,{manifest:deepFreeze(m),image});return handle;
}
// No caller-owned mutable byte array is retained by a validated handle or writer.
export function recoverySnapshot(handle) { const p=validated.get(handle);requireSafe(p,'PACKAGE_REQUIRED');return {manifest:structuredClone(p.manifest),image:p.image.slice()}; }
export function recoverySummary(handle) {const p=validated.get(handle);requireSafe(p,'PACKAGE_REQUIRED');return {createdAt:p.manifest.createdAt,firmware:p.manifest.original.firmware,bytes:PINS.flashBytes,activeApp:p.manifest.original.activeApp};}
export function destroyRecoveryPackage(handle) {const p=validated.get(handle);if(p){p.image.fill(0);validated.delete(handle);} }
